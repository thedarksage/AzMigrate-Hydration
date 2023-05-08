
#pragma once

#include "stdafx.h"

#define NUM_OF_ELEMS(n) (sizeof (n) / sizeof (*(n)))


void ShowMsg(LPCSTR msg,...);
void LogVssEvent(WORD EventLogType,DWORD dwEventVwrLogFlag,LPCSTR pFormat,...);
