import tinytrader as tt


def main():
	config = tt.Config()
	config.UserID = "12345678"
	config.Password = "my_password"
	config.AppID = "simnow_client_test"
	config.AuthCode = "0000000000000000"
	config.BrokerID = "9999"
	config.TradeFront = "tcp://182.254.243.31:30002"
	config.MarketFront = "tcp://182.254.243.31:30012"

	config.CachePath = "D:/test/contracts.bin"
	tt.MakeCache(config)

if __name__ == "__main__":
	main()
