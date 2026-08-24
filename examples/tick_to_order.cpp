#include "simnow.h"
#include <numeric>

using namespace std;
using namespace chrono;
using namespace tinytrader;

namespace {		// 使用匿名命名空间，防止名称冲突
	class TickToOrder : public Strategy		// 收到行情立即下单，以测试内部延迟
	{
		Contract mInstrument = "rb2610";
		vector<nanoseconds> mLatencies;
		bool mFinished = false;
		size_t mSampleCount = 300;

		void Report()
		{
			if (mLatencies.empty())
				return;

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

	public:
		vector<Contract> SubscribeList() const override
		{
			return { mInstrument };
		}

		void OnStart() override
		{
			fmt::print("{}\n", "OnStart");
		}

		void OnQuote(const Quote& q) override
		{
			if (!q.BidSize1 || !q.AskSize1)		// 涨跌停不下单
				return;

			if (mLatencies.size() < mSampleCount) {
				NewOrder newOrder;
				newOrder.Instrument = mInstrument;
				newOrder.Size = 1;
				newOrder.Price = q.LowerLimitPrice;	// 用跌停价买入，防止成交
				//newOrder.Type = OrderType::FOK;	// 使用 FOK 则无需 Cancel
				const Order* order = Insert(newOrder);

				mLatencies.push_back(Now() - q.ReceiveTime);
				if (order)
					Cancel(*order);
			}

			if (!mFinished && mLatencies.size() == mSampleCount) {
				Report();
				mFinished = true;
			}
		}
	};
}


int main()
{
	Config config = SimnowConfig();
	return AutoRun<TickToOrder>(config);
}