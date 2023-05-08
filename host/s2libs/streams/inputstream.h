//#pragma once


#ifndef INPUT_STREAM__H
#define INPUT_STREAM__H

struct InputStream
{
	virtual int Read(void*,const unsigned long int) = 0;
};
#endif

