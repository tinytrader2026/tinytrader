from tinytrader import Strategy, NewOrder, Config, AutoRun

class MyStrategy(Strategy):
    def OnStart(self):
        # Size正值为买，负值为卖
        self.Insert(NewOrder(Instrument="rb2701", Size=-2, Price=4100))

if __name__ == "__main__":
    config = Config(                    # CTP 账号、地址
        UserID="12345678",
        Password="my_password",
        BrokerID="9999",
        AppID="simnow_client_test",
        AuthCode="0000000000000000",
        TradeFront="tcp://182.254.243.31:30002",
        MarketFront="tcp://182.254.243.31:30012",
    )

    AutoRun(config, MyStrategy)         # 一行启动