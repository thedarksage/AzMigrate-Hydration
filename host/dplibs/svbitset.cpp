#include "svbitset.h"


bool svbitset::resize(boost::dynamic_bitset<>::size_type size, bool value)
{
	try
	{
		m_bitset.resize(size, value);
	}
	catch(std::bad_alloc bad)
	{
		m_valid = false;
		/* cout<<"Failed to allocate bitmap for size "<<size<<endl; */
	}

    return m_valid;
}

bool svbitset::initialize(boost::dynamic_bitset<>::size_type size, bool value=false)
{
	return resize(size, value);
}

bool svbitset::initialize(const std::string &bitsetStr)
{
    try
    {
        /* TODO: why assignment and not changing associated bitmap ? */
	    boost::dynamic_bitset<> bitset(bitsetStr);
    	m_bitset = bitset;
    }
	catch(std::bad_alloc bad)
	{
		m_valid = false;
		/* cout<<"Failed to allocate bitmap for size "<<size<<endl; */
	}

    return m_valid;
}

void svbitset::setByPos(size_t pos)
{
	m_bitset[pos] = 1;
}
void svbitset::clearbyPos(size_t pos)
{
	m_bitset[pos] = 0;
}
bool svbitset::isSetByPos(size_t pos)
{
	return (bool)m_bitset[pos];
}
bool svbitset::isClearByPos(size_t pos)
{
	return (bool)m_bitset[pos];
}
void svbitset::setByMask(unsigned long mask)
{
	boost::dynamic_bitset<> bitset_mask(m_bitset.size(), mask);
	m_bitset |= bitset_mask;
}
void svbitset::setExceptByMask(unsigned long mask)
{
	boost::dynamic_bitset<> bitset_mask(m_bitset.size(), mask);
	m_bitset |= ~bitset_mask;
}
void svbitset::clearByMask(unsigned long mask)
{
	boost::dynamic_bitset<> bitset_mask(m_bitset.size(), mask);
	m_bitset &= ~bitset_mask;
}
void svbitset::clearExceptByMask(unsigned long mask)
{
	boost::dynamic_bitset<> bitset_mask(m_bitset.size(), mask);
	m_bitset &= bitset_mask;
}

void svbitset::clearAll()
{
	boost::dynamic_bitset<> bitset_mask(m_bitset.size(), 0);
	m_bitset &= bitset_mask;
}
void svbitset::setAll()
{
	boost::dynamic_bitset<> bitset_mask(m_bitset.size(), ~0);
	m_bitset |= bitset_mask;
}

svbitset::svbitset()
{	
    m_valid = true;
}

unsigned long svbitset::size()
{
	return (unsigned long) m_bitset.size();
}
void svbitset::dump_long()
{
	cout<<m_bitset.to_ulong()<<endl;
}
void svbitset::dump_bits()
{
	cout<<m_bitset<<endl;
}
string svbitset::to_string()
{
	string str;
	boost::to_string(m_bitset, str);
	return str;
}

unsigned long svbitset::to_ulong()
{
	return m_bitset.to_ulong();
}
size_t svbitset::findFirstSetBit()
{
	size_t index = m_bitset.find_first();
	if ( index == boost::dynamic_bitset<>::npos )
		return boost::dynamic_bitset<>::npos;
	else
		return index;

}
size_t svbitset::findFirstUnsetBit()
{
	m_bitset.flip();
	size_t index = m_bitset.find_first();
	m_bitset.flip();
	if(  index == boost::dynamic_bitset<>::npos )
		return boost::dynamic_bitset<>::npos;
	else
		return index;
}
size_t svbitset::getNextSetBit(size_t pivot)
{
	size_t index = m_bitset.find_next(pivot);
	if( index == boost::dynamic_bitset<>::npos )
		return boost::dynamic_bitset<>::npos;
	else
		return index;
}
size_t svbitset::getNextUnsetBit(size_t pivot)
{
	m_bitset.flip();
	size_t index = m_bitset.find_next(pivot);
	m_bitset.flip();
	if(  index == boost::dynamic_bitset<>::npos )
		return boost::dynamic_bitset<>::npos;
	else
		return index;
}

bool svbitset::isAnyBitSet()
{
	if( this->findFirstSetBit() != boost::dynamic_bitset<>::npos )
		return true;
	else
		return false;
}
bool svbitset::isAnyBitUnset()
{
	if( this->findFirstUnsetBit() != boost::dynamic_bitset<>::npos )
		return true;
	else
		return false;
}
bool svbitset::isAllSet()
{
	if( this->findFirstUnsetBit() != boost::dynamic_bitset<>::npos )
		return true;
	else
		return false;
}
bool svbitset::isAllClear()
{
	if( this->findFirstSetBit() != boost::dynamic_bitset<>::npos )
		return true;
	else
		return false;

}


