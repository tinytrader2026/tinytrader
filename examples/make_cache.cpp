#include "tinytrader.h"

using namespace tinytrader;

int main()
{
	Config config;
	config.UserID = "12345678";
	config.Password = "my_password";
	config.AppID = "simnow_client_test";
	config.AuthCode = "0000000000000000";
	config.BrokerID = "9999";
	config.TradeFront = "tcp://182.254.243.31:30002";
	config.MarketFront = "tcp://182.254.243.31:30012";

	config.CachePath = "D:/test/contracts.bin";
	MakeCache(config);
}