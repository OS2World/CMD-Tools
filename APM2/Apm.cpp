/***********************************************************************\
 *                                APM/2                                *
 *  Copyright (C) by Stangl Roman (rstangl@vnet.ibm.com), 1997, 1998   *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
\***********************************************************************/
#pragma     strings(readonly)

#include    "APM.hpp"

/*--------------------------------------------------------------------------------------*\
 * UApm : UApm()                                                                        *
\*--------------------------------------------------------------------------------------*/
                        UApm::UApm(int iArgc, char *apcArgv[]) :
                              iReturnCode(NO_ERROR),
                              iStatusFlag(FLAG_NULL),
                              pcCommandline(0),
                              hfileAPM(0)
{
    this->iArgc=iArgc;
    this->ppcArgv=apcArgv;
    memset(acMessageFileUs, '\0', sizeof(acMessageFileUs));
    memset(acMessageFileNls, '\0', sizeof(acMessageFileNls));
    memset(acMessageBuffer, '\0', sizeof(acMessageBuffer));
    memset(&datetimeSchedule, 0, sizeof(datetimeSchedule));
}

/*--------------------------------------------------------------------------------------*\
 * UApm : ~UApm()                                                                       *
\*--------------------------------------------------------------------------------------*/
                        UApm::~UApm(void)
{
                                        /* Close APM.SYS */
    if(hfileAPM!=0)
        DosClose(hfileAPM);
    if(pcCommandline!=0)
        free(pcCommandline);
}

/*--------------------------------------------------------------------------------------*\
 * UApm : intialize()                                                                   *
\*--------------------------------------------------------------------------------------*/
UApm                   &UApm::initialize(void)
{
    TIB    *ptib;                       /* Thread information block */
    PIB    *ppib;                       /* Process information block */
    char   *pcTemp;

                                        /* Query the fully qualified path APM/2 was loaded from.
                                           Once we know the language to use we replace ".exe"
                                           by e.g. "xx.msg" where xx is the NLS message file,
                                           but for now we assume English as this one is shipped
                                           with APM/2 for sure */
    DosGetInfoBlocks(&ptib, &ppib);
    DosQueryModuleName(ppib->pib_hmte, sizeof(acMessageFileUs), (PCHAR)acMessageFileUs);
    pcCommandline=strdup(strchr(ppib->pib_pchcmd, '\0')+1);
    pcTemp=strrchr(acMessageFileUs, '.');
    if(pcTemp!=0)
        strcpy(pcTemp, "Us.msg");
    strcpy(acMessageFileNls, acMessageFileUs);
                                        /* Query the version we're running */
    DosQuerySysInfo(QSV_VERSION_MAJOR, QSV_VERSION_MINOR, ulVersion, sizeof(ulVersion));
    return(*this);
}

/*--------------------------------------------------------------------------------------*\
 * UApm : process()                                                                     *
 *        The APM/2 main function, throws a ERROR_* in case of errors                   *
\*--------------------------------------------------------------------------------------*/
UApm                   &UApm::process(void)
{
    char               *pcOption=0;
    COUNTRYCODE         countrycode={ 0 };
    COUNTRYINFO         countryinfo={ 0 };
    ULONG               ulInfoLength=0;
    int                 iTemp=0;
    char               *pcTemp;
    UCountryTranslate   ucountrytranslate[]= { 
                                               {  01L, "Us" },
                                               {  02L, "Fr" },
                                               {  07L, "Ru" },
                                               {  31L, "Nl" },
                                               {  32L, "Be" },
                                               {  33L, "Fr" },
                                               {  34L, "Es" },
                                               {  39L, "It" },
                                               {  41L, "Gr" },
                                               {  44L, "Us" },
                                               {  45L, "Dk" },
                                               {  46L, "Se" },
                                               {  47L, "No" },
                                               {  49L, "Gr" },
                                               {  61L, "Us" },
                                               {  81L, "Jp" },
                                               {  99L, "Us" },
                                               { 358L, "Fi" },
                                             };
    FILESTATUS3         filestatus3;

                                        /* Query country settings, to try to load NLS
                                           message file even if no /Language commandline
                                           option has been specified */
    DosQueryCtryInfo(sizeof(countryinfo), &countrycode, &countryinfo, &ulInfoLength);
    for(iTemp=0; iTemp<(sizeof(ucountrytranslate)/sizeof(ucountrytranslate[0])); iTemp++)
        {
        if(countryinfo.country==ucountrytranslate[iTemp].ulCountry)
            {
            pcTemp=strrchr(acMessageFileNls, '.');
            if(strlen(acMessageFileNls) && (pcTemp!=0))
                {
                pcTemp-=2;              /* Skip "Us" backwards to append "Xx.msg" */
                strcpy(pcTemp, ucountrytranslate[iTemp].acCountry);
                strcat(pcTemp, ".msg");
                }
            break;
            }
        }
                                        /* First, try to find the NLS message file, which
                                           of course can override our assumptions */
    if((pcOption=checkCommandlineOption("Language", TRUE))!=0)
        {
        if(pcOption==(char *)0xFFFFFFFF)
            {
            displayMessage(MSG_LOGO);
            displayMessage(MSG_PARAMETERS_MISSING);
            throw((int)ERROR_PARAMETERS_MISSING);
            }
                                        /* If language was specified, use it */
        iStatusFlag|=FLAG_NLS_MESSAGES_REQUESTED;
        pcTemp=strrchr(acMessageFileNls, '.');
        if(strlen(acMessageFileNls) && (pcTemp!=0))
            {
            pcTemp-=2;                  /* Skip "Us" backwards to append "Xx.msg" */
            strcpy(pcTemp, pcOption);
            strcat(pcTemp, ".msg");
            }
        }
                                        /* Now that we might have a NLS message file that is
                                           not Us, verify that it exists */
    if(strcmp(acMessageFileNls, acMessageFileUs))
        {
        if(DosQueryPathInfo(acMessageFileNls, FIL_STANDARD, &filestatus3, sizeof(filestatus3))!=NO_ERROR)
            {
                                        /* The NLS message file specified does not exists, so use
                                           Us English one, but inform the user if he has requested
                                           it (compared to when we found it in the ucountrytranslate[]
                                           table) */
            if(iStatusFlag & FLAG_NLS_MESSAGES_REQUESTED)
                displayMessage(ERROR_NLSMSG_NOT_EXIST, 1, (int)acMessageFileNls);
            strcpy(acMessageFileNls, acMessageFileUs);
            }
        else
                                        /* The NLS message file does exist, so any problem retrieving
                                           messages is caused by invalid contents of that file, which
                                           we should inform the user for */
            iStatusFlag|=FLAG_NLS_MESSAGES_FOUND;
        }
                                        /* Process further depending on the arguments specified */
    if(iArgc==1)
        {
                                        /* If no argument is specified, display simple help */
        displayMessage(MSG_LOGO);
        displayMessage(MSG_DESCRIPTION);
        displayMessage(MSG_PARAMETERS_MISSING);
        iReturnCode=ERROR_PARAMETERS_MISSING;
        }
    else
        {
                                        /* At least one option was specified */
        displayMessage(MSG_LOGO);
                                        /* If the language was the only (two) arguments, display
                                           simple help */
        if((iArgc==3) && (iStatusFlag & FLAG_NLS_MESSAGES_REQUESTED))
            {
            displayMessage(MSG_DESCRIPTION);
            displayMessage(MSG_PARAMETERS_MISSING);
            iReturnCode=ERROR_PARAMETERS_MISSING;
            throw(iReturnCode);
            }
        if(checkCommandlineOption("?")!=0)
            {
            displayMessage(MSG_PARAMETERS_INVALID);
            displayMessage(MSG_PARAMETERS_TEXT);
            iReturnCode=ERROR_PARAMETERS_INVALID;
            }
        else
            {
                                        /* Now just find all commandline parameters and trow
                                           an error if an invalid combination was found */
            try
                {
                matchParameters();
                }
            catch(int iErrorCode) 
                {
                displayMessage(iErrorCode);
                displayMessage(MSG_PARAMETERS_TEXT);
                iReturnCode=iErrorCode;
                throw(iReturnCode);
                }
                                        /* Now that we have found valid commandline
                                           parameters, process them */
            try
                {
                processParameters();
                }
            catch(int iErrorCode)
                {
                displayMessage(iErrorCode);
                iReturnCode=iErrorCode;
                throw(iReturnCode);
                }
            }
        }

    return(*this);
}

