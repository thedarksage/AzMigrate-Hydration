#ifndef __TYPED_VALUE_H_
#define __TYPED_VALUE_H_
#include <string>
#include <vector>
#include <stdlib.h>

using namespace std;
enum VALUETYPE {TYPENULL,TYPEINT,TYPEFLOAT,TYPEDOUBLE,TYPECHAR,TYPESTRING};

class TypedValue
{
	union{
		int ival;
		char cval;
		float fval;
		double dval;
	}u;

	string m_szVal;

	VALUETYPE m_type;

public:

	TypedValue(){
		m_type	= TYPENULL;
		u.dval	= 0;
		m_szVal	= "";
	}
	
	TypedValue(const TypedValue& tval)
	{
		//u.dval = 0;

		switch(tval.getType())
		{
		case TYPENULL:
			u.dval = 0;
			m_type = TYPENULL;
			break;

		case TYPEINT: u.ival = tval.getInt();
			m_type = TYPEINT;
			break;

		case TYPEFLOAT: u.fval = tval.getFloat();
			m_type = TYPEFLOAT;
			break;

		case TYPEDOUBLE: u.dval = tval.getDouble();
			m_type = TYPEDOUBLE;
			break;

		case TYPECHAR: u.cval = tval.getChar();
			m_type = TYPECHAR;
			break;

		case TYPESTRING: m_type = TYPESTRING;
			break;
		}

		m_szVal = tval.getString();
	}

	
	TypedValue(int val)
	{
		u.dval	= 0;
		m_type	= TYPEINT; 
		u.ival	= val;
		m_szVal	= "";
	}

	TypedValue(double val)
	{
		m_type	= TYPEDOUBLE; 
		u.fval	= (float) val;
		m_szVal	= "";
	}

	TypedValue(char val)
	{
		m_type	= TYPECHAR; 
		u.cval	= val;
		m_szVal	= "";
	}

	TypedValue(string val)
	{
		m_type = TYPESTRING; 
		m_szVal = val;
		u.dval	= 0;
	}

	VALUETYPE getType()const {
		return m_type;
	}

	int getInt()const {
		if(m_szVal.empty())
			return u.ival;
		else
			return atoi(m_szVal.c_str());
	}

	float getFloat()const {
		return u.fval;
	}

	double getDouble()const {
		return u.dval;
	}

	char getChar()const {
		return u.cval;
	}

	string getString()const {
		return m_szVal;
	}
	~TypedValue()
	{
		
	}	
};

#endif
