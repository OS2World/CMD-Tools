#===================================================================
#
#                           XComp/2 Makefile
#
#               Copyright (C) by Stangl Roman, 1999, 2002
#  This Code may be freely distributed, provided the Copyright isn't
#  removed, under the conditions indicated in the documentation.
#
#   supports: Compiler: IBM VisualAge C++ V3.6.5 FP2
#                       (req. due to 64-bit integer support)
#             Toolkit : IBM VisualAge C++ V3.6.5 (includes MS SDK)
#
#===================================================================

# ICC Flags
#   /Tdp    C++
#   /Ti     Include debug information for IPMD
#   /Kb     Warning, if no prototypes found (prevents from incorrect params)
#   /c      Compile only, we link more than one ressource
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

SRC=..

!ifndef NODEBUG
CFLAGS = /Tdp /Ti             /Tm /Se /Ms /Re /Gm /ss /W3 /G5 /DDEBUG
!else
CFLAGS = /Tdp     /Gs+ /O /Oc     /Se /Ms /Re /Gm /ss /W3 /G5
!endif # NODEBUG

CC = icc $(CFLAGS)

all: XComp.exe IsoComp.exe Sys.exe

clean:
    -@del *.exe
    -@del *.obj
    -@del *.map

UExcp.obj: $(SRC)\UExcp.cpp $(SRC)\UExcp.hpp
    $(CC) /c $(SRC)\UExcp.cpp

UProfiling.obj: $(SRC)\UProfiling.cpp $(SRC)\UProfiling.hpp
    $(CC) /c $(SRC)\UProfiling.cpp

UCrc32.obj: $(SRC)\UCrc32.cpp $(SRC)\UCrc32.hpp
    $(CC) /c $(SRC)\UCrc32.cpp

UMd5.obj: $(SRC)\UMd5.cpp $(SRC)\UMd5.hpp
    $(CC) /c $(SRC)\UMd5.cpp

XComp.obj: $(SRC)\XComp.cpp $(SRC)\XComp.hpp $(SRC)\UExcp.hpp $(SRC)\Uprofiling.hpp $(SRC)\UCrc32.hpp $(SRC)\UMd5.hpp 
    $(CC) /c $(SRC)\XComp.cpp

XComp.exe: XComp.obj UExcp.obj UProfiling.obj UCrc32.obj UMd5.obj
    $(CC) XComp.obj UExcp.obj UProfiling.obj UCrc32.obj UMd5.obj

IsoComp.obj: $(SRC)\IsoComp.cpp $(SRC)\IsoComp.hpp $(SRC)\UExcp.hpp $(SRC)\Uprofiling.hpp $(SRC)\UCrc32.hpp $(SRC)\UMd5.hpp 
    $(CC) /c $(SRC)\IsoComp.cpp

IsoComp.exe: IsoComp.obj UExcp.obj UProfiling.obj UCrc32.obj UMd5.obj
    $(CC) IsoComp.obj UExcp.obj UProfiling.obj UCrc32.obj UMd5.obj

Sys.exe: Sys.c
    icc /Rn Sys.c

