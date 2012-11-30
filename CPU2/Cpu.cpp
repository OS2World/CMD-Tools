/***********************************************************************\
 *                                CPU/2                                *
 *    Copyright (C) by Roman Stangl (Roman_Stangl@at.ibm.com), 1998    *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
\***********************************************************************/
#pragma     strings(readonly)

#include    "CPU.hpp"

/*--------------------------------------------------------------------------------------*\
 * UCpu : UCpu()                                                                        *
\*--------------------------------------------------------------------------------------*/
                        UCpu::UCpu(int iArgc, char *apcArgv[]) :
                              iReturnCode(NO_ERROR),
                              iStatusFlag(FLAG_NULL),
                              pcCommandline(0),
                              hmoduleDOSCALL1(0),
                              pfnDosGetProcessorStatus(0),
                              pfnDosSetProcessorStatus(0),
                              pfnDosPerfSysCall(0),
                              pcpuutilCurrent(0),
                              pcpuutilPrevious(0),
                              puticksCurrent(0),
                              puticksPrevious(0),
                              puperformanceStack(0)
{
    this->iArgc=iArgc;
    this->ppcArgv=apcArgv;
    memset(acMessageFileUs, '\0', sizeof(acMessageFileUs));
    memset(acMessageFileNls, '\0', sizeof(acMessageFileNls));
    memset(acMessageBuffer, '\0', sizeof(acMessageBuffer));
}

/*--------------------------------------------------------------------------------------*\
 * UCpu : ~UCpu()                                                                       *
\*--------------------------------------------------------------------------------------*/
                        UCpu::~UCpu(void)
{
    if(hmoduleDOSCALL1)
        DosFreeModule(hmoduleDOSCALL1);
    if(pcCommandline!=0)
        free(pcCommandline);
}

