#ifndef _SIGNMGR_H
#define _SIGNMGR_H

#include "extendedlengthpath.h"
#include "inmsafecapis.h"
#include "portable.h"
#include "logger.h"
#include <tchar.h>
#include <Windows.h>
#include <Softpub.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <vector>

class SignMgr
{
private:

	std::vector<PCCERT_CONTEXT> m_vCertContext;
	void HandleError(TCHAR* s);

	//PCCERT_CONTEXT GetCertContext(LPCWSTR pszSignedFile);
	bool SignMgr::GetCertContext(LPCWSTR szFileName,std::vector<PCCERT_CONTEXT> &vCertContext);

	bool CheckIfRootCAisMS(WINTRUST_DATA *pWinTrustData);

public:
	SignMgr();

	~SignMgr();

	bool VerifyEmbeddedSignature(TCHAR* pszSignedFile);
};
#endif
