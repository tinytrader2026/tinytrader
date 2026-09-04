#include "simnow.h"

using namespace std;
using namespace chrono;
using namespace tinytrader;

namespace {
	class ScheduleTasks : public Strategy
	{
		Contract mInstrument = "rb2701";
		TimePoint mSaveTime;

	public:
		void OnStart() override
		{
			fmt::print("{}\n", "OnStart");
			Subscribe(mInstrument);
			const char* order_time = "08:55:30";	// 集合竞价
			int delay = duration_cast<milliseconds>(TimeAt(order_time) - Now()).count();
			if (delay < 0)
				fmt::print("order_time already passed\n");
			else {
				DelayTask(delay, [this] {			// 定时下单
					Insert({ .Instrument = mInstrument, .Size = 1, .Price = 3000 });
				});
			}
		}

		void OnQuote(const Quote& q) override
		{
			// 每 10s 保存一次重要数据。耗时操作放到两笔行情中间的空档期进行。
			if (Now() > mSaveTime) {
				DelayTask(100, [this] { WriteDatabase(); });		// 延迟 100ms 执行
				mSaveTime = Now() + 10s;
			}
		}

		void WriteDatabase()
		{
			TimePoint tp = GetQuote(mInstrument).ReceiveTime;
			fmt::print("{:%T}, {:%T}, WriteDatabase\n", floor<milliseconds>(tp), floor<milliseconds>(Now()));
		}
	};
}


int main()
{
	Config config = SimnowConfig();
	return AutoRun<ScheduleTasks>(config);
}