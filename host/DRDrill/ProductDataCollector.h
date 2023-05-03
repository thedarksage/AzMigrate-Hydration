/****************************************************************************************************
*	"ProductDataCollector" class collects the Common Data across all the applications required for validations.
*	It will have logic/procedure to get this information from several places like getting information
*	from Source, from Cx and from Target.
*****************************************************************************************************/


#ifndef __PRODUCT_DATA_COLLECTOR_H
#define __PRODUCT_DATA_COLLECTOR_H



#include <string>
#include <list>
#include <map>
#include "Common.h"
#include "DrDrill.h"
#include "Drdefs.h"
#include "ADInterface.h"



namespace DrDrillNS
{
  //#ifdef _UNICODE
	typedef std::string _tstring;
    typedef char _tchar;
  //#endif

	class ProductDataCollector : public DrDrill 
	{

		public:
			_tstring m_srcHostName;
			_tstring m_tgtHostName;
			_tstring m_evsName;
			_tstring m_appName;
			bool m_bDrDrill;

			//Common Data collected from Source in pre-script
			//same is used to collect from Target in post-script
			char m_chostID[MAX_PATH];
			std::list<ULONG> m_ulIpAddress;
			char m_cHostName[MAX_PATH];
			char m_cSystemDrive;
			bool m_bvxSrvStatus;
			bool m_bfxSrvStatus;
			_tstring m_strFxLogOnAccount;
			_tstring m_cTimeStamp;

			//Source & Taget common data from Target collected in the Post-Script
			_tstring m_srcOSName;
			_tstring m_srcOSVersion;
			_tstring m_srcOSBuild;
			bool m_srcPingStatus;

			_tstring m_tgtOSName;
			_tstring m_tgtOSVersion;
			_tstring m_tgtOSBuild;
			bool m_tgtPageFile;

			//Data collected from Cx using Configurator in the Post-Script
			/***Pair Level***/
			bool m_bIsPairPaused;
			bool m_bIsVolumeHidden;
			bool m_bIsReSyncRequired;
			bool m_bIsVolumeResized;
			ULONG m_ulRPO;
			ULONG m_ulThresholdRPO;
			bool m_bIsRetentionEnabled;			
			bool m_bIsProtected;
			ULONG m_ulRetentionWindow;//Number of Days or Size of Retention Window in MB
				
			/***Server Level***/
			bool m_bHasConsistencyJobFailed;
			ULONG m_ulGreenOverlapPercentage;
			bool m_bLicenseStatus;

			std::list<VolumeConfiguration> m_lVolConfig;

			ProductDataCollector(void);
			~ProductDataCollector(void);
			ProductDataCollector(const _tstring &srcHost,
							const _tstring &tgtHost,
							const _tstring &evsName,
							const _tstring &appName,
							bool &bdrdrill);
			bool IsSourceUp(void);	//CHECK, feel like this function is not required!
			bool IsCxUp(void);
			void drInit(void);
			_tstring inmagePath;
			bool CollectData(void);
			bool CollectCommonData(void);
			bool GetDataFromSource(void);

			virtual bool PerformAppValidation();
			virtual bool CollectAppData();
			virtual bool GenerateAppReport();	
			ADInterface obj_adIntf; 

		protected:
			std::list<ULONG> GetIpAddress(const _tstring hostName);
				

		private:
					
			//Functions used to collect & validate data
			bool GetHostId();
			
			bool GetHostName(void);	
			char GetSystemDrive(void);
			_tstring GetOsName(void);
			_tstring GetOsVersion(void);
			SERVICESTATUS GetVxServiceStatus(void);
			SERVICESTATUS GetFxServiceStatus(void);
			bool GetAgentServiceStatus(void);
			_tstring GetFxLogOnAccount(void);
			bool GetTimeStamp(void);
			std::list<ULONG> GetVirtualServerIP(const _tstring vsName);
			std::list<struct VolumeConfiguration> GetVolumeConfigurations(void);

			//Query Data from Cx
			bool GetDataFromCx(void);
			ULONG GetRPO(void);
			bool IsProtected(void);
			bool IsPairPaused(void);
			bool IsReSyncRequired(void);
			bool IsRetentionEnabled(void);
			ULONG GetRetentionWindow(void);//MCHECK,is it number of days or Size in MB?
			ULONG GetThresholdRPO(void);
			bool IsVolumeResized(void);
			bool IsVolumeHidden(void);
			bool HasConsistencyJobFailed(void);
			ULONG GetGreenOverlapPercentage(void);
			bool GetLicenseStatus(void);

		  //Data from Target;All these are collected in the Post-Script
			bool GetDataFromTarget(void);

		 		

	  };

}
#endif