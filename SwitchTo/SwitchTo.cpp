/***********************************************************************\
 *                            SwitchTo.cpp                             *
 *                 Copyright (C) by Stangl Roman, 1998                 *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
 * SwitchTo.cpp SwitchTo/2 source file.                                *
 *                                                                     *
 * SwitchTo/2 switches to the Window List entry specified at the       *
 * commandline with the following syntax:                              *
 *                                                                     *
 * Syntax:  SWITCHTO SessionName                                       *
 * Where:   SessionName ... Name of Window List entry to switch to     * 
 *                          causing it to get into foreground.         *
 *                                                                     *
\***********************************************************************/

/***********************************************************************\
 * Technical Design:                                                   *
 *                                                                     *
 * This programs shows 2 ways to call the PM API from AVIO (advanced   *
 * video), in other words commandline-based, programs. There are at    *
 * least 2 methods how to do that:                                     *
 * 1) The AVIO application can lanch a copy of itself and define that  *
 *    that copy is started into a PM environment (by specifying that   *
 *    to DosStartSession() or WinStartApp().                           *
 * 2) The AVIO application can change its session type to PROG_PM, and *
 *    while the PM environment is happy with that you can still view   *
 *    the STDOUT output (e.g. when starting it from a command          *
 *    processor).                                                      *
 *                                                                     *
 * The important point is that not the application's type defined in   *
 * the executable (by the linker) is relevant if the PM environment    *
 * can be initialized but the environment the application runs in.     *
\***********************************************************************/

#pragma strings(readonly)

#include    <malloc.h>
#include    <stdio.h>
#include    <string.h>

#if defined(LAUNCHCOPY) || defined(CHANGETYPE)
#else
    #error Either macro LANCHCOPY or CHANGETYPE must be defined
    #error Invoke NMAKE with parameters "LAUNCHCOPY=1" or "CHANGETYPE=1"
#endif

#include    "SwitchTo.hpp"

                                        /* Required for PMPRINTF package, when used for
                                           debugging as we're only using the Subsystem 
                                           C Runtime library (we don't need the full blown
                                           runtime, so let's save some space) */
#ifdef  DEBUG
long    time(long *ptimer)
{
    return(*ptimer);
}
#endif  /* DEBUG */

