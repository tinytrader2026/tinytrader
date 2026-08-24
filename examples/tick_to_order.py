import tinytrader as tt
import statistics
import time
from simnow import *

class TickToOrder(tt.Strategy):
	def __init__(self):
		super().__init__()
		self.instrument = tt.Contract("rb2610")
		self.latencies = []
		self.finished = False
		self.sample_count = 300

	def SubscribeList(self):
		return [self.instrument]

	def OnStart(self):
		print("OnStart")

	def OnQuote(self, q):
		# 涨跌停不下单
		if q.BidSize1 == 0 or q.AskSize1 == 0:
			return

		if len(self.latencies) < self.sample_count:
			newOrder = tt.NewOrder()
			newOrder.Instrument = self.instrument
			newOrder.Size = 1
			newOrder.Price = q.LowerLimitPrice	# 用跌停价买入，防止成交
			# order.Type = tt.OrderType.FOK		# 使用 FOK 则无需 Cancel
			order = self.Insert(newOrder)

			# 从行情到达到指令发出
			latency_us = int((tt.Now() - q.ReceiveTime).total_seconds() * 1_000_000)
			self.latencies.append(latency_us)
			if order is not None:
				self.Cancel(order)

		if not self.finished and len(self.latencies) == self.sample_count:
			self.Report()
			self.finished = True

	def Report(self):
		if not self.latencies:
			return

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
	config = simnow_config()
	tt.AutoRun(config, TickToOrder)
