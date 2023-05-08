#include <string>
#include <sstream>
#include <map>
#include "error.h"
#include "operatingsystem.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "portablehelpersminor.h"
#include "executecommand.h"

int OperatingSystem::SetOsSpecificInfo(std::string &errmsg)
{
    m_OsVal = SV_LINUX_OS;
    int iStatus = SetOsReadableName(errmsg);
    if (SV_SUCCESS == iStatus)
    {
        iStatus = SetOsVersionBuild(errmsg);
    }
    
    if (SV_SUCCESS == iStatus)
    {
        const std::string cmd("who -b");
        std::stringstream results;
        if (!executePipe(cmd, results))
        {
            errmsg = "Failed to get system boot time. Error: ";
            errmsg += results.str();
            iStatus = SV_FAILURE;
        }
        else
        {
            m_OsInfo.m_Attributes.insert(std::make_pair(NSOsInfo::LASTBOOTUPTIME, results.str()));
        }
    }
    
    return iStatus;
}


int OperatingSystem::SetOsReadableName(std::string &errmsg)
{
    int iStatus = SV_SUCCESS;
    
    std::string name = "Linux";
    std::string linuxetcReleaseValue;
    LINUX_TYPE linuxFlavourType;
    bool bis32BitMachine;
    std::string strRHEL4BaseVerRegExp1 = "Red Hat Enterprise Linux ES release 4 (Nahant Update [4-9])";
    std::string strCentOS4BaseVerRegExp1 = "CentOS release 4.[4-9] (Final)";

    //Added Support for Red Hat Enterprise Linux [AE]S release 4 
    std::string strRHEL4AdvVerRegExp1 = "Red Hat Enterprise Linux AS release 4 (Nahant Update [4-9])";
    
    
    //Read the following two strings from /etc/enterprise-release
    std::string strOEL5BaseVerRegExp1 = "Enterprise Linux Enterprise Linux Server release 5 (Carthage)";
    std::string strOEL5BaseVerRegExp2 = "Enterprise Linux Enterprise Linux Server release 5.[1-9] (Carthage)";
    std::string strOEL5BaseVerRegExp3 = "Enterprise Linux Enterprise Linux Server release 5.[1-9][0-9] (Carthage)"; 
    
    std::string strRHEL5BaseVerRegExp1 = "Red Hat Enterprise Linux Server release 5 (Tikanga)";
    std::string strRHEL5BaseVerRegExp2 = "Red Hat Enterprise Linux Server release 5.[1-9] (Tikanga)";
    std::string strRHEL5BaseVerRegExp3 = "Red Hat Enterprise Linux Server release 5.[1-9][0-9] (Tikanga)";
    std::string strCentOS5BaseVerRegExp1 = "CentOS release 5 (Final)";
    std::string strCentOS5BaseVerRegExp2 = "CentOS release 5.[1-9] (Final)";
    std::string strCentOS5BaseVerRegExp3 = "'CentOS release 5* (Final)";
    std::string strCentOS5BaseVerRegExp4 = "CentOS release 5.[1-9][0-9] (Final)";

    std::string strOEL6BaseVerRegExp1 = "Oracle Linux Server release 6.*";///Read this string from etc/oracle-release
    std::string strOEL7BaseVerRegExp1 = "Oracle Linux Server release 7.[0-9]";
    std::string strOEL8BaseVerRegExp1 = "Oracle Linux Server release 8.*";
    std::string strOEL9BaseVerRegExp1 = "Oracle Linux Server release 9.*";
    
    std::string strRHEL6BaseVerRegExp1 = "Red Hat Enterprise Linux Server release 6.* (Santiago)";
    std::string strRHEL6BaseVerRegExp2 = "Red Hat Enterprise Linux Server release 6.*";
    std::string strRHEL6BaseVerRegExp3 = "Red Hat Enterprise Linux Workstation release 6.*";
    std::string strCentOS6BaseVerRegExp = "CentOS release 6.*";
    std::string strLinuxCentOS6BaseVerRegExp = "CentOS Linux release 6.*";

    std::string strRHEL7BaseVerRegExp = "Red Hat Enterprise Linux Server release 7.* (Maipo)";
    std::string strRHEL7BaseVerRegExp2 = "Red Hat Enterprise Linux Workstation release 7.* (Maipo)";
    std::string strCentOS7BaseVerRegExp = "CentOS Linux release 7.*";

    std::string strRHEL8BaseVerRegExp = "Red Hat Enterprise Linux release 8.*";
    std::string strCentOS8BaseVerRegExp = "CentOS Linux release 8.*";
    
    std::string strRHEL9BaseVerRegExp = "Red Hat Enterprise Linux release 9.*";
    std::string strCentOS9BaseVerRegExp = "CentOS Linux release 9.*";

    if( getLinuxReleaseValue( linuxetcReleaseValue, linuxFlavourType ) == SVS_FALSE )
    {
        errmsg = "Failed to open redhat, SuSE, debain version files. Un Supported Linux Flavor";
        iStatus = SV_FAILURE;
    }
    else
    {
        removeStringSpaces( linuxetcReleaseValue );
        struct utsname st_uts;
        if ( uname( &st_uts ) == -1 )
        {
            errmsg = "Failed to stat out uname command";
            iStatus = SV_FAILURE;
            return iStatus;
        }
        bis32BitMachine = isLocalMachine32Bit( st_uts.machine );
        if( linuxFlavourType == OEL5_RELEASE )
        {
            if ((RegExpMatch(strOEL5BaseVerRegExp1.c_str(),linuxetcReleaseValue.c_str())) ||									 
                (RegExpMatch(strOEL5BaseVerRegExp2.c_str(),linuxetcReleaseValue.c_str())) ||
                (RegExpMatch(strOEL5BaseVerRegExp3.c_str(),linuxetcReleaseValue.c_str()))
                )
                {
                    if(bis32BitMachine)
                    {
                        name="OL5-32";
                    }
                    else
                    {
                        name="OL5-64";
                    }
                }
        }
        else if( linuxFlavourType == OEL_RELEASE)
        {
            if (RegExpMatch(strOEL6BaseVerRegExp1.c_str(),linuxetcReleaseValue.c_str()))
                {
                    if(bis32BitMachine)
                    {
                        name="OL6-32";
                    }
                    else
                    {
                        name="OL6-64";
                    }
                }
            if (RegExpMatch(strOEL7BaseVerRegExp1.c_str(),linuxetcReleaseValue.c_str()))
                {
                    if(bis32BitMachine)
                    {
                        name="OL7-32";
                    }
                    else
                    {
                        name="OL7-64";
                    }
                }
            if (RegExpMatch(strOEL8BaseVerRegExp1.c_str(),linuxetcReleaseValue.c_str()))
                {
                    if(bis32BitMachine)
                    {
                        name="OL8-32";
                    }
                    else
                    {
                        name="OL8-64";
                    }
                }
            if (RegExpMatch(strOEL9BaseVerRegExp1.c_str(),linuxetcReleaseValue.c_str()))
                {
                    name="OL9-64";
                }
        }
        else if( linuxFlavourType == REDHAT_RELEASE )
        {
            if( linuxetcReleaseValue.find("release 3") != std::string::npos )
            {
                name="RHEL3-32";
            }
            else if( linuxetcReleaseValue.find("release 4 (Nahant Update 3)") != std::string::npos || 
                    linuxetcReleaseValue.find("CentOS release 4.3 (Final)") != std::string::npos )
            {
                if( bis32BitMachine )
                    name="RHEL4U3-32";
                else
                    name="RHEL4U3-64";	
            }
            //else if( linuxetcReleaseValue.find("release 4 (Nahant Update 4)") != std::string::npos || 
            //		linuxetcReleaseValue.find("CentOS release 4.4 (Final)") != std::string::npos )
            //{
            //	if( bis32BitMachine )
            //		name="RHEL4U4-32";
            //	else
            //		name="RHEL4U4-64";
            //}
            else if( linuxetcReleaseValue.find("Enterprise Linux Enterprise Linux AS release 4 (October Update 4)") != std::string::npos ) 
            {
                if( bis32BitMachine )
                    name="ELORCL4U4-32";
            }
            //else if( linuxetcReleaseValue.find("release 4 (Nahant Update 5)") != std::string::npos || 
            //		linuxetcReleaseValue.find("CentOS release 4.5 (Final)") != std::string::npos )
            //{
            //	if( bis32BitMachine )
            //		name="RHEL4U5-32";
            //	else
            //		name="RHEL4U5-64";
            //}
            
            else if ((RegExpMatch(strRHEL4BaseVerRegExp1.c_str(),linuxetcReleaseValue.c_str())) ||
                    (RegExpMatch(strRHEL4AdvVerRegExp1.c_str(),linuxetcReleaseValue.c_str())) ||
                    (RegExpMatch(strCentOS4BaseVerRegExp1.c_str(),linuxetcReleaseValue.c_str()))
                    )
            {
                if(bis32BitMachine)
                {
                    name="RHEL4-32";
                }
                else
                {
                    name="RHEL4-64";
                }
            }
            else if (RegExpMatch(strRHEL8BaseVerRegExp.c_str(),linuxetcReleaseValue.c_str()) ||
                     (RegExpMatch(strCentOS8BaseVerRegExp.c_str(),linuxetcReleaseValue.c_str())))
            {
                if(bis32BitMachine)
                {
                    name="RHEL8-32";
                }
                else
                {
                    name="RHEL8-64";
                }
            }
	    else if (RegExpMatch(strRHEL9BaseVerRegExp.c_str(),linuxetcReleaseValue.c_str()) ||
                     (RegExpMatch(strCentOS9BaseVerRegExp.c_str(),linuxetcReleaseValue.c_str())))
            {
                name="RHEL9-64";
            }
            else if ((RegExpMatch(strRHEL7BaseVerRegExp.c_str(),linuxetcReleaseValue.c_str())) ||
                     (RegExpMatch(strRHEL7BaseVerRegExp2.c_str(),linuxetcReleaseValue.c_str())) ||
                     (RegExpMatch(strCentOS7BaseVerRegExp.c_str(),linuxetcReleaseValue.c_str())))
            {
                if(bis32BitMachine)
                {
                    name="RHEL7-32";
                }
                else
                {
                    name="RHEL7-64";
                }
            }
            else if ((RegExpMatch(strRHEL6BaseVerRegExp1.c_str(),linuxetcReleaseValue.c_str())) ||
                     (RegExpMatch(strRHEL6BaseVerRegExp2.c_str(),linuxetcReleaseValue.c_str())) ||
                     (RegExpMatch(strRHEL6BaseVerRegExp3.c_str(),linuxetcReleaseValue.c_str())) ||
                     (RegExpMatch(strCentOS6BaseVerRegExp.c_str(),linuxetcReleaseValue.c_str())) ||
                     (RegExpMatch(strLinuxCentOS6BaseVerRegExp.c_str(),linuxetcReleaseValue.c_str()))
                     )
            {
                if(bis32BitMachine)
                {
                    name="RHEL6-32";
                }
                else
                {
                    name="RHEL6-64";
                }
            }
            else if ((RegExpMatch(strRHEL5BaseVerRegExp1.c_str(),linuxetcReleaseValue.c_str())) ||
                                         (RegExpMatch(strRHEL5BaseVerRegExp2.c_str(),linuxetcReleaseValue.c_str())) ||
                                         (RegExpMatch(strCentOS5BaseVerRegExp1.c_str(),linuxetcReleaseValue.c_str())) ||
                                         (RegExpMatch(strCentOS5BaseVerRegExp2.c_str(),linuxetcReleaseValue.c_str())) ||
                                         (RegExpMatch(strCentOS5BaseVerRegExp3.c_str(),linuxetcReleaseValue.c_str()))
                                         )
                        {
                                if(bis32BitMachine)
                                {
                                        name="RHEL5-32";
                                }
                                else
                                {
                                        name="RHEL5-64";
                                }
                        }
                else if ((RegExpMatch(strRHEL5BaseVerRegExp3.c_str(),linuxetcReleaseValue.c_str())) ||
                                         (RegExpMatch(strCentOS5BaseVerRegExp4.c_str(),linuxetcReleaseValue.c_str())) 
                                         )
                        {
                                if(bis32BitMachine)
                                {
                                        name="RHEL5-32";
                                }
                                else
                                {
                                        name="RHEL5-64";
                                }
                        }

            else if( linuxetcReleaseValue.find("CentOS release 5 (Final)") != std::string::npos ) 
            {
                std::string releaseValue = st_uts.release; 
                removeStringSpaces( releaseValue );
                ////if( releaseValue.find("2.6.18-8") != std::string::npos )
                if( releaseValue.find("2.6.18") != std::string::npos )
                {
                    if( bis32BitMachine )
                        name="RHEL5-32";
                    else
                        name="RHEL5-64";
                }
                //else if( releaseValue.find("2.6.18-53") != std::string::npos )
                else if( releaseValue.find("2.6.18") != std::string::npos )
                {
                    if( bis32BitMachine )
                        name="RHEL5-32";
                    else
                        name="RHEL5-64";
                }
                else
                {
                    errmsg = " Un Supported Cent Os 5 's vesion";
                    iStatus = SV_FAILURE;
                }	
            }
            else if( linuxetcReleaseValue.find("XenServer release 4.1.0-7843p (xenenterprise)") != std::string::npos ) 
            {
                if( bis32BitMachine )
                    name="CITRIX-XEN-4.1.0-7843p-32";
                else
                    name="CITRIX-XEN-4.1.0-7843p-64";	
            }
            else if( linuxetcReleaseValue.find("XenServer release 5.0.0-10918p (xenenterprise)") != std::string::npos ) 
            {
                if( bis32BitMachine )
                    name="CITRIX-XEN-5.0.0-10918p-32";
                else
                    name="CITRIX-XEN-5.0.0-10918p-64";
            }
            else if( linuxetcReleaseValue.find("XenServer release 4.0.1-4249p (xenenterprise)") != std::string::npos ) 
            {
                if( bis32BitMachine )
                    name="CITRIX-XEN-4.0.1-4249p-32";
                else
                    name="CITRIX-XEN-4.0.1-4249p-64";
            }
            else if( linuxetcReleaseValue.find("5.5.0-15119p (xenenterprise)") != std::string::npos ) 
            {
                if( bis32BitMachine )
                    name="CITRIX-XEN-5.5.0-15119p-32";
                else
                    name="CITRIX-XEN-5.5.0-15119p-64";
            }
            else if( linuxetcReleaseValue.find("5.5.0-25727p (xenenterprise)") != std::string::npos ) 
            {
                if( bis32BitMachine )
                    name="CITRIX-XEN-5.5.0-25727p-32";
                else
                    name="CITRIX-XEN-5.5.0-25727p-64";
            }
            else if( linuxetcReleaseValue.find("5.6.0-31188p (xenenterprise)") != std::string::npos ) 
            {
                if( bis32BitMachine )
                    name="CITRIX-XEN-5.6.0-31188p-32";
                else
                    name="CITRIX-XEN-5.6.0-31188p-64";
            }
            else if( linuxetcReleaseValue.find("5.6.100-39265p (xenenterprise)") != std::string::npos ) //for new name CitrixXen-5.6-FP1-32
            {
                if( bis32BitMachine )
                    name="CITRIX-XEN-5.6.100-39265p-32";					
            }
            else if( linuxetcReleaseValue.find("5.6.100-47101p (xenenterprise)") != std::string::npos ) //for new name CitrixXen-5.6-SP2-32
            {
                if( bis32BitMachine )
                    name="CITRIX-XEN-5.6.100-47101p-32";					
            }
            else if( linuxetcReleaseValue.find("6.0.0-50762p (xenenterprise)") != std::string::npos ) //1)XenServer release 6.0.0-50762p (xenenterprise) 2)XenServer SDK release 6.0.0-50762p (xenenterprise)
            {
                if( bis32BitMachine )
                    name="CITRIX-XEN-6.0.0-50762p-32";					
            }
            else if( linuxetcReleaseValue.find("6.0.2-53456p (xenenterprise)") != std::string::npos ) //1)XenServer release 6.0.2-53456p (xenenterprise) 2)XenServer SDK release 6.0.2-53456p (xenenterprise)
            {
                if( bis32BitMachine )
                    name="CITRIX-XEN-6.0.2-53456p-32";					
            }
            else if( (linuxetcReleaseValue.find("6.1.0-59235p (xenenterprise)") != std::string::npos ) )					
            {
                if( bis32BitMachine )
                    name="CITRIX-XEN-6.1.0-59235p-32";
            }
            else
            {
                errmsg = " Un Supported redhat release Flavour version.. ";
                iStatus = SV_FAILURE;
            }
        }		
        else if( linuxFlavourType == SUSE_RELEASE )
        {
            if( linuxetcReleaseValue.find("VERSION = 10") != std::string::npos )
            {		
                if( linuxetcReleaseValue.find("PATCHLEVEL") == std::string::npos )
                {
                    if( linuxetcReleaseValue.find("SUSE LINUX 10.0") != std::string::npos )
                    {
                        if( bis32BitMachine )
                            name="OPENSUSE10-32";
                        else
                            name="OPENSUSE10-64";
                    }
                    else if( linuxetcReleaseValue.find("SUSE Linux Enterprise Server 10") != std::string::npos )
                    {
                        if( bis32BitMachine )
                            name="SLES10-32";
                        else
                            name="SLES10-64";
                    }
                 }
                 else if( linuxetcReleaseValue.find("SUSE Linux Enterprise Server 10") != std::string::npos  &&
                         linuxetcReleaseValue.find("PATCHLEVEL = 1") != std::string::npos )
                 {
                    if( bis32BitMachine )
                        name="SLES10-SP1-32";
                    else
                        name="SLES10-SP1-64";
                 }
                 else if( linuxetcReleaseValue.find("SUSE Linux Enterprise Server 10") != std::string::npos &&
                         linuxetcReleaseValue.find("PATCHLEVEL = 2") != std::string::npos )
                 {
                    if( bis32BitMachine )
                        name="SLES10-SP2-32";
                    else
                        name="SLES10-SP2-64";
                 }
                 else if( linuxetcReleaseValue.find("SUSE Linux Enterprise Server 10") != std::string::npos &&
                         linuxetcReleaseValue.find("PATCHLEVEL = 3") != std::string::npos )
                 {
                    if( bis32BitMachine )
                        name="SLES10-SP3-32";
                    else
                        name="SLES10-SP3-64";
                 }
                 else if( linuxetcReleaseValue.find("SUSE Linux Enterprise Server 10") != std::string::npos &&
                         linuxetcReleaseValue.find("PATCHLEVEL = 4") != std::string::npos )
                 {
                    if( bis32BitMachine )
                        name="SLES10-SP4-32";
                    else
                        name="SLES10-SP4-64";
                 }				 
                 else if( linuxetcReleaseValue.find("PATCHLEVEL = 1") != std::string::npos )
                 {
                    if( bis32BitMachine )
                        name="OPENSUSE10-SP1-32";
                    else
                        name="OPENSUSE10-SP1-64";
                 }
                 else
                 {
                     errmsg = " Un Supported SuSE10 version ";
                 }
            }
            else if( linuxetcReleaseValue.find("VERSION = 11") != std::string::npos && 
                     linuxetcReleaseValue.find("SUSE Linux Enterprise Server 11") != std::string::npos && 
                     linuxetcReleaseValue.find("PATCHLEVEL = 0") != std::string::npos )
            {
                    if (linuxetcReleaseValue.find("SUSE Linux Enterprise Server 11") != std::string::npos )
                    {
                        if( bis32BitMachine )
                            name="SLES11-32";
                        else
                            name="SLES11-64";
                    }
                    else
                    {
                        errmsg = " Un Supported SuSE11 version ";
                    }
            }
            else if( linuxetcReleaseValue.find("VERSION = 11") != std::string::npos && 
                     linuxetcReleaseValue.find("SUSE Linux Enterprise Server 11") != std::string::npos && 
                     linuxetcReleaseValue.find("PATCHLEVEL = 1") != std::string::npos )
            {
                    if (linuxetcReleaseValue.find("SUSE Linux Enterprise Server 11") != std::string::npos )
                    {
                        if( bis32BitMachine )
                            name="SLES11-SP1-32";
                        else
                            name="SLES11-SP1-64";
                    }
                    else
                    {
                        errmsg = " Un Supported SuSE11 version ";
                    }
            }
            else if( linuxetcReleaseValue.find("VERSION = 11") != std::string::npos &&
                                         linuxetcReleaseValue.find("SUSE Linux Enterprise Server 11") != std::string::npos &&
                                         linuxetcReleaseValue.find("PATCHLEVEL = 2") != std::string::npos )
                        {
                                        if (linuxetcReleaseValue.find("SUSE Linux Enterprise Server 11") != std::string::npos )
                                        {
                                                if( bis32BitMachine )
                                                        name="SLES11-SP2-32";
                                                else
                                                        name="SLES11-SP2-64";
                                        }
                                        else
                                        {
                                                errmsg = " Un Supported SuSE11 version ";
                                        }
                        }
            else if( linuxetcReleaseValue.find("VERSION = 11") != std::string::npos &&
                                         linuxetcReleaseValue.find("SUSE Linux Enterprise Server 11") != std::string::npos &&
                                         linuxetcReleaseValue.find("PATCHLEVEL = 3") != std::string::npos )
                        {
                                        if (linuxetcReleaseValue.find("SUSE Linux Enterprise Server 11") != std::string::npos )
                                        {
                                                if( bis32BitMachine )
                                                        name="SLES11-SP3-32";
                                                else
                                                        name="SLES11-SP3-64";
                                        }
                                        else
                                        {
                                                errmsg = " Un Supported SuSE11 version ";
                                        }
                        }
            else if( linuxetcReleaseValue.find("VERSION = 11") != std::string::npos &&
                                         linuxetcReleaseValue.find("SUSE Linux Enterprise Server 11") != std::string::npos &&
                                         linuxetcReleaseValue.find("PATCHLEVEL = 4") != std::string::npos )
                        {
                                        if (linuxetcReleaseValue.find("SUSE Linux Enterprise Server 11") != std::string::npos )
                                        {
                                                if( bis32BitMachine )
                                                        name="SLES11-SP4-32";
                                                else
                                                        name="SLES11-SP4-64";
                                        }
                                        else
                                        {
                                                errmsg = " Un Supported SuSE11 version ";
                                        }
                        }
            else if( linuxetcReleaseValue.find("VERSION = 9") != std::string::npos )
            {
                if( linuxetcReleaseValue.find("PATCHLEVEL") == std::string::npos )
                {
                    if( bis32BitMachine )
                        name="SLES9-32";
                    else
                        name="SLES9-64";
                }
                else if( linuxetcReleaseValue.find("PATCHLEVEL = 2") != std::string::npos )
                {
                    if( bis32BitMachine )
                        name="SLES9-SP2-32";
                    else
                        name="SLES9-SP2-64";
                }
                else if( linuxetcReleaseValue.find("PATCHLEVEL = 3") != std::string::npos )
                {
                    if( bis32BitMachine )
                        name="SLES9-SP3-32";
                    else
                        name="SLES9-SP3-64";
                }
                else
                {
                    errmsg = " Un Supported SuSE9 version ";
                }
            }
            else if ( linuxetcReleaseValue.find("VERSION = 12") != std::string::npos )
            {
                name = bis32BitMachine ? "SLES12-32" : "SLES12-64";
            }
            else if ( linuxetcReleaseValue.find("VERSION=\"15") != std::string::npos )
            {
                name = bis32BitMachine ? "SLES15-32" : "SLES15-64";
            }
            else
            {
                errmsg = " Un Supported SuSE release Flavour version.. ";
                iStatus = SV_FAILURE;
            }
        }
        else if( linuxFlavourType == UBUNTU_RELEASE )
        {
            if(linuxetcReleaseValue.find("10.04.1") != std::string::npos)
            {
                if( bis32BitMachine )
                    name="UBUNTU10-32";
                else
                    name="UBUNTU10-64";				
            }
            else if(linuxetcReleaseValue.find("10.04.4 LTS") != std::string::npos)
            {
                if( bis32BitMachine )
                {
                    name=" ";
                }	
                else
                    name="UBUNTU-10.04.4-64";	
            }
            else if(linuxetcReleaseValue.find("12.04.4 LTS") != std::string::npos)
            {
                if( bis32BitMachine )
                {
                    name=" ";
                }	
                else
                {
                    name="UBUNTU-12.04.4-64";	
                }
            }
            else if(linuxetcReleaseValue.find("16.04") != std::string::npos)
            {
                if( bis32BitMachine )
                {
                    name=" ";
                }
                else
                    name="UBUNTU-16.04-64";
            }
            else if(linuxetcReleaseValue.find("14.04") != std::string::npos)
            {
                if( bis32BitMachine )
                {
                    name=" ";
                }
                else
                    name="UBUNTU-14.04-64";
            }
            else if(linuxetcReleaseValue.find("18.04") != std::string::npos)
            {
                if( bis32BitMachine )
                {
                    name=" ";
                }
                else
                    name="UBUNTU-18.04-64";
            }
            else if(linuxetcReleaseValue.find("20.04") != std::string::npos)
            {
                if( bis32BitMachine )
                {
                    name=" ";
                }
                else
                    name="UBUNTU-20.04-64";
            }
            else if(linuxetcReleaseValue.find("22.04") != std::string::npos)
            {
                if( bis32BitMachine )
                {
                    name=" ";
                }
                else
                    name="UBUNTU-22.04-64";
            }
            else
            {
                errmsg = " Un Supported Ubuntu release Flavour version.. ";
                iStatus = SV_FAILURE;
            }
        }
        else if( linuxFlavourType == DEBAIN_RELEASE )
        {
            if (linuxetcReleaseValue.find("VERSION_ID=\"7") != std::string::npos )
            {
                if( bis32BitMachine )
                {
                    errmsg = " Un Supported 32-bit Debian 7.. ";
                    iStatus = SV_FAILURE;
                }
                else
                    name="DEBIAN7-64";
            }
            else if( linuxetcReleaseValue.find("VERSION_ID=\"8") != std::string::npos )
            {
                if( bis32BitMachine )
                {
                    errmsg = " Un Supported 32-bit Debian 8.. ";
                    iStatus = SV_FAILURE;
                }
                else
                    name="DEBIAN8-64";
            }
            else if( linuxetcReleaseValue.find("VERSION_ID=\"9") != std::string::npos )
            {
                if( bis32BitMachine )
                {
                    errmsg = " Un Supported 32-bit Debian 9.. ";
                    iStatus = SV_FAILURE;
                }
                else
                    name="DEBIAN9-64";
            }
            else if( linuxetcReleaseValue.find("VERSION_ID=\"10") != std::string::npos )
            {
                name="DEBIAN10-64";
            }
            else if( linuxetcReleaseValue.find("VERSION_ID=\"11") != std::string::npos )
            {
                name="DEBIAN11-64";
            }
            else
            {
                errmsg = " Un Supported Debain release Flavour version.. ";
                iStatus = SV_FAILURE;
            }
        }
        else
        {
            errmsg = " Un Supported Linux Flavour.. ";
            iStatus = SV_FAILURE;
        }
    }
    removeStringSpaces( name );
    if( name.empty() == true )
    {
        name = "Linux";
    }

    if (SV_SUCCESS == iStatus)
    {
        m_OsInfo.m_Attributes.insert(std::make_pair(NSOsInfo::NAME, name));
		m_OsInfo.m_Attributes.insert(std::make_pair(NSOsInfo::DISTRO_NAME, name));
        m_OsInfo.m_Attributes.insert(std::make_pair(NSOsInfo::CAPTION, linuxetcReleaseValue));
        std::string systemType = bis32BitMachine ? "32-bit" : "64-bit";
        m_OsInfo.m_Attributes.insert(std::make_pair(NSOsInfo::SYSTEMTYPE, systemType));
    }

    return iStatus;
}


int OperatingSystem::SetOsVersionBuild(std::string &errmsg)
{
    return SV_SUCCESS;
}