/*--------------------------------------------------------------------------------------*\
 * UApm : matchParameters()                                                             *
 *        Match the commandline parameters and throw ERROR_* exceptions if inconsisten- *
 *        cies are found                                                                *
\*--------------------------------------------------------------------------------------*/
UApm                   &UApm::matchParameters(void)
{
    UOption uoption[]={
                        { FLAG_SHOWCONFIGURATION, "Verbose"   },
                        { FLAG_APMREADY         , "Ready"     },
                        { FLAG_APMSTANDBY       , "Standby"   },
                        { FLAG_APMSUSPEND       , "Suspend"   },
                        { FLAG_APMPOWEROFF      , "Poweroff"  },
                        { FLAG_APMPOWEROFF_MINUS, "Poweroff-" },
                        { FLAG_APMPOWEROFF_PLUS , "Poweroff+" },
                        { FLAG_APMPOWERON       , "Poweron"   },
                      };
    UOption udevice[]={
                        { FLAG_DEVICE_BIOS      , "BIOS"      },
                        { FLAG_DEVICE_ALL       , "All"       },
                        { FLAG_DEVICE_DISPLAY   , "Display"   },
                        { FLAG_DEVICE_DISK      , "Disk"      },
                        { FLAG_DEVICE_PARALLEL  , "Parallel"  },
                        { FLAG_DEVICE_SERIAL    , "Serial"    },
                      };
    int     iTemp=0;
    char   *pcParameter=0;


                                        /* Check for /Verbose option */
    if(checkCommandlineOption(uoption[0].acOption))
        iStatusFlag|=FLAG_SHOWCONFIGURATION;
                                        /* Check for /Standby ... /Poweroff+ options, which
                                           must be unique */
    for(iTemp=1; iTemp<(sizeof(uoption)/sizeof(uoption[0])); iTemp++)
        {
        if(checkCommandlineOption(uoption[iTemp].acOption))
            {
            if((iStatusFlag & FLAG_MASK_REQUEST)==FLAG_NULL)
                iStatusFlag|=uoption[iTemp].iStatusFlag;
            else
                throw((int)ERROR_REQUEST_BAD);
            }
        }
                                        /* Check for /Device xx option */
    pcParameter=checkCommandlineOption("Device", TRUE);
                                        /* If /Device was specified, then a parameter must
                                           also be given */
    if(pcParameter==(char *)0xFFFFFFFF)
        throw((int)ERROR_DEVICE_BAD);
    if(pcParameter!=0)
        {
        for(iTemp=0; iTemp<(sizeof(udevice)/sizeof(udevice[0])); iTemp++)
            {
            char   *pcS=pcParameter;
            char   *pcD=udevice[iTemp].acOption;
            int     iDeviceFound=TRUE;
    
            for( ; *pcS!='\0' && *pcD!='\0'; pcS++, pcD++)
                if((*pcS | 0x20)!=(*pcD | 0x20))
                    {
                    iDeviceFound=FALSE;
                    break;
                    }
            if((*pcD!='\0') || ((*pcS!=' ') && (*pcS!='\0')))
                iDeviceFound=FALSE;
            if(iDeviceFound==TRUE)
                iStatusFlag|=udevice[iTemp].iStatusFlag;
            }
                                        /* If /Device was specified, then a parameter must
                                           also be given (one that exists) */
        if((iStatusFlag & FLAG_MASK_DEVICE)==FLAG_NULL)
            throw((int)ERROR_DEVICE_BAD);
        }

    if((iStatusFlag & FLAG_MASK_DEVICE)==FLAG_NULL)
        iStatusFlag|=FLAG_DEVICE_ALL;
                                        /* Check for /Force option */
    if(checkCommandlineOption("Force"))
        iStatusFlag|=FLAG_IGNORE_VERSION;
                                        /* Check for /Schedule option. This option is a
                                           composite option and has some syntax limitations */
    pcParameter=checkCommandlineOption("Schedule", TRUE);
    if(pcParameter==(char *)0xFFFFFFFF)
        throw((int)ERROR_SCHEDULE_ERROR_HOUR);
    else if(pcParameter!=0)
        {
        char   *pcTempBegin;
        char   *pcTempEnd;
        char   *pcParameterEnd;

        pcParameterEnd=strchr(pcParameter, '\0');
                                        /* The first thing we expect is either the year or
                                           the hour */
        pcTempBegin=pcParameter;
        pcTempEnd=strchr(pcTempBegin, ':');
        if(pcTempEnd!=0)
            *pcTempEnd='\0';
        datetimeSchedule.year=atoi(pcTempBegin);
        if(datetimeSchedule.year>1000)
            {
                                        /* Followed by the month */
            if((pcTempEnd==0) || (pcTempEnd>=pcParameterEnd))
                throw((int)ERROR_SCHEDULE_ERROR_MONTH);
            pcTempBegin=pcTempEnd+1;
            pcTempEnd=strchr(pcTempBegin, ':');
            if(pcTempEnd!=0)
                *pcTempEnd='\0';
            datetimeSchedule.month=atoi(pcTempBegin);
                                        /* Followed by the day */
            if((pcTempEnd==0) || (pcTempEnd>=pcParameterEnd))
                throw((int)ERROR_SCHEDULE_ERROR_DAY);
            pcTempBegin=pcTempEnd+1;
            pcTempEnd=strchr(pcTempBegin, ':');
            if(pcTempEnd!=0)
                *pcTempEnd='\0';
            datetimeSchedule.day=atoi(pcTempBegin);
                                        /* Followed by the hour */
            if((pcTempEnd==0) || (pcTempEnd>=pcParameterEnd))
                throw((int)ERROR_SCHEDULE_ERROR_HOUR);
            pcTempBegin=pcTempEnd+1;
            pcTempEnd=strchr(pcTempBegin, ':');
            if(pcTempEnd!=0)
                *pcTempEnd='\0';
            datetimeSchedule.hours=atoi(pcTempBegin);
            }
        else
            {
                                        /* Ok, only hour and minutes specified */
            datetimeSchedule.hours=datetimeSchedule.year;
            datetimeSchedule.year=0;
            }
                                        /* Followed by the minutes */
        if((pcTempEnd==0) || (pcTempEnd>=pcParameterEnd))
            throw((int)ERROR_SCHEDULE_ERROR_MIN);
        pcTempBegin=pcTempEnd+1;
        pcTempEnd=strchr(pcTempBegin, ':');
        if(pcTempEnd!=0)
            throw((int)ERROR_SCHEDULE_ERROR_SEC);
        datetimeSchedule.minutes=atoi(pcTempBegin);
                                        /* Now check that the parameters are within
                                           the limits */
        if(datetimeSchedule.year>1000)
            {
            if((datetimeSchedule.year<1990) || (datetimeSchedule.year>2099) ||
                (datetimeSchedule.month==0) || (datetimeSchedule.month>12) ||
                (datetimeSchedule.day==0) || (datetimeSchedule.day>31) ||
                (datetimeSchedule.hours>23) ||
                (datetimeSchedule.minutes>59))
                throw((int)ERROR_SCHEDULE_ERROR_DATA);
            }
        else
            {
            if((datetimeSchedule.hours>23) ||
                (datetimeSchedule.minutes>59))
                throw((int)ERROR_SCHEDULE_ERROR_DATA);
            }
                                        /* Now that we found all data required for the
                                           scheduler, we can accept request */
        iStatusFlag|=FLAG_SCHEDULE_SYSTEM_ON;
        }
                                        /* At least one of the unique "request" parameters must
                                           be specified */
    if(!(iStatusFlag & FLAG_SHOWCONFIGURATION) && ((iStatusFlag & FLAG_MASK_REQUEST)==FLAG_NULL))
        throw((int)ERROR_REQUEST_MISSING);
                                        /* If we're scheduling, ensure that a "request" except
                                           /Verbose is scheduled (as I question the sense to
                                           schedule just a /Verbose - as OS/2 can't dynamically
                                           un/load device drivers) */
    if(iStatusFlag & FLAG_SCHEDULE_SYSTEM_ON)
        if((iStatusFlag & FLAG_SHOWCONFIGURATION) && ((iStatusFlag & FLAG_MASK_REQUEST)==FLAG_NULL))
            throw((int)ERROR_SCHEDULE_SEMANTIC1);
                                        /* On the other hand, a /Poweron request must be scheduled */
    if((iStatusFlag & FLAG_APMPOWERON) && !(iStatusFlag & FLAG_SCHEDULE_SYSTEM_ON))
        throw((int)ERROR_SCHEDULE_SEMANTIC2);
    return(*this);
}