/*--------------------------------------------------------------------------------------*\
 * UCpu : intialize()                                                                   *
\*--------------------------------------------------------------------------------------*/
UCpu                   &UCpu::initialize(void)
{
    TIB    *ptib;                       /* Thread information block */
    PIB    *ppib;                       /* Process information block */
    char   *pcTemp;

                                        /* Query the fully qualified path CPU/2 was loaded from.
                                           Once we know the language to use we replace ".exe"
                                           by e.g. "xx.msg" where xx is the NLS message file,
                                           but for now we assume English as this one is shipped
                                           with CPU/2 for sure */
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
 * UCpu : process()                                                                     *
 *        The CPU/2 main function, throws a ERROR_* in case of errors                   *
\*--------------------------------------------------------------------------------------*/
UCpu                   &UCpu::process(void)
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
                                               {  27L, "Us" },
                                               {  31L, "Nl" },
                                               {  32L, "Be" },
                                               {  33L, "Fr" },
                                               {  34L, "Es" },
                                               {  39L, "It" },
                                               {  41L, "Gr" },
                                               {  43L, "Gr" },
                                               {  44L, "Us" },
                                               {  45L, "Dk" },
                                               {  46L, "Se" },
                                               {  47L, "No" },
                                               {  49L, "Gr" },
                                               {  61L, "Us" },
                                               {  64L, "Us" },
                                               {  81L, "Jp" },
                                               {  99L, "Us" },
                                               { 353L, "Us" },
                                               { 358L, "Fi" },
                                               { 972L, "Il" },
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
 * UCpu : matchParameters()                                                             *
 *        Match the commandline parameters and throw ERROR_* exceptions if inconsisten- *
 *        cies are found                                                                *
\*--------------------------------------------------------------------------------------*/
UCpu                   &UCpu::matchParameters(void)
{
    UOption uoption[]={
                        { FLAG_SHOWCONFIGURATION, "Verbose"     },
                        { FLAG_CPUONLINE        , "Online"      },
                        { FLAG_CPUOFFLINE       , "Offline"     },
                        { FLAG_PERFORMANCE      , "Performance" },
                      };
    int     iTemp=0;
    char   *pcParameter=0;


                                        /* Check for /Verbose option */
    if(checkCommandlineOption(uoption[0].acOption))
        iStatusFlag|=FLAG_SHOWCONFIGURATION;
                                        /* Check for /Online ... /Offline options, which
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
                                        /* Check for /Online x or /Offline x option */
    pcParameter=checkCommandlineOption("Online", TRUE);
    if(pcParameter==0)
        pcParameter=checkCommandlineOption("Offline", TRUE);
                                        /* If /Online or /Offline was specified, then a parameter must
                                           also be given */
    if(pcParameter==(char *)0xFFFFFFFF)
        throw((int)ERROR_REQUEST_BAD);
                                        /* At least one of the unique "request" parameters must
                                           be specified */
    if(!(iStatusFlag & FLAG_SHOWCONFIGURATION) && ((iStatusFlag & FLAG_MASK_REQUEST)==FLAG_NULL))
        throw((int)ERROR_REQUEST_MISSING);
    return(*this);
}

/*--------------------------------------------------------------------------------------*\
 * UCpu : processParameters()                                                           *
 *        Process the commandline parameters and throw ERROR_* exceptions if problems   *
 *        are found.                                                                    *
\*--------------------------------------------------------------------------------------*/
UCpu                   &UCpu::processParameters(void)
{
    ULONG   ulNumProcessors=(ULONG)-1;
    APIRET  apiretRc=NO_ERROR;
    ULONG   ulNumProcessor=0;
    ULONG   ulStatusCurrent=0;
    ULONG   ulStatusNew=0;

                                        /* Try to dynamically link the SMP API */
    loadProcessorStatusAPI();
                                        /* Query number of installed processors */
    apiretRc=DosQuerySysInfo(QSV_NUMPROCESSORS, QSV_NUMPROCESSORS, &ulNumProcessors, sizeof(ulNumProcessors));
    if(apiretRc!=NO_ERROR)
        ulNumProcessors=(ULONG)-1;
                                        /* Verbose the CPU configuration */
    if(iStatusFlag & FLAG_SHOWCONFIGURATION)
        {
                                        /* Verbose requires the SMP API loaded */
        if((pfnDosGetProcessorStatus==0) || (pfnDosGetProcessorStatus==0))
            throw((int)ERROR_KERNELAPI);
                                        /* Query number of installed processors */
        if(ulNumProcessors==(ULONG)-1)
            throw((int)ERROR_SYSINFO);
                                        /* Do /Verbose output */
        displayMessage(MSG_VERBOSE_STATUS_HEADER);
        for(ulNumProcessor=1; ulNumProcessor<=ulNumProcessors; ulNumProcessor++)
            {
            apiretRc=(pfnDosGetProcessorStatus)(ulNumProcessor, &ulStatusCurrent);
            if(apiretRc==ERROR_INVALID_PARAMETER)
                break;
            if(apiretRc==NO_ERROR)
                {
                if(ulStatusCurrent)
                    displayMessage(MSG_VERBOSE_STATUS_ONLINE, 1, (int)ulNumProcessor);
                else
                    displayMessage(MSG_VERBOSE_STATUS_OFFLINE, 1, (int)ulNumProcessor);
                continue;
                }
            displayMessage(ERROR_STATUS, 1, (int)apiretRc);
            }
        displayMessage(MSG_NEWLINE);
                                        /* We do expect a status for all processors installed */
        if((--ulNumProcessor)!=ulNumProcessors)
            {
            throw((int)ERROR_STATUS_INVALID);
            }
        }
                                        /* Set a prozessor online or offline */
    if((iStatusFlag & FLAG_CPUONLINE) || (iStatusFlag & FLAG_CPUOFFLINE))
        {
                                        /* On/Offline requires the SMP API loaded */
        if((pfnDosGetProcessorStatus==0) || (pfnDosGetProcessorStatus==0))
            throw((int)ERROR_KERNELAPI);
        if(iStatusFlag & FLAG_CPUONLINE)
            {
            ulNumProcessor=atoi(checkCommandlineOption("Online", TRUE));
            ulStatusNew=PROCESSOR_ONLINE;
            }
        if(iStatusFlag & FLAG_CPUOFFLINE)
            {
            ulNumProcessor=atoi(checkCommandlineOption("Offline", TRUE));
            ulStatusNew=PROCESSOR_OFFLINE;
            }
                                        /* Query number of installed processors */
        if(ulNumProcessors==(ULONG)-1)
            throw((int)ERROR_SYSINFO);
                                        /* Check that requested processor is in range. Only 2 to n 
                                           is valid (yes 2!, CPUMONPM also does not allow to offline 
                                           CPU 1, so even if it's called SMP it isn't completely
                                           symmetric as I would expect CPU n being able to substitute 
                                           CPU 1 then) */
        if((ulNumProcessor<=1) || (ulNumProcessor>ulNumProcessors))
            {
            displayMessage(MSG_ONOFFLINE_ERROR_RANGE, 2, (int)ulNumProcessor, (int)ulNumProcessors);
            throw((int)ERROR_ONOFFLINE_RANGE);
            }
                                        /* Now, let's try what the user requested */
        apiretRc=(pfnDosSetProcessorStatus)(ulNumProcessor, ulStatusNew);
        if(apiretRc!=NO_ERROR)
            {
            displayMessage(MSG_ONOFFLINE_SET_ERROR, 2, (int)apiretRc, (int)ulNumProcessor);
            throw((int)ERROR_SET_RETRY);
            }
        apiretRc=(pfnDosGetProcessorStatus)(ulNumProcessor, &ulStatusCurrent);
        if((apiretRc!=NO_ERROR) || (ulStatusCurrent!=ulStatusNew))
            {
            displayMessage(MSG_ONOFFLINE_SET_DENIED);
            throw((int)ERROR_SET_RETRY);
            }
        else
            {
                                        /* Verbose processor status again */
            displayMessage(MSG_VERBOSE_STATUS_HEADER);
            for(ulNumProcessor=1; ulNumProcessor<=ulNumProcessors; ulNumProcessor++)
                {
                apiretRc=(pfnDosGetProcessorStatus)(ulNumProcessor, &ulStatusCurrent);
                if(apiretRc==ERROR_INVALID_PARAMETER)
                    break;
                if(apiretRc==NO_ERROR)
                    {
                    if(ulStatusCurrent)
                        displayMessage(MSG_VERBOSE_STATUS_ONLINE, 1, (int)ulNumProcessor);
                    else
                        displayMessage(MSG_VERBOSE_STATUS_OFFLINE, 1, (int)ulNumProcessor);
                    continue;
                    }
                displayMessage(ERROR_STATUS, 1, (int)apiretRc);
                }
            displayMessage(MSG_NEWLINE);
                                        /* We do expect a status for all processors installed */
            if((--ulNumProcessor)!=ulNumProcessors)
                {
                throw((int)ERROR_STATUS_INVALID);
                }
            }
        }
                                        /* Display the CPU utilization */
    if(iStatusFlag & FLAG_PERFORMANCE)
        {
        ULONG           ulInitialize=TRUE;
        ULONG           ulProcessor=0;
        ULONG           ulStackPointer=0;
        UPerformance   *puperformanceStackCpu=0;
        ULONG           ulWriteCount=0;
        double          dTimerDelta;
        double          dConvert;

                                        /* Verbose requires the Performance API loaded */
        if(pfnDosPerfSysCall==0)
            throw((int)ERROR_PERFSYSCALL);
                                        /* Initialize the counters. Spezial thanks to
                                           Scott E. Garfinkle (seg) for providing me that
                                           into. Without that, under Aurora one would get
                                           nonsense data */
        apiretRc=pfnDosPerfSysCall(CMD_KI_ENABLE, 0, 0, 0);
        if(apiretRc!=NO_ERROR)
            throw((int)ERROR_PERFSYSRESULT);
                                        /* Query number of installed processors, if that
                                           couldn't be determined, likely we're on a UP
                                           OS/2 version */
        if(ulNumProcessors==(ULONG)-1)
            ulNumProcessors=1;
                                        /* Allocate CPUUTIL structures (per processor) */
        pcpuutilCurrent=new(CPUUTIL[ulNumProcessors]);
        pcpuutilPrevious=new(CPUUTIL[ulNumProcessors]);
        memset(pcpuutilCurrent, 0xCC, sizeof(CPUUTIL)*ulNumProcessors);
        memset(pcpuutilPrevious, 0xDD, sizeof(CPUUTIL)*ulNumProcessors);
                                        /* Allocate UTicks structures (per processor) */
        puticksCurrent=new(UTicks[ulNumProcessors]);
        puticksPrevious=new(UTicks[ulNumProcessors]);
        memset(puticksCurrent, 0xAA, sizeof(UTicks)*ulNumProcessors);
        memset(puticksPrevious, 0xBB, sizeof(UTicks)*ulNumProcessors);
                                        /* Allocate performance history (per processor) */
        puperformanceStack=new(UPerformance[ulNumProcessors*PERFORMANCE_STACK]);
        memset(puperformanceStack, 0, sizeof(UPerformance)*ulNumProcessors*PERFORMANCE_STACK);
                                        /* Do /Performance output */
        displayMessage(MSG_PERFORMANCE_HEADER);
                                        /* Initialize */
        apiretRc=pfnDosPerfSysCall(CMD_KI_KLRDCNT, (ULONG)pcpuutilPrevious, 0, 0);
        if(apiretRc!=NO_ERROR)
            throw((int)ERROR_PERFSYSRESULT);
        for(ulNumProcessor=1; ulNumProcessor<=ulNumProcessors; ulNumProcessor++)
            {
            ulProcessor=ulNumProcessor-1;
            puticksPrevious[ulProcessor].dTotal=LL2F(pcpuutilPrevious[ulProcessor].ulTotalHigh,
                pcpuutilPrevious[ulProcessor].ulTotalLow);
            puticksPrevious[ulProcessor].dIdle=LL2F(pcpuutilPrevious[ulProcessor].ulIdleHigh,
                pcpuutilPrevious[ulProcessor].ulIdleLow);
            puticksPrevious[ulProcessor].dBusy=LL2F(pcpuutilPrevious[ulProcessor].ulBusyHigh,
                pcpuutilPrevious[ulProcessor].ulBusyLow);
            puticksPrevious[ulProcessor].dIntr=LL2F(pcpuutilPrevious[ulProcessor].ulIntrHigh,
                pcpuutilPrevious[ulProcessor].ulIntrLow);
            }
                                        /* Let some time pass */
        DosSleep(1000);
                                        /* Now display CPU performance until user presses
                                           space bar to quit */
        while(TRUE)
            {
            UPerformance   *puperformanceTemp;

                                        /* If we're not initializing then shift the current top of stack of
                                           the CPU performance stack downwards, as the current performance
                                           will be put onto top of stack newly */
            if(ulInitialize!=TRUE)
                {
                for(ulStackPointer=0; ulStackPointer<(PERFORMANCE_STACK-1); ulStackPointer++)
                    {
                    puperformanceTemp=&puperformanceStack[(ulNumProcessors*(ulStackPointer))];
                    puperformanceStackCpu=&puperformanceStack[(ulNumProcessors*(ulStackPointer+1))];
                    memcpy((void *)puperformanceTemp, (void *)puperformanceStackCpu, sizeof(UPerformance)*ulNumProcessors);
                    }
                }

                                        /* Query currrent CPU timer ticks and then calculate the relative
                                           amounts of individual times where the CPU performance went into.
                                           puperformanceStack is a dynamically allocated array of ulNumProcessors
                                           columns and (PERFORMANCE_STACK-1) rows, so in order to calculate data
                                           for one processors for a given stack, we access the current element
                                           array by calculating its offset by puperformanceStackCpu */
            apiretRc=pfnDosPerfSysCall(CMD_KI_KLRDCNT, (ULONG)pcpuutilCurrent, 0, 0);
            if(apiretRc!=NO_ERROR)
                throw((int)ERROR_PERFSYSRESULT);
            for(ulNumProcessor=1; ulNumProcessor<=ulNumProcessors; ulNumProcessor++)
                {
                ulProcessor=ulNumProcessor-1;
                puperformanceStackCpu=&puperformanceStack[ulProcessor+(ulNumProcessors*(PERFORMANCE_STACK-1))];
                puticksCurrent[ulProcessor].dTotal=LL2F(pcpuutilCurrent[ulProcessor].ulTotalHigh,
                    pcpuutilCurrent[ulProcessor].ulTotalLow);
                puticksCurrent[ulProcessor].dIdle=LL2F(pcpuutilCurrent[ulProcessor].ulIdleHigh,
                    pcpuutilCurrent[ulProcessor].ulIdleLow);
                puticksCurrent[ulProcessor].dBusy=LL2F(pcpuutilCurrent[ulProcessor].ulBusyHigh,
                    pcpuutilCurrent[ulProcessor].ulBusyLow);
                puticksCurrent[ulProcessor].dIntr=LL2F(pcpuutilCurrent[ulProcessor].ulIntrHigh,
                    pcpuutilCurrent[ulProcessor].ulIntrLow);
                dTimerDelta=puticksCurrent[ulProcessor].dTotal-
                    puticksPrevious[ulProcessor].dTotal;
                                        /* We have to do tricks during the percentage calculation, because
                                           we loose information during conversion from float to integer, and
                                           even checking for rounding can sum to 99% (e.g. 98.392 Idle,
                                           1.175 Busy and 0.434 Intr time results in 98, 1, 0 in summ 99%).
                                           To make things simple, we calculate the Intr time by subtracting
                                           others from 100 */
                puperformanceStackCpu->ulIdle=dConvert=
                    ((puticksCurrent[ulProcessor].dIdle-puticksPrevious[ulProcessor].dIdle)/dTimerDelta*100.0);
                if(dConvert-(double)(ULONG)dConvert>=0.5)
                    puperformanceStackCpu->ulIdle++;
                puperformanceStackCpu->ulBusy=dConvert=
                    ((puticksCurrent[ulProcessor].dBusy-puticksPrevious[ulProcessor].dBusy)/dTimerDelta*100.0);
                if(dConvert-(double)(ULONG)dConvert>=0.5)
                    puperformanceStackCpu->ulBusy++;
                                        /* Commented out 
                    puperformanceStackCpu->ulIntr=dConvert=
                        ((puticksCurrent[ulProcessor].dIntr-puticksPrevious[ulProcessor].dIntr)/dTimerDelta*100.0);
                    if(dConvert-(double)(ULONG)dConvert>=0.5)
                        puperformanceStackCpu->ulIntr++;
                                         */
                puperformanceStackCpu->ulIntr=
                    100-(puperformanceStackCpu->ulIdle+puperformanceStackCpu->ulBusy);
                                        /* Shift current performance to previous one */
                puticksPrevious[ulProcessor]=puticksCurrent[ulProcessor];
                }
                                        /* If we're initializing then we need to copy the CPU performance
                                           on top of stack to all elements lower on stack to calculate the
                                           correct average then (letting the default 0 to be used would
                                           force down the average) */
            if(ulInitialize==TRUE)
                {
                puperformanceStackCpu=&puperformanceStack[(ulNumProcessors*(PERFORMANCE_STACK-1))];
                for(ulStackPointer=0; ulStackPointer<(PERFORMANCE_STACK-1); ulStackPointer++)
                    {
                    puperformanceTemp=&puperformanceStack[(ulNumProcessors*(ulStackPointer))];
                    memcpy((void *)puperformanceTemp, (void *)puperformanceStackCpu, sizeof(UPerformance)*ulNumProcessors);
                    }
                ulInitialize=FALSE;
                }
                                        /* Now display for each processor the performance which we determine
                                           as the average for the last PERFORMANCE_STACK meassurements */
            for(ulNumProcessor=1; ulNumProcessor<=ulNumProcessors; ulNumProcessor++)
                {
                ULONG   ulIdleAverage=0;
                ULONG   ulBusyAverage=0;
                ULONG   ulIntrAverage=0;

                ulProcessor=ulNumProcessor-1;
                for(ulStackPointer=0; ulStackPointer<(PERFORMANCE_STACK); ulStackPointer++)
                    {
                    puperformanceTemp=&puperformanceStack[ulProcessor+(ulNumProcessors*(ulStackPointer))];
                    ulIdleAverage+=puperformanceTemp->ulIdle;
                    ulBusyAverage+=puperformanceTemp->ulBusy;
                    ulIntrAverage+=puperformanceTemp->ulIntr;
                    }
                ulIdleAverage/=PERFORMANCE_STACK;
                ulBusyAverage/=PERFORMANCE_STACK;
                                        /* Again, we may loose accuracy when doing that division, so
                                           we calculate the Intr time by subtracting Idle and Busy from 100 */
                                        /* Commented out
                    ulIntrAverage/=PERFORMANCE_STACK;
                                         */
                ulIntrAverage=100-(ulIdleAverage+ulBusyAverage);
                displayMessage(MSG_PERFORMANCE_CPU, 4, ulNumProcessor,
                    ulIdleAverage, ulBusyAverage, ulIntrAverage);
                                        /* Add a space to display next processor's data afterwards,
                                           except for the last one (to prevent a linefeed if it was
                                           the 4th one on 80 columns displays) */
                if(ulNumProcessor!=ulNumProcessors)
                    {
                    ulWriteCount=strlen(" ");
                    apiretRc=DosWrite(1, " ", ulWriteCount, &ulWriteCount);
                    }
                }
                                        /* After last processor performance was displayed, return to
                                           the beginning of the line (which for 80 characters allows
                                           up to 4 processors to displayed correctly) */
            ulWriteCount=strlen("\r");
            apiretRc=DosWrite(1, "\r", ulWriteCount, &ulWriteCount);
                                        /* Access the keyboard focus and test if the user pressed
                                           the space bar if any character is available, which let's
                                           us know he wants to quit now. As the keyboard can do at
                                           maximum about 32 keys/s checking the next 128 available
                                           keys should be enough (in case the user has hold down a
                                           key while we slept) */
            HKBD        hkbdFocus;
            KBDKEYINFO  kbdkeyinfoChar;

            apiretRc=KbdGetFocus(IO_WAIT, 0);
            if(apiretRc==NO_ERROR)
                {
                ULONG   ulSpaceBarFound=FALSE;
                ULONG   ulTemp;

                for(ulTemp=0; ulTemp<128; ulTemp++)
                    {
                    apiretRc=KbdCharIn(&kbdkeyinfoChar, IO_NOWAIT, 0);
                    if(apiretRc==NO_ERROR)
                        if(kbdkeyinfoChar.chChar==' ')
                            {
                            apiretRc=KbdFreeFocus(0);
                            ulSpaceBarFound=TRUE;
                            break;
                            }
                    }
                if(ulSpaceBarFound==TRUE)
                    break;
                }
            apiretRc=KbdFreeFocus(0);
                                        /* Let CPU perform */
            DosSleep(1000);
            }
                                        /* Cleanup */
        displayMessage(MSG_PERFORMANCE_FOOTER);
        delete puperformanceStack;
        delete puticksCurrent;
        delete puticksPrevious;
        delete pcpuutilCurrent;
        delete pcpuutilPrevious;
        puperformanceStack=0;
        puticksCurrent=0;
        puticksPrevious=0;
        pcpuutilCurrent=0;
        pcpuutilPrevious=0;
        }
    return(*this);
}

/*--------------------------------------------------------------------------------------*\
 * UCpu : loadProcessorStatusAPI()                                                      *
 *        Load the SMP Processor Status APIs, which allows CPU/2 to be loaded even on a *
 *        non-SMP OS/2 (if we were statically linking it from the SMP toolkits          *
 *        OS2386.LIB, the application can't be loaded on OS/2 installations where the   *
 *        Kernel does not provide those APIs).                                          *
\*--------------------------------------------------------------------------------------*/
UCpu                   &UCpu::loadProcessorStatusAPI(void)
{
    UCHAR       ucBuffer[CCHMAXPATH];   /* Buffer for possible return codes from DLL loading */
 
    APIRET      apiretRc;

                                        /* Do while an error is found */
    while(TRUE)
        {
        apiretRc=DosLoadModule((PCSZ)ucBuffer, sizeof(ucBuffer)-1, "DOSCALL1", &hmoduleDOSCALL1);
        if(apiretRc!=NO_ERROR)
            break;
        apiretRc=DosQueryProcAddr(hmoduleDOSCALL1, ORDDOSGETPROCESSORSTATUS, 
            NULL, (PFN *)(&pfnDosGetProcessorStatus));
        if(apiretRc!=NO_ERROR)
            break;
        apiretRc=DosQueryProcAddr(hmoduleDOSCALL1, ORDDOSSETPROCESSORSTATUS,
            NULL, (PFN *)(&pfnDosSetProcessorStatus));
        if(apiretRc!=NO_ERROR)
            break;
                                        /* If we're done, don't forget to break out of loop */
        break;
        }
    if(apiretRc!=NO_ERROR)
        {
        pfnDosGetProcessorStatus=0;
        pfnDosSetProcessorStatus=0;
        }
                                        /* Do while an error is found */
    while(TRUE)
        {
        apiretRc=DosQueryProcAddr(hmoduleDOSCALL1, ORDDOSPERFSYSCALL,
            NULL, (PFN *)(&pfnDosPerfSysCall));
        if(apiretRc!=NO_ERROR)
            break;
                                        /* If we're done, don't forget to break out of loop */
        break;
        }
    if(apiretRc!=NO_ERROR)
        {
        pfnDosPerfSysCall=0;
        }
    return(*this);
}

/*--------------------------------------------------------------------------------------*\
 * UCpu : displayMessage()                                                              *
\*--------------------------------------------------------------------------------------*/
UCpu                   &UCpu::displayMessage(int iMessage, int iParameterCount, 
                                             int iParam1, int iParam2, int iParam3, int iParam4)
{
    char    acLoadBuffer[MESSAGEBUFFER_LENGTH];
    ULONG   ulMessageLength;
    ULONG   ulWriteCount;
    APIRET  apiretRc=NO_ERROR;

    memset(acLoadBuffer, '\0', sizeof(acLoadBuffer));
    memset(acMessageBuffer, '\0', sizeof(acMessageBuffer));
    apiretRc=DosGetMessage(0, 0, acLoadBuffer,
        sizeof(acLoadBuffer)-sizeof("CPU0000I: "), iMessage,
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
                sprintf(acMessageBuffer, "CPU0002E: Can't load message %d from %s.\r\n"
                    "          The messagefile could be found,  but its contents seem to be invalid.\r\n"
                    "          If that file was shipped with CPU/2, please inform the author, other-\r\n"
                    "          wise a 3rd party has done the translation and needs to correct it.", iMessage, acMessageFileNls);
                ulWriteCount=strlen(acMessageBuffer);
                apiretRc=DosWrite(1, acMessageBuffer, ulWriteCount, &ulWriteCount);
                }
            }
        memset(acLoadBuffer, '\0', sizeof(acLoadBuffer));
        apiretRc=DosGetMessage(0, 0, acLoadBuffer,
            sizeof(acLoadBuffer)-sizeof("CPU0000I: "), iMessage,
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
            sprintf(acMessageBuffer, "\r\nCPU0002E: Can't load message %d from %s.\r\n"
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
                                        /* Replace $$ by % */
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
        if(!memcmp(acLoadBuffer, "CPU", sizeof("CPU")-1))
                                        /* We have a error or warning message */
            sprintf(acLoadBuffer, "%s%04d%c: %s",
                "CPU", iMessage, acMessageBuffer[9], &acMessageBuffer[11]);
        else
            {
                                        /* We have an information or special message (which
                                           we don't want a message number for) */
            if(acMessageBuffer[0]=='X')
                sprintf(acLoadBuffer, "%s", &acMessageBuffer[2]);
                                        /* Or a message we don't want a CRLF */
            else if(acMessageBuffer[0]=='P')
                {
                char   *pcTemp;

                sprintf(acLoadBuffer, "%s", &acMessageBuffer[2]);
                pcTemp=strchr(acLoadBuffer, '\0');
                while(--pcTemp>=acLoadBuffer)
                    {
                    if((*pcTemp!='\r') && (*pcTemp!='\n'))
                        break;
                    *pcTemp='\0';
                    }
                }
                                        /* Or a "normal" one */
            else
                sprintf(acLoadBuffer, "%s%04d%c: %s",
                    "CPU", iMessage, acMessageBuffer[0], &acMessageBuffer[2]);
            }
        if(iParameterCount==0)
            strcpy(acMessageBuffer, acLoadBuffer);
        else if(iParameterCount==1)
            sprintf(acMessageBuffer, acLoadBuffer, iParam1);
        else if(iParameterCount==2)
            sprintf(acMessageBuffer, acLoadBuffer, iParam1, iParam2);
        else if(iParameterCount==3)
            sprintf(acMessageBuffer, acLoadBuffer, iParam1, iParam2, iParam3);
        else if(iParameterCount==4)
            sprintf(acMessageBuffer, acLoadBuffer, iParam1, iParam2, iParam3, iParam4);
                                        /* STDOUT file handle is numerically 1 for all
                                           OS/2 processes */
#define STDOUT                          0x00000001
        ulWriteCount=strlen(acMessageBuffer);
        apiretRc=DosWrite(1, acMessageBuffer, ulWriteCount, &ulWriteCount);
        }
    return(*this);
}

/*--------------------------------------------------------------------------------------*\
 * UCpu : checkCommandlineOption()                                                      *
 *        Check if the commandline contains the passed parameter pcOption within the    *
 *        commandline. This parameter is either a single or a composite argument and    *
 *        may be case sensitive. When specified multiple times, the last one will be    *
 *        returned. If the 2nd parameter of a composite parameter is not found, -1 will *
 *        be returned.                                                                  *
\*--------------------------------------------------------------------------------------*/
char                   *UCpu::checkCommandlineOption(char *pcOption, BOOL bCompositeOption, BOOL bCaseSensitive)
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
 * UCpu : getAsBCD()                                                                    *
 *        Return the passed binary as a BCD. To avoid overflow, the passed binary must  *
 *        not exceep 0x5F5E0FF which equals 99999999 in BCD.                            *
\*--------------------------------------------------------------------------------------*/
ULONG                   UCpu::getAsBCD(ULONG ulBinary)
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
    UCpu       *puCpu=0;

                                        /* Create CPU/2 object */
    try 
        {
        puCpu=new UCpu(argc, argv);
        if(puCpu!=0)
            {
            (*puCpu).initialize().process();
            }
        }
    catch(int iError)
        {
        if(puCpu!=0)
            delete puCpu;
        return(iError);
        }
    if(puCpu!=0)
        {
        iReturnCode=puCpu->iReturnCode;
        delete puCpu;
        }
    return(iReturnCode);
}
