#include "simnow.h"

using namespace tinytrader;

int main()
{
	Config config = SimnowConfig();
	MakeCache(config);
}