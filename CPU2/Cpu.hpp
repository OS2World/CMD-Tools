/***********************************************************************\
 *                                CPU/2                                *
 *    Copyright (C) by Roman Stangl (Roman_Stangl@at.ibm.com), 1998    *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
\***********************************************************************/

#ifndef __CPU_HPP__
#define __CPU_HPP__

#define     INCL_DOSFILEMGR
#define     INCL_DOSMISC
#define     INCL_DOSDEVICES
#define     INCL_DOSDEVIOCTL
#define     INCL_DOSERRORS
#define     INCL_DOSPROCESS
#define     INCL_DOSMODULEMGR
#define     INCL_DOSNLS
#define     INCL_DOSDATETIME
#define     INCL_KBD
#include    <os2.h>

#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "Cpu.h"

/*--------------------------------------------------------------------------------------*\
 *                                                                                      *
 * Functional specification of UP and MP CPU Utility/2:                                 *
 *                                                                                      *
 * ------------------------------------------------------------------------------------ *
 *                                                                                      *
 * 1. Option /Verbose                                                                   *
 *                                                                                      *
 *    This option uses the OS/2 APIs DosQuerySysInfo(QSV_NUMPROCESSORS) and             *
 *    DosGetProcessorStatus() which have been introduced with Warp Server Advanced SMP  *
 *    to query the number of installed (better enabled from BIOS) processors and to     *
 *    display their status, which can either be Offline or Online.                      *
 *                                                                                      *
 *    On non SMP OS/2's, this will likely fail (at least up to Warp 4 "Merlin", maybe   *
 *    this will change with a future Warp 5 client based on the "Aurora" server).       *
 *                                                                                      *
 * ------------------------------------------------------------------------------------ *
 *                                                                                      *
 * 2. Options /Online and /Offline                                                      *
 *                                                                                      *
 *    Quite similar to /Verbose, DosQuerySysInfo(), DosGetProcessorStatus() and also    *
 *    DosSetProcessorStatus() are used to set a selected processor to Offline or        *
 *    Online. Note that processor 1 can't be set to offline (though the term            *
 *    "symmetrical" from SMP would imply that) I'm afraid the current Intel based       *
 *    multiprocessor specification APIC 1.4 defines CPU 1 as a special one which must   *
 *    run in order for the whole PC to run (I suppose that this is the only CPU that    *
 *    can process certain hardware interrupts).                                         *
 *                                                                                      *
 * ------------------------------------------------------------------------------------ *
 *                                                                                      *
 * 3. Option /Performance                                                               *
 *                                                                                      *
 *    This option uses the new OS/2 API DosPerfSysCall() which had been made available  *
 *    by Warp 4 and likely recent Warp 3 fixpacks. The API returns counters that count  *
 *    the total, idle, busy and interrupt time (though I haven't seen any documentation *
 *    I'm sure that it is based on the Pentium hardware counters, thus will only be     *
 *    available beginning with Pentium class machines.                                  *
 *                                                                                      *
 *    What this option displays is the relative amounts of idle, busy and interrupt     *
 *    times of the total performance per processor (that is, it will work with just 1   *
 *    processor). The relative amount is an average of the last PERFORMANCE_STACK       *
 *    measurements (so that temporary spikes will be "softened").                       *
 *                                                                                      *
 *    The key structure for that is pperformanceStack which will be allocated dynamic-  *
 *    ally in the following form:                                                       *
 *                                                                                      *
 *    Stackpointer:         CPU 1       CPU 2       ...         CPU n                   *
 *                        +-------+   +-------+   +-------+   +-------+                 *
 *    0                   !       !   !       !   !       !   !       !  ^              *
 *                        +-------+   +-------+   +-------+   +-------+  !              *
 *                        +-------+   +-------+   +-------+   +-------+  !              *
 *    1                   !       !   !       !   !       !   !       !  !              *
 *                        +-------+   +-------+   +-------+   +-------+  !              *
 *                        +-------+   +-------+   +-------+   +-------+  !              *
 *    ...                 !       !   !       !   !       !   !       !  !              *
 *                        +-------+   +-------+   +-------+   +-------+  !              *
 *                        +-------+   +-------+   +-------+   +-------+  !              *
 *    PERFORMANCE_STACK-1 !       !   !       !   !       !   !       !  !              *
 *                        +-------+   +-------+   +-------+   +-------+                 *
 *                                                                                      *
 *    The measurements will move upwards before the new current measurement is queried  *
 *    at PERFORMANCE_STACK-1 (except after the 1st measurment, in which case that one   *
 *    will be copied into all previous stack elements, otherwise we would include some  *
 *    0's into the average.                                                             *
 *                                                                                      *
 *    The average is calculate by summing up all entries for one CPU and then divide by *
 *    the number of stack elements. This average ensures that temporary spikes get      *
 *    softened.                                                                         *
 *                                                                                      *
 *    One measurement, which consists of the percentage the idle, busy or interrupt     *
 *    time spent by one CPU is done by letting the counters run for 1 second and then   *
 *    comparing how much the idle, busy and interrupt counters have increased compared  *
 *    to the increase in the total counter (in other words the available amount of      *
 *    total counts per period is "divided" between the idle, busy and interrupt         *
 *    counters and we just reverse that process). The pcticksCurrent and                *
 *    pcticksPrevious structures contain the 64-bit CPU counters converted to doubles   *
 *    (as VisaulAge C++ 3 does not yet support 64-bit integer arithmetic).              *
 *                                                                                      *
 *    The measurement is terminated only when the user presses the Space Bar.           *
 *                                                                                      *
\*--------------------------------------------------------------------------------------*/
                                        /* Define OS/2 major and minor versions */
