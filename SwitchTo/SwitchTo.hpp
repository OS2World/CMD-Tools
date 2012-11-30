/***********************************************************************\
 *                            SwitchTo.cpp                             *
 *                 Copyright (C) by Stangl Roman, 1998                 *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
 * SwitchTo.hpp SwitchTo/2 header file.                                *
 *                                                                     *
\***********************************************************************/

#define     INCL_DOSMISC
#define     INCL_DOSERRORS
#define     INCL_DOSPROCESS
#define     INCL_DOSSESMGR
#define     INCL_DOSQUEUES
#define     INCL_DOSMODULEMGR
#define     INCL_WINPROGRAMLIST
#define     INCL_WINWINDOWMGR
#define     INCL_WINDIALOGS
#define     INCL_WINERRORS
#define     INCL_SHLERRORS
#define     INCL_WINSWITCHLIST
#include    <os2.h>

#define NLSMSG_COPYRIGHT1               1
#define NLSMSG_COPYRIGHT2               2
#define NLSMSG_USAGE1                   3
#define NLSMSG_USAGE2                   4
#define NLSMSG_USAGE3                   5
#define NLSMSG_INVOKE1                  6
#define NLSMSG_INVOKE2                  7
#define NLSMSG_INVOKE3                  8
#define NLSMSG_INVOKE4                  9
#define NLSMSG_INVOKE5                  10

#define ERR_NOERROR                     0
#define ERR_SYNTAX                      1
#define ERR_SWITCHFAILED                2
#define ERR_READINGQUEUE                3

                                        /* STDOUT file handle is numerically 1 for all
                                           OS/2 processes */
#define STDOUT                          0x00000001

#define SWITCHTO_QUEUE                  "\\QUEUES\\SWITCHTO\\SELFPM.QUE"

                                        /* RESULTCODES is already defined in BASEDEF.H
                                           (used only for VDM support) and BSEDOS.H, but
                                           in the later header with ULONG members not
                                           USHORT which the Session Manager will return
                                           in the queue when the started session terminates */                    
typedef struct RESULTCODE
{
    USHORT              usCodeTerminate;
    USHORT              usCodeResult;
};

/****************************************************************************************\
 * Class: SwitchTo                                                                      *
\****************************************************************************************/
class   SwitchTo
{
public:
                        SwitchTo(int argc, char *argv[]);
protected:
    SwitchTo           &checkEnvironment(void);
#ifdef  CHANGETYPE
    SwitchTo           &setProcessType(ULONG ulProcessType);
#endif  /* CHANGETYPE */
    SwitchTo           &displayMessage(int iMessage, char *pcParameter=0);
#ifdef  LAUNCHCOPY
    SwitchTo           &relaunchSelf(void);
#endif  /* LAUNCHCOPY */
    ULONG               switchtoApplication(void);

public:
    int                 iResult;
protected:
    ULONG               ulProcessType;
    char               *pcSessionName;
    char                acPathExecutable[CCHMAXPATH];
    char                acPathMessage[CCHMAXPATH];

    HAB                 hab;
    HMQ                 hmq;
                                        /* At the moment we don't use all of that, but ... */
    HWND                hwndFrame;
    HWND                hwndClient;
};

extern "C"
{
    long time(long *ptimer);
}

