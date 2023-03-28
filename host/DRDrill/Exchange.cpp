#include "stdafx.h"

#include "Exchange.h"

Exchange::Exchange(void)
{
}
Exchange::~Exchange(void)
{
}
Exchange::Exchange(const _tstring &srcHost,
					const _tstring &tgtHost,
					const _tstring &evsName,
					const _tstring &appName,
					bool &bdrdrill)
:ExchangeCollector(srcHost,tgtHost,
				evsName,appName,bdrdrill)
{
}
bool Exchange::CollectExchData()
{
	bool result = true;
	return result;
}
