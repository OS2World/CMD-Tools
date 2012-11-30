/***********************************************************************\
 *                          UPROFILING class                           *
 *              Copyright (C) by Stangl Roman, 1996, 2002              *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
 * UProfiling.cpp Generic profiling code using OS/2's interface to the *
 *              PC architecture hardware timer running at 1.19Mhz.     *
 *                                                                     *
\***********************************************************************/

#pragma strings(readonly)

#ifdef  __WINDOWS__
#define     __WIN32__
#endif  /* __WINDOWS__ */

#ifdef  __OS2__
#define     INCL_DOSPROFILE
#include    <os2.h>
#endif  /* __OS2__ */

#ifdef  __WIN32__
#include    <windows.h>
#endif  /* __WIN32__ */  

#include    <stdio.h>
#include    <memory.h>
#include	<limits.h>

#include    "UProfiling.hpp"

/****************************************************************************************\
 * Class: UProfiling                                                                    *
\****************************************************************************************/

/*--------------------------------------------------------------------------------------*\
 * UProfiling : Constructor(s)                                                          *
\*--------------------------------------------------------------------------------------*/
UProfiling::UProfiling(void)
{
    ulTimeMicroSec=0;
    memset(&hrTimer, 0, sizeof(hrTimer));
    memset(acElapsedTime, '\0', sizeof(acElapsedTime));
}

/*--------------------------------------------------------------------------------------*\
 * UProfiling : Destructor                                                              *
\*--------------------------------------------------------------------------------------*/
UProfiling::~UProfiling(void)
{
}

/*--------------------------------------------------------------------------------------*\
 * UProfiling : start                                                                   *
 *              Start the profiling                                                     *
\*--------------------------------------------------------------------------------------*/
UProfiling             &UProfiling::start(void)
{
    ulTimeMicroSec=0;
    memset(&hrTimer, 0, sizeof(hrTimer));
                                        /* Get start time */
#ifdef  __OS2__
    DosTmrQueryTime(&hrTimer.qwTimerStart);
#endif  /* __OS2__ */
#ifdef  __WIN32__
    QueryPerformanceCounter(&hrTimer.liTimerStart);
#endif  /* __WIN32__ */
    return(*this);
}

/*--------------------------------------------------------------------------------------*\
 * UProfiling : stop                                                                    *
 *              Stop the profiling                                                      *
\*--------------------------------------------------------------------------------------*/
UProfiling             &UProfiling::stop(void)
{
    double              dTimeStart=0;
    double              dTimeStop=0;
                                        /* Timer ticks/second */
#ifdef  __OS2__
    LONG                lTimeMicroSec=0;
    ULONG               ulTimerFrequency=0;
#endif  /* __OS2__ */
#ifdef  __WIN32__
    double              dTimerFrequency=0;
    LARGE_INTEGER       liTimerFrequency;
#endif  /* __WIN32__ */

#ifdef  __OS2__
                                        /* Get stop time */
    DosTmrQueryTime(&hrTimer.qwTimerStop);
                                        /* Get timer resolution */
    DosTmrQueryFreq(&ulTimerFrequency);
#endif  /* __OS2__ */
#ifdef  __WIN32__
                                        /* Get stop time */
    QueryPerformanceCounter(&hrTimer.liTimerStop);
                                        /* Get timer resolution */
    QueryPerformanceFrequency(&liTimerFrequency);
#endif  /* __WIN32__ */
                                        /* Calculate elapsed time */
#ifdef  __OS2__
    dTimeStart=(double)ULONG_MAX+1;
    dTimeStart*=(double)hrTimer.qwTimerStart.ulHi;
    dTimeStart+=(double)hrTimer.qwTimerStart.ulLo;
    dTimeStop=(double)ULONG_MAX+1;
    dTimeStop*=(double)hrTimer.qwTimerStop.ulHi;
    dTimeStop+=(double)hrTimer.qwTimerStop.ulLo;
    lTimeMicroSec=(ULONG)(((dTimeStop-dTimeStart)/(double)ulTimerFrequency)*1000000);
                                        /* When accessing the timer with multiple
                                           threads, sometimes the start timestamp
                                           is higher than the stop timestamp. I
                                           have no idea why */
    if(lTimeMicroSec>0)
        ulTimeMicroSec=(ULONG)lTimeMicroSec;
    else
        ulTimeMicroSec=(ULONG)(lTimeMicroSec*(-1));
#endif  /* __OS2__ */
#ifdef  __WIN32__
    dTimeStart=(double)ULONG_MAX+1;
    dTimeStart*=(double)hrTimer.liTimerStart.HighPart;
    dTimeStart+=(double)hrTimer.liTimerStart.LowPart;
    dTimeStop=(double)ULONG_MAX+1;
    dTimeStop*=(double)hrTimer.liTimerStop.HighPart;
    dTimeStop+=(double)hrTimer.liTimerStop.LowPart;
    dTimerFrequency=(((double)liTimerFrequency.HighPart)*(1<<32))+
        (double)liTimerFrequency.LowPart;
    ulTimeMicroSec=(ULONG)(((dTimeStop-dTimeStart)/dTimerFrequency)*1000000);
#endif  /* __WIN32__ */
    memset(&hrTimer, 0, sizeof(hrTimer));
    return(*this);
}

/*--------------------------------------------------------------------------------------*\
 * UProfiling : printTime                                                               *
 *              Output the profiling result                                             *
\*--------------------------------------------------------------------------------------*/
UProfiling             &UProfiling::printTime(void)
{
    printf("UProfiling: Elapsed time %d us\n", ulTimeMicroSec);
    return(*this);
}

/*--------------------------------------------------------------------------------------*\
 * UProfiling : elapsedMicroSec                                                         *
 *              Return the elapsed time in microseconds                                 *
\*--------------------------------------------------------------------------------------*/
ULONG                   UProfiling::elapsedMicroSec(void)
{
    return(ulTimeMicroSec);
}

/*--------------------------------------------------------------------------------------*\
 * UProfiling : elapsedMilliSec                                                         *
 *              Return the elapsed time in milliseconds                                 *
\*--------------------------------------------------------------------------------------*/
ULONG                   UProfiling::elapsedMilliSec(void)
{
    return(ulTimeMicroSec/1000);
}

/*--------------------------------------------------------------------------------------*\
 * UProfiling : elapsed                                                                 *
 *              Return the elapsed time in microseconds as a string                     *
\*--------------------------------------------------------------------------------------*/
char                   *UProfiling::elapsed(void)
{
    sprintf(acElapsedTime, "%d æs", ulTimeMicroSec);
    return(acElapsedTime);
}

