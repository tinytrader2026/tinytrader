#include "fmtlog.h"
#include "tinytrader.h"
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/chrono.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/optional.h>
#include <nanobind/trampoline.h>

namespace nb = nanobind;
using namespace std::chrono;
using namespace tinytrader;

namespace {
	struct PyStrategy : public Strategy
	{
		NB_TRAMPOLINE(Strategy, 6);

		static const Order* Insert(NewOrder& newOrder)
		{
			return Strategy::Insert(newOrder);
		}

		static bool Cancel(const Order& order)
		{
			return Strategy::Cancel(order);
		}

		std::vector<Contract> SubscribeList() const override
		{
			NB_OVERRIDE(SubscribeList);
			return Strategy::SubscribeList();
		}

		void OnStart() override
		{
			NB_OVERRIDE(OnStart);
		}

		void OnTimer() override
		{
			NB_OVERRIDE(OnTimer);
		}

		void OnQuote(const Quote& q) override
		{
			NB_OVERRIDE(OnQuote, q);
		}

		void OnOrder(const Order& o) override
		{
			NB_OVERRIDE(OnOrder, o);
		}

		void OnTrade(const Trade& t, const Order& o) override
		{
			NB_OVERRIDE(OnTrade, t, o);
		}
	};
}

NB_MODULE(_tinytrader, m) {
	m.doc() = "TinyTrader Python bindings";

	// =========================== 枚举 ===========================
	nb::enum_<Market>(m, "Market", "交易所枚举")
		.value("OTHER", Market::OTHER, "其他")
		.value("CFFEX", Market::CFFEX, "中国金融期货交易所")
		.value("CZCE", Market::CZCE, "郑州商品交易所")
		.value("DCE", Market::DCE, "大连商品交易所")
		.value("GFEX", Market::GFEX, "广州期货交易所")
		.value("INE", Market::INE, "上海国际能源交易中心")
		.value("SHFE", Market::SHFE, "上海期货交易所");

	nb::enum_<OrderType>(m, "OrderType", "订单类型枚举")
		.value("GFD", OrderType::GFD, "当日有效")
		.value("FAK", OrderType::FAK, "立即成交，剩余撤销")
		.value("FOK", OrderType::FOK, "立即全部成交，否则撤销");

	nb::enum_<TradeFlag>(m, "TradeFlag", "开平标志枚举")
		.value("Auto", TradeFlag::Auto, "自动")
		.value("Open", TradeFlag::Open, "开仓")
		.value("Close", TradeFlag::Close, "平仓")
		.value("CloseToday", TradeFlag::CloseToday, "平今")
		.value("CloseYesterday", TradeFlag::CloseYesterday, "平昨");

	nb::enum_<OrderStatus>(m, "OrderStatus", "订单状态枚举")
		.value("Other", OrderStatus::Other, "其他")
		.value("Sent", OrderStatus::Sent, "本地已发送")
		.value("Queuing", OrderStatus::Queuing, "处于交易所队列中（含部分成交）")
		.value("Filled", OrderStatus::Filled, "全部成交")
		.value("Canceled", OrderStatus::Canceled, "撤单成功")
		.value("Rejected", OrderStatus::Rejected, "被柜台或交易所拒绝");

	// =========================== Contract ===========================
	nb::class_<Contract>(m, "Contract")
		.def(nb::init<>())
		.def(nb::init<const char*>())
		.def(nb::init<const std::string&>())
		.def("__int__", &Contract::operator int)
		.def("Code", [](const Contract& self) { return std::string(self.Code()); })
		.def("Exchange", &Contract::Exchange)
		.def("PriceTick", &Contract::PriceTick)
		.def("Multiplier", &Contract::Multiplier)
		.def("ToTicks", &Contract::ToTicks)
		.def("__eq__", [](const Contract& self, const Contract& other) {
		return int(self) == int(other);
			});

	// =========================== Quote ===========================
	nb::class_<Quote>(m, "Quote")
		.def(nb::init<>())
		.def_ro("Instrument", &Quote::Instrument)
		.def_ro("Volume", &Quote::Volume)
		.def_ro("Price", &Quote::Price)
		.def_ro("BidSize1", &Quote::BidSize1)
		.def_ro("AskSize1", &Quote::AskSize1)
		.def_ro("BidPrice1", &Quote::BidPrice1)
		.def_ro("AskPrice1", &Quote::AskPrice1)
		.def_ro("Turnover", &Quote::Turnover)
		.def_ro("OpenInterest", &Quote::OpenInterest)
		.def_ro("MarketTime", &Quote::MarketTime)
		.def_ro("ReceiveTime", &Quote::ReceiveTime)
		.def_ro("UpperLimitPrice", &Quote::UpperLimitPrice)
		.def_ro("LowerLimitPrice", &Quote::LowerLimitPrice);

	// =========================== NewOrder ===========================
	nb::class_<NewOrder>(m, "NewOrder")
		.def(nb::init<>())
		.def_rw("Instrument", &NewOrder::Instrument)
		.def_rw("Size", &NewOrder::Size)
		.def_rw("Price", &NewOrder::Price)
		.def_rw("Type", &NewOrder::Type)
		.def_rw("Flag", &NewOrder::Flag);

	// =========================== Order ===========================
	nb::class_<Order>(m, "Order")
		.def(nb::init<>())
		.def_ro("Instrument", &Order::Instrument)
		.def_ro("Size", &Order::Size)
		.def_ro("Price", &Order::Price)
		.def_ro("Type", &Order::Type)
		.def_ro("Flag", &Order::Flag)
		.def_ro("Status", &Order::Status)
		.def_ro("Error", &Order::Error)
		.def_ro("Canceling", &Order::Canceling)
		.def_ro("Ref", &Order::Ref)
		.def_ro("FilledSize", &Order::FilledSize)
		.def_ro("TradedSize", &Order::TradedSize)
		.def_ro("TradedValue", &Order::TradedValue)
		.def("Terminated", &Order::Terminated)
		.def("Cancelable", &Order::Cancelable)
		.def("AveragePrice", &Order::AveragePrice);

	// =========================== Trade ===========================
	nb::class_<Trade>(m, "Trade")
		.def(nb::init<>())
		.def_ro("Instrument", &Trade::Instrument)
		.def_ro("Size", &Trade::Size)
		.def_ro("Price", &Trade::Price)
		.def_ro("Flag", &Trade::Flag)
		.def_ro("OrderRef", &Trade::OrderRef)
		.def("Value", &Trade::Value);

	// =========================== Position / PositionEntry ===========================
	nb::class_<Position>(m, "Position")
		.def(nb::init<>())
		.def_ro("Long", &Position::Long)
		.def_ro("LongToday", &Position::LongToday)
		.def_ro("Short", &Position::Short)
		.def_ro("ShortToday", &Position::ShortToday);

	nb::class_<PositionEntry, Position>(m, "PositionEntry")
		.def(nb::init<>())
		.def_ro("Code", &PositionEntry::Code);

	// =========================== Config ===========================
	nb::class_<Config>(m, "Config")
		.def(nb::init<>())
		.def_rw("UserID", &Config::UserID)
		.def_rw("Password", &Config::Password)
		.def_rw("AppID", &Config::AppID)
		.def_rw("AuthCode", &Config::AuthCode)
		.def_rw("BrokerID", &Config::BrokerID)
		.def_rw("TradeFront", &Config::TradeFront)
		.def_rw("MarketFront", &Config::MarketFront)
		.def_rw("LogPath", &Config::LogPath)
		.def_rw("CachePath", &Config::CachePath)
		.def_rw("MaxOrderCount", &Config::MaxOrderCount)
		.def_rw("TimerInterval", &Config::TimerInterval)
		.def_rw("SleepOnIdle", &Config::SleepOnIdle);

	// =========================== Strategy ===========================
	nb::class_<Strategy, PyStrategy>(m, "Strategy")
		.def(nb::init<>())
		.def_static("Insert", &PyStrategy::Insert, nb::rv_policy::reference)
		.def_static("Cancel", &PyStrategy::Cancel)
		.def("SubscribeList", [](const Strategy& self) { return self.SubscribeList(); })
		.def("OnStart", [](Strategy& self) { self.OnStart(); })
		.def("OnTimer", [](Strategy& self) { self.OnTimer(); })
		.def("OnQuote", [](Strategy& self, const Quote& q) { self.OnQuote(q); })
		.def("OnOrder", [](Strategy& self, const Order& o) { self.OnOrder(o); })
		.def("OnTrade", [](Strategy& self, const Trade& t, const Order& o) { self.OnTrade(t, o); });

	// =========================== 自由函数 ===========================
	m.def("Now", [] {return Now() - 8h; });


	m.def("TradingDay", &TradingDay, "获取当前交易日。\n\n规则：18:00 前返回当日，否则返回次日，周末顺延至周一。\n\nReturns:\n    str: 交易日，格式 \"yyyyMMdd\"");
	m.def("GetPosition", &GetPosition, "获取单个合约的持仓。\n\nArgs:\n    c (Contract): 合约\n\nReturns:\n    Position: 持仓数据");
	m.def("GetPositions", &GetPositions, "获取所有合约的持仓。\n\nReturns:\n    list[PositionEntry]: 所有持仓列表");
	m.def("GetQuote", &GetQuote, "获取最新行情。\n\nArgs:\n    c (Contract): 合约\n\nReturns:\n    Quote: 最新行情数据");
	m.def("InitEngine", &InitEngine, "初始化交易引擎。\n\n登录 CTP 交易前置，初始化合约信息、持仓明细等。\n\nArgs:\n    config (Config): 引擎配置\n\n注意：必须在调用 Run() 之前执行。策略对象必须在 InitEngine() 之后创建。");
	m.def("Run", &Run, "运行策略。\n\n连接行情前置，订阅行情，启动主循环。\n\nArgs:\n    s (Strategy): 策略实例\n\n注意：此函数不返回，按 Ctrl+C 可退出。");
	m.def("MakeCache", &MakeCache, "缓存合约信息。\n\n登录 CTP 交易前置，查询合约信息并写入指定路径。\n\nArgs:\n    config (Config): 引擎配置");

	// =========================== 日志函数 ===========================
	m.def("logi", [](const std::string& msg) { logi("{}", msg); });
	m.def("logw", [](const std::string& msg) { logw("{}", msg); });
	m.def("loge", [](const std::string& msg) { loge("{}", msg); });
	m.def("logd", [](const std::string& msg) { logd("{}", msg); });
}