# Makefile for generating InMageVssProvider's Release and Debug Builds
# for Windows 2003.
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
#   {Release/Debug}\InMageVssProvider.dll is generated for Windows 2003
#   
#


## Specify Compiler and Linker Executables
CC=cl.exe
LINK=link.exe
RC=rc.exe
MIDL=midl.exe

# Specify HeaderFiles (.h suffix)
HEADERS=_IVacpProviderEvents_CP.h dllmain.h resource.h stdafx.h targetver.h VacpProvider.h VacpProviderCommon.h

# Specify Source Files (.cpp suffix)
SRC=stdafx.cpp VacpProvider.cpp utility.cpp InMageVssProvider.cpp dllmain.cpp

# Specify Source Files (.c suffix)
INTER_SRC=InMageVssProvider_i.c

#Specify resource Files (.rc suffix)
RC_SOURCE=InMageVssProvider.rc

DEBUG_RCFLAGS=/fo Debug\InMageVssProvider.res /i ".\Debug"
RELEASE_RCFLAGS =/fo Release\InMageVssProvider.res /i ".\Release"

# Specify precompile header file
PCHFILE="stdafx.h"

# Specify Compilation Flags for Release build
RELEASE_CPPFLAGS=/O2 /D "WIN32" /D "_CONSOLE" /D "_CRT_SECURE_NO_WARNINGS" /D "_CRT_NONSTDC_NO_DEPRECATE" /D "_WIN32_WINNT=0x0502"\
                 /D "VSS_SERVER" /D "_ATL_STATIC_REGISTRY" /D "_USING_V110_SDK71_" /D "_USRDLL" /FD /EHsc /MT\
                 /GS  /W3 /nologo /c /Zc:wchar_t- /Wp64 /Zi /TP /guard:cf /Qspectre /DYNAMICBASE
                 
RELEASE_CFLAGS=/O2 /D "WIN32" /D "_CONSOLE" /D "_CRT_SECURE_NO_WARNINGS" /D "_CRT_NONSTDC_NO_DEPRECATE" /D "_WIN32_WINNT=0x0502"\
                 /D "VSS_SERVER" /D "_ATL_STATIC_REGISTRY" /D "_USING_V110_SDK71_" /D "_USRDLL" /FD /EHsc /MT\
                 /GS  /W3 /nologo /c /Zc:wchar_t- /Wp64 /Zi /TC /guard:cf /Qspectre /DYNAMICBASE

RELEASE_HOST_CPPFLAGS= /Fo"Release/"

# Specify Compilation Flags for Debug build
# resolved atl issue after removing this flag at compile time --> /D "_ATL_NO_DEBUG_CRT"
DEBUG_CPPFLAGS=/Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_CRT_SECURE_NO_WARNINGS" /D "_CRT_NONSTDC_NO_DEPRECATE"\
               /D "VSS_SERVER" /D "_USING_V110_SDK71_" /D "_USRDLL" /EHsc /RTC1 /MTd\
               /D "_ATL_STATIC_REGISTRY"  /W3 /nologo /c /Zc:wchar_t- /Wp64 /Zi /TP /guard:cf /Qspectre /DYNAMICBASE
              
DEBUG_CFLAGS=/Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_CRT_SECURE_NO_WARNINGS" /D "_CRT_NONSTDC_NO_DEPRECATE"\
               /D "VSS_SERVER"  /D "_USING_V110_SDK71_" /D "_USRDLL" /EHsc /RTC1 /MTd\
               /D "_ATL_STATIC_REGISTRY"  /W3 /nologo /c /Zc:wchar_t- /Wp64 /Zi /TC /guard:cf /Qspectre /DYNAMICBASE


DEBUG_HOST_CPPFLAGS= /Fo"Debug/"

CUR_DIR=..\InMageVssProvider

# Specify VSS SDK Libraries
VSS_LIBS=vssapi.lib vss_uuid.lib

#This debug flag gets enabled
ENABLE_DEBUG=

# Specify Windows Static libraries
OS_LIBS=rpcrt4.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
        advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
        odbccp32.lib ws2_32.lib 

# Specify Linking flags for FabricRelease build
RELEASE_LFLAGS=/INCREMENTAL:NO /NOLOGO /SUBSYSTEM:CONSOLE,5.01 /OPT:NOREF /OPT:ICF /DYNAMICBASE /NXCOMPAT\
               /MACHINE:X86 /MAP /MAPINFO:EXPORTS /DEBUG /DEBUGTYPE:CV,FIXUP /guard:cf

# Specify Linking flags for Debug build
DEBUG_LFLAGS=/INCREMENTAL:NO /NOLOGO /SUBSYSTEM:CONSOLE,5.01 /MACHINE:X86 /DEBUG /DYNAMICBASE /NXCOMPAT /guard:cf

WIN_2K3_VERSION=/D "_WIN32_WINNT=0x502"

# Compute Include directories for Windows XP/2003 Build

WIN2K3_INC_DIR= /I $(CUR_DIR)) /I "..\inmsafecapis" /I "..\inmsafecapis\win" /I "..\common"



# Compute Link flags for WIndows XP/2003 Release Build
RELEASE_WIN2K3_DLL_LFLAGS=$(RELEASE_LFLAGS) /PDB:"Release/InMageVssProvider.pdb"

# Compute Link flags for WIndows XP/2003 Debug Build
DEBUG_WIN2K3_DLL_LFLAGS=$(DEBUG_LFLAGS) /PDB:"Debug/InMageVssProvider.pdb"

#Generate both Release and debug builds
all: Release Debug

#Generate Release build for Windows XP/2003
Release: Release\vacprpc_c.obj Release\InMageVssProvider.dll