/*--------------------------------------------------------------------------------------*\
 * UApm : processParameters()                                                           *
 *        Process the commandline parameters and throw ERROR_* exceptions if problems   *
 *        are found.                                                                    *
\*--------------------------------------------------------------------------------------*/
UApm                   &UApm::processParameters(void)
{
    USHORT          usRequest=(USHORT)-1;
    APIRET          apiretRc=NO_ERROR;
    ULONG           ulAction=0;
    ULONG           ulPacketSize;
    ULONG           ulDataSize;
    int             iPowerInfoCompatible=FALSE;
                                        /* APM diagnostics */
    POWERRETURNCODE powerRC;
                                        /* APM information */
    GETPOWERINFO    getpowerinfoAPM;
    
                                        /* Open APM.SYS */
    ulAction=0;
    apiretRc=DosOpen("\\DEV\\APM$", &hfileAPM, &ulAction, 0, FILE_NORMAL,
        OPEN_ACTION_OPEN_IF_EXISTS,
        OPEN_FLAGS_FAIL_ON_ERROR | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE, NULL);
    if(apiretRc!=NO_ERROR)
        {
        displayMessage(MSG_APMDD_OPEN_TEXT, 1, apiretRc);
        displayMessage(MSG_APMDD_OPEN);
        throw((int)ERROR_APMDD_OPEN);
        }
    memset(&getpowerinfoAPM, 0, sizeof(getpowerinfoAPM));
    getpowerinfoAPM.usParmLength=GETPOWERINFO_PARMLENGTH_NEW;
    ulPacketSize=sizeof(getpowerinfoAPM);
    powerRC.usReturnCode=0;
    ulDataSize=sizeof(powerRC);
    apiretRc=DosDevIOCtl(hfileAPM, IOCTL_POWER, POWER_GETPOWERINFO,
        &getpowerinfoAPM, ulPacketSize, &ulPacketSize,
        &powerRC, ulDataSize, &ulDataSize);
    if((apiretRc!=NO_ERROR) || (powerRC.usReturnCode==POWER_BADLENGTH))
        {
        iPowerInfoCompatible=TRUE;
        memset(&getpowerinfoAPM, 0, sizeof(getpowerinfoAPM));
        powerRC.usReturnCode=0;
        getpowerinfoAPM.usParmLength=GETPOWERINFO_PARMLENGTH_COMPATIBLE;
        ulPacketSize=sizeof(getpowerinfoAPM);
        ulDataSize=sizeof(powerRC);
        apiretRc=DosDevIOCtl(hfileAPM, IOCTL_POWER, POWER_GETPOWERINFO,
            &getpowerinfoAPM, ulPacketSize, &ulPacketSize,
            &powerRC.usReturnCode, ulDataSize, &ulDataSize);
        }
                                        /* Display error codes or verbose APM configuration */
    if((apiretRc!=NO_ERROR) || (powerRC.usReturnCode!=POWER_NOERROR))
        {
        displayMessage(MSG_VERBOSE_ERROR_TEXT, 1, apiretRc);
        displayAPMError(apiretRc, powerRC.usReturnCode);
        throw((int)ERROR_VERBOSE_ERROR);
        }
                                        /* Verbose the APM configuration */
    if(iStatusFlag & FLAG_SHOWCONFIGURATION)
        {
                                        /* Load APM get capabilities */
        POWEROEMFUNCTION    poweroemfunctionAPM;

        displayMessage(MSG_APM_VERBOSE);
        displayMessage(MSG_APM_VERBOSE_TEXT1, 1, (int)getpowerinfoAPM.usBIOSFlags);
        displayMessage(MSG_APM_VERBOSE_TEXT2, 2,
            (int)(getpowerinfoAPM.usBIOSVersion>>8), (int)(getpowerinfoAPM.usBIOSVersion & 0xFF));
        displayMessage(MSG_APM_VERBOSE_TEXT3, 2,
            (int)(getpowerinfoAPM.usSubsysVersion>>8), (int)(getpowerinfoAPM.usSubsysVersion & 0xFF));
        if(iPowerInfoCompatible==FALSE)
            {
            displayMessage(MSG_APM_VERBOSE_TEXT4, 2,
                (int)(getpowerinfoAPM.usAPMVersion>>8), (int)(getpowerinfoAPM.usAPMVersion & 0xFF));
            }
        else
            displayMessage(MSG_APM_ERROR_EOL);
                                        /* Display some info about APM configuration data */
        displayMessage(MSG_APM_VERBOSE_TEXT5);
                                        /* Use the APM OEM function of APM.SYS 1.2 */
        memset(&poweroemfunctionAPM, 0, sizeof(poweroemfunctionAPM));
        poweroemfunctionAPM.usParmLength=POWEROEMFUNCTION_PARMLENTH_NOSELREG;
                                        /* Get Capabilities */
        poweroemfunctionAPM.ulEAX=0x5311;
                                        /* Device ID */
        poweroemfunctionAPM.ulEBX=DEVID_APMBIOS;
        ulPacketSize=POWEROEMFUNCTION_PARMLENTH_NOSELREG;
        powerRC.usReturnCode=0;
        ulDataSize=sizeof(powerRC);
        apiretRc=DosDevIOCtl(hfileAPM, IOCTL_POWER, POWER_OEMFUNCTION,
            &poweroemfunctionAPM, ulPacketSize, &ulPacketSize,
            &powerRC, ulDataSize, &ulDataSize);
        if((apiretRc==NO_ERROR) || (powerRC.usReturnCode==POWER_NOERROR))
            {
            displayMessage(MSG_APM_VERBOSE_TEXT6, 2, 
                poweroemfunctionAPM.ulECX, poweroemfunctionAPM.ulEBX & 0x000000FF);
            displayMessage(MSG_APM_VERBOSE_TEXT7);
            }
                                        /* Display where to get more information */
        displayMessage(MSG_APM_VERBOSE_TEXT8);
        }
                                        /* If only /Verbose was requested, we're done */
    if((iStatusFlag & FLAG_MASK_REQUEST)==FLAG_NULL)
        return(*this);
                                        /* For /Poweron "Request", we need APM 1.2 support, and
                                           the power on date and/or time must be scheduled */
    if(iStatusFlag & FLAG_APMPOWERON)
        if(((getpowerinfoAPM.usAPMVersion>>8)<1) ||
            (((getpowerinfoAPM.usAPMVersion>>8)==1) && ((getpowerinfoAPM.usAPMVersion & 0xFF)<2)))
            if(!(iStatusFlag & FLAG_IGNORE_VERSION))
                throw((int)ERROR_APM_VERSION_POWERON);
                                        /* For /Shutdown "Request", we need Warp 4 otherwise
                                           at least a CHKDSK reliably occurs (if the request
                                           does work at all) */
    if(!(iStatusFlag & FLAG_IGNORE_VERSION))
        if((iStatusFlag & (FLAG_APMPOWEROFF | FLAG_APMPOWEROFF_MINUS | FLAG_APMPOWEROFF_PLUS))!=FLAG_NULL)
            {
            int iVersionFail=TRUE;

            if((ulVersion[0]==OS2_MAJOR) && (ulVersion[1]>=OS2_MINOR_400))
                iVersionFail=FALSE;
            if(ulVersion[0]>OS2_MAJOR)
                iVersionFail=FALSE;
            if(iVersionFail==TRUE)
                throw((int)ERROR_APM_VERSION_POWEROFF);
            }
                                        /* If user requested to schedule, invoke scheduler */
    if(iStatusFlag & FLAG_SCHEDULE_SYSTEM_ON)
        if(!(iStatusFlag & FLAG_APMPOWERON))
            processScheduler();
                                        /* Now invoke requeste APM function */
    processAPMOffRequest();

    return(*this);
}

