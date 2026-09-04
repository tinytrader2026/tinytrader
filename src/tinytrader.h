#pragma once

#include "fmtlog.h"
#include "fmt/chrono.h"
#include "fmt/compile.h"
#include "magic_enum.hpp"

namespace tinytrader
{
	enum class Market : uint8_t { OTHER, CFFEX, CZCE, DCE, GFEX, INE, SHFE };

	enum class OrderType : uint8_t {
		GFD,		// 当日有效（Good For Day）
		FAK,		// 立即成交，剩余撤销（Fill And Kill）
		FOK			// 立即全部成交，否则撤销（Fill Or Kill）
	};

	enum class TradeFlag : uint8_t {
		Auto,			// 自动开平（平仓优先）
		Open,			// 开仓
		Close,			// 平仓
		CloseToday,		// 平今
		CloseYesterday	// 平昨
	};

	enum class OrderStatus : uint8_t {
		Other,		// 其他
		Sent,		// 本地已发送
		Queuing,	// 处于交易所队列中（含部分成交）
		Filled,		// 全部成交
		Canceled,	// 撤单成功
		Rejected	// 被柜台或交易所拒绝
	};

	class Contract
	{
	public:
		Contract() {}
		Contract(const char* code);
		Contract(const std::string& code);
		operator int() const { return mIndex; }		// 合约序号（用于内部索引）		
		std::string_view Code() const;				// 合约代码		
		Market Exchange() const;					// 所属交易所		
		double PriceTick() const;					// 最小变动价位		
		int Multiplier() const;						// 合约乘数		
		int ToTicks(double price) const				// 将价格转换为跳数
		{
			return static_cast<int>(lround(price / PriceTick()));
		}

	private:
		int mIndex = 0;
	};

	using TimePoint = std::chrono::system_clock::time_point;

	struct Quote
	{
		Contract Instrument;		// 合约
		int Volume = 0;				// 成交量
		double Price = 0;			// 最新价
		int BidSize1 = 0;			// 买一量
		int AskSize1 = 0;			// 卖一量
		double BidPrice1 = 0;		// 买一价
		double AskPrice1 = 0;		// 卖一价
		double Turnover = 0;		// 成交金额
		double OpenInterest = 0;	// 持仓量
		TimePoint MarketTime;		// 交易所时间
		TimePoint ReceiveTime;		// 本地接收时间
		double UpperLimitPrice = 0;	// 涨停价
		double LowerLimitPrice = 0;	// 跌停价
	};

	struct NewOrder
	{
		Contract Instrument;				// 合约
		int Size = 0;						// 委托量（正=买，负=卖）
		double Price = 0;					// 价格
		OrderType Type = OrderType::GFD;	// 订单类型
		TradeFlag Flag = TradeFlag::Auto;	// 开平标志
	};

	struct Order : NewOrder
	{
		OrderStatus Status = OrderStatus::Other;	// 订单状态
		bool Canceling = false;						// 是否有撤单指令在途
		int Ref = 0;								// 订单索引
		int Error = 0;								// 错误码（0 表示正常）
		int FilledSize = 0;							// 已成交数量（带方向，来自订单状态回报）
		int TradedSize = 0;							// 已成交数量（带方向，成交回报累计）
		double TradedValue = 0;						// 成交金额（成交回报累计）
		TimePoint SendTime;							// 下单时间

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
		int Size = 0;						// 成交量（正=买，负=卖）
		double Price = 0;					// 成交价
		TradeFlag Flag = TradeFlag::Auto;	// 开平标志
		int OrderRef = 0;					// 对应的订单索引
		
		double Value() const				// 成交金额
		{
			return abs(Size) * Instrument.Multiplier() * Price;
		}
	};

	struct Position
	{
		int Long = 0;			// 多头总持仓
		int LongToday = 0;		// 多头今仓
		int Short = 0;			// 空头总持仓
		int ShortToday = 0;		// 空头今仓
	};

	struct PositionEntry : Position
	{
		char Code[32] = "NULL";	// 合约代码
	};

	struct Config
	{
		// ----- 必填：CTP 账户信息 -----
		std::string UserID;			// 资金账号
		std::string Password;		// 密码
		std::string BrokerID;		// 经纪公司代码
		std::string AppID;			// App代码
		std::string AuthCode;		// 认证码
		std::string TradeFront;		// 交易前置地址
		std::string MarketFront;	// 行情前置地址