#Generate Debug build for Windows XP/2003
Debug:  Debug\vacprpc_c.obj Debug\InMageVssProvider.dll

# vacprpcs stub file
Release\vacprpc_c.obj: ..\vacp\vacprpc\vacprpc.idl ..\vacp\vacprpc\vacprpc.acf
	@echo ---------------------------------------------------------
	@echo Generating Release build(Release\vacprpc_c.obj)...
	@echo ---------------------------------------------------------
	DEL vacprpc*.*
	@$(MIDL) /Oicf /acf ..\vacp\vacprpc\vacprpc.acf ..\vacp\vacprpc\vacprpc.idl
	@$(CC) $(RELEASE_CPPFLAGS) $(RELEASE_HOST_CPPFLAGS) $(WIN_2K3_VERSION) $(WIN2K3_INC_DIR) vacprpc_c.c

Debug\vacprpc_c.obj: ..\vacp\vacprpc\vacprpc.idl ..\vacp\vacprpc\vacprpc.acf
	@echo ---------------------------------------------------------
	@echo Generating Debug build(Debug\vacprpc_c.obj)...
	@echo ---------------------------------------------------------
	DEL vacprpc*.*
	@$(MIDL) /Oicf /acf ..\vacp\vacprpc\vacprpc.acf ..\vacp\vacprpc\vacprpc.idl
	@$(CC) $(DEBUG_CPPFLAGS) $(DEBUG_HOST_CPPFLAGS) $(WIN_2K3_VERSION) $(WIN2K3_INC_DIR) vacprpc_c.c

#Generate Debug build for Windows 2003
Debug\InMageVssProvider.dll: $(HEADERS) $(SRC) Makefile 
	@echo ---------------------------------------------------------
	@echo Generating Windows 2003 Debug build(Debug\InMageVssProvider.dll)...
	@echo ---------------------------------------------------------
	@IF NOT EXIST Debug MD Debug
	@$(MIDL) InMageVssProvider.idl /D "_DEBUG" /nologo /char signed /env win32 /Oicf /tlb "Debug\InMageVssProvider.tlb" /h "InMageVssProvider_i.h" /iid "InMageVssProvider_i.c" /proxy "InMageVssProvider_p.c" /error stub_data
	@$(CC) $(DEBUG_CPPFLAGS) /D "ACE_AS_STATIC_LIBS"  $(DEBUG_HOST_CPPFLAGS) $(WIN_2K3_VERSION) $(WIN2K3_INC_DIR) $(SRC)
	@$(CC) $(DEBUG_CFLAGS) /D "ACE_AS_STATIC_LIBS"  $(DEBUG_HOST_CPPFLAGS) $(WIN_2K3_VERSION) $(WIN2K3_INC_DIR) $(INTER_SRC)
	@$(RC) $(DEBUG_RCFLAGS) /d "InMageVssProvider_DLL" $(RC_SOURCE)
	@$(LINK) $(DEBUG_WIN2K3_DLL_LFLAGS) /DEF:InMageVssProvider.def /DLL /implib:Debug\InMageVssProvider.lib /OUT:Debug\InMageVssProvider.dll Debug\*.obj Debug\*.res $(VSS_LIBS) $(OS_LIBS)


#Generate Release build for Windows 2003
Release\InMageVssProvider.dll: $(HEADERS) $(SRC) Makefile 
	@echo -------------------------------------------------------------
	@echo Generating Windows 2003 Release build(Release\InMageVssProvider.dll)...
	@echo -------------------------------------------------------------
	@IF NOT EXIST Release MD Release
	@$(MIDL) InMageVssProvider.idl /D "NDEBUG" /nologo /char signed /env win32 /Oicf /tlb "Release\InMageVssProvider.tlb" /h "InMageVssProvider_i.h" /iid "InMageVssProvider_i.c" /proxy "InMageVssProvider_p.c" /error stub_data
	@$(CC) $(RELEASE_CPPFLAGS) /D "ACE_AS_STATIC_LIBS" $(RELEASE_HOST_CPPFLAGS) $(WIN_2K3_VERSION) $(WIN2K3_INC_DIR) $(SRC)
	@$(CC) $(RELEASE_CFLAGS) /D "ACE_AS_STATIC_LIBS" $(RELEASE_HOST_CPPFLAGS) $(WIN_2K3_VERSION) $(WIN2K3_INC_DIR) $(INTER_SRC)
	@$(RC) $(RELEASE_RCFLAGS) /d "InMageVssProvider_DLL" $(RC_SOURCE)
	@$(LINK) $(RELEASE_WIN2K3_DLL_LFLAGS) /DEF:InMageVssProvider.def /DLL /implib:release\InMageVssProvider.lib /OUT:Release\InMageVssProvider.dll Release\*.obj Release\*.res $(VSS_LIBS) $(OS_LIBS)

#Clean both Release and Debug builds
clean:
	@echo ----------------------------------------------------
	@echo Cleaning Windows 2000/XP/2003 Release,Debug builds...
	@echo ----------------------------------------------------
	@IF EXIST Release DEL /Q Release\*.*
	@IF EXIST Debug DEL /Q Debug\*.*
	DEL vacprpc*.*

#Clean both build only
clean_release:
	@echo ----------------------------------------------------
	@echo Cleaning Windows 2000/XP/2003 Release build...
	@echo ----------------------------------------------------
	@IF EXIST Release DEL /Q Release\*.*
	DEL vacprpc*.*

#Clean build only
clean_debug:
	@echo ----------------------------------------------------
	@echo Cleaning Windows 2000/XP/2003 Debug build...
	@echo ----------------------------------------------------
	@IF EXIST Debug DEL /Q Debug\*.*
	DEL vacprpc*.*



