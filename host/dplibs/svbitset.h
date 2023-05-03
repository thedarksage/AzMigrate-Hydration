// bitset.cpp : Defines the entry point for the console application.
//
#ifndef BITMAP__H
#define BITMAP__H
#include <iostream>
#include<string>
#include <boost/dynamic_bitset.hpp>

using namespace std;

class svbitset
{
	bool m_valid;
	boost::dynamic_bitset<> m_bitset;	
public:
	bool resize(boost::dynamic_bitset<>::size_type size, bool value);
	bool initialize(boost::dynamic_bitset<>::size_type size, bool value);
	bool initialize(const std::string &bitsetStr);
	svbitset();
	void setByPos(size_t pos);
	void clearbyPos(size_t pos);
	bool isSetByPos(size_t pos);
	bool isClearByPos(size_t pos);
	void setByMask(unsigned long mask);
	void setExceptByMask(unsigned long mask);
	void clearByMask(unsigned long mask);
	void clearExceptByMask(unsigned long mask);
	void clearAll();
	void setAll();
	size_t findFirstSetBit();
	size_t findFirstUnsetBit();
	size_t getNextSetBit(size_t pivot);
	size_t getNextUnsetBit(size_t pivot);
	bool isAnyBitSet();
	bool isAnyBitUnset();
	bool isAllSet();
	bool isAllClear();
	unsigned long size();
	void dump_long();
	void dump_bits();
	unsigned long to_ulong();
	string to_string();
};


#endif