		// ----- 可选 -----
		std::string LogPath;		// 日志文件路径（为空则输出到屏幕）
		std::string CachePath;		// 合约信息缓存路径（为空则不使用缓存）
		int MaxOrderCount = 1000;	// 订单笔数上限（可防止 bug 疯狂下单）
		bool SleepOnIdle = false;	// 空闲时是否短暂休眠（延迟不敏感场景可设为 true 以节省 CPU）
	};

	class Strategy
	{
	public:
		/// 下单。返回订单指针，失败返回 nullptr
		static const Order* Insert(const NewOrder& newOrder);

		/// 撤单。返回是否成功发送撤单请求
		static bool Cancel(const Order& order);

	    /// 订阅行情，可在构造函数或 OnStart 中调用
		void Subscribe(Contract c);

		/// 延时任务，可将耗时任务放到两笔行情间的空档期执行
		/// @param ms 延迟毫秒数
		/// @param task 无参数无返回值的任务函数
		void DelayTask(int ms, std::function<void()> task);

		// ----- 可重写的回调 -----

		/// 策略启动时触发
		virtual void OnStart() {}

		/// 每笔行情到达时触发
		virtual void OnQuote(const Quote& quote) {}

		/// 订单状态变化时触发
		virtual void OnOrder(const Order& order) {}

		/// 产生成交时触发（trade: 成交信息，order: 关联订单）
		virtual void OnTrade(const Trade& trade, const Order& order) {}
	};

	/// 获取当前北京时间
	inline TimePoint Now()
	{
		using namespace std::chrono;
		return system_clock::now() + 8h;
	}

	/// 获取替换时间部分后的时间戳（time 为 HH:MM:SS 格式，不检查）
	inline TimePoint TimeAt(const char* time, TimePoint tp = Now())
	{
		using namespace std::chrono;
		int h = 10 * (time[0] - '0') + time[1] - '0';
		int m = 10 * (time[3] - '0') + time[4] - '0';
		int s = 10 * (time[6] - '0') + time[7] - '0';
		return floor<days>(tp) + hours(h) + minutes(m) + seconds(s);
	}

	/// 获取当前交易日（18 点前返回当日，18 点后返回次日，周末顺延至周一）
	std::string TradingDay();

	/// 获取指定合约的持仓（无需柜台查询）
	Position GetPosition(Contract c);

	/// 获取所有持仓（无需柜台查询）
	std::vector<PositionEntry> GetPositions();

	/// 获取指定合约的最新行情（无需柜台查询）
	Quote GetQuote(Contract c);

	/// 初始化交易引擎
	void InitEngine(const Config& config);

	/// 运行策略（不返回）
	void Run(Strategy& s);

	/// 一行启动：初始化引擎 + 创建策略 + 运行
	/// 策略类的构造函数可以带参数
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

	/// 查询合约信息并写入缓存文件（路径由 Config.CachePath 指定）
	void MakeCache(const Config& config);

	// ============================================================
	// 格式化支持（fmtlib）
	// ============================================================

	inline std::string_view format_as(Contract c)
	{
		return c.Code();
	}

	template<typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
	std::string_view format_as(T t)
	{
		return magic_enum::enum_name(t);
	}
}

// ----- 自定义 fmt::formatter 特化 -----
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

FMT_FORMATTER(tinytrader::Order, "{}, Ref:{}, Size:{:+}, Price:{}, Type:{}, Flag:{}, Status:{}, Error:{}, FilledSize:{:+}, TradedSize:{:+}, TradedValue:{}, Canceling:{}",
	v.Instrument, v.Ref, v.Size, v.Price, v.Type, v.Flag, v.Status, v.Error, v.FilledSize, v.TradedSize, v.TradedValue, v.Canceling);

FMT_FORMATTER(tinytrader::Trade, "{}, Size:{:+}, Price:{}, Flag:{}, OrderRef:{}",
	v.Instrument, v.Size, v.Price, v.Flag, v.OrderRef);

FMT_FORMATTER(tinytrader::Position, "Long:{}, LongToday:{}, Short:{}, ShortToday:{}",
	v.Long, v.LongToday, v.Short, v.ShortToday);

FMT_FORMATTER(tinytrader::PositionEntry, "Code:{}, {}",
	v.Code, static_cast<const tinytrader::Position&>(v));