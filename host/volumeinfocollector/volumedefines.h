#ifndef _VOLUME__DEFINES__H_
#define _VOLUME__DEFINES__H_

#include <map>
#include <string>
#include <iterator>
#include <utility>

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

const char HOSTCHAR = 'h';
const char CHANNELCHAR = 'c';
const char TARGETCHAR = 't';
const char LUNCHAR = 'l';
const char HCTLSEP = ';';
const char HCTLLABELVALUESEP = ':';

/* The format is "h:11;c:44;t:ABC;l:990" */
typedef std::map<std::string, std::string> DeviceToHCTL_t;
typedef DeviceToHCTL_t::const_iterator ConstDeviceToHCTLIter_t;
typedef DeviceToHCTL_t::iterator DeviceToHCTLIter_t;
typedef std::pair<std::string, std::string> DeviceHCTLPair_t;

typedef boost::function<void (DeviceHCTLPair_t devicehctlpair)> CollectDeviceHCTLPair_t;
typedef void (*GetDeviceHCTL_t)(CollectDeviceHCTLPair_t collectfunction);

/* partition ID is disk ID underscore partition number */
#define SEPINPARTITIONID "_" 

/* TODO: move to its own file */
class SharedDisksInformer
{
public:
	typedef boost::shared_ptr<SharedDisksInformer> Ptr;

	virtual bool LoadInformation(void) = 0;
	virtual bool IsShared(const std::string &diskid) = 0;
	virtual std::string GetErrorMessage(void) = 0;
    virtual ~SharedDisksInformer() {}
};

#endif /* _VOLUME__DEFINES__H_ */
