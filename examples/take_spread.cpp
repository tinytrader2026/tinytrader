#include "tinytrader.h"

using namespace std;
using namespace chrono;
using namespace tinytrader;

namespace {		// 使用匿名命名空间，防止名称冲突
	class TakeSpread : public Strategy
	{
		// 价差交易，可用于套利或移仓
		// 行情满足指定价差时，先下流动性较差的合约，减小滑点概率
		// 第一腿使用对价FAK单，立即成交剩余自动撤销
		// 第一腿成交后，立即反向下另一腿，使用涨跌停价以确保成交

		static constexpr double eps = 0.000001;		// 用于浮点数比较

	public:
		void LoadFromFile()
		{
			// 价差：rb2610-rb2701
			mPositive = "rb2610";	// 方向与价差相同
			mNegative = "rb2701";	// 方向与价差相反
			mFirst = mNegative;		// 流动性差的合约先下单
			mSize = -5;				// 负值表示做空价差(卖rb2610买rb2701)
			mSpread = -35;			// 目标价差
			mMaxLot = 2;			// 单笔委托量上限

			if (!mPositive || !mNegative || mFirst != mPositive && mFirst != mNegative)
				throw runtime_error("contract error");
			if (mMaxLot < 1)
				throw runtime_error("MaxLot error");
		}

	private:
		vector<Contract> SubscribeList() const override
		{
			return { mPositive, mNegative };
		}

		void OnQuote(const Quote& q) override
		{
			if (q.Instrument != mFirst)
				return;

			const Quote& pos = GetQuote(mPositive);
			const Quote& neg = GetQuote(mNegative);
			// 涨跌停或数据不完整不下单
			if (!pos.BidSize1 || !pos.AskSize1 || !neg.BidSize1 || !neg.AskSize1)
				return;

			// 价差的盘口价
			double bid = pos.BidPrice1 - neg.AskPrice1;
			double ask = pos.AskPrice1 - neg.BidPrice1;
			// 行情满足指定价差
			if (mSize > 0 && ask <= mSpread + eps || mSize < 0 && bid >= mSpread - eps) {
				SendFirstLeg();
			}

			if (q.MarketTime >= mPrintTime) {		// 每5s打印一次行情
				auto tp = floor<milliseconds>(q.MarketTime);	// 保留到毫秒
				fmt::print("{:%T}, {}-{}, bid:{}, ask:{}\n", tp, mPositive, mNegative, bid, ask);
				mPrintTime = q.MarketTime + 5s;
			}
		}

		void OnOrder(const Order& o) override
		{
			if (&o == mLeg1 && o.Terminated() && o.FilledSize) {
				NewOrder req;
				req.Instrument = o.Instrument == mPositive ? mNegative : mPositive;
				req.Size = Limited(-o.FilledSize);
				const Quote& q = GetQuote(req.Instrument);
				req.Price = req.Size > 0 ? q.UpperLimitPrice : q.LowerLimitPrice;
				mLeg2 = Insert(req);
			}
			else if (&o == mLeg2 && o.Terminated() && o.FilledSize) {
				mFilledSize += o.FilledSize;
				mLeg1 = mLeg2 = nullptr;
			}
		}

		void OnTrade(const Trade& t, const Order& o) override
		{
			Position p = GetPosition(mPositive);
			Position n = GetPosition(mNegative);
			fmt::print("NetPosition, {}: {}, {}: {}\n", mPositive, p.Long - p.Short, mNegative, n.Long - n.Short);
		}

		void SendFirstLeg()
		{
			int remain = mSize - mFilledSize;		// 剩余量
			if (remain && !mLeg1 && !mLeg2) {
				NewOrder req;
				req.Instrument = mFirst;

				int sz = req.Instrument == mPositive ? remain : -remain;
				req.Size = Limited(sz);

				const Quote& q = GetQuote(req.Instrument);
				req.Price = req.Size > 0 ? q.AskPrice1 : q.BidPrice1;	// 对价

				req.Type = OrderType::FAK;		// 立即成交，剩余自动撤销
				mLeg1 = Insert(req);
			}
		}

		int Limited(int size)	// 限制单笔委托数量
		{
			if (abs(size) > mMaxLot)
				return size > 0 ? mMaxLot : -mMaxLot;
			return size;
		}

		Contract mPositive;			// 交易方向与价差相同
		Contract mNegative;			// 交易方向与价差相反
		Contract mFirst;			// 先下单的合约
		int mSize = 0;				// 目标成交量
		double mSpread = 0;			// 目标价差
		int mMaxLot = 0;			// 单笔委托量上限

		int mFilledSize = 0;		// 已成交量
		TimePoint mPrintTime;		// 下次打印行情的时间
		const Order* mLeg1 = nullptr;
		const Order* mLeg2 = nullptr;
	};
}


int main()
{
	Config config;
	config.UserID = "12345678";
	config.Password = "my_password";
	config.AppID = "simnow_client_test";
	config.AuthCode = "0000000000000000";
	config.BrokerID = "9999";
	config.TradeFront = "tcp://182.254.243.31:30002";
	config.MarketFront = "tcp://182.254.243.31:30012";

	// 日志写入文件，而不是打印在屏幕上
	config.LogPath = TradingDay() + ".log";		// yyyymmdd.log
	config.CachePath = "D:/test/contracts.bin";

	// 不使用 AutoRun，增加屏幕打印
	try {
		InitEngine(config);
		fmt::print("engine initialized\n");
		TakeSpread s;
		fmt::print("loading data ...\n");
		s.LoadFromFile();
		Run(s);
		return 0;
	}
	catch (const std::exception& e) {
		loge("exception: {}", e.what());
		fmt::print("exception: {}", e.what());
		return -1;
	}
	catch (...) {
		loge("unknown exception");
		fmt::print("unknown exception");
		return -1;
	}
}
