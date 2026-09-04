#include "simnow.h"
#include <numeric>

using namespace std;
using namespace chrono;
using namespace tinytrader;

namespace {		// 使用匿名命名空间，防止名称冲突
	class TickToOrder : public Strategy		// 收到行情立即下单，以测试内部延迟
	{
	public:
		TickToOrder(Contract c)
		{
			mInstrument = c;
			Subscribe(mInstrument);
			fmt::print("Subscribe {}\n", c);
		}

		void OnQuote(const Quote& q) override
		{
			if (!q.BidSize1 || !q.AskSize1)		// 涨跌停不下单
				return;

			if (mLatencies.size() < mSampleCount) {
				const Order* order = Insert({
					.Instrument = mInstrument,
					.Size = 1,
					.Price = q.LowerLimitPrice,
					.Flag = TradeFlag::Open
				});
				if (order) {
					// ReceiveTime 为 OnRtnDepthMarketData 第一行的时间戳
					// SendTime 为 ReqOrderInsert 完成后的时间戳
					mLatencies.push_back(order->SendTime - q.ReceiveTime);
					fmt::print("{:>03d}: {:>05.1f} us\n", mLatencies.size(), mLatencies.back().count() / 1000.0);
				}
			}
		}

		void OnOrder(const Order& order) override
		{
			if (order.Status == OrderStatus::Queuing && !order.Canceling)
				Cancel(order);
			else if (order.Terminated() && mLatencies.size() == mSampleCount)
				Report();
		}

	private:
		void Report()
		{
			sort(mLatencies.begin(), mLatencies.end());
			size_t n = mLatencies.size();
			auto to_us = [](nanoseconds ns) -> double {	return ns.count() / 1000.0;	};

			auto total = accumulate(mLatencies.begin(), mLatencies.end(), 0ns);
			double avg = to_us(total) / n;
			double min = to_us(mLatencies.front());
			double max = to_us(mLatencies.back());
			double p50 = to_us(mLatencies[n * 50 / 100]);
			double p90 = to_us(mLatencies[n * 90 / 100]);
			double p95 = to_us(mLatencies[n * 95 / 100]);
			double p99 = to_us(mLatencies[n * 99 / 100]);

			fmt::print("\n========== Tick-to-Order 延迟统计 ==========\n");
			fmt::print("样本数: {}\n", n);
			fmt::print("平均值: {:.1f} μs\n", avg);
			fmt::print("最小值: {:.1f} μs\n", min);
			fmt::print("最大值: {:.1f} μs\n", max);
			fmt::print("P50:    {:.1f} μs\n", p50);
			fmt::print("P90:    {:.1f} μs\n", p90);
			fmt::print("P95:    {:.1f} μs\n", p95);
			fmt::print("P99:    {:.1f} μs\n", p99);
			fmt::print("============================================\n");
		}

		Contract mInstrument;
		vector<nanoseconds> mLatencies;
		size_t mSampleCount = 300;
	};
}


int main()
{
	Config config = SimnowConfig();
	config.LogPath = TradingDay() + ".log";
	return AutoRun<TickToOrder>(config, "rb2701");
}