//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : svmarker.h
//
// Description: 
//

#ifndef SVMARKER__H
#define SVMARKER__H

#include "svtypes.h"
#include "boost/shared_ptr.hpp"
#include "boost/shared_array.hpp"

#include <string>
#include <vector>

class DifferentialFile;


class SV_MARKER
{

public:
	
	SV_MARKER(boost::shared_array<char> buffer, SV_ULONG Length);
	~SV_MARKER();

	bool Parse( SV_UINT& dataFormatFlags );

	inline SV_EVENT_TYPE TagType()      const              { return m_tagtype;     }
	inline const std::string & AppName() const              { return m_appname;     } 
	inline const boost::shared_array<char> & Tag() const { return m_tag;       }
	inline SV_ULONG TagLength() const                  { return m_taglength;   }
	inline const char * RawData() const                { return m_buffer.get(); }
	inline const SV_ULONG RawLength()                  { return m_length;       }
    inline bool IsRevocationTag()        { return m_bRevocationTag ; }
	inline const boost::shared_array<char> & GuidBuffer() const { return m_guid;}
    inline SV_ULONGLONG LifeTime() const  { return m_lifetime; }

	
private:

	SV_ULONG   m_length;
	boost::shared_array<char>  m_buffer; 

	SV_EVENT_TYPE m_tagtype;
	std::string   m_appname;
	SV_ULONG  m_taglength;
	boost::shared_array<char>  m_tag;
	boost::shared_array<char>  m_guid;
    SV_ULONGLONG m_lifetime;

    bool m_bRevocationTag;
	bool m_bParsed;
	bool m_rv;

};

typedef boost::shared_ptr<SV_MARKER> SvMarkerPtr;
typedef std::vector< SvMarkerPtr > UserTags;
typedef UserTags::iterator UserTagsIterator;
typedef UserTags::const_iterator UserTagsConstIterator;

#endif

