#include "tinytrader.h"

using namespace std;
using namespace chrono;
using namespace tinytrader;

namespace {		// 使用匿名命名空间，防止名称冲突
	class StopLossStrategy : public Strategy	// 定时下单，带止损
	{
	private:
		void LoadFromFile()
		{
			mNewOrder.Instrument = "rb2610";
			mNewOrder.Size = -3;
			mNewOrder.Price = 3450;
			mNewOrder.Type = OrderType::GFD;
			mNewOrder.Flag = TradeFlag::Open;
			mStopLossTicks = 2;
			mTriggerTime = TodayAt("10:53:00");
		}

	public:
		vector<Contract> SubscribeList() const override
		{
			return { mNewOrder.Instrument };
		}

		void OnStart() override
		{
			fmt::print("{}\n", "OnStart");
			LoadFromFile();

			if (!mNewOrder.Instrument)
				throw runtime_error("invalid contract");
			if (!mNewOrder.Size || abs(mNewOrder.Size) > 20)
				throw runtime_error("invalid order size");
			if (mStopLossTicks < 1)
				throw runtime_error("invalid stop loss ticks");

			fmt::print("trigger time: {}\n", mTriggerTime);
			if (Now() > mTriggerTime - 1s)		// 程序启动太晚，距离下单时间已不足1s
				throw runtime_error("too late");
		}

		void OnTimer() override
		{
			// 定时下单逻辑不能放到OnQuote中，因为指定的时间可能处于集合竞价阶段，没有行情推送
			// onTimer函数会被调用很多次，通过检查 mOrder1 是否为空来防止重复下单
			if (!mOrder1 && Now() >= mTriggerTime)		// 尚未下单且时间已到
				mOrder1 = Insert(mNewOrder);
		}

		void OnQuote(const Quote& q) override
		{
			// 需要止损时原订单必然已全部成交
			// TradedSize 等于 Size 说明完全成交且已收到所有成交回报，可以准确计算成交均价
			bool finished = mOrder1 && mOrder1->TradedSize == mOrder1->Size;
			Contract C = mNewOrder.Instrument;

			// 原始订单完全成交、未下过止损单、当前行情确为目标合约
			if (finished && !mOrder2 && q.Instrument == C) {
				double cost = mOrder1->AveragePrice();
				double loss_spread = mOrder1->Size > 0 ? cost - q.Price : q.Price - cost;

				// 价差转换成跳数，不能直接用浮点数比较
				int loss_ticks = C.ToTicks(loss_spread);
				if (C.ToTicks(loss_spread) >= mStopLossTicks) {
					NewOrder req;
					req.Instrument = C;
					req.Size = -mOrder1->Size;		// 反手
					req.Price = req.Size > 0 ? q.UpperLimitPrice : q.LowerLimitPrice;
					// 自动开平标志
					mOrder2 = Insert(req);

					// 写日志
					logi("Quote, {}, Size:{}, Price:{}, Cost:{}, LossTicks:{}", C, mOrder1->Size, q.Price, cost, loss_ticks);
				}
			}

			if (q.MarketTime >= mPrintTime) {
				auto tp = floor<milliseconds>(q.MarketTime);	// 保留到毫秒
				fmt::print("{:%T}, {}, {}\n", tp, q.Instrument, q.Price);
				mPrintTime = q.MarketTime + 5s;					// 每5s打印一次行情
			}
		}

		void OnOrder(const Order& o) override
		{
			fmt::print("Order, {}\n", o);
		}

		void OnTrade(const Trade& t, const Order& o) override
		{
			fmt::print("Trade, {}, {}\n", t, GetPosition(t.Instrument));	// 实时仓位
		}

		NewOrder mNewOrder;
		int mStopLossTicks = 1;			// 止损价差(跳数)
		TimePoint mTriggerTime;			// 下单时间(本机)
		TimePoint mPrintTime;			// 下次打印行情的时间
		const Order* mOrder1 = nullptr;
		const Order* mOrder2 = nullptr;
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

	config.MaxOrderCount = 10;					// 防止 bug 导致疯狂下单

	fmt::print("current time: {}\n", Now());
	return AutoRun<StopLossStrategy>(config);
}