#
#                            SwitchTo.cpp
#                 Copyright (C) by Stangl Roman, 1998
# This Code may be freely distributed, provided the Copyright isn't
# removed, under the conditions indicated in the documentation.
#
# Makefile     SwitchTo/2 makefile
#
# Requires     IBM VisualAge C++ 3.0
#              OS/2 Warp Toolkit
#

!ifndef NODEBUG
CFLAGS = /Ti /Tm /DDEBUG
LFLAGS = /Debug
!else
CFLAGS = /O /Oc
LFLAGS =
!endif # NODEBUG

!ifdef LAUNCHCOPY
RUNMODE = /DLAUNCHCOPY
!ifndef NODEBUG
LOBJECT = +printf.obj
!endif  # NODEBUG
!endif  # LAUNCHCOPY

!ifdef CHANGETYPE
RUNMODE = /DCHANGETYPE
!endif  # CHANGETYPE

All: SwitchTo.exe

SW2Us.msg : SW2Us.txt
    MKMSGF SW2Us.txt SW2Us.msg

SW2Gr.msg : SW2Gr.txt
    MKMSGF SW2Gr.txt SW2Gr.msg

SwitchTo.msg : SW2Us.msg SW2Gr.msg
    Copy SW2Us.msg SwitchTo.msg /v

SwitchTo.obj : SwitchTo.cpp SwitchTo.hpp Makefile
    icc /Tdp /Rn /c $(CFLAGS) $(RUNMODE) SwitchTo.cpp /FoSwitchTo.obj

SwitchTo.exe : SwitchTo.obj SwitchTo.msg SwitchTo.def
    ilink /Nofree /Nod /Map $(LFLAGS) SwitchTo.obj $(LOBJECT),SwitchTo.exe,SwitchTo.map,CPPON30+OS2386,SwitchTo.def