/*--------------------------------------------------------------------------------------*\
 * UApm : processScheduler()                                                            *
 *        For scheduler requests we have 2 different situtations, one is to wait until  *
 *        the current date equals the scheduled one (for all APM "off" requests) and    *
 *        the other is just to load the APM "on" request.                               *
\*--------------------------------------------------------------------------------------*/
UApm                   &UApm::processScheduler(void)
{
    DATETIME        datetimeCurrent;
    APIRET          apiretRc=NO_ERROR;
    ULONG           ulPacketSize;
    ULONG           ulDataSize;
                                        /* APM diagnostics */
    POWERRETURNCODE powerRC;
 
    if(iStatusFlag & FLAG_APMPOWERON)
        {
                                        /* Load APM power on scheduler data */
        POWEROEMFUNCTION    poweroemfunctionAPM;
        
                                        /* Get current time, an error should never happen
                                           but would be unrecoverable */
        apiretRc=DosGetDateTime(&datetimeCurrent);
        if(apiretRc!=NO_ERROR)
            {
            displayMessage(MSG_SCHEDULE_ERROR, 1, apiretRc);
            throw((int)ERROR_UNRECOVERABLE);
            }
        displayMessage(MSG_APM_ON);
        displayDelay(10);
                                        /* Use the APM OEM function of APM.SYS 1.2 */
        memset(&poweroemfunctionAPM, 0, sizeof(poweroemfunctionAPM));
        poweroemfunctionAPM.usParmLength=POWEROEMFUNCTION_PARMLENTH_NOSELREG;
                                        /* Get/Set/Disable resume timer */
        poweroemfunctionAPM.ulEAX=0x5311;
                                        /* Device ID */
        poweroemfunctionAPM.ulEBX=DEVID_APMBIOS;
                                        /* ss:set resume timer */
        poweroemfunctionAPM.ulECX=0x0002;
                                        /* hh:mm */
        poweroemfunctionAPM.ulEDX=(getAsBCD(datetimeSchedule.hours)<<8) |
                                  (getAsBCD(datetimeSchedule.minutes));
                                        /* If supplied used supplied date, otherwise use
                                           current */
        if(datetimeSchedule.year>1990)
            {
                                        /* mm:dd */
            poweroemfunctionAPM.ulESI=(getAsBCD(datetimeSchedule.month)<<8) |
                                      (getAsBCD(datetimeSchedule.day));
                                        /* yyyy */
            poweroemfunctionAPM.ulEDI=getAsBCD(datetimeCurrent.year);
            }
        else
            {
            poweroemfunctionAPM.ulESI=(getAsBCD(datetimeCurrent.month)<<8) |
                                      (getAsBCD(datetimeCurrent.day));
            poweroemfunctionAPM.ulEDI=getAsBCD(datetimeCurrent.year);
            }
        ulPacketSize=POWEROEMFUNCTION_PARMLENTH_NOSELREG;
        powerRC.usReturnCode=0;
        ulDataSize=sizeof(powerRC);
        apiretRc=DosDevIOCtl(hfileAPM, IOCTL_POWER, POWER_OEMFUNCTION,
            &poweroemfunctionAPM, ulPacketSize, &ulPacketSize,
            &powerRC, ulDataSize, &ulDataSize);
        if((apiretRc!=NO_ERROR) || (powerRC.usReturnCode!=POWER_NOERROR))
            {
            displayMessage(MSG_APM_ON_ERROR_TEXT);
            displayAPMError(apiretRc, powerRC.usReturnCode);
            throw((int)ERROR_APM_ON_ERROR);
            }
        }
    else
        {
                                        /* Wait until the scheduled date arrives */
        displayMessage(MSG_SCHEDULE_INVOKING);
        for ( ; ; DosSleep(10000))
            {
                                        /* Get current time, an error should never happen
                                           but would be unrecoverable */
            apiretRc=DosGetDateTime(&datetimeCurrent);
            if(apiretRc!=NO_ERROR)
                {
                displayMessage(MSG_SCHEDULE_ERROR, 1, apiretRc);
                throw((int)ERROR_UNRECOVERABLE);
                }
                                        /* If yyyy:mm:dd doesn't equal, continue to
                                           sleep */
            if(datetimeSchedule.year!=0)
                {
                if((datetimeSchedule.year!=datetimeCurrent.year) ||
                    (datetimeSchedule.month!=datetimeCurrent.month) ||
                    (datetimeSchedule.day!=datetimeCurrent.day))
                    continue;
                }
                                        /* If hh:mm doesn't equal, continue to sleep too */
            if((datetimeSchedule.hours!=datetimeCurrent.hours) ||
                (datetimeSchedule.minutes!=datetimeCurrent.minutes))
                continue;
                                        /* Yeah, let's schedule that event */
            displayMessage(MSG_SCHEDULE_MATCH);
            break;
            }
        }
    return(*this);
}

