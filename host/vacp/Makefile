# Makefile for generating VACP's Release and Debug Builds
# for Windows 2008 and higher platforms
#
# To build all Release and Debug builds, use below command
# 	nmake -f Makefile All
#
# To build only Release build, use below command
# 	nmake -f Makefile Release
#
# To build only Debug build, use below command
# 	nmake -f Makefile Debug
#
# To Clean existing builds, use below command
# 	nmake -f Makefile Clean
#
#   {Release/Debug}\vacp.exe is 32 bit binary generated for Windows 2008 and higher 32-bit OS
#


## Specify Compiler and Linker Executables
CC=cl.exe
LINK=link.exe
RC=rc.exe
MIDL=midl.exe
MT=mt.exe

# Specify HeaderFiles (.h suffix)
HEADERS=Communicator.h ArgParser.h ..\common\win32\Failoverclusterinfocollector.h ..\common\win32\registry.h \
		CmdRequest.h macros.h stdafx.h util.h common.h \
        VssRequestor.h VssWriter.h vacp.h VacpConf.h PerfCounter.h SecureTransport.h \
        deviceutils.h  diskutils.h  vacpwmiutils.h \
        ..\common\win32\DiskHelpers.h ..\common\VacpErrors.h \
		..\hydrationconfigurator\HydrationConfigurator.h \
		..\hydrationconfigurator\win\HydrationProvider.h \
        StreamEngine\InvoltTypes.h StreamEngine\StreamEngine.h StreamEngine\StreamRecords.h \
		..\config\vacp_tag_info.h ..\devicefilter\devicefilter.h \
		..\drivers\InVolFlt\windows\interface\InmFltIOCTL.h \
		..\drivers\InVolFlt\windows\interface\InmFltInterface.h \
		..\errorexception\errorexception.h ..\cxpslib\win\setsockettimeoutsminor.h \
        ..\vacplibs\VacpErrorCodes.h ..\s2libs\thread\event.h ..\InMageVssProvider\VacpProviderCommon.h

# Specify Source Files (.cpp suffix)
SRC=Communicator.cpp ..\common\win32\registry.cpp \
	..\common\win32\Failoverclusterinfocollector.cpp \
	CmdRequest.cpp VssRequestor.cpp vacp.cpp util.cpp \
    ArgParser.cpp VssWriter.cpp StreamEngine\StreamEngine.cpp VacpLogger.cpp \
    ..\common\win32\DiskHelpers.cpp \
    SecureTransport.cpp deviceutils.cpp diskutils.cpp wmiutils.cpp \
	..\hydrationconfigurator\HydrationConfigurator.cpp \
	..\hydrationconfigurator\win\HydrationProvider.cpp 

#Specify resource Files (.rc suffix)
RC_SOURCE=vacp.rc

DEBUG_RCFLAGS=/fo Debug\vacp.res
RELEASE_RCFLAGS =/fo Release\vacp.res

# Specify precompile header file
PCHFILE="stdafx.h"

# Specify Compilation Flags for FabricRelease build
RELEASE_CPPFLAGS=/O2 /D "WIN32" /D "_CONSOLE" /D "_CRT_SECURE_NO_WARNINGS" /D "_CRT_NONSTDC_NO_DEPRECATE" /D "_WIN32_WINNT=0x0600"\
                 /D "VSS_SERVER" /D "_ATL_STATIC_REGISTRY" /D "_USING_V110_SDK71_" /FD /EHsc /MT\
                 /GS  /W3 /nologo /c /Zc:wchar_t /Wp64 /Zi /TP /D"ACE_AS_STATIC_LIBS" /D"SV_WINDOWS" /D"VACP" /guard:cf /Qspectre /DYNAMICBASE /D"NOMINMAX"\
				 /D "VACP_CONTEXT"

RELEASE_HOST_CPPFLAGS= /Fo"Release/"

