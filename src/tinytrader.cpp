#include "ThostFtdcMdApi.h"
#include "ThostFtdcTraderApi.h"
#include "tinytrader.h"
#include <array>
#include <charconv>
#include <csignal>
#include <fstream>
#include <queue>
#include <set>
#include <unordered_map>
#include <variant>

using namespace std;
using namespace chrono;
using namespace tinytrader;

namespace {
	struct alignas(64) Data : PositionEntry
	{
		static constexpr int N = 2;

		double PriceTick = 0;
		int Multiplier = 0;
		Market Exchange = Market::OTHER;
		Quote Quotes[N];
		int Count = 0;

		Quote& Acquire()
		{
			return Quotes[Count % N];
		}

		void Commit()
		{
			atomic_thread_fence(memory_order_release);
			++Count;
		}

		const Quote& GetQuote() const
		{
			return Quotes[(Count + N - 1) % N];
		}

		void UpdatePosition(int size, TradeFlag flag)
		{
			if (int x = abs(size); flag == TradeFlag::Open) {
				(size > 0 ? Long : Short) += x;
				(size > 0 ? LongToday : ShortToday) += x;
			}
			else {
				int& pos = size > 0 ? Short : Long;
				int& today = size > 0 ? ShortToday : LongToday;
				pos -= x;
				if (flag == TradeFlag::Close)
					today = min(today, pos);
				else if (flag == TradeFlag::CloseToday)
					today -= x;
			}
		}
	};

	template<size_t N>
	void strcpy_safe(char(&dst)[N], string_view src)
	{
		static_assert(N > 0, "strcpy_safe: invalid array");
		size_t len = src.copy(dst, N - 1);
		dst[len] = '\0';
	}

	class ContractRegistry
	{
	public:
		int Find(string_view sv) const
		{
			if (!Count())
				throw runtime_error("Contracts uninitialized");
			auto it = mIndecis.find(sv);
			return it != mIndecis.end() ? it->second : 0;
		}

		int Count() const
		{
			return static_cast<int>(mIndecis.size());
		}

		vector<PositionEntry> Positions() const
		{
			vector<PositionEntry> v;
			for (auto [sv, i] : mIndecis)
				if (mData[i].Long || mData[i].Short)
					v.emplace_back(mData[i]);
			return v;
		}

		void InitContract(string_view code, Market market, double priceTick, int multiplier)
		{
			if (mIndecis.count(code))
				return;
			if (mIndecis.size() + 1 >= mData.size())
				throw runtime_error("too many contracts");

			int i = Count() + 1;
			strcpy_safe(mData[i].Code, code);
			mData[i].PriceTick = priceTick;
			mData[i].Multiplier = multiplier;
			mData[i].Exchange = market;
			mIndecis[mData[i].Code] = i;
		}

		Data& operator[](int i)
		{
			return mData[i];
		}

	private:
		unordered_map<string_view, int> mIndecis;
		array<Data, 40000> mData;
	};

	ContractRegistry gRegistry;
}

namespace tinytrader {
	Contract::Contract(const char* code)
	{
		mIndex = gRegistry.Find(code);
	}

	Contract::Contract(const string& code)
	{
		mIndex = gRegistry.Find(code);
	}

	string_view Contract::Code() const
	{
		return gRegistry[mIndex].Code;
	}

	Market Contract::Exchange() const
	{
		return gRegistry[mIndex].Exchange;
	}

	double Contract::PriceTick() const
	{
		return gRegistry[mIndex].PriceTick;
	}

	int Contract::Multiplier() const
	{
		return gRegistry[mIndex].Multiplier;
	}

	Position GetPosition(Contract c)
	{
		return static_cast<Position&>(gRegistry[c]);
	}

	vector<PositionEntry> GetPositions()
	{
		return gRegistry.Positions();
	}

	Quote GetQuote(Contract c)
	{
		return gRegistry[c].GetQuote();
	}