/****************************************************************************************\
 * Class: SwitchTo                                                                      *
\****************************************************************************************/
/*--------------------------------------------------------------------------------------*\
 * SwitchTo : Constructor(s)                                                            *
\*--------------------------------------------------------------------------------------*/
SwitchTo::SwitchTo(int argc, char *argv[]) :
          iResult(ERR_NOERROR),
          ulProcessType(0),
          pcSessionName(0),
          hab(0),
          hmq(0),
          hwndFrame(0),
          hwndClient(0)
{
                                        /* Query environment including our process type */
    checkEnvironment();
#ifdef  LAUNCHCOPY
    if(ulProcessType!=PROG_PM)
        {
        ULONG   ulMessageLength=0;

                                        /* If we're a non-PM process we are a windowcompatible
                                           process so we can do printing to STDOUT */
        displayMessage(NLSMSG_COPYRIGHT1);
        displayMessage(NLSMSG_COPYRIGHT2);
                                        /* The name of the session we want to switch into
                                           foreground must be specified, otherwise we don't
                                           know what to do */
        if(argc!=2)
            {
            displayMessage(NLSMSG_USAGE1);
            displayMessage(NLSMSG_USAGE2);
            displayMessage(NLSMSG_USAGE3);
            throw(ERR_SYNTAX);
            }
        else
            {
            pcSessionName=(char *)malloc(strlen(argv[1])+3);
            if(strchr(argv[1], ' '))
                {
                strcpy(pcSessionName, "\"");
                strcat(pcSessionName, argv[1]);
                strcat(pcSessionName, "\"");
                }
            else 
                strcpy(pcSessionName, argv[1]);
            relaunchSelf();
            free(pcSessionName);
            pcSessionName=0;
            }
        }
    else
        {
                                        /* If we're a PM process we can't do much except
                                           PM calls, at least not printing to STDOUT (in the
                                           sense that STDOUT does exists, but will not be
                                           displayed on a window normally, e.g. redirection
                                           is of course possible) */
        if(argc!=2)
            throw(ERR_SYNTAX);
        else
            {
            int     iSwitchResult=0;

            pcSessionName=argv[1];
            iSwitchResult=(int)switchtoApplication();
            if(iSwitchResult!=NO_ERROR)
                throw(iSwitchResult);
            }
        }
#elif   CHANGETYPE
                                        /* If we're running as a windowed or fullscreen
                                           AVIO session change our session type to PM */
    if((ulProcessType==PROG_FULLSCREEN) || (ulProcessType==PROG_WINDOWABLEVIO))
        {
        ulProcessType=PROG_PM;
        setProcessType(ulProcessType);
        }
                                        /* Now we're a PM process, but we can still output to
                                           stdout, which will get displayed in the command 
                                           window when launched from such a program */
    displayMessage(NLSMSG_COPYRIGHT1);
    displayMessage(NLSMSG_COPYRIGHT2);
                                        /* The name of the session we want to switch into
                                           foreground must be specified, otherwise we don't
                                           know what to do */
    if(argc!=2)
        {
        displayMessage(NLSMSG_USAGE1);
        displayMessage(NLSMSG_USAGE2);
        displayMessage(NLSMSG_USAGE3);
        throw(ERR_SYNTAX);
        }
    else
        {
                                        /* Now switch to the requested application */
        displayMessage(NLSMSG_INVOKE1, pcSessionName);
        pcSessionName=argv[1];
        iResult=(int)switchtoApplication();
        if(iResult==NO_ERROR)
            {
            iResult=ERR_NOERROR;
            displayMessage(NLSMSG_INVOKE3, pcSessionName);
            }
        else
            {
            iResult=ERR_SWITCHFAILED;
            displayMessage(NLSMSG_INVOKE4, pcSessionName);
            throw(iResult);
            }
        }
#endif
}

/*--------------------------------------------------------------------------------------*\
 * SwitchTo : checkEnvironment(void)                                                    *
\*--------------------------------------------------------------------------------------*/
SwitchTo               &SwitchTo::checkEnvironment(void)
{
    TIB    *ptib;
    PIB    *ppib;
    char   *pcTemp;
 
                                        /* Get fully qualified path we were loaded from 
                                           and save it */
    DosGetInfoBlocks(&ptib, &ppib);
    ulProcessType=ppib->pib_ultype;
    DosQueryModuleName(ppib->pib_hmte, sizeof(acPathExecutable), acPathExecutable);
    strcpy(acPathMessage, acPathExecutable);
    pcTemp=strrchr(acPathMessage, '.');
    if(pcTemp!=0)
        strcpy(pcTemp, ".msg");    
    return(*this);
}

#ifdef  CHANGETYPE
/*--------------------------------------------------------------------------------------*\
 * SwitchTo : setProcessType(ULONG ulProcessType)                                       *
\*--------------------------------------------------------------------------------------*/
SwitchTo               &SwitchTo::setProcessType(ULONG ulProcessType)
{
    TIB    *ptib;
    PIB    *ppib;
    char   *pcTemp;

                                        /* Get process information block and change process
                                           type */
    DosGetInfoBlocks(&ptib, &ppib);
    ppib->pib_ultype=ulProcessType;
    return(*this);
}
#endif  /* CHANGETYPE */