/*--------------------------------------------------------------------------------------*\
 * UApm : processAPMOffRequest()                                                        *
 *        Process the APM "off" requests, that is /Standby, /Suspend and /Poweroff, but *
 *        not the /Poweron requests. Any errors are thrown.                             *
\*--------------------------------------------------------------------------------------*/
UApm                   &UApm::processAPMOffRequest(void)
{
    APIRET          apiretRc=NO_ERROR;
    ULONG           ulPacketSize;
    ULONG           ulDataSize;
                                        /* APM diagnostics */
    POWERRETURNCODE powerRC;
                                        /* Just be sure and enable APM */
    SENDPOWEREVENT  sendpowereventAPM;
 
                                        /* Enable APM */
    displayMessage(MSG_APM_ENABLE);
    displayDelay(10);
    memset(&sendpowereventAPM, 0, sizeof(sendpowereventAPM));
    powerRC.usReturnCode=0;
                                    /* Enable PWR MGMT function */
    sendpowereventAPM.usSubID=SUBID_ENABLE_POWER_MANAGEMENT;   
    ulPacketSize=sizeof(sendpowereventAPM);
    ulDataSize=sizeof(powerRC);
    apiretRc=DosDevIOCtl(hfileAPM, IOCTL_POWER, POWER_SENDPOWEREVENT,
        &sendpowereventAPM, ulPacketSize, &ulPacketSize,
        &powerRC, ulDataSize, &ulDataSize);
    if((apiretRc!=NO_ERROR) || (powerRC.usReturnCode!=POWER_NOERROR))
        {
        displayMessage(MSG_APM_ENABLE_ERROR_TEXT);
        displayAPMError(apiretRc, powerRC.usReturnCode);
        throw((int)ERROR_APM_ENABLE_ERROR);
        }
                                        /* For /Poweron just load APM power on timer and
                                           we're done */
    if(iStatusFlag & FLAG_APMPOWERON)
        {
        processScheduler();
        return(*this);
        }
                                        /* When requested, perform a DosShutdown() */
    if(iStatusFlag & FLAG_APMPOWEROFF_MINUS)
        {
        displayMessage(MSG_APM_SHUTDOWN);
        displayDelay(10);
        apiretRc=DosShutdown(1);
        }
    if(iStatusFlag & FLAG_APMPOWEROFF_PLUS)
        {
        displayMessage(MSG_APM_SHUTDOWN);
        displayDelay(10);
        apiretRc=DosShutdown(0);
        }
    if(apiretRc!=NO_ERROR)
        {
        displayMessage(MSG_APM_SHUTDOWN_ERROR_TEXT);
        displayAPMError(apiretRc, powerRC.usReturnCode);
        throw((int)ERROR_APM_SHUTDOWN_ERROR);
        }
                                    /* Invoke APM request */
    displayMessage(MSG_APM_OFF);
    displayDelay(10);
    memset(&sendpowereventAPM, 0, sizeof(sendpowereventAPM));
    powerRC.usReturnCode=0;
                                    /* Set PWR event */
    sendpowereventAPM.usSubID=SUBID_SET_POWER_STATE;   
                                    /* All APM devices (see header and/or APMV12.PDF) */
    if(iStatusFlag & FLAG_DEVICE_BIOS)
        sendpowereventAPM.usData1=DEVID_APMBIOS;
    else if(iStatusFlag & FLAG_DEVICE_ALL)
        sendpowereventAPM.usData1=DEVID_ALL_DEVICES;
    else if(iStatusFlag & FLAG_DEVICE_DISPLAY)
        sendpowereventAPM.usData1=DEVID_DISPLAY | 0xFF;
    else if(iStatusFlag & FLAG_DEVICE_DISK)
        sendpowereventAPM.usData1=DEVID_SECONDARY_STORAGE | 0xFF;
    else if(iStatusFlag & FLAG_DEVICE_PARALLEL)
        sendpowereventAPM.usData1=DEVID_PARALLEL_PORTS | 0xFF;
    else if(iStatusFlag & FLAG_DEVICE_SERIAL)
        sendpowereventAPM.usData1=DEVID_SERIAL_PORTS | 0xFF;
                                    /* APM request */
    if(iStatusFlag & FLAG_APMSTANDBY)
        sendpowereventAPM.usData2=POWERSTATE_STANDBY;
    else if(iStatusFlag & FLAG_APMSUSPEND)
        sendpowereventAPM.usData2=POWERSTATE_SUSPEND;
    else if((iStatusFlag & FLAG_APMPOWEROFF) || 
        (iStatusFlag & FLAG_APMPOWEROFF_MINUS) ||
        (iStatusFlag & FLAG_APMPOWEROFF_PLUS))
        sendpowereventAPM.usData2=POWERSTATE_OFF;
    else if(iStatusFlag & FLAG_APMREADY)
        sendpowereventAPM.usData2=POWERSTATE_READY;
    ulPacketSize=sizeof(sendpowereventAPM);
    ulDataSize=sizeof(powerRC);
    apiretRc=DosDevIOCtl(hfileAPM, IOCTL_POWER, POWER_SENDPOWEREVENT,
        &sendpowereventAPM, ulPacketSize, &ulPacketSize,
        &powerRC, ulDataSize, &ulDataSize);
    if((apiretRc!=NO_ERROR) || (powerRC.usReturnCode!=POWER_NOERROR))
        {
        displayMessage(MSG_APM_OFF_ERROR_TEXT);
        displayAPMError(apiretRc, powerRC.usReturnCode);
        throw((int)ERROR_APM_OFF_ERROR);
        }
                                        /* When getting hereto, it is likely that
                                           our request worked (or is being worked on) */
    displayMessage(MSG_APM_OFF_SUCCESS);
    return(*this);
}

