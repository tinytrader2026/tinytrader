import tinytrader as tt
from datetime import timedelta
import math

EPS = 0.000001  # 用于浮点数比较


class TakeSpread(tt.Strategy):
	"""
	价差交易，可用于套利或移仓
	行情满足指定价差时，先下流动性较差的合约，减小滑点概率
	第一腿使用对价FAK单，立即成交剩余自动撤销
	第一腿成交后，立即反向下另一腿，使用涨跌停价以确保成交
	"""

	def __init__(self):
		super().__init__()
		self.Positive = tt.Contract()					# 交易方向与价差相同
		self.Negative = tt.Contract()					# 交易方向与价差相反
		self.First = tt.Contract()						# 先下单的合约
		self.Size = 0									# 目标成交量
		self.Spread = 0.0								# 目标价差
		self.MaxLot = 0									# 单笔委托量上限
		self.FilledSize = 0								# 已成交量
		self.PrintTime = tt.Now() - timedelta(hours=1)	# 下次打印行情的时间
		self.Leg1 = None
		self.Leg2 = None

	def LoadFromFile(self):
		# 价差：rb2610-rb2701
		self.Positive = tt.Contract("rb2610")   # 方向与价差相同
		self.Negative = tt.Contract("rb2701")   # 方向与价差相反
		self.First = self.Negative              # 流动性差的合约先下单
		self.Size = 5                          # 负值表示做空价差(卖rb2610买rb2701)
		self.Spread = -40.0                     # 目标价差
		self.MaxLot = 2                         # 单笔委托量上限

		if not self.Positive or not self.Negative or (self.First != self.Positive and self.First != self.Negative):
			raise RuntimeError("contract error")
		if self.MaxLot < 1:
			raise RuntimeError("MaxLot error")

	def SubscribeList(self):
		return [self.Positive, self.Negative]

	def OnQuote(self, q):
		if q.Instrument != self.First:
			return

		pos = tt.GetQuote(self.Positive)
		neg = tt.GetQuote(self.Negative)
		# 涨跌停或数据不完整不下单
		if not pos.BidSize1 or not pos.AskSize1 or not neg.BidSize1 or not neg.AskSize1:
			return

		# 价差的盘口价
		bid = pos.BidPrice1 - neg.AskPrice1
		ask = pos.AskPrice1 - neg.BidPrice1
		# 行情满足指定价差
		if (self.Size > 0 and ask <= self.Spread + EPS) or (self.Size < 0 and bid >= self.Spread - EPS):
			self.SendFirstLeg()

		if q.MarketTime >= self.PrintTime:
			# 每5s打印一次行情，保留到毫秒
			print(f"{q.MarketTime.strftime('%H:%M:%S.%f')[:-3]}, {self.Positive}-{self.Negative}, bid:{bid:.2f}, ask:{ask:.2f}")
			self.PrintTime = q.MarketTime + timedelta(seconds=5)

	def OnOrder(self, o):
		if o == self.Leg1 and o.Terminated() and o.FilledSize != 0:
			req = tt.NewOrder()
			req.Instrument = self.Negative if o.Instrument == self.Positive else self.Positive
			req.Size = self.Limited(-o.FilledSize)
			q = tt.GetQuote(req.Instrument)
			req.Price = q.UpperLimitPrice if req.Size > 0 else q.LowerLimitPrice
			self.Leg2 = self.Insert(req)

		elif o == self.Leg2 and o.Terminated() and o.FilledSize != 0:
			self.FilledSize += o.FilledSize
			self.Leg1 = None
			self.Leg2 = None

	def OnTrade(self, t, o):
		p = tt.GetPosition(self.Positive)
		n = tt.GetPosition(self.Negative)
		print(f"NetPosition, {self.Positive}: {p.Long - p.Short}, {self.Negative}: {n.Long - n.Short}")

	def SendFirstLeg(self):
		remain = self.Size - self.FilledSize
		if remain != 0 and not self.Leg1 and not self.Leg2:
			req = tt.NewOrder()
			req.Instrument = self.First

			sz = remain if req.Instrument == self.Positive else -remain
			req.Size = self.Limited(sz)

			q = tt.GetQuote(req.Instrument)
			req.Price = q.AskPrice1 if req.Size > 0 else q.BidPrice1

			req.Type = tt.OrderType.FAK  # 立即成交，剩余自动撤销
			self.Leg1 = self.Insert(req)

	def Limited(self, size):
		"""限制单笔委托数量"""
		if abs(size) > self.MaxLot:
			return self.MaxLot if size > 0 else -self.MaxLot
		return size


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
	config.CachePath = "D:/test/contracts.bin"

	print(f"current time: {tt.Now()}")
	tt.InitEngine(config)
	print("engine initialized")
	s = TakeSpread()
	s.LoadFromFile()
	tt.Run(s)


if __name__ == "__main__":
	main()