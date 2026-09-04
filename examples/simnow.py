import os
from tinytrader import Config

def SimnowConfig():
	return Config(
		UserID = os.getenv("SIMNOW_USERID", ""),
		Password = os.getenv("SIMNOW_PASSWORD", ""),
		BrokerID = "9999",
		AppID = "simnow_client_test",
		AuthCode = "0000000000000000",
		TradeFront = "tcp://182.254.243.31:30002",
		MarketFront = "tcp://182.254.243.31:30012",

		CachePath = "contracts.bin",
	)