/*--------------------------------------------------------------------------------------*\
 * UApm : displayDelay()                                                                *
 *        Display a progressbar with dots for the given period.                         *
\*--------------------------------------------------------------------------------------*/
UApm                   &UApm::displayDelay(int iSeconds)
{
    ULONG   ulWriteCount;

    strcpy(acMessageBuffer, "          ");
    ulWriteCount=strlen(acMessageBuffer);
    DosWrite(1, acMessageBuffer, ulWriteCount, &ulWriteCount);
    do  {
        strcpy(acMessageBuffer, ".");
        ulWriteCount=strlen(acMessageBuffer);
        DosWrite(1, acMessageBuffer, ulWriteCount, &ulWriteCount);
        DosSleep(1000);
        } while(--iSeconds>0);
    strcpy(acMessageBuffer, "\r\n\r\n");
    ulWriteCount=strlen(acMessageBuffer);
    DosWrite(1, acMessageBuffer, ulWriteCount, &ulWriteCount);
    return(*this);
}

/*--------------------------------------------------------------------------------------*\
 * UApm : displayMessage()                                                              *
\*--------------------------------------------------------------------------------------*/
UApm                   &UApm::displayMessage(int iMessage, int iParameterCount, int iParam1, int iParam2)
{
    char    acLoadBuffer[MESSAGEBUFFER_LENGTH];
    ULONG   ulMessageLength;
    ULONG   ulWriteCount;
    APIRET  apiretRc=NO_ERROR;

    memset(acLoadBuffer, '\0', sizeof(acLoadBuffer));
    memset(acMessageBuffer, '\0', sizeof(acMessageBuffer));
    apiretRc=DosGetMessage(0, 0, acLoadBuffer,
        sizeof(acLoadBuffer)-sizeof("APM0000I: "), iMessage,
        (PSZ)acMessageFileNls, &ulMessageLength);
    if(apiretRc!=NO_ERROR)
        {
                                        /* For internal messages 0 to 2 just skip trying to load
                                           a NLS message as we already know it can't be found as
                                           the messagefile not found was the cause to display that
                                           messages. Directly load the Us English message which we
                                           know exists as the Us English message file was shipped */
        if(iMessage>ERROR_NLSMSG_INVALID)
            {
                                        /* In case we have NLS message file found, but got an error 
                                           error retrieving messages inform the user and try the Us 
                                           message file instead. If we're already using the Us 
                                           message file inform the user about that fatal error too */
            if(iStatusFlag & FLAG_NLS_MESSAGES_FOUND)
                {
                sprintf(acMessageBuffer, "APM0002E: Can't load message %d from %s.\r\n"
                    "          The messagefile could be found,  but its contents seem to be invalid.\r\n"
                    "          If that file was shipped with APM/2, please inform the author, other-\r\n"
                    "          wise a 3rd party has done the translation and needs to correct it.", iMessage, acMessageFileNls);
                ulWriteCount=strlen(acMessageBuffer);
                apiretRc=DosWrite(1, acMessageBuffer, ulWriteCount, &ulWriteCount);
                }
            }
        memset(acLoadBuffer, '\0', sizeof(acLoadBuffer));
        apiretRc=DosGetMessage(0, 0, acLoadBuffer,
            sizeof(acLoadBuffer)-sizeof("APM0000I: "), iMessage,
            (PSZ)acMessageFileUs, &ulMessageLength);
        if(apiretRc==NO_ERROR)
            {
            if(iMessage>ERROR_NLSMSG_INVALID)
                {
                strcpy(acMessageBuffer, "\r\n          In English the message would be:");
                ulWriteCount=strlen(acMessageBuffer);
                apiretRc=DosWrite(1, acMessageBuffer, ulWriteCount, &ulWriteCount);
                }
            }
        else
            {
                                        /* Message couldn't be loaded from Us message file also,
                                           inform user */
            sprintf(acMessageBuffer, "\r\nAPM0002E: Can't load message %d from %s.\r\n"
                "          As this messagefile is part of the shipped code, this is a fatal flaw\r\n"
                "          so please inform the author!\r\n", iMessage, acMessageFileUs);
            ulWriteCount=strlen(acMessageBuffer);
            apiretRc=DosWrite(1, acMessageBuffer, ulWriteCount, &ulWriteCount);
            }
        strcpy(acMessageBuffer, "\r\n\r\n");
        ulWriteCount=strlen(acMessageBuffer);
        apiretRc=DosWrite(1, acMessageBuffer, ulWriteCount, &ulWriteCount);
        }
    if(apiretRc==NO_ERROR)
        {
        char   *pcTemp;
                                        /* Replace $$ by $ */
        pcTemp=acLoadBuffer;
        for( ; *pcTemp!='\0'; pcTemp++)
            if((*pcTemp=='$') && ((*(pcTemp+1))=='$'))
                {
                *pcTemp='%';
                pcTemp++;
                strcpy(pcTemp, pcTemp+1);
                }
                                        /* DosGetMessage() returns message number only for error and
                                           warning messages, so we have to adjust them ourselves */
        strcpy(acMessageBuffer, acLoadBuffer);
        if(!memcmp(acLoadBuffer, "APM", sizeof("APM")-1))
                                        /* We have a error or warning message */
            sprintf(acLoadBuffer, "%s%04d%c: %s",
                "APM", iMessage, acMessageBuffer[9], &acMessageBuffer[11]);
        else
            {
                                        /* We have an information or special message (which
                                           we don't want a message number for */
            if(acMessageBuffer[0]=='X')
                sprintf(acLoadBuffer, "%s", &acMessageBuffer[2]);
            else
                sprintf(acLoadBuffer, "%s%04d%c: %s",
                    "APM", iMessage, acMessageBuffer[0], &acMessageBuffer[2]);
            }
        if(iParameterCount==0)
            strcpy(acMessageBuffer, acLoadBuffer);
        else if(iParameterCount==1)
            sprintf(acMessageBuffer, acLoadBuffer, iParam1);
        else if(iParameterCount==2)
            sprintf(acMessageBuffer, acLoadBuffer, iParam1, iParam2);
                                        /* STDOUT file handle is numerically 1 for all
                                           OS/2 processes */
#define STDOUT                          0x00000001


        ulWriteCount=strlen(acMessageBuffer);
        apiretRc=DosWrite(1, acMessageBuffer, ulWriteCount, &ulWriteCount);
        }
    return(*this);
}