# Specify Compilation Flags for Debug build
DEBUG_CPPFLAGS=/Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_CRT_SECURE_NO_WARNINGS" /D "_CRT_NONSTDC_NO_DEPRECATE" /D "_ATL_NO_DEBUG_CRT"\
               /D "VSS_SERVER"  /D "_USING_V110_SDK71_" /EHsc /RTC1 /MTd\
               /D "_ATL_STATIC_REGISTRY"  /W3 /nologo /c /Zc:wchar_t /Wp64 /Zi /TP /D"ACE_AS_STATIC_LIBS" /D"VACP" /D"SV_WINDOWS" /guard:cf /Qspectre /DYNAMICBASE /D"NOMINMAX"\
			   /D "VACP_CONTEXT"

DEBUG_HOST_CPPFLAGS= /Fo"Debug/"

VACP_CUR_DIR=..\Vacp

# Specify VSS SDK Libraries
VSS_LIBS=vssapi.lib vss_uuid.lib

# specify boost libraries
BOOST_LIBS=

# specify openssl libraries
OPENSSL_LIBS=libssl.lib libcrypto.lib

# specify ACE libraries
ACE_RELEASE_LIB=ACEs.lib
ACE_DEBUG_LIB=ACEsd.lib

# specify CURL libraries
CURL_RELEASE_LIBS=libcurl.lib
CURL_DEBUG_LIBS=libcurld.lib

# specify other libraries
AGENT_LIBS_RELEASE=..\AzureADAuthProviderLib\Release\AzureADAuthProviderLib.lib \
                   ..\AzureRecoveryLib\Release\AzureRecoveryLib.lib \
                   ..\securitylib\Release\securitylib.lib
                   
AGENT_LIBS_DEBUG=..\AzureADAuthProviderLib\Debug\AzureADAuthProviderLib.lib \
                 ..\AzureRecoveryLib\Debug\AzureRecoveryLib.lib \
                 ..\securitylib\Debug\securitylib.lib

#This debug flag gets enabled 
ENABLE_DEBUG=

# Specify Windows Static libraries
OS_LIBS=rpcrt4.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
        advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
        odbccp32.lib ws2_32.lib WinInet.lib secur32.lib Crypt32.lib NCrypt.lib clusapi.lib

# Specify Linking flags for FabricRelease build
RELEASE_LFLAGS=/INCREMENTAL:NO /NOLOGO /SUBSYSTEM:CONSOLE,5.01 /OPT:NOREF /OPT:ICF /DYNAMICBASE /NXCOMPAT\
               /MACHINE:X86 /MAP /MAPINFO:EXPORTS /DEBUG /DEBUGTYPE:CV,FIXUP /guard:cf

# Specify Linking flags for Debug build
DEBUG_LFLAGS=/INCREMENTAL:NO /NOLOGO /SUBSYSTEM:CONSOLE,5.01 /MACHINE:X86 /DEBUG /DYNAMICBASE /NXCOMPAT /guard:cf

# current boost version
BOOST_LOC=..\..\thirdparty\boost\boost_1_78_0
OPENSSL_LOC=..\..\thirdparty\openssl-1.1.1n
ACE_LOC=..\..\thirdparty\ace-6.4.6\ACE_wrappers
CURL_LOC=..\..\thirdparty\curl-7.87.0
JSON_LOC=..\..\thirdparty\g40-esj-1.05

# Path of thirdparty libraries that are converted as NuGet packages.
OPENSSL_LIB=..\packages\openssl.1.1.1-n-1
ACE_LIB=..\packages\ace.6.4.6\ACE_wrappers

# specify boost flags
BOOST_LFLAGS=/LIBPATH:"$(BOOST_LOC)\stage\win\win32\lib"
ACE_LFLAGS=/LIBPATH:"$(ACE_LIB)\lib"

#specify openssl flags
OPENSSL_LFLAGS_DEBUG=/LIBPATH:"$(OPENSSL_LIB)\out32.dbg"
OPENSSL_LFLAGS_RELEASE=/LIBPATH:"$(OPENSSL_LIB)\out32"

