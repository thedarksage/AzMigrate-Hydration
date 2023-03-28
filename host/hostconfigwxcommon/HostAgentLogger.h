#pragma once

void CloseHostConfigLog();
void SetHostConfigLogFileName(const char* fileName);
void DebugPrintf( int nLogLevel, const char* format, ... );
