#ifndef _MICROSOFTTESTINMAGEDATAVALIDATOR_H_
#define _MICROSOFTTESTINMAGEDATAVALIDATOR_H_

#include <string>

#ifdef MICROSOFTTESTINMAGEDATAVALIDATOR_EXPORTS
#define MICROSOFTTESTINMAGEDATAVALIDATOR_API __declspec(dllexport)
#else
#define MICROSOFTTESTINMAGEDATAVALIDATOR_API __declspec(dllimport)
#endif

// This class is exported from the Microsoft.Test.Inmage.DataValidator.dll
class MICROSOFTTESTINMAGEDATAVALIDATOR_API CInmageDataValidator {
public:
	CInmageDataValidator(void);
	void AddSourceDisk(std::string deviceId);
	void AddSourceConsistencyProvider();
	void AddTargetConsistencyProvider();
	void AddSourceConsistencyProvider(std::string deviceId);
};

#endif