#specify CURL flags
CURL_LFLAGS_DEBUG=/LIBPATH:"$(CURL_LOC)\build\Win32\VC14.30\LIB Debug - LIB OpenSSL"
CURL_LFLAGS_RELEASE=/LIBPATH:"$(CURL_LOC)\build\Win32\VC14.30\LIB Release - LIB OpenSSL"

BOOST_BUILD_CMD=..\..\thirdparty\boost\boost-build-win\inmage_config_build.bat

# Specify other include directories
OTHER_INC_DIR=.\StreamEngine

WIN_2K8_VERSION=/D "_WIN32_WINNT=0x0600"

# Compute Include directories for Windows 2008 Build
WIN2K8_INC_DIR=/I $(VACP_CUR_DIR) /I $(OTHER_INC_DIR)\
	/I"..\devicefilter" /I"..\devicefilter\win" /I"..\drivers\InVolFlt\windows\Interface"  /I"..\drivers\include\win32" \
	/I"..\inmsafecapis" /I"..\inmsafecapis\win" /I"..\common" \
	/I"..\inmsafeint" /I"..\inmsafeint\win" /I "$(JSON_LOC)" \
	/I"$(BOOST_LOC)" /I"$(OPENSSL_LOC)\include" /I"..\securitylib" /I"..\securitylib\win" /I"..\scopeguard" /I"..\errorexception" \
        /I"..\cxpslib\win" /I"..\vacplibs" /I"$(ACE_LOC)" /I"$(CURL_LOC)\include" \
		/I..\vacp \
	/I"..\cxpslib" /I"..\AzureADAuthProviderLib" \
	/I"..\AzureRecoveryLib" /I"..\AzureRecoveryLib\resthelper" /I"..\AzureRcmLib"  /I"..\InMageVssProvider" \
	/I"..\common\win32"
    
# Compute Link flags for WIndows 2008 Release Build
RELEASE_WIN2K8_LFLAGS=$(RELEASE_LFLAGS) $(BOOST_LFLAGS) $(OPENSSL_LFLAGS_RELEASE) $(ACE_LFLAGS) $(CURL_LFLAGS_RELEASE) /PDB:"Release/vacp.pdb"

# Compute Link flags for WIndows 2008 Debug Build
DEBUG_WIN2K8_LFLAGS=$(DEBUG_LFLAGS) $(BOOST_LFLAGS) $(OPENSSL_LFLAGS_DEBUG) $(ACE_LFLAGS) $(CURL_LFLAGS_DEBUG) /PDB:"Debug/vacp.pdb"

#Generate both release and debug builds
all: Release Debug

#Generate Release build for Windows 2008
Release : release\vacprpc_s.obj Release\vacp.exe 

#Generate Debug build for Windows 2008
Debug:   debug\vacprpc_s.obj Debug\vacp.exe 

# vacprpcs stub file
debug\vacprpc_s.obj : vacprpc\vacprpc.idl vacprpc\vacprpc.acf
	@$(MIDL) /env win32 /acf vacprpc\vacprpc.acf  vacprpc\vacprpc.idl
	@$(CC) $(DEBUG_CPPFLAGS) $(DEBUG_HOST_CPPFLAGS) $(WIN_2K8_VERSION) $(WIN2K8_INC_DIR) vacprpc_s.c


release\vacprpc_s.obj : vacprpc\vacprpc.idl vacprpc\vacprpc.acf
	@$(MIDL) /env win32 /acf vacprpc\vacprpc.acf  vacprpc\vacprpc.idl
	@$(CC) $(RELEASE_CPPFLAGS) $(RELEASE_HOST_CPPFLAGS) $(WIN_2K8_VERSION) $(WIN2K8_INC_DIR) vacprpc_s.c