#ifdef  LAUNCHCOPY
/*--------------------------------------------------------------------------------------*\
 * SwitchTo : relaunchSelf(char *pcSessionName)                                         *
\*--------------------------------------------------------------------------------------*/
SwitchTo               &SwitchTo::relaunchSelf(void)
{
    HQUEUE      hqueueSelf=0;
    ULONG       ulPidSelf=0;
    STARTDATA   startdataSelf;
    char        acObjectBuffer[CCHMAXPATH];
    PID         pidSelf=0;
    ULONG       ulSessionIDSelf=0;
    APIRET      apiretRc=NO_ERROR;
    PSZ         pszBuffer=0;
    REQUESTDATA requestdataSelf={ 0 };
    ULONG       ulDataLen=0;
    BYTE        bPriority=0;
    RESULTCODE *presultcodeSelf;

    displayMessage(NLSMSG_INVOKE1, pcSessionName);
                                        /* Create queue to receive return codes of our
                                           PM instance into the VIO instance */
    apiretRc=DosCreateQueue(&hqueueSelf, QUE_FIFO | QUE_CONVERT_ADDRESS, SWITCHTO_QUEUE);
    apiretRc=DosOpenQueue(&ulPidSelf, &hqueueSelf, SWITCHTO_QUEUE);
                                        /* Relaunch ourself but force a PM environment */
    memset(&startdataSelf, 0, sizeof(startdataSelf));
    startdataSelf.Length=sizeof(startdataSelf);
    startdataSelf.Related=SSF_RELATED_CHILD;
    startdataSelf.FgBg=SSF_FGBG_BACK;
    startdataSelf.TraceOpt=SSF_TRACEOPT_NONE;
    startdataSelf.PgmTitle="SwitchTo/2";
    startdataSelf.PgmName=(PSZ)acPathExecutable;
    startdataSelf.PgmInputs=(PBYTE)pcSessionName;    
    startdataSelf.TermQ=SWITCHTO_QUEUE;
    startdataSelf.Environment=0;    
    startdataSelf.InheritOpt=SSF_INHERTOPT_SHELL;    
    startdataSelf.SessionType=SSF_TYPE_PM;    
    startdataSelf.ObjectBuffer=(PSZ)acObjectBuffer;    
    startdataSelf.ObjectBuffLen=sizeof(acObjectBuffer);    
    apiretRc=DosStartSession(&startdataSelf, &ulSessionIDSelf, &pidSelf);
    if(apiretRc!=NO_ERROR)
        displayMessage(NLSMSG_INVOKE2);
    else
        {
        pszBuffer="";
        requestdataSelf.pid=ulPidSelf;

        apiretRc=DosReadQueue(hqueueSelf, &requestdataSelf, &ulDataLen, (PPVOID)&pszBuffer,
            0L, DCWW_WAIT, &bPriority, 0);
                                        /* When the queue contains data to read
                                           our PM instance has finished */
        if(apiretRc==NO_ERROR)
            {
            presultcodeSelf=(RESULTCODE *)pszBuffer;

            if(presultcodeSelf->usCodeResult==NO_ERROR)
                {
                iResult=ERR_NOERROR;
                displayMessage(NLSMSG_INVOKE3, pcSessionName);
                }
            else
                {
                iResult=ERR_SWITCHFAILED;
                displayMessage(NLSMSG_INVOKE4, pcSessionName);
                }
            }
        else
            {
            iResult=ERR_READINGQUEUE;
            displayMessage(NLSMSG_INVOKE5);
            }
        DosFreeMem(pszBuffer);
        }
    apiretRc=DosCloseQueue(hqueueSelf);
    return(*this);
}
#endif  /* LAUNCHCOPY */

