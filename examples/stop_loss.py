import tinytrader as tt
from datetime import timedelta

class StopLossStrategy(tt.Strategy):
	""" 定时委托(带止损) """
	def __init__(self):
		super().__init__()
		self.NewOrder = tt.NewOrder()
		self.StopLossTicks = 1                             # 止损价差(跳数)
		self.TriggerTime = None                            # 下单时间(本机)
		self.PrintTime = tt.Now() - timedelta(hours=1)     # 下次打印行情的时间
		self.Order1 = None
		self.Order2 = None

	def _LoadFromFile(self):
		self.NewOrder.Instrument = tt.Contract("rb2610")
		self.NewOrder.Size = -1
		self.NewOrder.Price = 2950
		self.StopLossTicks = 1
		self.TriggerTime = tt.TodayAt("10:53:00")

	def SubscribeList(self):
		return [self.NewOrder.Instrument]

	def OnStart(self):
		print("OnStart")
		self._LoadFromFile()

		if not self.NewOrder.Instrument:
			raise RuntimeError("invalid contract")
		if self.NewOrder.Size == 0 or abs(self.NewOrder.Size) > 20:
			raise RuntimeError("invalid order size")
		if self.StopLossTicks < 1:
			raise RuntimeError("invalid stop loss ticks")

		print(f"trigger time: {self.TriggerTime}")
		if tt.Now() > self.TriggerTime - timedelta(seconds=1):
			raise RuntimeError("too late")

	def OnTimer(self):
		# 定时下单逻辑不能放到OnQuote中，因为指定的时间可能处于集合竞价阶段，没有行情推送
		if self.Order1 is None and tt.Now() >= self.TriggerTime:
			self.Order1 = self.Insert(self.NewOrder)

	def OnQuote(self, q):
		# 需要止损时原订单必然已全部成交
		# TradedSize 等于 Size 说明完全成交且已收到所有成交回报，可以准确计算成交均价
		finished = (self.Order1 is not None and self.Order1.TradedSize == self.Order1.Size)
		C = self.NewOrder.Instrument

		if finished and self.Order2 is None and q.Instrument == C:
			cost = self.Order1.AveragePrice()
			loss_spread = cost - q.Price if self.Order1.Size > 0 else q.Price - cost
			loss_ticks = C.ToTicks(loss_spread)     # 亏损价差转成跳数
			if loss_ticks >= self.StopLossTicks:
				req = tt.NewOrder()
				req.Instrument = C
				req.Size = -self.Order1.Size
				req.Price = q.UpperLimitPrice if req.Size > 0 else q.LowerLimitPrice
				self.Order2 = self.Insert(req)
				# 写日志
				tt.logi(f"Quote, {C}, Size:{self.Order1.Size}, Price:{q.Price}, Cost:{cost}, LossTicks:{loss_ticks}")

		if q.MarketTime >= self.PrintTime:
			print(f"{q.MarketTime.strftime('%H:%M:%S.%f')[:-3]}, {q.Instrument}, {q.Price}")
			self.PrintTime = q.MarketTime + timedelta(seconds=5)

	def OnOrder(self, o):
		print(o)

	def OnTrade(self, t, o):
		print(t)
		print(tt.GetPosition(t.Instrument))


def main():
	config = tt.Config()
	config.UserID = "12345678"
	config.Password = "my_password"
	config.AppID = "simnow_client_test"
	config.AuthCode = "0000000000000000"
	config.BrokerID = "9999"
	config.TradeFront = "tcp://182.254.243.31:30002"
	config.MarketFront = "tcp://182.254.243.31:30012"

	config.LogPath = tt.TradingDay() + ".log"	# yyyymmdd.log
	config.MaxOrderCount = 10					# 防止 bug 导致疯狂下单

	print(f"current time: {tt.Now()}")
	tt.AutoRun(config, StopLossStrategy)


if __name__ == "__main__":
	main()
