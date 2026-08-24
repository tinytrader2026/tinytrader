import tinytrader as tt
from datetime import datetime, timedelta
from simnow import *


class ScheduleTasks(tt.Strategy):
    def __init__(self):
        super().__init__()
        self.Instrument = tt.Contract("rb2610")
        self.SaveTime = None

    def SubscribeList(self):
        return [self.Instrument]

    def OnStart(self):
        print("OnStart")
        order_time = "08:55:30"
        delay = int((tt.TodayAt(order_time) - tt.Now()).total_seconds() * 1000)
        if delay < 0:
            print("order_time already passed")
        else:       # 定时下单
            order = tt.NewOrder(Instrument=self.Instrument, Size=1, Price=3000)
            self.ScheduleTask(delay, lambda: self.Insert(order))

    def OnQuote(self, q):
        # 每 10s 保存一次重要数据。耗时操作放到两笔行情中间的空档期进行。
        if self.SaveTime is None or tt.Now() > self.SaveTime:
            self.ScheduleTask(100, self.WriteDatabase)      # 延迟 100ms
            self.SaveTime = tt.Now() + timedelta(seconds=10)

    def WriteDatabase(self):
        tp = tt.GetQuote(self.Instrument).ReceiveTime
        print(f"{tp.strftime('%H:%M:%S.%f')[:-3]}, "
              f"{tt.Now().strftime('%H:%M:%S.%f')[:-3]}, "
              f"WriteDatabase")


if __name__ == "__main__":
    config = simnow_config()
    tt.AutoRun(config, ScheduleTasks)
