from tinytrader import Strategy, Contract, NewOrder, TimeAt, Now, GetQuote, AutoRun
from datetime import datetime, timedelta
from simnow import SimnowConfig


class ScheduleTasks(Strategy):
    def __init__(self):
        super().__init__()
        self.Instrument = Contract("rb2701")
        self.SaveTime = None

    def OnStart(self):
        print("OnStart")
        self.Subscribe(self.Instrument)
        order_time = "08:55:30"         # 集合竞价
        delay = int((TimeAt(order_time) - Now()).total_seconds() * 1000)
        if delay < 0:
            print("order_time already passed")
        else:                           # 定时下单
            order = NewOrder(Instrument=self.Instrument, Size=1, Price=3000)
            self.DelayTask(delay, lambda: self.Insert(order))

    def OnQuote(self, q):
        # 每 10s 保存一次重要数据。耗时操作放到两笔行情中间的空档期进行。
        if self.SaveTime is None or Now() > self.SaveTime:
            self.DelayTask(100, self.WriteDatabase)      # 延迟 100ms
            self.SaveTime = Now() + timedelta(seconds=10)

    def WriteDatabase(self):
        tp = GetQuote(self.Instrument).ReceiveTime
        print(f"{tp.strftime('%H:%M:%S.%f')[:-3]}, "
              f"{Now().strftime('%H:%M:%S.%f')[:-3]}, "
              f"WriteDatabase")


if __name__ == "__main__":
    config = SimnowConfig()
    AutoRun(config, ScheduleTasks)