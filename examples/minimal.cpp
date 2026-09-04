#include "tinytrader.h"

using namespace tinytrader;

class MyStrategy : public Strategy
{
	void OnStart() override				// 策略启动时执行
	{
		// Size正值为买，负值为卖
		Insert({ .Instrument = "rb2701", .Size = -2, .Price = 4100 });
	}
};

int main()
{
	Config config{							// CTP 账号、地址
		.UserID = "12345678",
		.Password = "my_password",
		.BrokerID = "9999",
		.AppID = "simnow_client_test",
		.AuthCode = "0000000000000000",
		.TradeFront = "tcp://182.254.243.31:30002",
		.MarketFront = "tcp://182.254.243.31:30012",
	};

	return AutoRun<MyStrategy>(config);		// 一行启动
}