#Generate Release build for Windows 2008 and above
Release\vacp.exe: $(HEADERS) $(SRC) Makefile 
	@echo -------------------------------------------------------------
	@echo Generating Windows 2008 Release build(Release\vacp.exe)...
	@echo -------------------------------------------------------------
	@IF NOT EXIST Release MD Release
	@$(MIDL) ..\InMageVssProvider\InMageVssProvider.idl /nologo /char signed /Oicf /h "InMageVssProvider_i.h" /iid "InMageVssProvider_i.c" /error stub_data
	@$(CC) $(RELEASE_CPPFLAGS) $(RELEASE_HOST_CPPFLAGS) $(WIN_2K8_VERSION) /Yc$(PCHFILE) $(WIN2K8_INC_DIR) $(SRC)
	@$(CC) $(RELEASE_CPPFLAGS) $(RELEASE_HOST_CPPFLAGS) $(WIN_2K8_VERSION) $(PCHFILE) $(WIN2K8_INC_DIR) $(SRC)
	@$(RC) $(RELEASE_RCFLAGS) $(RC_SOURCE)
	@$(LINK) $(RELEASE_WIN2K8_LFLAGS) -out:Release\vacp.exe Release\*.obj Release\*.res $(VSS_LIBS) $(OS_LIBS) $(OPENSSL_LIBS) $(BOOST_LIBS) $(ACE_RELEASE_LIB) $(AGENT_LIBS_RELEASE) $(CURL_RELEASE_LIBS)
	@$(MT) -manifest ..\common\win32\osmanifest.xml -outputresource:Release\vacp.exe;1

#Generate Debug build for Windows 2008 and above
Debug\vacp.exe: $(HEADERS) $(SRC) Makefile
	@echo ---------------------------------------------------------
	@echo Generating Windows 2008 Debug build(Debug\vacp.exe)...
	@echo ---------------------------------------------------------
	@IF NOT EXIST Debug MD Debug
	@$(MIDL) ..\InMageVssProvider\InMageVssProvider.idl /D "_DEBUG" /nologo /char signed /Oicf /h "InMageVssProvider_i.h" /iid "InMageVssProvider_i.c" /error stub_data
	@$(CC) $(DEBUG_CPPFLAGS) $(DEBUG_HOST_CPPFLAGS) $(WIN_2K8_VERSION) /Yc$(PCHFILE) $(WIN2K8_INC_DIR) $(SRC)
	@$(CC) $(DEBUG_CPPFLAGS) $(DEBUG_HOST_CPPFLAGS) $(WIN_2K8_VERSION) $(PCHFILE) $(WIN2K8_INC_DIR) $(SRC)
	@$(RC) $(DEBUG_RCFLAGS) $(RC_SOURCE)
	@$(LINK) $(DEBUG_WIN2K8_LFLAGS) -out:Debug\vacp.exe Debug\*.obj Debug\*.res $(VSS_LIBS) $(OS_LIBS) $(BOOST_LIBS) $(OPENSSL_LIBS) $(ACE_DEBUG_LIB) $(AGENT_LIBS_DEBUG) $(CURL_DEBUG_LIBS) libcmtd.lib



#Clean both Release and Debug builds
clean:
	@echo ------------------------------------------------------------------
	@echo Cleaning Windows 2008 and above Release,Debug builds...
	@echo ------------------------------------------------------------------
	@IF EXIST Release DEL /Q Release\*.*
	@IF EXIST Debug DEL /Q Debug\*.*
	DEL InMageVssProvider*.*

#Clean both build only
clean_release:
	@echo ------------------------------------------------------------------
	@echo Cleaning Windows 2008 and above Release build...
	@echo ------------------------------------------------------------------
	@IF EXIST Release DEL /Q Release\*.*
	DEL InMageVssProvider*.*

#Clean build only
clean_debug:
	@echo ------------------------------------------------------------------
	@echo Cleaning Windows 2008 and above Debug build...
	@echo ------------------------------------------------------------------
	@IF EXIST Debug DEL /Q Debug\*.*
	DEL InMageVssProvider*.*
