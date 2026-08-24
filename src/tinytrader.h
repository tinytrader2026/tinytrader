#pragma once

#include "fmtlog.h"
#include "fmt/chrono.h"
#include "fmt/compile.h"
#include "magic_enum.hpp"

namespace tinytrader
{
	enum class Market : uint8_t { OTHER, CFFEX, CZCE, DCE, GFEX, INE, SHFE };

	class Contract
	{
	public:
		Contract() {}
		Contract(const char* code);
		Contract(const std::string& code);
		operator int() const				// 合约序号
		{
			return mIndex;
		}
		std::string_view Code() const;		// 合约代码
		Market Exchange() const;			// 交易所
		double PriceTick() const;			// 最小变动价位
		int Multiplier() const;				// 合约乘数
		int ToTicks(double price) const		// 价格转换成跳数
		{
			return static_cast<int>(lround(price / PriceTick()));
		}

	private:
		int mIndex = 0;
	};

	using TimePoint = std::chrono::system_clock::time_point;

	struct Quote
	{
		Contract Instrument;			// 合约
		int Volume = 0;					// 成交量
		double Price = 0;				// 最新价
		int BidSize1 = 0;				// 买一量
		int AskSize1 = 0;				// 卖一量
		double BidPrice1 = 0;			// 买一价
		double AskPrice1 = 0;			// 卖一价
		double Turnover = 0;			// 成交金额
		double OpenInterest = 0;		// 持仓量
		TimePoint MarketTime;			// 交易所时间
		TimePoint ReceiveTime;			// 本地接收时间
		double UpperLimitPrice = 0;		// 涨停价
		double LowerLimitPrice = 0;		// 跌停价
	};

	enum class OrderType : uint8_t {
		GFD,		// 当日有效
		FAK,		// 立即成交，剩余撤销
		FOK			// 立即全部成交，否则撤销
	};

	enum class TradeFlag : uint8_t { Auto, Open, Close, CloseToday, CloseYesterday };

	struct NewOrder
	{
		Contract Instrument;				// 合约
		int Size = 0;						// 委托量(正=买，负=卖)
		double Price = 0;					// 价格
		OrderType Type = OrderType::GFD;	// 订单类型
		TradeFlag Flag = TradeFlag::Auto;	// 开平标志
	};

	enum class OrderStatus : uint8_t {
		Other,			// 其他
		Sent,			// 本地已发送
		Queuing, 		// 处于交易所队列中(含部分成交)
		Filled, 		// 全部成交
		Canceled, 		// 撤单成功
		Rejected 		// 被柜台或交易所拒绝
	};

	struct Order : NewOrder
	{
		OrderStatus Status = OrderStatus::Other;	// 订单状态
		bool Canceling = false;						// 是否有撤单指令在途
		int Ref = 0;								// 订单索引
		int Error = 0;								// 错误码
		int FilledSize = 0;							// 已成交数量(带方向，来自订单回报)
		int TradedSize = 0;							// 已成交数量(带方向，成交回报累计)
		double TradedValue = 0;						// 成交金额(成交回报累计)

		bool Terminated() const						// 是否已终结
		{
			return Status == OrderStatus::Filled || Status == OrderStatus::Canceled || Status == OrderStatus::Rejected;
		}

		double AveragePrice() const					// 成交均价
		{
			return TradedSize ? TradedValue / abs(TradedSize * Instrument.Multiplier()) : 0.0;
		}
	};

	struct Trade
	{
		Contract Instrument;				// 合约
		int Size = 0;						// 成交量(符号表示方向)
		double Price = 0;					// 成交价
		TradeFlag Flag = TradeFlag::Auto;	// 开平标志
		int OrderRef = 0;					// 订单索引

		double Value() const				// 成交金额
		{
			return abs(Size) * Instrument.Multiplier() * Price;
		}
	};

	class Strategy
	{
	public:
		static const Order* Insert(const NewOrder& newOrder);	// 下单
		static bool Cancel(const Order& order);					// 撤单

		// 行情订阅列表，默认不订阅
		virtual std::vector<Contract> SubscribeList() const	{ return {}; }	
		virtual void OnStart() {}								// 启动时触发
		virtual void OnQuote(const Quote& q) {}					// 行情数据触发
		virtual void OnOrder(const Order& o) {}					// 订单回报触发
		virtual void OnTrade(const Trade& t, const Order& o) {}	// 成交回报触发

