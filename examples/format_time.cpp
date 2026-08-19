#include "tinytrader.h"

using namespace std;
using namespace chrono;
using namespace tinytrader;

int main()
{
	TimePoint tp = Now();
	// 默认输出 yyyy-mm-dd HH:MM:SS.xxxxxxxxx 小数点后位数与平台有关
	fmt::print("{}\n", tp);
	fmt::print("{:%Y-%m-%d %T}\n", tp);				// 同上
	fmt::print("{:%H:%M:%S}\n", tp);				// 时间精度平台相关
	fmt::print("{:%T}\n", tp);						// 同上

	fmt::print("{:%Y%m%d}\n", tp);					// yyyymmdd

	// 提前转换精度
	fmt::print("{}\n", floor<nanoseconds>(tp));		// yyyy-mm-dd HH:MM:SS.xxxxxxxxx
	fmt::print("{:%T}\n", floor<seconds>(tp));		// HH:MM:SS
	fmt::print("{:%T}\n", floor<milliseconds>(tp));	// HH:MM:SS.xxx
	fmt::print("{:%T}\n", floor<microseconds>(tp));	// HH:MM:SS.xxxxxx
	fmt::print("{:%T}\n", floor<nanoseconds>(tp));	// HH:MM:SS.xxxxxxxxx

	string s = fmt::format("{:%Y%m%d %T}", floor<microseconds>(tp));
	fmt::print("{}\n", s);							// yyyymmdd HH:MM:SS.xxxxxx
}