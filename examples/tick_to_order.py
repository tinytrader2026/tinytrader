from tinytrader import Strategy, Contract, NewOrder, OrderStatus, TradeFlag, AutoRun, TradingDay
import statistics
import sys
from simnow import SimnowConfig


class TickToOrder(Strategy):
    def __init__(self, code):
        super().__init__()
        self.instrument = Contract(code)
        print(self.instrument)
        self.latencies = []
        self.sample_count = 300
        self.Subscribe(self.instrument)

    def OnQuote(self, q):
        if q.BidSize1 == 0 or q.AskSize1 == 0:  # 涨跌停不下单
            return

        if len(self.latencies) < self.sample_count:
            newOrder = NewOrder(Instrument=self.instrument, Size=1, Price=q.LowerLimitPrice, Flag=TradeFlag.Open)
            order = self.Insert(newOrder)
            if order is not None:
                # ReceiveTime 为 OnRtnDepthMarketData 第一行的时间戳
                # SendTime 为 ReqOrderInsert 完成后的时间戳
                latency_us = int((order.SendTime - q.ReceiveTime).total_seconds() * 1_000_000)
                self.latencies.append(latency_us)
                print(f"{len(self.latencies):>3d}: {latency_us:>5} us")

    def OnOrder(self, order):
        if order.Status == OrderStatus.Queuing and not order.Canceling:
            self.Cancel(order)
        elif order.Terminated() and len(self.latencies) == self.sample_count:
            self.Report()

    def Report(self):
        sorted_lat = sorted(self.latencies)
        n = len(sorted_lat)
        avg = statistics.mean(sorted_lat)
        min_lat = sorted_lat[0]
        max_lat = sorted_lat[-1]
        p50 = sorted_lat[int(n * 50 / 100)]
        p90 = sorted_lat[int(n * 90 / 100)]
        p95 = sorted_lat[int(n * 95 / 100)]
        p99 = sorted_lat[int(n * 99 / 100)]

        print("\n========== Tick-to-Order 延迟统计 ==========")
        print(f"样本数: {n}")
        print(f"平均值: {avg:.1f} μs")
        print(f"最小值: {min_lat} μs")
        print(f"最大值: {max_lat} μs")
        print(f"P50:    {p50} μs")
        print(f"P90:    {p90} μs")
        print(f"P95:    {p95} μs")
        print(f"P99:    {p99} μs")
        print("============================================")



if __name__ == "__main__":
    config = SimnowConfig()
    config.LogPath = TradingDay() + ".log"
    AutoRun(config, TickToOrder, "rb2701")