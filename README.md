# TinyTrader —— Strategy to Live Trading in One Line

TinyTrader 是一个直连 CTP 的交易引擎，使用 C++ 20 编写，同时提供 Python 接口。支持单账户、单策略的期货与期权交易。

**TinyTrader 适合谁？**

- 想用 Python 写策略，但是不喜欢架构复杂的大型框架
- 想用 C++ 写策略，但受够了柜台原生 API 的繁琐
- 需要微秒级响应，而不是毫秒级
- 不信任闭源黑盒，希望代码完全透明

如果有一条以上符合你的想法，那么 TinyTrader 可能就是你需要的工具。

**它做到了什么？**

- **极简**————无需了解柜台接口，填上账号，一行代码即可将策略接入实盘。
- **轻量**————核心代码不到 1000 行。
- **快速**————无锁，微秒级响应，Tick-to-Order 延迟中位数 C++ 4.5μs Python 8μs (普通 PC)。

## 📦 快速开始

```bash
pip install tinytrader
```

**零依赖。** 不安装任何第三方包。支持 Python 3.10 至 3.14 版本。

一个完整的交易程序(`examples/minimal.py`)：启动后立即下单。

```python
from tinytrader import Strategy, NewOrder, Config, AutoRun

class MyStrategy(Strategy):
	def OnStart(self):
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
```

策略启动后立即以 4100 的价格卖出 2 手 rb2701。`Size` 大于 0 为买单，小于 0 为卖单。开平标志默认自动(平仓优先)。

如果使用 Simnow 模拟平台，修改 `UserID` 和 `Password` 即可运行。

同样的功能，C++ 版 (`examples/minimal.cpp`)：

```cpp
#include "tinytrader.h"

using namespace tinytrader;

class MyStrategy : public Strategy
{
	void OnStart() override
	{
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
```

C++ 20 写法和 Python 一样简洁！编译成二进制文件可以更好地保护策略。

更多示例见 `examples` 目录，同时有 C++ 和 Python 版本。

## 📦 策略接口

| 接口 | 说明 |
| :--- | :--- |
| `Insert(NewOrder)` | 下单 |
| `Cancel(Order)` | 撤单 |
| `Subscribe(Contract)` | 订阅行情 |
| `DelayTask(int, Callable)` | 延时执行任务，可放到行情空档期 |
| `OnStart()` | 策略启动时触发 |
| `OnQuote(Quote)` | 行情到达时触发 |
| `OnOrder(Order)` | 订单状态变化时触发 |
| `OnTrade(Trade, Order)` | 成交时触发 |

### 延时任务

`DelayTask(ms, task)` 可用于定时任务，也可将耗时操作放到两笔行情之间的空档期执行，避免拖慢行情响应。
示例见 `examples/delay_tasks.py` 或 `examples/delay_tasks.cpp`。

### 数据随时可用

- `GetQuote(contract)`：获取指定合约的最新行情快照
- `GetPosition(contract)`：获取单个合约持仓
- `GetPositions()`：获取所有持仓

所有数据由引擎实时维护，无需查询柜台，初始化以后随时可用。

### 成交回调自带关联订单

`OnTrade(trade, order)` 回调中直接携带关联的 `Order` 对象，无需手动查询订单状态。

**📖 完整 API 参考：[docs/python-api.md](docs/python-api.md)**

C++ 用户可直接参考头文件 `src/tinytrader.h`。

## 📊 响应时间

Tick-to-Trade 延迟中网络传输时间占了绝大部分，不能反映交易引擎的响应速度。

普通 PC 机上实测 Tick-to-Order 延迟（从收到行情到发出委托）：

| 指标 | C++ | Python |
| :--- | ---: | ---: |
| 平均值 | 4.7 μs | 8.3 μs |
| 最小值 | 3.5 μs | 5 μs |
| 最大值 | 12.1 μs | 58 μs |
| P50 | 4.5 μs | 8 μs |
| P90 | 5.6 μs | 10 μs |
| P95 | 6.5 μs | 13 μs |
| P99 | 10.1 μs | 17 μs |