/*--------------------------------------------------------------------------------------*\
 * UApm : checkCommandlineOption()                                                      *
 *        Check if the commandline contains the passed parameter pcOption within the    *
 *        commandline. This parameter is either a single or a composite argument and    *
 *        may be case sensitive. When specified multiple times, the last one will be    *
 *        returned. If the 2nd parameter of a composite parameter is not found, -1 will *
 *        be returned.                                                                  *
\*--------------------------------------------------------------------------------------*/
char                   *UApm::checkCommandlineOption(char *pcOption, BOOL bCompositeOption, BOOL bCaseSensitive)
{
    TIB    *ptib;                       /* Thread information block */
    PIB    *ppib;                       /* Process information block */
    char   *pcNextParameter;            /* Pointer to the current commandline parameter */
    char   *pcOptionFound=0;            /* (Composite) option found */
    char   *pcOptionReturned=0;         /* Returned option */

    DosGetInfoBlocks(&ptib, &ppib);
                                        /* The commandline immediately follows the (fully qualified)
                                           path the application was invoked from, we just now
                                           get a fresh copy as a previous match has modified it */
    strcpy(pcCommandline, strchr(ppib->pib_pchcmd, '\0')+1);
                                        /* Loop as long as commandline parameters are available and
                                           we haven't found it yet */
    for(pcNextParameter=pcCommandline;
        (pcNextParameter!=0 && *pcNextParameter!='\0');
        pcNextParameter=strchr(++pcNextParameter, ' '))
        {
                                        /* Skip spaces and see if - or / starts a commandline
                                           parameter */
        while(*pcNextParameter==' ')
            pcNextParameter++;
        if((*pcNextParameter!='/') && (*pcNextParameter!='-'))
            continue;
                                        /* Skip - or / of parameter */
        pcNextParameter++;
                                        /* Test for /"pcOption" or -"pcOption" to get a
                                           option name */
        if(bCaseSensitive==FALSE)
            {
            char   *pcS;
            char   *pcD;

            pcOptionFound=pcNextParameter;
            for(pcS=pcNextParameter, pcD=pcOption;
                *pcS!='\0' && *pcD!='\0';
                pcS++, pcD++)
                if((*pcS | 0x20)!=(*pcD | 0x20))
                    {
                    pcOptionFound=0;
                    break;
                    }
                                        /* An option can only be match successfully if the
                                           string we compare has the same length */
            if((*pcD!='\0') || ((*pcS!=' ') && (*pcS!='\0')))
                pcOptionFound=0;
            }
        else
            {
            char   *pcS=pcNextParameter+strlen(pcOption);

            if(strncmp(pcNextParameter, pcOption, strlen(pcOption))==0)
                pcOptionFound=pcNextParameter;
            if((*pcS!=' ') && (*pcS!='\0'))
                pcOptionFound=0;
            }
                                        /* For composite parameters check for next one */
        if((bCompositeOption==TRUE) && (pcOptionFound!=0))
            {
            char   *pcCompositeOption=0;
            ULONG   ulCounter=0;

                                        /* Find next parameter */
            pcNextParameter=strchr(pcNextParameter, ' ');
            if(pcNextParameter==0)
                return((char *)0xFFFFFFFF);
            while(*pcNextParameter==' ')
                pcNextParameter++;
            if(*pcNextParameter=='\0')
                return((char *)0xFFFFFFFF);
                                        /* For e.g. HPFS filenames, the option may be enclosed 
                                           in quotes (") */
            if(*pcNextParameter=='"')
                {
                pcNextParameter++;
                while((*(pcNextParameter+ulCounter)!='"') && (*(pcNextParameter+ulCounter)!='\0'))
                    ulCounter++;
                }
            else
                {
                while((*(pcNextParameter+ulCounter)!=' ') && (*(pcNextParameter+ulCounter)!='\0'))
                    ulCounter++;
                }
                                        /* Ensure correct termination of filename */
            *(pcNextParameter+ulCounter)='\0';
                                        /* Save the composite option found */
            pcOptionFound=pcNextParameter;
            }
        if(pcOptionFound!=0)
            pcOptionReturned=pcOptionFound;
        }
    return(pcOptionReturned);
}

