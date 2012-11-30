/***********************************************************************\
 *                                CPU/2                                *
 *    Copyright (C) by Roman Stangl (Roman_Stangl@at.ibm.com), 1998    *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
\***********************************************************************/

#ifndef __CPU_H__
#define __CPU_H__

#pragma pack(1)
#pragma pack()

                                        /* If SMP Toolkit is not included, define: */
#ifndef QSV_NUMPROCESSORS
#define QSV_NUMPROCESSORS               26
#endif  /* QSV_NUMPROCESSORS */

                                        /* Not defined in SMP or Warp Toolkit */
#ifndef ORDDOSGETPROCESSORSTATUS
#define ORDDOSGETPROCESSORSTATUS        447
#endif  /* ORDDOSGETPROCESSORSTATUS */

#ifndef ORDDOSSETPROCESSORSTATUS
#define ORDDOSSETPROCESSORSTATUS        448
#define PROCESSOR_OFFLINE               0
#define PROCESSOR_ONLINE                1
#endif  /* ORDDOSSETPROCESSORSTATUS */

#ifndef ORDDOSPERFSYSCALL
#define ORDDOSPERFSYSCALL               976
#define CMD_KI_ENABLE                   0x60
#define CMD_KI_KLRDCNT                  0x63
#define CMD_KI_SOFTTRACE_LOG            0x14
#endif  /* ORDDOSPERFSYSCALL */

#ifndef CPUUTIL
typedef struct _CPUUTIL
{
    ULONG   ulTotalLow;
    ULONG   ulTotalHigh;
    ULONG   ulIdleLow;
    ULONG   ulIdleHigh;
    ULONG   ulBusyLow;
    ULONG   ulBusyHigh;
    ULONG   ulIntrLow;
    ULONG   ulIntrHigh;
} CPUUTIL, *PCPUUTIL;
#endif  /* CPUUTIL */

                                        /* Convert 8-byte (low, high) time value to double */
#define LL2F(high, low) (4294967296.0*(high)+(low))

APIRET APIENTRY DosGetProcessorStatus(ULONG ulProcNum, PULONG pulStatus);
APIRET APIENTRY DosSetProcessorStatus(ULONG ulProcNum, ULONG ulStatus);
APIRET APIENTRY DosPerfSysCall(ULONG ulCommand, ULONG ulParm1, ULONG ulParm2, ULONG ulParm3);

typedef APIRET  (APIENTRY GETPROCESSORSTATUS)(ULONG ulProcNum, PULONG pulStatus);
typedef APIRET  (APIENTRY SETPROCESSORSTATUS)(ULONG ulProcNum, ULONG pulStatus);
typedef APIRET  (APIENTRY DOSPERFSYSCALL)(ULONG ulCommand, ULONG ulParm1, ULONG ulParm2, ULONG ulParm3);

#endif  /* __CPU_H__ */
