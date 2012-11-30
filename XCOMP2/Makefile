#===================================================================
#
#                           XComp/2 Makefile
#                          IsoComp/2 Makefile
#
#               Copyright (C) by Stangl Roman, 1999, 2002              
#  This Code may be freely distributed, provided the Copyright isn't
#  removed, under the conditions indicated in the documentation.
#
#   supports: Compiler: IBM VisualAge C++ V3.6.5 FP2
#                       (req. due to 64-bit integer support)
#             Toolkit : IBM Developers Toolkit 2.1
#                       IBM Warp 3 Developers Toolkit
#                       IBM Warp 4 Developers Toolkit
#                       IBM Warp 4.5 Developers Toolkit
#
#===================================================================

# ICC Flags
#   /Tdp    C++
#   /Ti     Include debug information for IPMD
#   /Kb     Warning, if no prototypes found (prevents from incorrect params)
#   /c      Compile only, we link more than one ressource
#   /Tm     Enable debug memory management support
#   /Se     Allow IBM C language extentions and migration
#   /Ms     Set the default linkage to _System
#   /Re     Produce Code in IBM C Set/2 Run Time environment
#   /Rn     No language environment. Allow system programming and subsystem development
#   /ss     Allow // as comments
#   /Gm     Link with multitasking libraries, because we're multithreaded
#   /Gn     Do not generate default libraries in object
#   /Ge-    Link with libraries that assume a DLL
#   /Gs     Remove stack probes
#   /O      Turn on code optimization (turns also function inlining on)
#   /Oi     Turn on function inlining
#   /Oc     Optimize for speed and size
#   /G5     Optimize for Pentium


!ifndef NODEBUG
CFLAGS = /Tdp /Ti             /Tm /Se /Ms /Re /Gm /ss /W3 /G5 /DDEBUG
!else
CFLAGS = /Tdp     /Gs+ /O /Oc     /Se /Ms /Re /Gm /ss /W3 /G5
!endif # NODEBUG


CC = icc $(CFLAGS)

all: XComp.exe IsoComp.exe

clean:
    -@del *.exe
    -@del *.obj
    -@del *.map
    -@del WIN32\*.exe
    -@del WIN32\*.obj
    -@del WIN32\*.map

save:
    zip -9 -r XComp *
    unzip -t XComp

UExcp.obj: UExcp.cpp UExcp.hpp
    $(CC) /c UExcp.cpp

UProfiling.obj: UProfiling.cpp UProfiling.hpp
    $(CC) /c UProfiling.cpp

UCrc32.obj: UCrc32.cpp UCrc32.hpp
    $(CC) /c UCrc32.cpp

UMd5.obj: UMd5.cpp UMd5.hpp
    $(CC) /c UMd5.cpp


XComp.obj: XComp.cpp XComp.hpp UExcp.hpp UProfiling.hpp UCrc32.hpp UMd5.hpp
    $(CC) /c XComp.cpp

XComp.exe: XComp.obj UExcp.obj UProfiling.obj UCrc32.obj UMd5.obj XComp.def
    $(CC) XComp.obj UExcp.obj UProfiling.obj UCrc32.obj UMd5.obj XComp.def

IsoComp.obj: IsoComp.cpp IsoComp.hpp UExcp.hpp UProfiling.hpp UCrc32.hpp UMd5.hpp
    $(CC) /c IsoComp.cpp

IsoComp.exe: IsoComp.obj UExcp.obj UProfiling.obj UCrc32.obj UMd5.obj IsoComp.def
    $(CC) IsoComp.obj UExcp.obj UProfiling.obj UCrc32.obj UMd5.obj IsoComp.def

