#ifndef INMAGE_AGENTPROCESS_H_
#define INMAGE_AGENTPROCESS_H_

#include <string>
#include "ace/OS_main.h"
#include "ace/OS_NS_stdio.h"
#include "ace/OS_NS_strings.h"
#include "ace/OS_NS_string.h"
#include "ace/OS_NS_wchar.h"
#include "ace/Log_Msg.h"
#include "ace/OS_NS_unistd.h"
#include "ace/Process_Manager.h"
#include "ace/Manual_Event.h"
#include <boost/shared_ptr.hpp>


class AgentId {
public:
    AgentId(std::string deviceName, int group)
            : m_deviceId(deviceName), m_Group(group) { }

	AgentId(char *deviceName, int group)
            : m_deviceId(deviceName), m_Group(group) { }

	/**
	* 1 TO N sync: Added 2 constructors that initialize the end point device names
	*/
    AgentId(std::string deviceName, std::string endpointDeviceName, int group)
            : m_deviceId(deviceName), m_endpointDeviceId(endpointDeviceName), m_Group(group) { }

	AgentId(char *deviceName, char *endpointDeviceName, int group)
            : m_deviceId(deviceName), m_endpointDeviceId(endpointDeviceName), m_Group(group) { }

    bool LessAgentId(AgentId  & lhs, AgentId  & rhs) const {
        return lhs.Less(rhs);
    }

    bool LessAgentId(AgentId  & rhs) const {
        return Less(rhs);
    }

    std::string Id() { return m_deviceId; }
	/**
	* 1 TO N sync: Added end point device name
	*/
    std::string EndPointId() { return m_endpointDeviceId; }
	int GroupId() { return (m_Group); }
    bool IsGroup() { return (0 != m_Group); }	

protected:
    bool Less(AgentId  & rhs) const;

private:
   	std::string m_deviceId;
	/**
	* 1 TO N sync: Added end point device name
	*/
    std::string m_endpointDeviceId;
    int m_Group;   
};

class AgentProcess {
public:
    typedef boost::shared_ptr<AgentProcess> Ptr;   

    class LessAgentProcess {
    public:              
        bool operator()(Ptr const & lhs, Ptr const & rhs) const {
            return lhs->m_Id.LessAgentId(rhs->m_Id);
        }
    };

    AgentProcess(std::string deviceName, int group) 
        : m_Id(deviceName, group), m_ProcessId(ACE_INVALID_PID),m_quitEvent(NULL),isAlive(false)
    {  
    }

    AgentProcess(char* deviceName, int group) 
        : m_Id(deviceName, group), m_ProcessId(ACE_INVALID_PID),m_quitEvent(NULL),isAlive(false)
    {  
    }

	/**
	* 1 TO N sync: Added 2 constructors that initialize the end point device names
	*/
    AgentProcess(std::string deviceName, std::string endpointDeviceName, int group) 
        : m_Id(deviceName, endpointDeviceName, group), m_ProcessId(ACE_INVALID_PID),m_quitEvent(NULL),isAlive(false)
    {  
    }

    AgentProcess(char* deviceName, char *endpointDeviceName, int group) 
        : m_Id(deviceName, endpointDeviceName, group), m_ProcessId(ACE_INVALID_PID),m_quitEvent(NULL),isAlive(false)
    {  
    }

    ~AgentProcess();

    pid_t ProcessHandle() { return m_ProcessId; }

    bool ProcessCreated() { return ACE_INVALID_PID != m_ProcessId; }

    bool Start(ACE_Process_Manager *pm, const char * exePath, const char * args);
    
    bool Stop(ACE_Process_Manager *pm);

    int PostQuitMessage();

	std::string DeviceName() { return Id(); }

    std::string Id() { return m_Id.Id(); }

	int GroupId() { return m_Id.GroupId(); }

	std::string EndpointDeviceName() { return m_Id.EndPointId(); }

    bool IsGroup() { return m_Id.IsGroup(); }
	bool IsAlive() { return isAlive; }
	void markDead() { isAlive = false; }

private:
    AgentId m_Id;
    pid_t m_ProcessId;  
	ACE_Manual_Event* m_quitEvent;
	bool isAlive;
private:
    void CreateQuitEvent();
    void DestroyQuitEvent();	
};


class DoesAgentProcessHaveDeviceName 
{
	std::string m_DeviceName;
public:
	DoesAgentProcessHaveDeviceName(const std::string &devicename) 
		: m_DeviceName(devicename) 
	{
	}

	bool operator()(const AgentProcess::Ptr &ap)
	{
		return (ap->DeviceName() == m_DeviceName);
	}
};

#endif /* INMAGE_AGENTPROCESS_H */

