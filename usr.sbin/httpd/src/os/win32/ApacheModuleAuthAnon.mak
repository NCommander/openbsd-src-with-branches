# Microsoft Developer Studio Generated NMAKE File, Based on ApacheModuleAuthAnon.dsp
!IF "$(CFG)" == ""
CFG=ApacheModuleAuthAnon - Win32 Release
!MESSAGE No configuration specified. Defaulting to ApacheModuleAuthAnon - Win32\
 Release.
!ENDIF 

!IF "$(CFG)" != "ApacheModuleAuthAnon - Win32 Release" && "$(CFG)" !=\
 "ApacheModuleAuthAnon - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ApacheModuleAuthAnon.mak"\
 CFG="ApacheModuleAuthAnon - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ApacheModuleAuthAnon - Win32 Release" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE "ApacheModuleAuthAnon - Win32 Debug" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ApacheModuleAuthAnon - Win32 Release"

OUTDIR=.\ApacheModuleAuthAnonR
INTDIR=.\ApacheModuleAuthAnonR
# Begin Custom Macros
OutDir=.\.\ApacheModuleAuthAnonR
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\ApacheModuleAuthAnon.dll"

!ELSE 

ALL : "$(OUTDIR)\ApacheModuleAuthAnon.dll"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\mod_auth_anon.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\ApacheModuleAuthAnon.dll"
	-@erase "$(OUTDIR)\ApacheModuleAuthAnon.exp"
	-@erase "$(OUTDIR)\ApacheModuleAuthAnon.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "..\..\include" /D "NDEBUG" /D "WIN32" /D\
 "_WINDOWS" /D "SHARED_MODULE" /Fp"$(INTDIR)\ApacheModuleAuthAnon.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\ApacheModuleAuthAnonR/
CPP_SBRS=.
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\ApacheModuleAuthAnon.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=..\..\CoreR\ApacheCore.lib kernel32.lib user32.lib gdi32.lib\
 winspool.lib comdlg32.lib advapi32.lib shell32.lib /nologo /subsystem:windows\
 /dll /incremental:no /pdb:"$(OUTDIR)\ApacheModuleAuthAnon.pdb" /machine:I386\
 /out:"$(OUTDIR)\ApacheModuleAuthAnon.dll"\
 /implib:"$(OUTDIR)\ApacheModuleAuthAnon.lib" 
LINK32_OBJS= \
	"$(INTDIR)\mod_auth_anon.obj"

"$(OUTDIR)\ApacheModuleAuthAnon.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "ApacheModuleAuthAnon - Win32 Debug"

OUTDIR=.\ApacheModuleAuthAnonD
INTDIR=.\ApacheModuleAuthAnonD
# Begin Custom Macros
OutDir=.\.\ApacheModuleAuthAnonD
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\ApacheModuleAuthAnon.dll"

!ELSE 

ALL : "$(OUTDIR)\ApacheModuleAuthAnon.dll"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\mod_auth_anon.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\vc50.pdb"
	-@erase "$(OUTDIR)\ApacheModuleAuthAnon.dll"
	-@erase "$(OUTDIR)\ApacheModuleAuthAnon.exp"
	-@erase "$(OUTDIR)\ApacheModuleAuthAnon.ilk"
	-@erase "$(OUTDIR)\ApacheModuleAuthAnon.lib"
	-@erase "$(OUTDIR)\ApacheModuleAuthAnon.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MDd /W3 /Gm /GX /Zi /Od /I "..\..\include" /D "_DEBUG" /D\
 "WIN32" /D "_WINDOWS" /D "SHARED_MODULE"\
 /Fp"$(INTDIR)\ApacheModuleAuthAnon.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\"\
 /FD /c 
CPP_OBJS=.\ApacheModuleAuthAnonD/
CPP_SBRS=.
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\ApacheModuleAuthAnon.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=..\..\CoreD\ApacheCore.lib kernel32.lib user32.lib gdi32.lib\
 winspool.lib comdlg32.lib advapi32.lib shell32.lib /nologo /subsystem:windows\
 /dll /incremental:yes /pdb:"$(OUTDIR)\ApacheModuleAuthAnon.pdb" /debug\
 /machine:I386 /out:"$(OUTDIR)\ApacheModuleAuthAnon.dll"\
 /implib:"$(OUTDIR)\ApacheModuleAuthAnon.lib" 
LINK32_OBJS= \
	"$(INTDIR)\mod_auth_anon.obj"

"$(OUTDIR)\ApacheModuleAuthAnon.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(CFG)" == "ApacheModuleAuthAnon - Win32 Release" || "$(CFG)" ==\
 "ApacheModuleAuthAnon - Win32 Debug"
SOURCE=..\..\modules\standard\mod_auth_anon.c

!IF  "$(CFG)" == "ApacheModuleAuthAnon - Win32 Release"

DEP_CPP_MOD_A=\
	"..\..\include\alloc.h"\
	"..\..\include\ap.h"\
	"..\..\include\ap_mmn.h"\
	"..\..\include\buff.h"\
	"..\..\include\conf.h"\
	"..\..\include\hsregex.h"\
	"..\..\include\http_config.h"\
	"..\..\include\http_core.h"\
	"..\..\include\http_log.h"\
	"..\..\include\http_protocol.h"\
	"..\..\include\http_request.h"\
	"..\..\include\httpd.h"\
	"..\..\include\util_uri.h"\
	".\os.h"\
	".\readdir.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_MOD_A=\
	"..\..\include\ebcdic.h"\
	"..\..\include\os.h"\
	"..\..\include\sfio.h"\
	

"$(INTDIR)\mod_auth_anon.obj" : $(SOURCE) $(DEP_CPP_MOD_A) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "ApacheModuleAuthAnon - Win32 Debug"

DEP_CPP_MOD_A=\
	"..\..\include\alloc.h"\
	"..\..\include\ap.h"\
	"..\..\include\ap_mmn.h"\
	"..\..\include\buff.h"\
	"..\..\include\conf.h"\
	"..\..\include\hsregex.h"\
	"..\..\include\http_config.h"\
	"..\..\include\http_core.h"\
	"..\..\include\http_log.h"\
	"..\..\include\http_protocol.h"\
	"..\..\include\http_request.h"\
	"..\..\include\httpd.h"\
	"..\..\include\util_uri.h"\
	".\os.h"\
	".\readdir.h"\
	

"$(INTDIR)\mod_auth_anon.obj" : $(SOURCE) $(DEP_CPP_MOD_A) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