**测试平台：** Windows 10，AMD Ryzen 9 3900X（2019）。未绑核、未对操作系统进行调优，以贴近小白用户开箱即用的环境。

**测试方法：** 在 `OnRtnDepthMarketData` 入口打时间戳（起点），在 `ReqOrderInsert` 返回后打时间戳（终点）。每次测试 300 笔委托，运行多次，选择 P99 居中的一组数据呈现（未选择最优数据）。

测试程序见 `examples/tick_to_order.cpp` 及 `examples/tick_to_order.py`。

## 📦 C++ 编译

依赖项 CTP API 、fmtlib、fmtlog、magic_enum 均已包含在 third_party 目录中。

编译器需支持 C++20 标准 (GCC 10.2 和 MSVC 2022 已测试)。

进入 TinyTrader 代码目录，执行以下命令，即可生成所有示例程序。
```bash
mkdir build
cd build
cmake .. 
cmake --build . --config Release
```

如需调试版，将 `Release` 改成 `Debug` 即可。

如需从源码编译 Python 接口，可参考 `docs/build_python.md`。

## 📦 FAQ

### Q: 在快期等客户端进行操作，是否会影响 TinyTrader 正在运行的策略？

A: 正常情况 TinyTrader 不会被客户端操作所干扰，具体反应如下：

| 客户端操作				| TinyTrader 的反应							|
|:---					|:---										|
| 撤销 TinyTrader 的订单	| 继续执行策略撤单后的逻辑（如追单）			|
| 客户端手动下单			| 忽略该订单，不触发 `OnOrder`				|
| 客户端订单成交			| 仅更新持仓数据，不触发 `OnOrder`、`OnTrade`	|

### Q: 成交发生时，策略类的 `OnOrder` 和 `OnTrade` 都会被触发，先后顺序是确定的吗？

A: 成交发生时，引擎既会收到订单回报(立即触发 `OnOrder`)，也会收到成交回报(立即触发 `OnTrade`)。
一般来说，订单回报会排在前面，即 `OnOrder` 会先触发，据此操作会更快。但是消息的先后顺序取决于交易所的消息机制，并不是确定不变的。

### Q: 每次启动都要查询合约信息，有时侯特别慢，能否加速？

A: 查询合约信息的数据量较大，柜台可能限流，导致查询异常缓慢。可以创建一个工具程序，在每个交易日首次启动 TinyTrader 之前，进行一次合约信息查询，并将结果写入缓存文件。
缓存路径由配置项 `CachePath` 指定。示例见 `examples/make_cache.cpp` 或 `examples/make_cache.py`。
交易引擎初始化时将优先从 `CachePath` 指定的文件读取合约信息，读取失败才向柜台查询。这样就可以加速启动，同时减轻柜台的查询压力。

### Q: 如何查看 `TimePoint` 的值？

A: `TimePoint` 是 `std::chrono::system_clock::time_point` 的别名，可以使用 fmtlib 打印或转换成字符串。
fmtlib 根据 `time_point` 的精度来确定秒以下的位数，默认精度与平台有关，需要时可先进行精度转换。
```cpp
TimePoint tp = Now();
fmt::print("{:%Y%m%d}", tp);					// yyyymmdd
fmt::print("{:%T}", floor<seconds>(tp));		// HH:MM:SS
fmt::print("{:%T}", floor<milliseconds>(tp));	// HH:MM:SS.xxx
fmt::print("{:%T}", floor<nanoseconds>(tp));	// HH:MM:SS.xxxxxxxxx
```
更多示例可参考 `examples/format_time.cpp` 及 fmtlib 文档。

Python 版本用 `datetime` 表示时间，精度为微秒，可使用 `strftime` 进行格式化。

### Q: 日志中出现 `ErrorID`，如何查看具体含义？

A: 可查看 `third_party/CTP-6.7.11/error.xml` 获取错误码对应的信息。

## 📦 联系方式

有任何问题，请联系 `tinytrader@163.com`。


