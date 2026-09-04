from tinytrader import Strategy, Contract, NewOrder, TimeAt, Now, GetQuote, GetPosition, AutoRun, logi, TradingDay
from datetime import datetime, timedelta
from simnow import SimnowConfig


class Bar:
    def __init__(self, begin_time):
        self.BeginTime = begin_time
        self.Open = 0.0
        self.High = 0.0
        self.Low = 0.0
        self.Close = 0.0
        self.OpenInterest = 0.0
        self.Volume = 0
        self.HasQuote = False


class MinuteBarStrategy(Strategy):
    def __init__(self, target: str, active: str, close_time: str = "15:00:00"):
        super().__init__()        
        self.Target = Contract(target)  # 交易合约
        self.Active = Contract(active)  # 活跃合约，用做时钟源
        self.CloseTime = close_time
        
        # 交易时间配置
        self.StartTime = ""
        self.Break1 = ""
        self.Break2 = ""
        self._init_schedule(close_time)
        
        # K线状态
        self.Bar = None
        self.Price = 0.0
        self.OpenInterest = 0.0
        self.TotalVolume = 0
        self.HasTask = False
        
        # 订阅合约
        self.Subscribe(self.Active)
        self.Subscribe(self.Target)
    
    def Target(self) -> Contract:
        """获取交易合约"""
        return self.Target
    
    def _init_schedule(self, close_time: str):
        """初始化交易时间表"""
        self.CloseTime = close_time
        
        if close_time in ("15:00:00", "15:15:00"):
            # 根据交易所设置不同的开盘时间
            if self.Target.Exchange() == "CFFEX":
                self.StartTime = "09:30:00"
                self.Break1 = "11:30:00"
            else:
                self.StartTime = "09:00:00"
                self.Break1 = "10:15:00"
                self.Break2 = "11:30:00"
        elif close_time in ("23:00:00", "01:00:00", "02:30:00"):
            self.StartTime = "21:00:00"
        else:
            raise ValueError(f"invalid closeTime: {close_time}")
    
    def _minute_begin(self, tp: datetime) -> datetime:
        """获取分钟K线的开始时间"""
        begin = tp.replace(second=0, microsecond=0)
        begin_str = begin.strftime("%H:%M:%S")
        
        # 集合竞价行情并入后一分钟
        start_time_dt = TimeAt(self.StartTime)
        if begin + timedelta(minutes=1) == start_time_dt:
            begin += timedelta(minutes=1)
        # 收盘行情并入前一分钟
        elif begin_str in (self.Break1, self.Break2, self.CloseTime):
            begin -= timedelta(minutes=1)
        
        return begin
    
    def _is_trading_time(self, begin: datetime) -> bool:
        """判断是否交易时间"""
        time_str = begin.strftime("%H:%M:%S")
        close_hour = int(self.CloseTime[:2])
        
        # 夜盘情况
        if close_hour <= 2:
            return time_str >= self.StartTime or time_str < self.CloseTime
        return self.StartTime <= time_str < self.CloseTime
    
    def _close_bar(self):
        """结束当前K线"""
        if self.Bar and self._is_trading_time(self.Bar.BeginTime):
            self.OnBar(self.Bar)
    
    def OnQuote(self, q):
        # 计算分钟K线的开始时间
        begin = self._minute_begin(q.MarketTime)
        
        # 新K线开始
        if self.Bar is None or begin > self.Bar.BeginTime:
            self._close_bar()
            self.Bar = Bar(begin)
            self.Bar.Open = self.Bar.High = self.Bar.Low = self.Bar.Close = self.Price
            self.Bar.OpenInterest = self.OpenInterest

        close_time_dt = TimeAt(self.CloseTime)
        if (not self.HasTask and begin + timedelta(seconds=60) == close_time_dt):
            self.HasTask = True
            self.DelayTask(63000, self._close_bar)      # 收盘3秒后补齐最后一根K线

        if q.Instrument == self.Target:     # 更新K线数据            
            self.Price = q.Price
            self.OpenInterest = q.OpenInterest            
            if not self.Bar.HasQuote:
                self.Bar.Open = self.Bar.High = self.Bar.Low = self.Bar.Close = self.Price
            else:
                self.Bar.High = max(self.Bar.High, self.Price)
                self.Bar.Low = min(self.Bar.Low, self.Price)
                self.Bar.Close = self.Price
            
            self.Bar.OpenInterest = self.OpenInterest
            self.Bar.Volume += q.Volume - self.TotalVolume
            self.TotalVolume = q.Volume
            self.Bar.HasQuote = True
    
    def OnBar(self, bar: Bar):
        """K线回调 - 由子类实现"""
        raise NotImplementedError("Subclasses must implement OnBar")