#define         OS2_MAJOR                   0x00000014
#define         OS2_MINOR_200               0x00000000
#define         OS2_MINOR_210               0x0000000A
#define         OS2_MINOR_211               0x0000000B
#define         OS2_MINOR_300               0x0000001E
#define         OS2_MINOR_400               0x00000028

                                        /* NLS Message file messages */
#define     MSG_NLSMSG_NOT_EXIST        1 
#define     MSG_NLSMSG_INVALID          2 
#define     MSG_LOGO                    3 
#define     MSG_DESCRIPTION             4 
#define     MSG_PARAMETERS_MISSING      5 
#define     MSG_PARAMETERS_INVALID      6 
#define     MSG_PARAMETERS_TEXT         7 
#define     MSG_REQUEST_MISSING         8 
#define     MSG_REQUEST_BAD             9 
#define     MSG_VERBOSE_KERNELAPI       10
#define     MSG_VERBOSE_PERFSYSCALL     11
#define     MSG_VERBOSE_ERROR_SYSINFO   12
#define     MSG_VERBOSE_STATUS_HEADER   13
#define     MSG_VERBOSE_STATUS_ONLINE   14
#define     MSG_VERBOSE_STATUS_OFFLINE  15
#define     MSG_NEWLINE                 16
#define     MSG_VERBOSE_STATUS_ERROR    17
#define     MSG_VERBOSE_STATUS_INVALID  18
#define     MSG_ONOFFLINE_ERROR_RANGE   19
#define     MSG_ONOFFLINE_RANGE         2á
#define     MSG_ONOFFLINE_SET_RETRY     21
#define     MSG_ONOFFLINE_SET_ERROR     22
#define     MSG_ONOFFLINE_SET_DENIED    23
#define     MSG_PERFORMANCE_HEADER      24
#define     MSG_PERFORMANCE_CPU         25
#define     MSG_PERFORMANCE_FOOTER      26
#define     MSG_PERFSYSRESULT           27

struct  UCountryTranslate
{
    ULONG   ulCountry;
    char    acCountry[3];
};

struct  UOption
{
    int     iStatusFlag;
    char    acOption[16];
};

struct  UPerformance
{
    ULONG   ulIdle;
    ULONG   ulBusy;
    ULONG   ulIntr;
};

struct  UTicks
{
    double  dTotal;
    double  dIdle;
    double  dBusy;
    double  dIntr;
};

/****************************************************************************************\
 * Class: UCpu                                                                          *
\****************************************************************************************/

