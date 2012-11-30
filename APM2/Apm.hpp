/***********************************************************************\
 *                                APM/2                                *
 *  Copyright (C) by Stangl Roman (rstangl@vnet.ibm.com), 1997, 1998   *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
\***********************************************************************/

#ifndef __APM_HPP__
#define __APM_HPP__

#define     INCL_DOSFILEMGR
#define     INCL_DOSMISC
#define     INCL_DOSDEVICES
#define     INCL_DOSDEVIOCTL
#define     INCL_DOSERRORS
#define     INCL_DOSPROCESS
#define     INCL_DOSMODULEMGR
#define     INCL_DOSNLS
#define     INCL_DOSDATETIME
#include    <os2.h>

#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    <apm.h>

#ifndef LANG_ENGLISH
    #ifndef LANG_GERMAN
        #define LANG_ENGLISH
    #endif  /* LANG_GERMAN */
#endif  /* LANG_ENGLISH */

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
#define     MSG_DEVICE_BAD              10 
#define     MSG_APMDD_OPEN_TEXT         11 
#define     MSG_APMDD_OPEN              12 
#define     MSG_APM_VERBOSE             13 
#define     MSG_APM_VERBOSE_TEXT1       14 
#define     MSG_APM_VERBOSE_TEXT2       15 
#define     MSG_APM_VERBOSE_TEXT3       16 
#define     MSG_APM_VERBOSE_TEXT4       17 
#define     MSG_APM_VERBOSE_TEXT5       18 
#define     MSG_APM_VERBOSE_TEXT6       19 
#define     MSG_APM_VERBOSE_TEXT7       20 
#define     MSG_APM_VERBOSE_TEXT8       21 
#define     MSG_APM_ERROR               22 
#define     MSG_APM_ERROR_0X0           23 
#define     MSG_APM_ERROR_0X1           24 
#define     MSG_APM_ERROR_0X2           25 
#define     MSG_APM_ERROR_0X3           26 
#define     MSG_APM_ERROR_0X4           27 
#define     MSG_APM_ERROR_0X5           28 
#define     MSG_APM_ERROR_0X6           29 
#define     MSG_APM_ERROR_0X7           30 
#define     MSG_APM_ERROR_0X8           31 
#define     MSG_APM_ERROR_0X9           32 
#define     MSG_APM_ERROR_0XA           33 
#define     MSG_APM_ERROR_0XB           34 
#define     MSG_APM_ERROR_0XC           35 
#define     MSG_APM_ERROR_0XD           36 
#define     MSG_APM_ERROR_0XE           37 
#define     MSG_APM_ERROR_0XF           38 
#define     MSG_APM_ERROR_0X10          39 
#define     MSG_APM_ERROR_0X11          40 
#define     MSG_APM_ERROR_UNKNOWN       41 
#define     MSG_APM_ERROR_EOL           42 
#define     MSG_VERBOSE_ERROR_TEXT      43
#define     MSG_VERBOSE_ERROR           44
#define     MSG_SCHEDULE_ERROR_MONTH    45 
#define     MSG_SCHEDULE_ERROR_DAY      46 
#define     MSG_SCHEDULE_ERROR_HOUR     47 
#define     MSG_SCHEDULE_ERROR_MIN      48 
#define     MSG_SCHEDULE_ERROR_SEC      49 
#define     MSG_SCHEDULE_ERROR_DATA     50 
#define     MSG_SCHEDULE_SEMANTIC1      51 
#define     MSG_SCHEDULE_SEMANTIC2      52 
#define     MSG_SCHEDULE_INVOKING       53 
#define     MSG_SCHEDULE_MATCH          54 
#define     MSG_SCHEDULE_ERROR          55 
#define     MSG_APM_VERSION_POWERON     56 
#define     MSG_APM_VERSION_POWEROFF    57 
#define     MSG_UNRECOVERABLE           58 
#define     MSG_APM_ENABLE              59 
#define     MSG_APM_ENABLE_ERROR_TEXT   60 
#define     MSG_APM_ENABLE_ERROR        61 
#define     MSG_APM_SHUTDOWN            62 
#define     MSG_APM_SHUTDOWN_ERROR_TEXT 63 
#define     MSG_APM_SHUTDOWN_ERROR      64 
#define     MSG_APM_OFF                 65 
#define     MSG_APM_OFF_ERROR_TEXT      66 
#define     MSG_APM_OFF_ERROR           67 
#define     MSG_APM_OFF_SUCCESS         68 
#define     MSG_APM_ON                  69 
#define     MSG_APM_ON_ERROR_TEXT       70
#define     MSG_APM_ON_ERROR            71

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

/****************************************************************************************\
 * Class: UApm                                                                          *
\****************************************************************************************/

