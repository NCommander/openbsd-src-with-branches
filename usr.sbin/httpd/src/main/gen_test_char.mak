# Microsoft Developer Studio Generated NMAKE File, Based on gen_test_char.dsp
!IF "$(CFG)" == ""
CFG=gen_test_char - Win32 Debug
!MESSAGE No configuration specified. Defaulting to gen_test_char - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "gen_test_char - Win32 Release" && "$(CFG)" !=\
 "gen_test_char - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "gen_test_char.mak" CFG="gen_test_char - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "gen_test_char - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "gen_test_char - Win32 Debug" (based on\
 "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "gen_test_char - Win32 Release"

OUTDIR=.
INTDIR=.\Release
# Begin Custom Macros
OutDir=.
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\gen_test_char.exe"

!ELSE 

ALL : "$(OUTDIR)\gen_test_char.exe"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\gen_test_char.idb"
	-@erase "$(INTDIR)\gen_test_char.obj"
	-@erase "$(OUTDIR)\gen_test_char.exe"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MD /W3 /O2 /I "..\include" /I "..\os\win32" /D "WIN32" /D\
 "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\gen_test_char"\
 /FD /c 
CPP_OBJS=.\Release/
CPP_SBRS=.

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\gen_test_char.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=/nologo /subsystem:console /incremental:no\
 /pdb:"$(OUTDIR)\Release\gen_test_char.pdb" /machine:I386\
 /out:"$(OUTDIR)\gen_test_char.exe" 
LINK32_OBJS= \
	"$(INTDIR)\gen_test_char.obj"

"$(OUTDIR)\gen_test_char.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE=$(InputPath)
PostBuild_Desc=Create test_char.h
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\gen_test_char.exe"
   .\gen_test_char > test_char.h
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "gen_test_char - Win32 Debug"

OUTDIR=.
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\gen_test_char.exe"

!ELSE 

ALL : "$(OUTDIR)\gen_test_char.exe"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\gen_test_char.idb"
	-@erase "$(INTDIR)\gen_test_char.obj"
	-@erase "$(OUTDIR)\Debug\gen_test_char.pdb"
	-@erase "$(OUTDIR)\gen_test_char.exe"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MDd /W3 /GX /Zi /Od /I "..\include" /I "..\os\win32" /D\
 "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\gen_test_char" /FD /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\gen_test_char.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=/nologo /subsystem:console /incremental:no\
 /pdb:"$(OUTDIR)\Debug\gen_test_char.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)\gen_test_char.exe" 
LINK32_OBJS= \
	"$(INTDIR)\gen_test_char.obj"

"$(OUTDIR)\gen_test_char.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE=$(InputPath)
PostBuild_Desc=Create test_char.h
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\gen_test_char.exe"
   .\gen_test_char > test_char.h
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ENDIF 


!IF "$(CFG)" == "gen_test_char - Win32 Release" || "$(CFG)" ==\
 "gen_test_char - Win32 Debug"
SOURCE=.\gen_test_char.c
DEP_CPP_GEN_T=\
	"..\include\ap.h"\
	"..\include\ap_alloc.h"\
	"..\include\ap_config.h"\
	"..\include\ap_ctype.h"\
	"..\include\ap_ebcdic.h"\
	"..\include\ap_mmn.h"\
	"..\include\buff.h"\
	"..\include\hsregex.h"\
	"..\include\httpd.h"\
	"..\include\util_uri.h"\
	"..\os\win32\os.h"\
	"..\os\win32\readdir.h"\
	
NODEP_CPP_GEN_T=\
	"..\include\ap_config_auto.h"\
	"..\include\sfio.h"\
	

"$(INTDIR)\gen_test_char.obj" : $(SOURCE) $(DEP_CPP_GEN_T) "$(INTDIR)"



!ENDIF 