/*--------------------------------------------------------------------------------------*\
 * UApm : displayAPMError()                                                             *
 *        Verbose errors returned by APM$ DosDevIOCtl() calls                           *
\*--------------------------------------------------------------------------------------*/
UApm                   &UApm::displayAPMError(int iApiRet, int iReturnCode)
{
                                        /* Display APM$ return codes as defined in toolkit */
    displayMessage(MSG_APM_ERROR, 1, iApiRet);
    if((iReturnCode>=POWER_NOERROR) && (iReturnCode<=POWER_DISENGAGED))
        displayMessage(MSG_APM_ERROR_0X0+iReturnCode);
    else
        displayMessage(MSG_APM_ERROR_UNKNOWN);
    displayMessage(MSG_APM_ERROR_EOL);
    return(*this);
}

/*--------------------------------------------------------------------------------------*\
 * UApm : getAsBCD()                                                                    *
 *        Return the passed binary as a BCD. To avoid overflow, the passed binary must  *
 *        not exceep 0x5F5E0FF which equals 99999999 in BCD.                            *
\*--------------------------------------------------------------------------------------*/
ULONG                   UApm::getAsBCD(ULONG ulBinary)
{
    ULONG   ulBCD=0;
    int     iTemp=0;
 
                                        /* Loop for 7 nibbles. The logic is not too difficult:
                                           0xFF <=> 255 , and 255 in decimal is calculated as
                                           ((((0+0)*0xA+0)*0xA+2)*0xA+5)*0xA+5
                                                 !      1      !      !      !
                                                 +------+------+------+------+-- ... digits 
                                           so we just do the inverse calculation */
    for(iTemp=1; iTemp<(sizeof(ulBinary)<<1); iTemp++)
        {
        ulBCD|=((ulBinary % 0xA)<<28);
        ulBCD>>=4;
        ulBinary/=0xA;
        }
    return(ulBCD);
}

int     main(int argc, char *argv[])
{
    int         iReturnCode=NO_ERROR;
    UApm       *puApm=0;

                                        /* Create APM/2 object */
    try 
        {
        puApm=new UApm(argc, argv);
        if(puApm!=0)
            {
            (*puApm).initialize().process();
            }
        }
    catch(int iError)
        {
        if(puApm!=0)
            delete puApm;
        return(iError);
        }
    if(puApm!=0)
        {
        iReturnCode=puApm->iReturnCode;
        delete puApm;
        }
    return(iReturnCode);
}