# ============ 双均线策略 ============
class DualMovingAverage(MinuteBarStrategy):
    def __init__(self, target: str, active: str, close_time: str = "15:00:00"):
        super().__init__(target, active, close_time)        
        self.FastPeriod = 5
        self.SlowPeriod = 10        
        self.Bars = []              # K线列表
        self.Order = None           # 当前买单
    
    def OnStart(self):
        print("OnStart")
        logi(f"NetPosition, {self.Target}, {self.NetPosition()}")
    
    def OnBar(self, bar: Bar):
        logi(f"MinuteBar, {self.Target}, "
            f"BeginTime:{bar.BeginTime.strftime('%Y%m%d%H%M')}, "
            f"Open:{bar.Open}, High:{bar.High}, "
            f"Low:{bar.Low}, Close:{bar.Close}, "
            f"OpenInterest:{bar.OpenInterest}, Volume:{bar.Volume}")
        
        self.Bars.append(bar)
        if len(self.Bars) < self.SlowPeriod + 1:
            return
        
        # 计算均线
        fast_ma = self._calculate_ma(self.FastPeriod)
        slow_ma = self._calculate_ma(self.SlowPeriod)
        prev_fast_ma = self._calculate_ma(self.FastPeriod, 1)
        prev_slow_ma = self._calculate_ma(self.SlowPeriod, 1)
        
        net_pos = self.NetPosition()
        
        # 金叉：快线上穿慢线 -> 买入
        if prev_fast_ma < prev_slow_ma and fast_ma > slow_ma and net_pos == 0:
            self._buy(bar.Close)
        # 死叉：快线下穿慢线 -> 卖出
        elif prev_fast_ma > prev_slow_ma and fast_ma < slow_ma and net_pos > 0:
            self._sell()
    
    def _calculate_ma(self, period: int, offset: int = 0) -> float:
        """计算移动平均线
        
        Args:
            period: 周期
            offset: 0表示当前，1表示前一根
        """
        if len(self.Bars) < period + offset:
            return 0.0
        
        total = 0.0
        end = len(self.Bars) - offset
        for i in range(end - period, end):
            total += self.Bars[i].Close
        return total / period
    
    def NetPosition(self) -> int:
        """获取净持仓"""
        pos = GetPosition(self.Target)
        return pos.Long - pos.Short
    
    def _buy(self, price: float):
        order = NewOrder(Instrument=self.Target, Size=1, Price=price)
        self.Order = self.Insert(order)
        logi(f"Buy order inserted: {order}")        
        self.DelayTask(30000, self._cancel_buy)     # 30秒后未成交则撤单
    
    def _sell(self):
        quote = GetQuote(self.Target)
        order = NewOrder(Instrument=self.Target, Size=-1, Price=quote.LowerLimitPrice)
        self.Insert(order)
    
    def _cancel_buy(self):
        if self.Order and self.Order.Status == "Queuing":
            self.Cancel(self.Order)


if __name__ == "__main__":
    config = SimnowConfig()
    config.LogPath = f"{TradingDay()}_dma.log"
    config.SleepOnIdle = True
    
    target = "hc2701"   # 交易合约
    active = "rb2701"   # 活跃合约，用做时钟源
    close_time = "23:00:00" if Now() > TimeAt("15:00:00") else "15:00:00"
    
    AutoRun(config, DualMovingAverage, target=target, active=active, close_time=close_time)