class   UApm
{
public:
                        UApm(int iArgc, char *apcArgv[]);
                       ~UApm(void);
                                        /* Initialize after construction */
    UApm               &initialize(void);
                                        /* Process what we were designed to do */
    UApm               &process(void);
protected:
                                        /* Match commandline parameters */
    UApm               &matchParameters(void);
                                        /* Process commandline parameters */
    UApm               &processParameters(void);
                                        /* Process scheduler data */
    UApm               &processScheduler(void);
                                        /* Process the APM off requests */
    UApm               &processAPMOffRequest(void);
                                        /* Display a dotted progress bar for the give time */
    UApm               &displayDelay(int iSeconds);
                                        /* Display a message from the message file */
    UApm               &displayMessage(int iMessage, int iParameterCount=0, int iParam1=0, int iParam2=0);
                                        /* Check for commandline option */
    char               *checkCommandlineOption(char *pcOption, BOOL bCompositeOption=FALSE, BOOL bCaseSensitive=FALSE);
                                        /* Verbose error returned by APM$ DosDevIOCtl() */
    UApm               &displayAPMError(int iApiRet, int iReturnCode);
                                        /* Convert a hex decimal into a BCD decimal */
    ULONG               getAsBCD(ULONG ulBinary);
public:
                                        /* Highest return code */
#define     ERROR_NLSMSG_NOT_EXIST      1L
#define     ERROR_NLSMSG_INVALID        2L
#define     ERROR_PARAMETERS_MISSING    5L
#define     ERROR_PARAMETERS_INVALID    6L
#define     ERROR_REQUEST_MISSING       7L
#define     ERROR_REQUEST_BAD           9L
#define     ERROR_DEVICE_BAD            10L
#define     ERROR_APMDD_OPEN            12L
#define     ERROR_APM_DOSDEVIOCTL       22L
#define     ERROR_VERBOSE_ERROR         44L
#define     ERROR_SCHEDULE_ERROR_MONTH  45L
#define     ERROR_SCHEDULE_ERROR_DAY    46L
#define     ERROR_SCHEDULE_ERROR_HOUR   47L
#define     ERROR_SCHEDULE_ERROR_MIN    48L
#define     ERROR_SCHEDULE_ERROR_SEC    49L
#define     ERROR_SCHEDULE_ERROR_DATA   50L
#define     ERROR_SCHEDULE_SEMANTIC1    51L
#define     ERROR_SCHEDULE_SEMANTIC2    52L
#define     ERROR_SCHEDLUE_ERROR        54L
#define     ERROR_APM_VERSION_POWERON   56L
#define     ERROR_APM_VERSION_POWEROFF  57L
#define     ERROR_UNRECOVERABLE         58L
#define     ERROR_APM_ENABLE_ERROR      59L
#define     ERROR_APM_SHUTDOWN_ERROR    64L
#define     ERROR_APM_OFF_ERROR         67L
#define     ERROR_APM_ON_ERROR          71L
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

#define     FLAG_MASK_REQUEST           0x000007F0L
#define     FLAG_APMREADY               0x00000010L
#define     FLAG_APMSTANDBY             0x00000020L
#define     FLAG_APMSUSPEND             0x00000040L
#define     FLAG_APMPOWEROFF            0x00000080L
#define     FLAG_APMPOWEROFF_MINUS      0x00000100L
#define     FLAG_APMPOWEROFF_PLUS       0x00000200L
#define     FLAG_APMPOWERON             0x00000400L

#define     FLAG_MASK_DEVICE            0x0003F000L
#define     FLAG_DEVICE_BIOS            0x00001000L
#define     FLAG_DEVICE_ALL             0x00002000L
#define     FLAG_DEVICE_DISPLAY         0x00004000L
#define     FLAG_DEVICE_DISK            0x00008000L
#define     FLAG_DEVICE_PARALLEL        0x00010000L
#define     FLAG_DEVICE_SERIAL          0x00020000L

#define     FLAG_MASK_SCHEDULE          0x00100000L
#define     FLAG_SCHEDULE_SYSTEM_ON     0x00100000L

#define     FLAG_NLS_MESSAGES_REQUESTED 0x01000000L
#define     FLAG_NLS_MESSAGES_FOUND     0x01000000L
#define     FLAG_IGNORE_VERSION         0x04000000L
    int                 iStatusFlag;
                                        /* Fully qualified path to message file which is
                                           expected to be in the directory APM/2 was
                                           launched from */
    char                acMessageFileUs[CCHMAXPATH];
    char                acMessageFileNls[CCHMAXPATH];
                                        /* Message buffer */
#define     MESSAGEBUFFER_LENGTH        4096
    char                acMessageBuffer[MESSAGEBUFFER_LENGTH];
                                        /* OS/2 major and minor version */
    ULONG               ulVersion[2];
                                        /* File handle of APM$ */
    HFILE               hfileAPM;
                                        /* Date and Time when request is to be scheduled */
    DATETIME            datetimeSchedule;
};



#endif  /* __APM_HPP__ */