	string TradingDay()
	{
		TimePoint tp = Now();
		if (tp >= TimeAt("18:00:00"))
			tp += 24h;
		int n = duration_cast<days>(tp.time_since_epoch()).count();
		if (int weekday = (n + 4) % 7; weekday == 0)
			tp += 24h;
		else if (weekday == 6)
			tp += 48h;
		return fmt::format(FMT_COMPILE("{:%Y%m%d}"), tp);
	}
}

namespace {
	bool gStop = false;

	void SignalHandler(int sig)
	{
		gStop = true;
		signal(sig, SignalHandler);
	}

	template<typename T, int N = 8 * 1024>
	class Buffer
	{
	public:
		void Push(const T& t)
		{
			mData[mCount % N] = t;
			atomic_thread_fence(memory_order_release);
			++mCount;
		}

		size_t Count() const
		{
			atomic_thread_fence(memory_order_acquire);
			return mCount;
		}

		const T& Get(size_t i)
		{
			return mData[i % N];
		}

	private:
		size_t mCount = 0;
		T mData[N] = {};
	};

	class Waiter
	{
	protected:
		void Wait(const char* name)
		{
			for (TimePoint beg = Now(); !gStop && !mStatus;) {
				if (Now() > beg + 90s)
					throw runtime_error(fmt::format("{} init timeout", name));
				this_thread::sleep_for(10ms);
			}

			if (mStatus < 0)
				throw runtime_error(mErrMsg);
		}

		template <typename... Args>
		void SetError(const char* fmtStr, Args&&... args) {
			mErrMsg = fmt::format(fmt::runtime(fmtStr), forward<Args>(args)...);
			atomic_thread_fence(memory_order_release);
			mStatus = -1;
		}

		void SetReady()
		{
			mStatus = 1;
		}

	private:
		int mStatus = 0;
		string mErrMsg;
	};

	class CTPMarket : public CThostFtdcMdSpi, public Waiter, public Buffer<const Quote*>
	{
		static TimePoint MarketTime(TimePoint receive_time, const char* time, int ms)
		{
			TimePoint tp = TimeAt(time, receive_time) + milliseconds(ms);
			if (tp - receive_time > 12h)
				tp -= 24h;
			else if (receive_time - tp > 12h)
				tp += 24h;
			return tp;
		}

	public:
		~CTPMarket()
		{
			if (mApi) mApi->Release();
		}

		void Init(string addr, const set<string>& codes)
		{
			if (codes.empty()) return;

			mAddr = addr;
			mSubList.assign(codes.begin(), codes.end());
			mApi = CThostFtdcMdApi::CreateFtdcMdApi();
			mApi->RegisterSpi(this);
			mApi->RegisterFront(&addr[0]);
			mApi->Init();
			Wait("CTPMarket");
		}

	private:
		void OnFrontConnected() override
		{
			logi("CTPMarket Login ...", __func__);
			CThostFtdcReqUserLoginField req = {};
			if (int err = mApi->ReqUserLogin(&req, 0))
				SetError("ReqUserLogin error:{}", err);
		}

		void OnFrontDisconnected(int nReason) override
		{
			loge("CTPMarket::{}, Reason:{}", __func__, nReason);
		}

		void OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) override
		{
			if (pRspInfo && pRspInfo->ErrorID)
				return SetError("CTPMarket::{}, ErrorID:{}", __func__, pRspInfo->ErrorID);

			SetReady();
			this_thread::sleep_for(50ms);
			logi("SubscribeMarketData {}", mSubList.size());
			vector<char*> ids;
			for (string& s : mSubList)
				ids.push_back(&s[0]);
			if (int err = mApi->SubscribeMarketData(&ids[0], int(ids.size())))
				SetError("SubscribeMarketData error:{}", err);
		}