		// 计划任务，可将耗时操作放到两笔行情间的空档期执行
		void ScheduleTask(int delayMs, std::function<void()> task);
	};

	struct Config
	{
		std::string UserID;				// 资金账号
		std::string Password;			// 密码
		std::string BrokerID;			// 经纪公司代码
		std::string AppID;				// App代码
		std::string AuthCode;			// 认证码
		std::string TradeFront;			// 交易前置地址
		std::string MarketFront;		// 行情前置地址

		std::string LogPath;			// 日志文件路径
		std::string CachePath;			// 合约信息缓存路径
		int MaxOrderCount = 1000;		// 订单笔数上限
		bool SleepOnIdle = false;		// 空闲时短暂休眠
	};

	struct Position
	{
		int Long = 0;
		int LongToday = 0;
		int Short = 0;
		int ShortToday = 0;
	};

	struct PositionEntry : Position
	{
		std::string Code = "NULL";
	};



	inline TimePoint Now()							// 当前时刻(北京时间)
	{
		using namespace std::chrono;
		return system_clock::now() + 8h;
	}

	TimePoint TodayAt(std::string_view time);		// HH:MM:SS

	std::string TradingDay();						// yyyymmdd 交易日

	Position GetPosition(Contract c);				// 持仓

	std::vector<PositionEntry> GetPositions();		// 所有持仓

	const Quote& GetQuote(Contract c);				// 最新行情

	void InitEngine(const Config& config);			// 初始化交易引擎

	void Run(Strategy& s);							// 运行策略，不返回

	// 初始化交易引擎、创建策略并运行
	template<typename S, typename... Args>
	int AutoRun(const Config& config, Args&&... args) {
		static_assert(std::is_base_of_v<Strategy, S>, "S must be derived from Strategy");

		try {
			InitEngine(config);
			S strategy(std::forward<Args>(args)...);
			Run(strategy);
			return 0;
		}
		catch (const std::exception& e) {
			loge("exception: {}", e.what());
			return -1;
		}
		catch (...) {
			loge("unknown exception");
			return -1;
		}
	}

	// 查询合约信息，写入缓存文件
	void MakeCache(const Config& config);



	// 格式化Contract
	inline std::string_view format_as(Contract c)
	{
		return c.Code();
	}

	// 格式化枚举类型
	template<typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
	std::string_view format_as(T t)
	{
		return magic_enum::enum_name(t);
	}
}



#define FMT_FORMATTER(Type, FmtStr, ...) \
	template<> struct fmt::formatter<Type> { \
		constexpr auto parse(fmt::format_parse_context& ctx) { \
			auto it = ctx.begin(); \
			while (it != ctx.end() && *it != '}') ++it; \
			return it; \
		} \
		template<typename FormatContext> \
		auto format(const Type& v, FormatContext& ctx) const { \
			return fmt::format_to(ctx.out(), FMT_COMPILE(FmtStr), __VA_ARGS__); \
		} \
	};

// 格式化Order
FMT_FORMATTER(tinytrader::Order, "{}, Ref:{}, Size:{:+}, Price:{}, Type:{}, Flag:{}, Status:{}, Error:{}, FilledSize:{:+}, TradedSize:{:+}, TradedValue:{}, Canceling:{}",
	v.Instrument, v.Ref, v.Size, v.Price, v.Type, v.Flag, v.Status, v.Error, v.FilledSize, v.TradedSize, v.TradedValue, v.Canceling);
// 格式化Trade
FMT_FORMATTER(tinytrader::Trade, "{}, Size:{:+}, Price:{}, Flag:{}, OrderRef:{}", v.Instrument, v.Size, v.Price, v.Flag, v.OrderRef);
// 格式化Position
FMT_FORMATTER(tinytrader::Position, "Long:{}, LongToday:{}, Short:{}, ShortToday:{}", v.Long, v.LongToday, v.Short, v.ShortToday);
// 格式化PositionEntry
FMT_FORMATTER(tinytrader::PositionEntry, "Code:{}, {}", v.Code, static_cast<const tinytrader::Position&>(v));