class   UCpu
{
public:
                        UCpu(int iArgc, char *apcArgv[]);
                       ~UCpu(void);
                                        /* Initialize after construction */
    UCpu               &initialize(void);
                                        /* Process what we were designed to do */
    UCpu               &process(void);
protected:
                                        /* Match commandline parameters */
    UCpu               &matchParameters(void);
                                        /* Process commandline parameters */
    UCpu               &processParameters(void);
                                        /* Dynamically link OS/2 API */
    UCpu               &loadProcessorStatusAPI(void);
                                        /* Display a message from the message file */
    UCpu               &displayMessage(int iMessage, int iParameterCount=0, int iParam1=0, int iParam2=0, 
                                                                            int iParam3=0, int iParam4=0);
                                        /* Check for commandline option */
    char               *checkCommandlineOption(char *pcOption, BOOL bCompositeOption=FALSE, BOOL bCaseSensitive=FALSE);
                                        /* Convert a hex decimal into a BCD decimal */
    ULONG               getAsBCD(ULONG ulBinary);
public:
                                        /* Highest return code */
#define     ERROR_NLSMSG_NOT_EXIST      1L
#define     ERROR_NLSMSG_INVALID        2L
#define     ERROR_PARAMETERS_MISSING    5L
#define     ERROR_PARAMETERS_INVALID    6L
#define     ERROR_REQUEST_MISSING       8L
#define     ERROR_REQUEST_BAD           9L
#define     ERROR_KERNELAPI             10L
#define     ERROR_PERFSYSCALL           11L
#define     ERROR_SYSINFO               12L
#define     ERROR_STATUS                17L
#define     ERROR_STATUS_INVALID        18L
#define     ERROR_ONOFFLINE_RANGE       20L
#define     ERROR_SET_RETRY             21L
#define     ERROR_PERFSYSRESULT         27L

    int                 iReturnCode;
protected:
                                        /* Commandline arguments */
    int                 iArgc;
    char              **ppcArgv;
                                        /* Commandline parameters */
    char               *pcCommandline;
                                        /* Status flags, from a group of flags (usually
                                           nibbles) only one can be set or a syntax error
                                           will happen */
#define     FLAG_NULL                   0x00000000L
#define     FLAG_SYNTAXERROR            0xFFFFFFFFL
#define     FLAG_SHOWCONFIGURATION      0x00000001L

#define     FLAG_MASK_REQUEST           0x000000F0L
#define     FLAG_CPUONLINE              0x00000010L
#define     FLAG_CPUOFFLINE             0x00000020L
#define     FLAG_PERFORMANCE            0x00000040L

#define     FLAG_NLS_MESSAGES_REQUESTED 0x01000000L
#define     FLAG_NLS_MESSAGES_FOUND     0x02000000L
    int                 iStatusFlag;
                                        /* Fully qualified path to message file which is
                                           expected to be in the directory CPU/2 was
                                           launched from */
    char                acMessageFileUs[CCHMAXPATH];
    char                acMessageFileNls[CCHMAXPATH];
                                        /* Message buffer */
#define     MESSAGEBUFFER_LENGTH        4096
    char                acMessageBuffer[MESSAGEBUFFER_LENGTH];
                                        /* OS/2 major and minor version */
    ULONG               ulVersion[2];
private:
                                        /* OS/2 kernel entrypoints */
    HMODULE             hmoduleDOSCALL1;
                                        /* Dynamic linking of DosGetProcessorStatus()
                                           (DOSCALL1.447) */
    GETPROCESSORSTATUS *pfnDosGetProcessorStatus;
                                        /* Dynamic linking of DosSetProcessorStatus()
                                           (DOSCALL1.448) */
    SETPROCESSORSTATUS *pfnDosSetProcessorStatus;
                                        /* Dynamic linking of DosPerfSysCall()
                                           (DOSCALL1.976) */
    DOSPERFSYSCALL     *pfnDosPerfSysCall;
                                        /* DosPerfSysCall() results (per processor) */
    CPUUTIL            *pcpuutilCurrent;
    CPUUTIL            *pcpuutilPrevious;
                                        /* Timer ticks (per processor) */
    UTicks             *puticksCurrent;
    UTicks             *puticksPrevious;
                                        /* Performance history (per processor) */
#define     PERFORMANCE_STACK           5
    UPerformance       *puperformanceStack;
};


#endif  /* __CPU_HPP__ */
