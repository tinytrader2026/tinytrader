import tinytrader as tt

class MyStrategy(tt.Strategy):
	def OnStart(self):
		"""策略启动时执行"""
		order = tt.NewOrder()						# 默认GFD，自动开平标志
		order.Instrument = tt.Contract("rb2610")	# 合约
		order.Size = -2								# 委托量(正=买，负=卖)
		order.Price = 4100							# 价格
		self.Insert(order)							# 下单

if __name__ == "__main__":
	config = tt.Config()
	config.UserID = "12345678"
	config.Password = "my_password"
	config.AppID = "simnow_client_test"
	config.AuthCode = "0000000000000000"
	config.BrokerID = "9999"
	config.TradeFront = "tcp://182.254.243.31:30002"
	config.MarketFront = "tcp://182.254.243.31:30012"
	# 以上为 CTP 账号及地址

	tt.AutoRun(config, MyStrategy)		# 一行启动
