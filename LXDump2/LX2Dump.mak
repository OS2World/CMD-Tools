All: LX2Dump.exe

CC          = icc
BLDLEVEL    = BuildLevel.cmd
!ifndef NODEBUG
CFLAGS      = /ss /Ti /DDEBUG
!else
CFLAGS = /ss
!endif  # NODEBUG

LX2Dump.exe : LX2Dump.c LX2Dump.h LX2Dump.def
    $(BLDLEVEL) LX2Dump.def LX2Dump.h
    $(CC) $(CFLAGS) LX2Dump.c LX2Dump.def

