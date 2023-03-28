
///
///  \file comconversion.h
///
///  \brief utility routines for converting std c++ object to COM instance
///
///


#ifndef INMAGE_COMCONVERSION_H
#define INMAGE_COMCONVERSION_H

#include <atlbase.h>
#include <atlsafe.h>
#include <comutil.h>

#include <string>
#include <vector>
#include <sstream>


#include "errorexception.h"

namespace ComConversion {

	/// \brief multi-byte string to wide-char string conversion
	
	inline std::wstring ToWstring(const std::string & s)
	{
		CA2W ca2w(s.c_str());
		std::wstring w = ca2w;
		return w;
	}

	/// \brief wide-char string to multi-byte string conversion
	
	inline std::string ToString(const std::wstring & ws)
	{
		CW2A cw2a(ws.c_str());
		std::string s = cw2a;
		return s;
	}

	/// \brief convert guid in string representation to guid format
	/// input should be *without* parenthesis
	/// eg: "A8405013-E816-4594-A2FD-AEDE86AB452C"
	

	inline GUID ToGUID(const WCHAR* wzGuid)
	{
		UUID rawGuid;
		std::wstring parenGuid(L"{");
		parenGuid += wzGuid;
		parenGuid += L"}";
		HRESULT hr = CLSIDFromString(parenGuid.c_str(), &rawGuid);
		if (FAILED(hr))	{
			throw ERROR_EXCEPTION << "Conversion of " << wzGuid
				<< " to GUID failed with HRESULT :" << hr;
		}

		return rawGuid;
	}

	inline GUID ToGUID(const std::string & s)
	{
		return ToGUID(ToWstring(s).c_str());
	}

	
	/// \brief multi-byte string to BSTR conversion
	
	inline BSTR ToBstr(const std::string & s)
	{
		return _com_util::ConvertStringToBSTR(s.c_str());
	}

	/// \brief wstring to BSTR conversion

	inline BSTR ToBstr(const std::wstring & ws)
	{
		return _com_util::ConvertStringToBSTR(ToString(ws).c_str());
	}

	/// \brief BSTR to multi-byte string conversion
	
	inline std::string ToString(BSTR bstr)
	{
		return _com_util::ConvertBSTRToString(bstr);
	}

	/// \brief template for copying objects stored in c++ STL vector
	///  to COM SafeArray
	
	template<typename T>
	void ToSafeArray(const std::vector<T> &v, CComSafeArray<T> & sa)
	{
		SafeArrayDestroyData(sa);
		sa.Add(v.size(), &v[0]);
	}


	/// \brief specialized routine for copying string stored in c++ STL vector
	/// to COM SafeArray of BSTR
	
	inline void ToSafeArray(const std::vector<std::string> &v, CComSafeArray<BSTR> & sa)
	{
		SafeArrayDestroyData(sa);
		for (size_t i = 0; i < v.size(); ++i)
		{
			CComBSTR c((v[i]).c_str());
			sa.Add(c);
		}
	}


	/// \brief template for copying objects stored in COM SafeArray
	///  to c++ STL vector


	template<typename T>
	void ToVector(const CComSafeArray<T> & csa, std::vector<T> & v)
	{
		v.clear();
		for (size_t i = 0; i < csa.GetCount(); ++i)
		{
			v.push_back(csa.GetAt(i));
		}
	}

	/// \brief specialized routine for copying BSTR(s) stored in COM SafeArray
	/// to c++ STL vector of string(s)

	inline void ToVector(const CComSafeArray<BSTR> & csa, std::vector<std::string> & v)
	{
		v.clear();
		for (size_t i = 0; i < csa.GetCount(); ++i)
		{
			v.push_back(ToString(csa.GetAt(i)));
		}
	}

