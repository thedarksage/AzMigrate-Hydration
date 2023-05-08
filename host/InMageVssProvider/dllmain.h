// dllmain.h : Declaration of module class.

class CInMageVssProviderModule : public CAtlDllModuleT< CInMageVssProviderModule >
{
public :
	DECLARE_LIBID(LIBID_InMageVssProviderLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_INMAGEVSSPROVIDER, "{0CF67784-626E-4242-BA41-933931FB90D3}")
};

extern class CInMageVssProviderModule _AtlModule;
