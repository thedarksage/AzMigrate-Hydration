#ifndef __DRDRILL_H
#define __DRDRILL_H

namespace DrDrillNS
{
	//#ifdef _UNICODE
	typedef std::string _tstring;
	typedef char _tchar;
	//#endif
	class DrDrill //throw DrDrillException
	{
		public:
			bool PerformValidation();
			bool GenerateReport();
			bool CollectCoreProductData();


			virtual bool PerformAppValidation()= 0;
			virtual bool CollectAppData() = 0;
			virtual bool GenerateAppReport() = 0;
		
			//bool CreateAppCollector(Collector *pCollector);
			//bool CreateAppValidator(Validator *pValidator);
			//bool CreateAppReport(Report *pReport);
		protected:
			
			   
		private:
			bool PerformCoreProductValidation();

			//Data Members
			/*SqlServerCollector  m_sqlCollector;
			SqlServerValidator  m_sqlValidator;
			SqlServerReport     m_sqlReport;

			ExchangeCollector   m_exchangeCollector;
			ExchangeValidator   m_exchangeValidator;
			ExchangeReport      m_exchangeReport;*/
			
	  };
	  
	  
	  
	  /*class MsSql : public DrDrill
	  {
	  };
	  /*bool DrDrill::CreateAppCollector(Collector *pCollector)
	  {
		pCollector->Create();
	  }*/

}
#endif