	template<typename T>
	CComPtr<IRecordInfo> GetRecordInfo(const GUID & typeLibGuid)
	{
		HRESULT hr = E_FAIL;
		CComPtr<ITypeInfo> pTypeInfo;
		CComPtr<ITypeLib> pTypelib;
		CComPtr<IRecordInfo> pRecInfo;


		// load the type librarary describing the UDT
		hr = LoadRegTypeLib(typeLibGuid, 1, 0, GetUserDefaultLCID(), &pTypelib);
		if (FAILED(hr) || !pTypelib) {

			std::string err;
			convertErrorToString(hr, err);
			throw ERROR_EXCEPTION <<
				"internal error, LoadRegTypeLib failed  to return library type info with error"
				<< std::hex << hr << "(" << err << ")";
		}

		// get the type information by looking up for uid of the specified type in type library
		hr = pTypelib->GetTypeInfoOfGuid(__uuidof(T), &pTypeInfo);
		if (FAILED(hr) || !pTypeInfo) {

			std::string err;
			convertErrorToString(hr, err);
			throw ERROR_EXCEPTION
				<< "internal error, GetTypeInfoOfGuid failed"
				" to return type info for " << typeid(T).name() << " with error"
				<< std::hex << hr << "(" << err << ")";
		}

		// convert type information to record information
		hr = GetRecordInfoFromTypeInfo(pTypeInfo, &pRecInfo);
		if (FAILED(hr) || !pRecInfo) {

			std::string err;
			convertErrorToString(hr, err);
			throw ERROR_EXCEPTION << "internal error, GetRecordInfoFromTypeInfo failed"
				" to return record info " << typeid(T).name() << " with error"
				<< std::hex << hr << "(" << err << ")";
		}

		return pRecInfo;
	}


	template<typename T>
	void CreateSafeArrayUDT(CComPtr<IRecordInfo> pRecInfo, unsigned long nElems, LPSAFEARRAY * ppsa)
	{
		SAFEARRAYBOUND rgbounds = { nElems, 0 };

		if (!ppsa){
			throw ERROR_EXCEPTION << "internal error, recieved null ptr!";
		}

		*ppsa = SafeArrayCreateEx(VT_RECORD, 1, &rgbounds, pRecInfo);
		if (!(*ppsa)) {
			throw ERROR_EXCEPTION << "internal error, SafeArrayCreateEx returned null ptr!";
		}
	}



	template<typename T>
	class SafeArrayUDT
	{
	public:

		SafeArrayUDT(const GUID & typeLibGuid, unsigned long nElems)
			:m_maxElems(nElems),
			m_idx(0),
			m_psa(0),
			m_dataptr(0)
		{
			CComPtr<IRecordInfo *> pRecordInfo;

			pRecordInfo = GetRecordInfo<T>(typeLibGuid);
			CreateSafeArrayUDT<T>(pRecordInfo, nElems, &m_psa);
		}

		SafeArrayUDT(CComPtr<IRecordInfo> pRecordInfo, unsigned long nElems)
			:m_maxElems(nElems),
			m_idx(0),
			m_psa(0),
			m_dataptr(0)
		{
			CreateSafeArrayUDT<T>(pRecordInfo, nElems, &m_psa);
		}


		~SafeArrayUDT()
		{
			HRESULT hr = S_OK;
			if (m_dataptr)
			{
				hr = SafeArrayUnaccessData(m_psa);
				if (FAILED(hr)) {

					std::string err;
					convertErrorToString(hr, err);
					std::stringstream errMsg;
					errMsg << "internal error, SafeArrayUnaccessData failed with error"
						<< std::hex << hr << "(" << err << ")";
					DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.str().c_str());
				}

				m_dataptr = NULL;
			}

			hr = SafeArrayDestroy(m_psa);
			if (FAILED(hr)) {

				std::string err;
				convertErrorToString(hr, err);
				std::stringstream errMsg;
				errMsg << "internal error, SafeArrayDestroy failed with error"
					<< std::hex << hr << "(" << err << ")";
				DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.str().c_str());
			}
		}

		void Add(T& elem)
		{
			if (!m_dataptr) {

				HRESULT hr = SafeArrayAccessData(m_psa, reinterpret_cast<PVOID*>(&m_dataptr));
				if (FAILED(hr) || !m_dataptr) {

					std::string err;
					convertErrorToString(hr, err);
					throw ERROR_EXCEPTION << "internal error, SafeArrayAccessData failed  to return struct "
						<< typeid(T).name()
						<< " with error"
						<< std::hex << hr << "(" << err << ")";
				}
			}

			if (m_idx >= m_maxElems){

				throw ERROR_EXCEPTION << "internal error, exceeding max elements for SafeArray for "
					<< typeid(T).name()
					<< " max count: " << m_maxElems;
			}

			m_dataptr[m_idx] = elem;
			++m_idx;
		}

		LPSAFEARRAY operator *() { return m_psa; }

	protected:

	private:

		unsigned long m_maxElems;
		unsigned long m_idx;
		LPSAFEARRAY m_psa;
		T * m_dataptr;

	};




	template<typename Ta, typename Tb>
	void ToSafeArray(const std::vector<Ta> &v, SafeArrayUDT<Tb> & sa, Tb(*converter) (const Ta &))
	{
		for (size_t i = 0; i < v.size(); ++i)
		{
			sa.Add(converter(v[i]));
		}

	}

}

#endif