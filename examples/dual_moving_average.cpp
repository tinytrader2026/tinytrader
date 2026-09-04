#include "simnow.h"

using namespace std;
using namespace chrono;
using namespace tinytrader;

namespace {
	struct Bar
	{
		TimePoint BeginTime;
		double Open = 0;
		double High = 0;
		double Low = 0;
		double Close = 0;
		double OpenInterest = 0;
		int Volume = 0;
		bool HasQuote = false;
	};

	constexpr bool operator==(TimePoint tp, const string& time)
	{
		return tp == TimeAt(time.c_str(), tp);
	}

	class MinuteBarStrategy : public Strategy
	{
	public:
		MinuteBarStrategy(string target, string active, string closeTime = "15:00:00")
		{
			mTarget = target;			// 交易合约
			mActive = active;			// 活跃合约，用做时钟源
			InitSchedule(closeTime);	// 设置开盘、收盘及休市时间
			Subscribe(mActive);
			Subscribe(mTarget);
		}

		Contract Target() const
		{
			return mTarget;
		}

	private:
		virtual void OnBar(const Bar& bar) = 0;

		void OnQuote(const Quote& q) override
		{
			TimePoint begin = MinuteBegin(q.MarketTime);
			if (begin > mBar.BeginTime) {
				CloseBar();
				mBar = { begin };
				mBar.Open = mBar.High = mBar.Low = mBar.Close = mPrice;
				mBar.OpenInterest = mOpenInterest;
			}

			if (!mHasTask && begin + 60s == mCloseTime) {
				mHasTask = true;
				DelayTask(63000, [this] { CloseBar(); });	// 收盘 3s 后补齐最后一根K线
			}

			if (q.Instrument == mTarget) {
				mPrice = q.Price;
				mOpenInterest = q.OpenInterest;
				if (!mBar.HasQuote)
					mBar.Open = mBar.High = mBar.Low = mBar.Close = mPrice;
				else {
					mBar.High = max(mBar.High, mPrice);
					mBar.Low = min(mBar.Low, mPrice);
					mBar.Close = mPrice;
				}
				mBar.OpenInterest = mOpenInterest;
				mBar.Volume += q.Volume - mTotalVolume;
				mTotalVolume = q.Volume;
				mBar.HasQuote = true;
			}
		}

		void InitSchedule(string closeTime)
		{
			mCloseTime = closeTime;
			if (closeTime == "15:00:00" || closeTime == "15:15:00") {
				if (mTarget.Exchange() == Market::CFFEX) {
					mStartTime = "09:30:00";
					mBreak1 = "11:30:00";
				}
				else {
					mStartTime = "09:00:00";
					mBreak1 = "10:15:00";
					mBreak2 = "11:30:00";
				}
			}
			else if (closeTime == "23:00:00" || closeTime == "01:00:00" || closeTime == "02:30:00")
				mStartTime = "21:00:00";
			else
				throw runtime_error("invalid closeTime");
		}

		TimePoint MinuteBegin(TimePoint tp) const
		{
			TimePoint begin = floor<minutes>(tp);
			if (begin + 1min == mStartTime)
				begin += 1min;		// 集合竞价行情并入后一分钟
			else if (begin == mBreak1 || begin == mBreak2 || begin == mCloseTime)
				begin -= 1min;		// 收盘行情并入前一分钟
			return begin;
		}

		bool IsTradingTime(TimePoint begin) const
		{
			string time = fmt::format("{:%H:%M:%S}", begin);
			if (mCloseTime <= "02:30:00")
				return time >= mStartTime || time < mCloseTime;
			return time >= mStartTime && time < mCloseTime;
		}

		void CloseBar()
		{
			if (IsTradingTime(mBar.BeginTime))
				OnBar(mBar);
		}

		Contract mTarget;		// 交易合约
		Contract mActive;		// 活跃合约，用做时钟源
		string mStartTime;		// 连续竞价开始时间
		string mCloseTime;		// 收盘时间
		string mBreak1;			// 第一次休市开始时间
		string mBreak2;			// 第二次休市开始时间
		Bar mBar;
		double mPrice = 0.0;
		double mOpenInterest = 0;
		int mTotalVolume = 0;
		bool mHasTask = false;
	};

	class DualMovingAverage : public MinuteBarStrategy
	{
		vector<Bar> mBars;
		int mFastPeriod = 5;
		int mSlowPeriod = 10;
		const Order* mOrder = nullptr;

	public:
		using MinuteBarStrategy::MinuteBarStrategy;

		void OnStart() override
		{
			fmt::print("OnStart\n");
			logi("NetPosition, {}, {}", Target(), NetPosition());
		}

	private:
		void OnBar(const Bar& bar) override
		{
			logi("MinuteBar, {}, BeginTime:{:%Y%m%d%H%M}, Open:{}, High:{}, Low:{}, Close:{}, OpenInterest:{}, Volume:{}",
				Target(), bar.BeginTime, bar.Open, bar.High, bar.Low, bar.Close, bar.OpenInterest, bar.Volume);

			mBars.push_back(bar);
			if (mBars.size() < mSlowPeriod + 1)
				return;

			double fastMA = CalculateMA(mFastPeriod);
			double slowMA = CalculateMA(mSlowPeriod);
			double prevFastMA = CalculateMA(mFastPeriod, 1);
			double prevSlowMA = CalculateMA(mSlowPeriod, 1);

			// 金叉：快线上穿慢线 -> 买入
			if (prevFastMA < prevSlowMA && fastMA > slowMA && NetPosition() == 0) {
				Buy(bar.Close);
			}
			// 死叉：快线下穿慢线 -> 卖出
			else if (prevFastMA > prevSlowMA && fastMA < slowMA && NetPosition() > 0) {
				Sell();
			}
		}

		int NetPosition() const
		{
			Position p = GetPosition(Target());
			return p.Long - p.Short;
		}

		// 计算均线，offset=0表示当前，offset=1表示前一根
		double CalculateMA(int period, int offset = 0) const
		{
			if (mBars.size() < period + offset)
				return 0;

			double sum = 0;
			int end = mBars.size() - offset;
			for (int i = end - period; i < end; i++)
				sum += mBars[i].Close;
			return sum / period;
		}

		void Buy(double price)
		{
			mOrder = Insert({ .Instrument = Target(), .Size = 1, .Price = price });
			DelayTask(30000, [this] {		// 30s 后未成交则撤单
				if (mOrder && mOrder->Status == OrderStatus::Queuing)
					Cancel(*mOrder);
			});
		}

		void Sell()
		{
			Quote q = GetQuote(Target());
			Insert({ .Instrument = Target(), .Size = -1, .Price = q.LowerLimitPrice });
		}
	};
}



int main()
{
	Config config = SimnowConfig();
	config.LogPath = TradingDay() + "_dma.log";
	config.SleepOnIdle = true;

	string target = "hc2701";		// 不能使用 Contract，尚未初始化
	string active = "rb2701";
	string closeTime = Now() > TimeAt("15:00:00") ? "23:00:00" : "15:00:00";
	return AutoRun<DualMovingAverage>(config, target, active, closeTime);
}