/*--------------------------------------------------------------------------------------*\
 * SwitchTo : switchtoApplication(void)                                                 *
\*--------------------------------------------------------------------------------------*/
ULONG                   SwitchTo::switchtoApplication(void)
{
    ULONG       ulRc=0;
    ULONG       ulCount=0;
    ULONG       ulIndex=0;
    SWBLOCK    *pswblkCurrent=0;
    SWENTRY    *pswentryCurrent=0;
    ULONG       ulSwitchResult=PMERR_INVALID_SWITCH_HANDLE;
    ULONG       flStyle=FCF_TITLEBAR | FCF_TASKLIST | FCF_SYSMENU;
    QMSG        qmsg;

    hab=WinInitialize(0);
    hmq=WinCreateMsgQueue(hab, 0);
                                        /* Uncomment the following code an you will see a
                                           sample window (which used the default window
                                           procedure from PM, so the client won't draw)
    WinRegisterClass(hab, "SwitchTo", WinDefWindowProc, 0, 0);
    hwndFrame=WinCreateStdWindow(HWND_DESKTOP, WS_VISIBLE, &flStyle, "SwitchTo", "SwitchTo", 0, NULLHANDLE, 1, &hwndClient);
    WinSetWindowPos(hwndFrame, HWND_TOP, 10, 10, 200, 200, SWP_SHOW | SWP_SIZE | SWP_MOVE | SWP_ZORDER);
    WinShowWindow(hwndFrame, TRUE);
    while(WinGetMsg(hab, &qmsg, 0, 0, 0))
        WinDispatchMsg(hab, &qmsg);
    WinDestroyWindow(hwndFrame);
                                         */
    ulCount=WinQuerySwitchList(hab, 0, 0);
    pswblkCurrent=(PSWBLOCK)malloc(ulCount=((ulCount+1)*sizeof(SWENTRY)+sizeof(ULONG)));
    ulCount=WinQuerySwitchList(hab, pswblkCurrent, ulCount);
    for(ulIndex=0; ulIndex<ulCount; ulIndex++)
        {
        pswentryCurrent=&pswblkCurrent->aswentry[ulIndex];
#ifdef  DEBUG
        printf("Window List #%ld : %s\n", ulIndex, pswentryCurrent->swctl.szSwtitle);
#endif
        if(!strcmp(pswentryCurrent->swctl.szSwtitle, pcSessionName))
            {
            ulSwitchResult=WinSwitchToProgram(pswentryCurrent->hswitch);
            break;
            }
        }
    free(pswblkCurrent);
    WinDestroyMsgQueue(hmq);
    WinTerminate(hab);
    return(ulSwitchResult);
}

/*--------------------------------------------------------------------------------------*\
 * SwitchTo : displayMessage(int iMessage)                                              *
\*--------------------------------------------------------------------------------------*/
SwitchTo               &SwitchTo::displayMessage(int iMessage, char *pcParameter)
{
                                        /* Table of message parameters */
    char   *acParameters[]={ 0, 0 };
    char    acMessage[2048]="";         /* Message text loaded from message file */
    char   *pcMessageFormatted;         /* Message text formatted to common syntax */
    ULONG   ulParameters=0;             /* Number of message parameters passed */
    ULONG   ulMessageLength=0;          /* Length of loaded and parsed message */
    APIRET  apiretRc=NO_ERROR;  

    if(pcParameter!=0)
        acParameters[ulParameters++]=pcParameter;
    apiretRc=DosGetMessage((CHAR **)acParameters, ulParameters, acMessage, 
        sizeof(acMessage)-sizeof("SW20000: "), (ULONG)iMessage, 
        (PSZ)acPathMessage, &ulMessageLength);
    if(apiretRc!=NO_ERROR)
        {
        strcpy(acMessage, "SW20000E: Can't load message\r\n");
        ulMessageLength=strlen(acMessage);
        }
    DosWrite(STDOUT, acMessage, ulMessageLength, &ulMessageLength);
    return(*this);
}

int     main(int argc, char *argv[])
{
    int         iResult=0;
    SwitchTo   *pSwitchTo=0;

    try {
        pSwitchTo=new SwitchTo(argc, argv);
        }
                                        /* Catch error and return with it */
    catch (int iError)
        {
        DosExit(EXIT_PROCESS, (ULONG)iError);
        }
    iResult=pSwitchTo->iResult;
    delete pSwitchTo;
                                        /* Exit with return code 0 */
    DosExit(EXIT_PROCESS, iResult);
}
