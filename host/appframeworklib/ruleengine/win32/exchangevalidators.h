#ifndef __EXCHNAGE__VALIDATORS__H
#define __EXCHNAGE__VALIDATORS__H

#include "ruleengine/validator.h"
#include "exchange/exchangediscovery.h"
#include "exchange/exchangemetadata.h"
#include "config/applicationsettings.h"

class ExchangeHostNameValidator: public Validator
{
	const ExchAppVersionDiscInfo& m_discInfo; 
    ExchangeMetaData metadata ;
	Exchange2k10MetaData metadata2;
    std::string m_hostname ;
    ExchangeHostNameValidator(const std::string& name, 
							  const std::string& description, 
							  const ExchAppVersionDiscInfo& discInfo, 
							  const ExchangeMetaData& data,
                              const std::string& hostname,
                              InmRuleIds ruleId) ;

	ExchangeHostNameValidator(const std::string& name,
							  const std::string& description, 
							  const ExchAppVersionDiscInfo& discInfo, 
							  const Exchange2k10MetaData& data,
                              const std::string& hostname,
                              InmRuleIds ruleId) ;

public:

    ExchangeHostNameValidator(const ExchAppVersionDiscInfo& discInfo, 
							  const ExchangeMetaData& data,
                              const std::string& hostname,
                              InmRuleIds ruleId) ;
	ExchangeHostNameValidator(const ExchAppVersionDiscInfo& discInfo, 
							  const Exchange2k10MetaData& data,
                              const std::string& hostname,
                              InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;

} ;

class ExchangeNestedMountPointValidator: public Validator
{
    const ExchAppVersionDiscInfo& m_info ;
    ExchangeMetaData m_data ;
	Exchange2k10MetaData m_data2 ;
    ExchangeNestedMountPointValidator(const std::string& name, 
                                    const std::string& description, 
                                    const ExchAppVersionDiscInfo & discInfo, 
                                    const ExchangeMetaData& data,
                                    InmRuleIds ruleId) ;
	ExchangeNestedMountPointValidator(const std::string& name, 
                                    const std::string& description, 
                                    const ExchAppVersionDiscInfo & discInfo, 
									const Exchange2k10MetaData& data,
                                    InmRuleIds ruleId) ;
public:

     ExchangeNestedMountPointValidator(const ExchAppVersionDiscInfo & discInfo, 
                                    const ExchangeMetaData& data,
                                    InmRuleIds ruleId) ;
	ExchangeNestedMountPointValidator(const ExchAppVersionDiscInfo & discInfo, 
									const Exchange2k10MetaData& data,
                                    InmRuleIds ruleId) ;

    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
	bool isMointPointExist(const std::string & mountPoint);
} ;

class ExchangeDataOnSysDrive:public Validator
{
    const ExchAppVersionDiscInfo& m_info ;
    ExchangeMetaData m_data ;
	Exchange2k10MetaData m_data2 ;
    std::string m_sysDrive ;
    ExchangeDataOnSysDrive(const std::string& name, 
                            const std::string& description, 
                            const ExchAppVersionDiscInfo & discInfo, 
                            const ExchangeMetaData& data,
                            const std::string& sysDrive,
                            InmRuleIds ruleId) ;
    ExchangeDataOnSysDrive(const std::string& name, 
                            const std::string& description, 
                            const ExchAppVersionDiscInfo & discInfo, 
                            const Exchange2k10MetaData& data,
                            const std::string& sysDrive,
                            InmRuleIds ruleId) ;
public:

    ExchangeDataOnSysDrive(const ExchAppVersionDiscInfo & discInfo, 
                            const ExchangeMetaData& data,
                            const std::string& sysDrive,
                            InmRuleIds ruleId) ;
    ExchangeDataOnSysDrive(const ExchAppVersionDiscInfo & discInfo, 
                            const Exchange2k10MetaData& data,
                            const std::string& sysDrive,
                            InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;
class ExchangeDCEqualityValidator : public Validator
{
    std::string m_srcDn ;
    std::string m_tgtDn ;
    ExchangeDCEqualityValidator(const std::string& name, 
        const std::string& description, 
        const std::string & srcDn,
        const std::string& tgtDn,
        InmRuleIds ruleId) ;
public:

     ExchangeDCEqualityValidator(const std::string & srcDn,
        const std::string& tgtDn,
        InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;
/*
class ExchangeMTAEqualityValidator : public Validator
{
    bool m_isSrcMTA ;
    bool m_isTgtMTA ;
    ExchangeMTAEqualityValidator(const std::string& name, 
        const std::string& description, 
        bool & isSrcMTA,
        bool & isTgtMTA,
        InmRuleIds ruleId) ;
public:

     ExchangeMTAEqualityValidator(bool & isSrcMTA,
        bool & isTgtMTA,
        InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;
*/
class ExchangeAdminGroupEqualityValidator : public Validator
{
    std::string m_srcDn ;
    std::string m_tgtDn ;
    ExchangeAdminGroupEqualityValidator(const std::string& name, 
        const std::string& description, 
        const std::string & srcDn,
        const std::string& tgtDn,
        InmRuleIds ruleId) ;
public:
    ExchangeAdminGroupEqualityValidator(  const std::string & srcDn,
        const std::string& tgtDn,
        InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;

class SingleMailStoreCopyRule : public Validator
{
	const ExchAppVersionDiscInfo& m_info ;
    ExchangeMetaData m_data ;
	Exchange2k10MetaData m_data2 ;
	SingleMailStoreCopyRule(const std::string& name, 
            const std::string& description, 
            const ExchAppVersionDiscInfo & discInfo, 
			const Exchange2k10MetaData& data,
            InmRuleIds ruleId) ;

public:
	SingleMailStoreCopyRule(const ExchAppVersionDiscInfo & discInfo, 
			const Exchange2k10MetaData& data,
            InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;


};

class LcrStatusCheck:public Validator
{ 
    const ExchangeMetaData& m_exchMetaData ;
    LcrStatusCheck(const std::string& name, 
                    const std::string& description, 
                    const ExchangeMetaData& exchMetaData,
                    InmRuleIds ruleId) ;

public:
    LcrStatusCheck(const ExchangeMetaData& exchMetaData,
                    InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;
} ;

class CcrStatusCheck:public Validator
{
    const ExchAppVersionDiscInfo m_discInfo;
    CcrStatusCheck(const std::string& name, 
                    const std::string& description, 
                    const ExchAppVersionDiscInfo discoveryInfo,
                    InmRuleIds ruleId) ;
public:
    CcrStatusCheck(const ExchAppVersionDiscInfo discoveryInfo,
                    InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;

} ;

class SCRStatusCheck : public Validator
{
     const ExchangeMetaData& m_exchMetaData ;

    SCRStatusCheck(const std::string& name, 
                    const std::string& description, 
                    const ExchangeMetaData& exchMetaData,
                    InmRuleIds ruleId) ;
public:
    SCRStatusCheck(const ExchangeMetaData& exchMetaData,
                    InmRuleIds ruleId) ;
    SVERROR evaluate() ;
    SVERROR fix() ;
    bool canfix() ;

};
#endif
