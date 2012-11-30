#ifndef _UPROFILING_HPP_
#define _UPROFILING_HPP_

/***********************************************************************\
 *                          UPROFILING class                           *
 *              Copyright (C) by Stangl Roman, 1996, 2002              *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
 * UProfiling.hpp Generic profiling code using OS/2's interface to the *
 *              PC architecture hardware timer running at 1.19Mhz.     *
 *                                                                     *
\***********************************************************************/

#ifdef  __OS2__
typedef struct  _HRTIMER
{
    QWORD               qwTimerStart;
    QWORD               qwTimerStop;
} HRTIMER;
#endif  /* __OS2__ */

#ifdef  __WIN32__
typedef struct  _HRTIMER
{
    LARGE_INTEGER       liTimerStart;
    LARGE_INTEGER       liTimerStop;
} HRTIMER;
#endif  /* __WIN32__ */

#ifdef  __cplusplus
    extern "C" 
        {
#endif  /* __cplusplus */

                                        /* Call with HRTIMER initialized to 0 to
                                           start the timer, call with the previous
                                           HRTIMER structure to calculate the
                                           difference in microseconds */
extern ULONG    getTime(HRTIMER *pTimerTime);

#ifdef  __cplusplus
        }
#endif  /* __cplusplus */

/****************************************************************************************\
 * Class: UProfiling                                                                    *
\****************************************************************************************/

class   UProfiling
{
public:
                                        /* Constructor */
                        UProfiling(void);
                                        /* Destructor */
    virtual             ~UProfiling(void);
                                        /* Request profiling to start */ 
    UProfiling         &start(void);
                                        /* Request profiling to stop and calculate elapsed time */ 
    UProfiling         &stop(void);
                                        /* Request profiling to output the elapsed time */ 
    UProfiling         &printTime(void);
                                        /* Request profiling to return the elapsed time */ 
    ULONG               elapsedMicroSec(void);
                                        /* Request profiling to return the elapsed time */ 
    ULONG               elapsedMilliSec(void);
                                        /* Request profiling to return the elapsed time */ 
    char               *elapsed(void);
protected:
                                        /* Elapsed time in microseconds */
    ULONG               ulTimeMicroSec;
                                        /* HRTIMER */
    HRTIMER             hrTimer;
                                        /* Elapsed time as a string */
    char                acElapsedTime[32];
};

#endif  /* _UPROFILING_HPP_ */
