"""
TinyTrader - Lightweight C++ trading engine with Python bindings
"""

from datetime import datetime
import sys
import importlib.util
import os
from importlib.metadata import version

__version__ = version("tinytrader")

# 1. 添加 DLL 搜索路径
_pkg_dir = os.path.dirname(__file__)
if hasattr(os, "add_dll_directory"):
	os.add_dll_directory(_pkg_dir)

# 2. 选择正确的 pyd 路径
_ext = '.pyd' if sys.platform == 'win32' else '.so'
_module_name = "_tinytrader" + _ext
_module_path = os.path.join(_pkg_dir, _module_name)
if not os.path.exists(_module_path):
	_sub_dir = f"{sys.version_info.major}.{sys.version_info.minor}"
	_module_path = os.path.join(_pkg_dir, _sub_dir, _module_name)	
	if not os.path.exists(_module_path):
		raise ImportError(_module_name + " not found")

# 3. 如果模块已经加载，先清理（防止重复加载）
if "_tinytrader" in sys.modules:
	del sys.modules["_tinytrader"]

# 4. 动态加载
spec = importlib.util.spec_from_file_location("_tinytrader", _module_path)
_tinytrader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(_tinytrader)

# 5. 注册到 sys.modules（让后续导入使用缓存）
sys.modules["_tinytrader"] = _tinytrader


_current_module = sys.modules[__name__]
for name in dir(_tinytrader):
	if not name.startswith("_"):
		setattr(_current_module, name, getattr(_tinytrader, name))

# 设置 __all__
__all__ = [name for name in dir(_tinytrader) if not name.startswith("_")]




# ========== __repr__ ==========

def _contract_repr(self):
	return f"{self.Code()}"


def _enum_repr(self):
	return f"{self.name}"


def _order_repr(self):
	return (f"Order(Instrument={self.Instrument}, Ref={self.Ref}, "
			f"Size={self.Size:+d}, Price={self.Price}, Type={self.Type}, "
			f"Flag={self.Flag}, Status={self.Status}, Error={self.Error}, "
			f"FilledSize={self.FilledSize:+d}, TradedSize={self.TradedSize:+d}, "
			f"TradedValue={self.TradedValue}, Canceling={self.Canceling})")


def _trade_repr(self):
	return (f"Trade(Instrument={self.Instrument}, Size={self.Size:+d}, "
			f"Price={self.Price}, Flag={self.Flag}, OrderRef={self.OrderRef})")


def _position_repr(self):
	return (f"Position(Long={self.Long}, LongToday={self.LongToday}, "
			f"Short={self.Short}, ShortToday={self.ShortToday})")


def _position_entry_repr(self):
	return (f"PositionEntry(Code={self.Code}, Long={self.Long}, "
			f"LongToday={self.LongToday}, Short={self.Short}, "
			f"ShortToday={self.ShortToday})")


# 注入 __repr__
Contract.__repr__ = _contract_repr

for enum_cls in [Market, OrderType, TradeFlag, OrderStatus]:
	enum_cls.__repr__ = _enum_repr

Order.__repr__ = _order_repr
Trade.__repr__ = _trade_repr
Position.__repr__ = _position_repr
PositionEntry.__repr__ = _position_entry_repr




def _order_eq(self, other):
	if not isinstance(other, Order):
		return False
	return self.Ref == other.Ref

Order.__eq__ = _order_eq



def TimeAt(time: str, tp: datetime = Now()):
	"""获取指定时间的时间戳。
	Args:
		time (str): 时间字符串，格式 "HH:MM:SS"
	Returns:
		datetime: 当日指定时间点
	"""
	h, m, s = map(int, time.split(':'))
	return tp.replace(hour=h, minute=m, second=s, microsecond=0)



def AutoRun(config, strategy_class, *args, **kwargs):
	"""初始化引擎、创建策略并运行"""
	InitEngine(config)
	strategy = strategy_class(*args, **kwargs)
	Run(strategy)



# ========== 类文档 ==========

Contract.__doc__ = """
合约对象。

用法：
	c = Contract("rb2701")

方法：
	Code() -> str: 合约代码
	Exchange() -> Market: 所属交易所
	TickSize() -> float: 最小变动价位
	Multiplier() -> int: 合约乘数
	ToTicks(price: float) -> int: 将价格转换为跳数

示例：
	c = Contract("rb2701")
	print(c.Code())        # "rb2701"
	print(c.Exchange())    # SHFE
	print(c.TickSize())    # 1.0
"""

Quote.__doc__ = """
行情数据。

字段说明：
	Instrument (Contract): 合约
	Price (float): 最新价
	Volume (int): 成交量
	BidPrice1 (float): 买一价
	BidSize1 (int): 买一量
	AskPrice1 (float): 卖一价
	AskSize1 (int): 卖一量
	Turnover (float): 成交金额
	OpenInterest (float): 持仓量
	MarketTime (datetime): 交易所时间
	ReceiveTime (datetime): 本地接收时间
	UpperLimitPrice (float): 涨停价
	LowerLimitPrice (float): 跌停价
"""

NewOrder.__doc__ = """
下单请求结构。

字段说明：
	Instrument (Contract): 合约
	Size (int): 委托量，正=买，负=卖
	Price (float): 委托价格
	Type (OrderType): 订单类型
	Flag (TradeFlag): 开平标志
"""

Order.__doc__ = """
订单对象（继承自 NewOrder）。

额外字段：
	Status (OrderStatus): 订单状态
	Canceling (bool): 是否有撤单指令在途
	SendTime (datetime): 下单时间
	Ref (int): 订单索引
	Error (OrderError): 错误码，0 表示无错误
	FilledSize (int): 已成交数量（带方向）
	TradedSize (int): 累计成交量（带方向）
	TradedValue (float): 累计成交金额

方法：
	Terminated() -> bool: 订单是否已结束（Filled / Canceled / Rejected）
	AveragePrice() -> float: 成交均价
"""

Trade.__doc__ = """
成交回报。

字段说明：
	Instrument (Contract): 合约
	Size (int): 成交量，正=买，负=卖
	Price (float): 成交价
	Flag (TradeFlag): 开平标志
	OrderRef (int): 对应的订单索引

方法：
	Value() -> float: 成交金额 = abs(Size) * Multiplier * Price
"""

Position.__doc__ = """
持仓汇总。

字段说明：
	Long (int): 多头总持仓
	LongToday (int): 多头今仓
	Short (int): 空头总持仓
	ShortToday (int): 空头今仓
"""

PositionEntry.__doc__ = """
单个合约的持仓（继承自 Position）。

额外字段：
	Code (str): 合约代码
"""

Config.__doc__ = """
引擎配置。

字段说明：
	UserID (str): 资金账号
	Password (str): 密码
	BrokerID (str): 经纪公司代码
	AppID (str): AppID
	AuthCode (str): 授权码
	TradeFront (str): 交易前置地址
	MarketFront (str): 行情前置地址
	LogPath (str): 日志文件路径
	CachePath (str): 合约信息缓存路径
	MaxOrderCount (int): 订单笔数上限，默认 10000
	TimerInterval (int): 定时器间隔（毫秒），默认 100
	BusyLoop (bool): 是否占满 CPU 核，默认 True
"""