		void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField* pDepthMarketData) override
		{
			TimePoint recvTime = Now();
			Contract c(pDepthMarketData->InstrumentID);
			Data& d = gRegistry[c];
			Quote& q = d.Acquire();

			q.Instrument = c;
			q.Volume = pDepthMarketData->Volume;
			q.Price = pDepthMarketData->LastPrice;
			q.BidSize1 = pDepthMarketData->BidVolume1;
			q.AskSize1 = pDepthMarketData->AskVolume1;
			q.BidPrice1 = pDepthMarketData->BidPrice1;
			q.AskPrice1 = pDepthMarketData->AskPrice1;
			q.Turnover = pDepthMarketData->Turnover;
			q.OpenInterest = pDepthMarketData->OpenInterest;
			q.MarketTime = MarketTime(recvTime, pDepthMarketData->UpdateTime, pDepthMarketData->UpdateMillisec);
			q.ReceiveTime = recvTime;
			q.UpperLimitPrice = pDepthMarketData->UpperLimitPrice;
			q.LowerLimitPrice = pDepthMarketData->LowerLimitPrice;
			d.Commit();

			Push(&q);
		}

		CThostFtdcMdApi* mApi = nullptr;
		string mAddr;
		vector<string> mSubList;
	};

	struct Report
	{
		int OrderRef = 0;
		int ErrorID = 0;
		int FilledSize = 0;
		OrderStatus Status = OrderStatus::Other;
	};

	struct CancelReject
	{
		int OrderRef = 0;
	};

	using Message = variant<Report, Trade, CancelReject>;

	class CTPGateway : public CThostFtdcTraderSpi, public Waiter, public Buffer<Message>
	{
		static Market ToMarket(const char* s)
		{
			return magic_enum::enum_cast<Market>(s).value_or(Market::OTHER);
		}

		static OrderStatus ToStatus(TThostFtdcOrderStatusType s)
		{
			switch (s) {
			case THOST_FTDC_OST_AllTraded:
				return OrderStatus::Filled;
			case THOST_FTDC_OST_PartTradedQueueing:
			case THOST_FTDC_OST_NoTradeQueueing:
				return OrderStatus::Queuing;
			case THOST_FTDC_OST_PartTradedNotQueueing:
			case THOST_FTDC_OST_Canceled:
				return OrderStatus::Canceled;
			}
			return OrderStatus::Other;
		}

		static TradeFlag ToFlag(TThostFtdcOffsetFlagType c)
		{
			switch (c) {
			case THOST_FTDC_OF_Open:
				return TradeFlag::Open;
			case THOST_FTDC_OF_CloseToday:
				return TradeFlag::CloseToday;
			case THOST_FTDC_OF_CloseYesterday:
				return TradeFlag::CloseYesterday;
			}
			return TradeFlag::Close;
		}

		template<typename T>
		static int64_t OrderKey(const T* ptr)
		{
			return atoll(ptr->OrderSysID) * 100 + (int)ToMarket(ptr->ExchangeID);
		}

		static void SetRef(TThostFtdcOrderRefType& buf, int ref)
		{
			auto [ptr, ec] = to_chars(buf, buf + sizeof(buf) - 1, ref);
			*ptr = 0;
		}

		struct CacheHeader
		{
			char Date[9] = {};
			char Version[39] = {};
		};

	public:
		~CTPGateway()
		{
			if (mApi) mApi->Release();
		}

		void Init(const Config* config, bool CacheOnly = false)
		{
			mConfig = config;
			mCacheOnly = CacheOnly;
			mApi = CThostFtdcTraderApi::CreateFtdcTraderApi();
			mApi->RegisterSpi(this);
			mApi->RegisterFront(const_cast<char*>(config->TradeFront.c_str()));
			mApi->SubscribePrivateTopic(THOST_TERT_QUICK);
			mApi->Init();
			strcpy_safe(mHeader.Version, mApi->GetApiVersion());
			Wait("CTPGateway");

			strcpy_safe(mOrder.BrokerID, mConfig->BrokerID);
			strcpy_safe(mOrder.InvestorID, mConfig->UserID);
			mOrder.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
			mOrder.MinVolume = 1;
			mOrder.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
			mOrder.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
			mOrder.ContingentCondition = THOST_FTDC_CC_Immediately;

			strcpy_safe(mCancel.BrokerID, mConfig->BrokerID);
			strcpy_safe(mCancel.InvestorID, mConfig->UserID);
			mCancel.ActionFlag = THOST_FTDC_AF_Delete;

			for (auto& e : GetPositions())
				logi("PositionEntry: {}", e);
		}

		int Insert(const NewOrder& input, TradeFlag flag, int ref)
		{
			static constexpr char values[] = { 0, THOST_FTDC_OF_Open, THOST_FTDC_OF_Close, THOST_FTDC_OF_CloseToday, THOST_FTDC_OF_CloseYesterday };

			strcpy_safe(mOrder.InstrumentID, input.Instrument.Code());
			mOrder.Direction = input.Size > 0 ? THOST_FTDC_D_Buy : THOST_FTDC_D_Sell;
			mOrder.CombOffsetFlag[0] = values[static_cast<int>(flag)];
			mOrder.LimitPrice = input.Price;
			mOrder.VolumeTotalOriginal = abs(input.Size);
			mOrder.TimeCondition = (input.Type == OrderType::FAK || input.Type == OrderType::FOK) ? THOST_FTDC_TC_IOC : THOST_FTDC_TC_GFD;
			mOrder.VolumeCondition = input.Type == OrderType::FOK ? THOST_FTDC_VC_CV : THOST_FTDC_VC_AV;
			SetRef(mOrder.OrderRef, ref);
			return mApi->ReqOrderInsert(&mOrder, mReqid++);
		}

		bool Cancel(const Order& order)
		{
			strcpy_safe(mCancel.InstrumentID, order.Instrument.Code());
			SetRef(mCancel.OrderRef, order.Ref);
			int err = mApi->ReqOrderAction(&mCancel, mReqid++);
			logi("Cancel, {}, Ref:{}, Err:{}", order.Instrument, order.Ref, err);
			return err == 0;
		}

	private:
		void OnFrontConnected() override
		{
			logi("Authenticate ...");
			CThostFtdcReqAuthenticateField req = {};
			strcpy_safe(req.BrokerID, mConfig->BrokerID);
			strcpy_safe(req.UserID, mConfig->UserID);
			strcpy_safe(req.AuthCode, mConfig->AuthCode);
			strcpy_safe(req.AppID, mConfig->AppID);
			if (int err = mApi->ReqAuthenticate(&req, mReqid++))
				SetError("ReqAuthenticate error:{}", err);
		}

		void OnFrontDisconnected(int nReason) override
		{
			loge("CTPGateway::{}, Reason:{}", __func__, nReason);
			gStop = true;
		}

		void OnRspAuthenticate(CThostFtdcRspAuthenticateField* pRspAuthenticateField, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) override
		{
			if (pRspInfo && pRspInfo->ErrorID)
				return SetError("CTPGateway::{}, ErrorID:{}", __func__, pRspInfo->ErrorID);

			logi("CTPGateway Login ...");
			CThostFtdcReqUserLoginField req = {};
			strcpy_safe(req.BrokerID, mConfig->BrokerID);
			strcpy_safe(req.UserID, mConfig->UserID);
			strcpy_safe(req.Password, mConfig->Password);
			if (int err = mApi->ReqUserLogin(&req, mReqid++))
				loge("ReqUserLogin error:{}", err);
		}

		void OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) override
		{
			if (pRspInfo && pRspInfo->ErrorID)
				return SetError("CTPGateway::{}, ErrorID:{}", __func__, pRspInfo->ErrorID);

			strcpy_safe(mHeader.Date, pRspUserLogin->TradingDay);
			mCancel.FrontID = pRspUserLogin->FrontID;
			mCancel.SessionID = pRspUserLogin->SessionID;
			logi("TradingDay:{}, UserID:{}, FrontID:{}, SessionID:{}", mHeader.Date, mConfig->UserID, mCancel.FrontID, mCancel.SessionID);

			CThostFtdcSettlementInfoConfirmField req = {};
			strcpy_safe(req.BrokerID, mConfig->BrokerID);
			strcpy_safe(req.InvestorID, mConfig->UserID);
			if (int err = mApi->ReqSettlementInfoConfirm(&req, mReqid++))
				SetError("ReqSettlementInfoConfirm error:{}", err);
		}

		void OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField* pSettlementInfoConfirm, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) override
		{
			logi("{}", __func__);

			if (pRspInfo && pRspInfo->ErrorID)
				return SetError("CTPGateway::{}, ErrorID:{}", __func__, pRspInfo->ErrorID);

			if (mCacheOnly || !LoadCache()) {
				logi("QryInstrument ...");
				CThostFtdcQryInstrumentField req = {};
				if (int err = mApi->ReqQryInstrument(&req, mReqid++))
					SetError("ReqQryInvestorPosition error:{}", err);
				return;
			}

			for (size_t i = 0; i < mCache.size(); ++i)
				OnRspQryInstrument(&mCache[i], nullptr, 0, i + 1 == mCache.size());
		}

		void OnRspQryInstrument(CThostFtdcInstrumentField* pInstrument, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) override
		{
			if (pRspInfo && pRspInfo->ErrorID)
				return SetError("CTPGateway::{}, ErrorID:{}", __func__, pRspInfo->ErrorID);

			if (mCacheOnly)	return SaveCache(pInstrument, bIsLast);

			if (auto market = ToMarket(pInstrument->ExchangeID); market != Market::OTHER)
				gRegistry.InitContract(pInstrument->InstrumentID, market, pInstrument->PriceTick, pInstrument->VolumeMultiple);
			else
				loge("Unknown Exchange {}, {}", pInstrument->ExchangeID, pInstrument->InstrumentID);

			if (!bIsLast) return;

			logi("{} Contracts initialized", gRegistry.Count());
			this_thread::sleep_for(500ms);
			CThostFtdcQryInvestorPositionField req = {};
			strcpy_safe(req.BrokerID, mConfig->BrokerID);
			strcpy_safe(req.InvestorID, mConfig->UserID);
			if (int err = mApi->ReqQryInvestorPosition(&req, mReqid++))
				SetError("ReqQryInvestorPosition error:{}", err);
		}

		void OnRspQryInvestorPosition(CThostFtdcInvestorPositionField* pInvestorPosition, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) override
		{
			if (pRspInfo && pRspInfo->ErrorID)
				return SetError("CTPGateway::{}, ErrorID:{}", __func__, pRspInfo->ErrorID);

			if (pInvestorPosition) {
				logi("OnRspQryInvestorPosition, {}, PosiDirection:{}, Position:{}, TodayPosition:{}",
					pInvestorPosition->InstrumentID, pInvestorPosition->PosiDirection, pInvestorPosition->Position, pInvestorPosition->TodayPosition);

				Position& pos = gRegistry[Contract(pInvestorPosition->InstrumentID)];
				if (pInvestorPosition->PosiDirection == THOST_FTDC_PD_Long) {
					pos.Long = pInvestorPosition->Position;
					pos.LongToday = pInvestorPosition->TodayPosition;
				}
				else if (pInvestorPosition->PosiDirection == THOST_FTDC_PD_Short) {
					pos.Short = pInvestorPosition->Position;
					pos.ShortToday = pInvestorPosition->TodayPosition;
				}
			}

			if (bIsLast) SetReady();
		}

		void OnRspOrderInsert(CThostFtdcInputOrderField* pInputOrder, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) override
		{
			if (pRspInfo && pRspInfo->ErrorID) {
				loge("CTPGateway::{}, {}, OrderRef:{}, ErrorID:{}", __func__, pInputOrder->InstrumentID, pInputOrder->OrderRef, pRspInfo->ErrorID);
				Report report;
				report.OrderRef = atoi(pInputOrder->OrderRef);
				report.ErrorID = pRspInfo->ErrorID;
				report.FilledSize = 0;
				report.Status = OrderStatus::Rejected;
				Push(report);
			}
		}

		void OnRspOrderAction(CThostFtdcInputOrderActionField* pInputOrderAction, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) override
		{
			if (pRspInfo && pRspInfo->ErrorID) {
				logi("CTPGateway::{}, {}, OrderRef:{}, OrderSysID:{}, ErrorID:{}", __func__, pInputOrderAction->InstrumentID, pInputOrderAction->OrderRef, pInputOrderAction->OrderSysID, pRspInfo->ErrorID);
				CancelReject reject;
				reject.OrderRef = atoi(pInputOrderAction->OrderRef);
				Push(reject);
			}
		}

		void OnRtnOrder(CThostFtdcOrderField* pOrder) override
		{
			logi("OnRtnOrder, {}, OrderRef:{}, OrderSysID:{}, VolumeTotalOriginal:{}, VolumeTraded:{}, VolumeTotal:{}, OrderStatus:{}, FrontID:{}, SessionID:{}",
				pOrder->InstrumentID, pOrder->OrderRef, pOrder->OrderSysID, pOrder->VolumeTotalOriginal, pOrder->VolumeTraded, pOrder->VolumeTotal, pOrder->OrderStatus, pOrder->FrontID, pOrder->SessionID);

			if (pOrder->OrderStatus != THOST_FTDC_OST_Unknown && pOrder->FrontID == mCancel.FrontID && pOrder->SessionID == mCancel.SessionID) {
				Report report;
				report.OrderRef = atoi(pOrder->OrderRef);
				report.ErrorID = 0;
				report.FilledSize = pOrder->Direction == THOST_FTDC_D_Buy ? pOrder->VolumeTraded : -pOrder->VolumeTraded;
				report.Status = ToStatus(pOrder->OrderStatus);
				Push(report);
				if (pOrder->OrderSysID[0])
					mKeys.emplace(OrderKey(pOrder));
			}
		}

		void OnRtnTrade(CThostFtdcTradeField* pTrade) override
		{
			logi("OnRtnTrade, {}, Direction:{}, Price:{:.2f}, Volume:{}, OffsetFlag:{}, OrderRef:{}, OrderSysID:{}, TradeID:{}",
				pTrade->InstrumentID, pTrade->Direction, pTrade->Price, pTrade->Volume, pTrade->OffsetFlag, pTrade->OrderRef, pTrade->OrderSysID, pTrade->TradeID);
			Trade trade;
			trade.Instrument = pTrade->InstrumentID;
			trade.Size = pTrade->Direction == THOST_FTDC_D_Buy ? pTrade->Volume : -pTrade->Volume;
			trade.Price = pTrade->Price;
			trade.Flag = ToFlag(pTrade->OffsetFlag);
			trade.OrderRef = mKeys.count(OrderKey(pTrade)) ? atoi(pTrade->OrderRef) : -1;
			Push(trade);
		}

		void OnErrRtnOrderInsert(CThostFtdcInputOrderField* pInputOrder, CThostFtdcRspInfoField* pRspInfo) override
		{
			if (pRspInfo && pRspInfo->ErrorID)
				logi("CTPGateway::{}, {}, OrderRef:{}, ErrorID:{}", __func__, pInputOrder->InstrumentID, pInputOrder->OrderRef, pRspInfo->ErrorID);
		}

		void OnErrRtnOrderAction(CThostFtdcOrderActionField* pOrderAction, CThostFtdcRspInfoField* pRspInfo) override
		{
			if (pRspInfo && pRspInfo->ErrorID)
				logi("CTPGateway::{}, {}, OrderRef:{}, ErrorID:{}, FrontID:{}, SessionID:{}",
					__func__, pOrderAction->InstrumentID, pOrderAction->OrderRef, pRspInfo->ErrorID, pOrderAction->FrontID, pOrderAction->SessionID);

			if (pOrderAction->FrontID == mCancel.FrontID && pOrderAction->SessionID == mCancel.SessionID) {
				CancelReject reject;
				reject.OrderRef = atoi(pOrderAction->OrderRef);
				Push(reject);
			}
		}

		bool LoadCache()
		{
			ifstream fin(mConfig->CachePath, ios::binary);
			int64_t fileSize = fin.seekg(0, ios::end).tellg();
			int64_t headerLen = sizeof(CacheHeader);
			CacheHeader header;
			if (fileSize < headerLen || !fin.seekg(0).read((char*)&header, headerLen) || strcmp(header.Date, mHeader.Date) || strcmp(header.Version, mHeader.Version))
				return false;

			if (int64_t N = sizeof(CThostFtdcInstrumentField); int64_t count = (fileSize - headerLen) / N) {
				mCache.resize(count);
				if (fin.read((char*)mCache.data(), N * count))
					logi("load {} instruments from cache", mCache.size());
				else
					mCache.resize(0);
			}
			return mCache.size();
		}

		void SaveCache(CThostFtdcInstrumentField* pInstrument, bool bIsLast)
		{
			mCache.push_back(*pInstrument);
			if (!bIsLast) return;

			ofstream file(mConfig->CachePath, ios::binary);
			file.write((const char*)&mHeader, sizeof(CacheHeader));
			file.write((const char*)mCache.data(), mCache.size() * sizeof(CThostFtdcInstrumentField));
			if (!file)
				return SetError("cannot write cache file: {}", mConfig->CachePath);
			logi("write {} instruments in cache file: {}", mCache.size(), mConfig->CachePath);
			SetReady();
		}

		const Config* mConfig = nullptr;
		CThostFtdcTraderApi* mApi = nullptr;
		int mReqid = 0;
		CThostFtdcInputOrderField mOrder = {};
		CThostFtdcInputOrderActionField mCancel = {};
		set<int64_t> mKeys;
		bool mCacheOnly = false;
		CacheHeader mHeader;
		vector<CThostFtdcInstrumentField> mCache;
	};

	class CTPEngine
	{
		struct Task
		{
			TimePoint ExecuteTime;
			function<void()> Func;

			bool operator>(const Task& other) const
			{
				return ExecuteTime > other.ExecuteTime;
			}
		};

		static TradeFlag UseFlag(const NewOrder& input)
		{
			auto flag = input.Flag;
			if (input.Flag == TradeFlag::Auto) {
				flag = TradeFlag::Open;
				Position p = GetPosition(input.Instrument);
				int n = abs(input.Size);
				if (Market m = input.Instrument.Exchange(); m == Market::SHFE || m == Market::INE) {
					if (input.Size > 0 && p.ShortToday >= n || input.Size < 0 && p.LongToday >= n)
						flag = TradeFlag::CloseToday;
					else if (input.Size > 0 && (p.Short - p.ShortToday) >= n || input.Size < 0 && (p.Long - p.LongToday) >= n)
						flag = TradeFlag::CloseYesterday;
				}
				else if (input.Size > 0 && p.Short >= n || input.Size < 0 && p.Long >= n)
					flag = TradeFlag::Close;
			}
			return flag;
		}

	public:
		void Init(const Config& cfg, bool CacheOnly = false)
		{
			signal(SIGINT, SignalHandler);

			atexit([]() {
				logi("========================= end ========================");
				fmtlog::stopPollingThread();
				fmtlog::poll(true);
			});

			mConfig = cfg;
			if (!mConfig.LogPath.empty())
				fmtlog::setLogFile(mConfig.LogPath.c_str(), false);
			fmtlog::setHeaderPattern("{HMSf}, {l}[{t:>6}], ");
			fmtlog::startPollingThread(50 * 1000 * 1000);
			logi("========================= begin ========================");
			mOrders = make_unique<Order[]>(mConfig.MaxOrderCount + 1);
			mGateway.Init(&mConfig, CacheOnly);
		}

		void Run(Strategy& s)
		{
			s.OnStart();
			mMarket.Init(mConfig.MarketFront, mCodes);
			for (size_t qi = 0, mi = 0; !gStop;) {
				int n = 0;
				for (size_t count = mMarket.Count(); qi < count; ++qi) {
					s.OnQuote(*mMarket.Get(qi));
					++n;
				}
				for (size_t count = mGateway.Count(); mi < count; ++mi) {
					visit([this, &s](auto&& arg) { Handle(arg, s); }, mGateway.Get(mi));
					++n;
				}
				for (auto now = Now(); !mTasks.empty() && mTasks.top().ExecuteTime <= now;) {
					mTasks.top().Func();
					mTasks.pop();
					++n;
				}
				if (!n && mConfig.SleepOnIdle)
					this_thread::sleep_for(50us);
			}
		}

		void Subscribe(Contract c)
		{
			if (c) mCodes.emplace(c.Code());
		}

		const Order* Insert(const NewOrder& input)
		{
			if (mCount < mConfig.MaxOrderCount) {
				TradeFlag flag = UseFlag(input);
				int ref = mCount + 1;
				int err = mGateway.Insert(input, flag, ref);
				TimePoint sendTime = Now();
				logi("Insert, {}, Ref:{}, Size:{:+}, Price:{}, Type:{}, Flag:{}, Error:{}", input.Instrument, ref, input.Size, input.Price, input.Type, flag, err);

				if (!err) {
					Order& order = mOrders[ref];
					static_cast<NewOrder&>(order) = input;
					order.Flag = flag;
					order.Status = OrderStatus::Sent;
					order.Ref = ref;
					order.SendTime = sendTime;
					++mCount;
					return &order;
				}
			}
			else
				loge("exceed MaxOrderCount {}", mConfig.MaxOrderCount);

			return nullptr;
		}

		bool Cancel(const Order& order)
		{
			if (mGateway.Cancel(order))
				return const_cast<Order&>(order).Canceling = true;
			return false;
		}

		void DelayTask(int ms, function<void()> task)
		{
			mTasks.emplace(Now() + milliseconds(ms), move(task));
		}

	private:
		void Handle(const Report& report, Strategy& s)
		{
			if (Order& order = mOrders[report.OrderRef]; !order.Terminated()) {
				order.Status = report.Status;
				if (order.Status == OrderStatus::Canceled)
					order.Canceling = false;
				order.Error = report.ErrorID;
				order.FilledSize = report.FilledSize;
				logi("OrderUpdate, {}", order);
				s.OnOrder(order);
			}
		}

		void Handle(const Trade& trade, Strategy& s)
		{
			gRegistry[trade.Instrument].UpdatePosition(trade.Size, trade.Flag);
			logi("Trade, {}, {}", trade, GetPosition(trade.Instrument));

			if (trade.OrderRef > 0) {
				Order& order = mOrders[trade.OrderRef];
				order.TradedSize += trade.Size;
				order.TradedValue += trade.Value();
				logi("TradeUpdate, {}", order);
				s.OnTrade(trade, order);
			}
		}

		void Handle(const CancelReject& reject, Strategy& s)
		{
			Order& order = mOrders[reject.OrderRef];
			order.Canceling = false;
			logi("CancelReject, {}", order);
		}

		Config mConfig;
		int mCount = 0;
		unique_ptr<Order[]> mOrders;
		CTPGateway mGateway;
		CTPMarket mMarket;
		priority_queue<Task, vector<Task>, greater<Task>> mTasks;
		set<string> mCodes;
	};

	CTPEngine gEngine;
}

namespace tinytrader {
	const Order* Strategy::Insert(const NewOrder& newOrder)
	{
		return gEngine.Insert(newOrder);
	}

	bool Strategy::Cancel(const Order& order)
	{
		return gEngine.Cancel(order);
	}

	void Strategy::Subscribe(Contract c)
	{
		logi("Subscribe {}", c);
		gEngine.Subscribe(c);
	}

	void Strategy::DelayTask(int ms, function<void()> task)
	{
		gEngine.DelayTask(ms, move(task));
	}

	void InitEngine(const Config& config)
	{
		gEngine.Init(config);
	}

	void Run(Strategy& s)
	{
		gEngine.Run(s);
	}

	void MakeCache(const Config& config)
	{
		gEngine.Init(config, true);
	}
}
