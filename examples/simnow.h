#include "tinytrader.h"

namespace {
	auto GetEnviromentVariable(const std::string& key)
	{
		const char* value = std::getenv(key.c_str());
		return value ? value : "";
	}

	auto SimnowConfig()
	{
		return tinytrader::Config{
			.UserID = GetEnviromentVariable("SIMNOW_USERID"),
			.Password = GetEnviromentVariable("SIMNOW_PASSWORD"),
			.BrokerID = "9999",
			.AppID = "simnow_client_test",
			.AuthCode = "0000000000000000",
			.TradeFront = "tcp://182.254.243.31:30002",
			.MarketFront = "tcp://182.254.243.31:30012",

			.CachePath = "D:/test/contracts.bin",
		};
	}
}