# tinytrader Python API 文档

## 枚举类型

### Market

交易所枚举。

```python
class Market(Enum):
    OTHER = 0
    CFFEX = 1   # 中国金融期货交易所
    CZCE = 2    # 郑州商品交易所
    DCE = 3     # 大连商品交易所
    GFEX = 4    # 广州期货交易所
    INE = 5     # 上海国际能源交易中心
    SHFE = 6    # 上海期货交易所
```

### OrderType

订单类型。

```python
class OrderType(Enum):
    GFD = 0     # 当日有效（Good For Day）
    FAK = 1     # 立即成交，剩余撤销（Fill And Kill）
    FOK = 2     # 立即全部成交，否则撤销（Fill Or Kill）
```

### TradeFlag

开平仓标志。

```python
class TradeFlag(Enum):
    Auto = 0           # 自动
    Open = 1           # 开仓
    Close = 2          # 平仓
    CloseToday = 3     # 平今
    CloseYesterday = 4 # 平昨
```

### OrderStatus

订单状态。

```python
class OrderStatus(Enum):
    Other = 0      # 其他
    Sent = 1       # 本地已发送
    Queuing = 2    # 处于交易所队列中（含部分成交）
    Filled = 3     # 全部成交
    Canceled = 4   # 撤单成功
    Rejected = 5   # 被柜台或交易所拒绝
```

## 数据类型

### Contract

合约信息。

```python
class Contract:
    """交易合约"""
    
    def __init__(self, code: str):
        """
        创建合约对象。
        
        Args:
            code: 合约代码，如 "rb2501"
        """
        pass
    
    @property
    def Code(self) -> str:
        """合约代码"""
        pass
    
    @property
    def Exchange(self) -> Market:
        """所属交易所"""
        pass
    
    @property
    def PriceTick(self) -> float:
        """最小变动价位"""
        pass
    
    @property
    def Multiplier(self) -> int:
        """合约乘数"""
        pass
    
    def ToTicks(self, price: float) -> int:
        """
        将价格转换为跳数。
        
        Args:
            price: 价格
            
        Returns:
            跳数（价格/最小变动价位四舍五入取整）
        """
        pass
```

### Quote

行情数据。

```python
class Quote:
    """行情快照"""
    
    Instrument: Contract   # 合约
    Volume: int            # 成交量
    Price: float           # 最新价
    BidSize1: int          # 买一量
    AskSize1: int          # 卖一量
    BidPrice1: float       # 买一价
    AskPrice1: float       # 卖一价
    Turnover: float        # 成交金额
    OpenInterest: float    # 持仓量
    MarketTime: datetime   # 交易所时间
    ReceiveTime: datetime  # 本地接收时间
    UpperLimitPrice: float # 涨停价
    LowerLimitPrice: float # 跌停价
```

### NewOrder

新订单。

```python
class NewOrder:
    """新订单"""
    
    Instrument: Contract   # 合约
    Size: int              # 委托量（正=买，负=卖）
    Price: float           # 价格
    Type: OrderType        # 订单类型，默认为 GFD
    Flag: TradeFlag        # 开平标志，默认为 Auto
```

### Order

订单（包含状态）。

```python
class Order(NewOrder):
    """订单（含状态）"""
    
    Status: OrderStatus    # 订单状态
    Canceling: bool        # 是否有撤单指令在途
    Ref: int               # 订单索引
    Error: int             # 错误码
    FilledSize: int        # 已成交数量（带方向，来自订单回报）
    TradedSize: int        # 已成交数量（带方向，成交回报累计）
    TradedValue: float     # 成交金额（成交回报累计）
    SendTime: datetime     # 下单时间
    
    def Terminated(self) -> bool:
        """
        判断订单是否已终结。
        
        Returns:
            如果状态为 Filled、Canceled 或 Rejected 则返回 True
        """
        pass
    
    def AveragePrice(self) -> float:
        """
        计算成交均价。
        
        Returns:
            成交均价（如果没有成交则返回 0）
        """
        pass
```

### Trade

成交回报。

```python
class Trade:
    """成交"""
    
    Instrument: Contract   # 合约
    Size: int              # 成交量（符号表示方向）
    Price: float           # 成交价
    Flag: TradeFlag        # 开平标志
    OrderRef: int          # 订单索引
    
    def Value(self) -> float:
        """
        计算成交金额。
        
        Returns:
            成交金额 = abs(Size) * Multiplier * Price
        """
        pass
```

### Position

持仓。

```python
class Position:
    """持仓"""
    
    Long: int          # 多头持仓
    LongToday: int     # 多头今仓
    Short: int         # 空头持仓
    ShortToday: int    # 空头今仓
```

### PositionEntry

持仓条目（带合约代码）。

```python
class PositionEntry(Position):
    """持仓条目"""
    
    Code: str   # 合约代码
```

### Config

配置信息。

```python
class Config:
    """交易配置"""
    
    UserID: str          # 资金账号
    Password: str        # 密码
    BrokerID: str        # 经纪公司代码
    AppID: str           # App代码
    AuthCode: str        # 认证码
    TradeFront: str      # 交易前置地址
    MarketFront: str     # 行情前置地址
    LogPath: str         # 日志文件路径
    CachePath: str       # 合约信息缓存路径
    MaxOrderCount: int   # 订单笔数上限，默认 1000
    SleepOnIdle: bool    # 空闲时短暂休眠，默认 False
```

## 策略基类

### Strategy

策略基类，用户需继承此类并重写相应方法。

```python
class Strategy:
    """策略基类"""
    
    # 类方法
    @staticmethod
    def Insert(order: NewOrder) -> Optional[Order]:
        """
        下单。
        
        Args:
            order: 新订单
            
        Returns:
            订单对象，如果下单失败返回 None
        """
        pass
    
    @staticmethod
    def Cancel(order: Order) -> bool:
        """
        撤单。
        
        Args:
            order: 要撤销的订单
            
        Returns:
            撤单请求是否成功发送
        """
        pass
    
    # 实例方法
    def Subscribe(self, c: Contract) -> None:
        """
        订阅行情。
        """
        pass
        
    def DelayTask(self, ms: int, task: Callable[[], None]) -> None:
        """
        延时任务。
        
        将耗时操作放到两笔行情间的空档期执行。
        
        Args:
            ms: 延迟毫秒数
            task: 要执行的任务函数
        """
        pass

    # 实例方法（可重写）
    def OnStart(self) -> None:
        """
        策略启动时触发。
        
        在引擎初始化完成后、主循环开始前调用。
        """
        pass
    
    def OnQuote(self, quote: Quote) -> None:
        """
        行情数据触发。
        
        每次收到行情快照时调用。
        
        Args:
            quote: 行情数据
        """
        pass
    
    def OnOrder(self, order: Order) -> None:
        """
        订单回报触发。
        
        订单状态发生变化时调用。
        
        Args:
            order: 订单对象（最新状态）
        """
        pass
    
    def OnTrade(self, trade: Trade, order: Order) -> None:
        """
        成交回报触发。
        
        有成交发生时调用。
        
        Args:
            trade: 成交信息
            order: 对应的订单对象
        """
        pass
```

## 核心函数

### InitEngine

初始化交易引擎。

```python
def InitEngine(config: Config) -> None:
    """
    初始化交易引擎。    
    
    Args:
        config: 交易配置
    """
```

### Run

运行策略。

```python
def Run(strategy: Strategy) -> None:
    """
    运行策略，此函数会阻塞当前线程。
    
    Args:
        strategy: 策略实例
    """
```

### AutoRun

初始化引擎、创建策略并运行。

```python
def AutoRun(config: Config, strategy_class: Type[Strategy], *args, **kwargs) -> None:
    """
    初始化交易引擎，创建策略实例并运行。
    
    Args:
        config: 引擎配置
        strategy_class: 策略类（继承自 Strategy）
        *args: 策略构造函数的 positional 参数
        **kwargs: 策略构造函数的 keyword 参数
    """
```

### Now

获取当前北京时间。

```python
def Now() -> datetime:
    """
    获取当前北京时间。
    
    Returns:
        当前北京时间
    """
```

### TimeAt

获取替换时间部分后的时间戳。

```python
def TimeAt(time: str, tp: datetime | None = None) -> datetime:
    """
    获取指定时间点（替换时分秒）。
    
    Args:
        time: 时间字符串，格式为 "HH:MM:SS"（不检查）
        tp: 基准时间点，默认为当前时间
        
    Returns:
        替换时分秒后的 datetime 对象
    """
```

### TradingDay

获取当前交易日。

```python
def TradingDay() -> str:
    """
    获取当前交易日。18 点后返回次日。周末顺延至周一。
    
    Returns:
        交易日字符串，格式为 "yyyymmdd"
    """
```

### GetPosition

获取指定合约的持仓。

```python
def GetPosition(contract: Contract) -> Position:
    """
    获取指定合约的持仓信息。
    
    Args:
        contract: 合约
        
    Returns:
        持仓信息
    """
```

### GetPositions

获取所有持仓。

```python
def GetPositions() -> List[PositionEntry]:
    """
    获取所有持仓条目。
    
    Returns:
        持仓列表
    """
```

### GetQuote

获取最新行情。

```python
def GetQuote(contract: Contract) -> Quote:
    """
    获取指定合约的最新行情。
    
    Args:
        contract: 合约
        
    Returns:
        最新行情数据
    """
```

### MakeCache

查询合约信息并写入缓存文件。

```python
def MakeCache(config: Config) -> None:
    """
    查询合约信息，写入缓存文件。
    
    用于预先生成合约信息缓存，加快启动速度。
    
    Args:
        config: 交易配置
    """
```


### logd / logi / logw / loge

写日志。

```python
def logd(msg: str) -> None:
    """写入 DEBUG 级别日志"""
    pass

def logi(msg: str) -> None:
    """写入 INFO 级别日志"""
    pass

def logw(msg: str) -> None:
    """写入 WARNING 级别日志"""
    pass

def loge(msg: str) -> None:
    """写入 ERROR 级别日志"""
    pass
```
