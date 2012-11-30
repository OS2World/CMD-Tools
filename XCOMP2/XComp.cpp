/***********************************************************************\
 *                               XComp/2                               *
 *              Copyright (C) by Stangl Roman, 1999, 2002              *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
 * XComp.cpp    XComp/2 main code.                                     *
 *                                                                     *
\***********************************************************************/

/***********************************************************************\
 *                                                                     *
 * Dualthreaded compare design:                                        *
 *                                                                     *
 * Using the /MP commandline option performs a dualthreaded            *
 * comparison, that is thread 1 is responsible reading the source file * 
 * and thread  2 is responsible reading the target file. Thread 1 then *
 * does the actual comparison and descending to the next file.         *
 *                                                                     *
 * Thread 1:                        Thread 2:                          *
 *                                                                     *
 * Initially Thread 1 has created   Initially Thread 2 gets SEM_2      *
 * all semaphores and owns SEM_1    immediately after thread creation  *
 * and SEM_3                                                           *
 *                                                                     *
 * :loop                            :loop                              *
 *                                  Thread 2 waits for SEM_1           *
 * Thread 1 releases SEM_1 ------->                                    *
 * Thread 2 reads source file       Thread 2 now owns SEM_1            *
 * Thread 1 waits for SEM_2         Thread 2 reads target file         *
 *                         <------- Thread 2 releases SEM_2            *
 * Thread 1 now owns SEM_2          Thread 2 waits for SEM_3           *
 * Thread 1 releases SEM_3 ------->                                    *
 * Thread 1 waits for SEM_1         Thread 2 now owns SEM_3            *
 *                         <------- Thread 2 releases SEM_1            *
 * Thread 1 now owns SEM_1          Thread 2 waits for SEM_2           *
 * Thread 1 releases SEM_2 ------->                                    *
 * Thread 1 waits for SEM_3         Thread 2 owns SEM_2                *
 *                         <------- Thread 2 releases SEM_3            *
 * Thread 1 now owns SEM_3                                             *
 * goto loop                        goto loop                          *
 *                                                                     *
 * The logic used here may look like overly complex and one may notice *
 * that 2 semaphores may be enought, but field tests show that it      *
 * won't work that way. The reason I believe is that if 1 thread       *
 * executes a DosReleaseMutexSem(), DosRequestMutexSem() while another *
 * thread waits with DosRequestMutexSem(), the first thread may        *
 * immediately release and receive ownership without the 2'nd thread   *
 * running (and thus having a chance to receive ownership) at all.     *
 * (Assume here a single semaphore). Define the DEBUG macro to see     *
 * the synchronization in action.                                      *
 *                                                                     *
 * I thought that releasing a semaphore while another thread is        *
 * blocked on the same semaphore causes the current thread to preempt  *
 * and the other thread to execute. This does not seem to be true      *
 * (nor sufficient documented in the toolkit).                         *
 *                                                                     *
\***********************************************************************/
#pragma strings(readonly)

#ifdef  __WINDOWS__
#define __WIN32__
#endif  /* __WINDOWS__ */

#include    "XComp.hpp"

            XCOMP::XCOMP(int argc, char *argv[]) : argc(argc),
                                                   argv(argv),
                                                   iCompareBufferSize(0),
                                                   iSourcePathRootLen(0),
                                                   iTargetPathRootLen(0),
                                                   hfileSourceFile(0),
                                                   hfileTargetFile(0),
                                                   ulBytesReadSourceFile(0),
                                                   ulBytesReadTargetFile(0),
                                                   pbSourceData(0),
                                                   pbTargetData(0),
                                                   iCountProblemNone(0),
                                                   iCountProblemFindFiles(0),
                                                   iCountProblemFindDirs(0),
                                                   iCountProblemOpen(0),
                                                   iCountProblemCompare(0),
                                                   iCountProblemChkSum(0),
                                                   iCountProblemChkSum2Much(0),
                                                   ullSourceTotalRead(0),
                                                   ullTargetTotalRead(0),
                                                   ullSourceTotalMS(0),
                                                   ullTargetTotalMS(0),
                                                   ullTotalMS(0),
                                                   puProfilingSource(0),
                                                   puProfilingTarget(0),
                                                   puProfilingTotal(0),
                                                   apiretRcSource(0),
                                                   apiretRcTarget(0),
                                                   hmtxThread1(0),
                                                   hmtxThread2(0),
                                                   hmtxThread3(0),
                                                   pChkSumEntryRoot(0),
                                                   iStatusFlag(XCOMP_STATUS_ABORT)
{
}

            XCOMP::~XCOMP(void)
{
    if(iStatusFlag & XCOMP_STATUS_ABORT)
        return;
    if(iStatusFlag & XCOMP_STATUS_LOG)
        fstreamLogFile << endl;
    if(iStatusFlag & XCOMP_STATUS_CHKSUM)
        fstreamChkSumFile.close();
                                        /* Report performance data */
    ullTotalMS=puProfilingTotal->elapsedMilliSec();
                                        /* To calculate the throughput we take the number of bytes
                                           read, multiply that by 1000 and divide it by the time in
                                           milliseconds resulting in bytes/second. We then divide that
                                           by 1024 (>>10) to calculate kB out of bytes */
    if((iStatusFlag & XCOMP_STATUS_SOURCE_ONLY) && (ullSourceTotalMS!=0))
        {
        cout << MSG_THROUGHPUT ": Throughput Source "
            << ((ullSourceTotalRead/ullSourceTotalMS*1000)>>10)
            << "kB/s"
            << endl;
        if(iStatusFlag & XCOMP_STATUS_LOG)
            {
            fstreamLogFile << MSG_THROUGHPUT ": Throughput Source " << ((ullSourceTotalRead>>10)*1000/ullSourceTotalMS) << "kB/s" << endl;
            }
        }
    else if((ullSourceTotalMS!=0) && (ullTargetTotalMS!=0) && (ullTotalMS!=0))
        {
        cout << MSG_THROUGHPUT ": Throughput Source "
            << ((ullSourceTotalRead/ullSourceTotalMS*1000)>>10)
            << "kB/s, Destination "
            << ((ullTargetTotalRead/ullTargetTotalMS*1000)>>10)
            << "kB/s, Total "
            << ((((ullSourceTotalRead/ullTotalMS)+(ullTargetTotalRead/ullTotalMS))*1000)>>10)
            << "kB/s"
            << endl;
        if(iStatusFlag & XCOMP_STATUS_LOG)
            {
            fstreamLogFile << MSG_THROUGHPUT ": Throughput Source " << ((ullSourceTotalRead>>10)*1000/ullSourceTotalMS) << "kB/s, Destination " <<
                ((ullTargetTotalRead>>10)*1000/ullTargetTotalMS) << "kB/s, Total " <<
                (((ullSourceTotalRead+ullTargetTotalRead)>>10)*1000/ullTotalMS) << "kB/s" << endl;
            }
        }
    if(iCountProblemOpen!=0)
        {
        cout << MSG_PROBLEMOPEN ": " << iCountProblemOpen << " file(s) could not be opened. This may be caused by non existing or" << endl;
        cout << "locked files." << endl;
        if(iStatusFlag & XCOMP_STATUS_LOG)
            {
            fstreamLogFile << MSG_PROBLEMOPEN ": " << iCountProblemOpen << " file(s) could not be opened. This may be caused by non existing or" << endl;
            fstreamLogFile << "locked files." << endl;
            }
        }
    if(iCountProblemCompare!=0)
        {
        cout << MSG_PROBLEMCOMPARE ": " << iCountProblemCompare << " file(s) did not match. You don't have an exact copy of the source" << endl;
        cout << "files with the target files or a media may be defective. Not matching files are\nlisted above. Please check the medias and rerun the copy operation!" << endl;
        if(iStatusFlag & XCOMP_STATUS_LOG)
            {
            fstreamLogFile << MSG_PROBLEMCOMPARE ": " << iCountProblemCompare << " file(s) did not match. You don't have an exact copy of the source" << endl;
            fstreamLogFile << "files with the target files or a media may be defective. Not matching files are\nlisted above. Please check the medias and rerun the copy operation!" << endl;
            }
        }
    if(iCountProblemFindFiles!=0)
        {
        cout << MSG_PROBLEMFINDFILES ": " << iCountProblemFindFiles << " directory(ies) could not be opened. This may be caused by non" << endl;
        cout << "accessible directories due to ACLs (e.g. on HPFS386), an incorrect/outdated" << endl;
        cout << "file system driver (e.g CD-ROM) or file system corruption." << endl;
        if(iStatusFlag & XCOMP_STATUS_LOG)
            {
            fstreamLogFile << MSG_PROBLEMFINDFILES ": " << iCountProblemFindFiles << " directory(ies) could not be opened. This may be caused by non" << endl;
            fstreamLogFile << "accessible directories due to ACLs (e.g. on HPFS386), an incorrect/outdated" << endl;
            fstreamLogFile << "file system driver (e.g CD-ROM) or file system corruption." << endl;
            }
        }
    if(iCountProblemFindDirs!=0)
        {
        cout << MSG_PROBLEMFINDDIRS ": " << iCountProblemFindDirs << " subdirectory(ies) could not be descended into. This may be" << endl;
        cout << "caused by non accessible directories due to ACLs (e.g. on HPFS386), an" << endl;
        cout << "incorrect/outdated file system driver (e.g CD-ROM) or file system corruption." << endl;
        if(iStatusFlag & XCOMP_STATUS_LOG)
            {
            fstreamLogFile << MSG_PROBLEMFINDDIRS ": " << iCountProblemFindDirs << " subdirectory(ies) could not be descended into. This may be" << endl;
            fstreamLogFile << "caused by non accessible directories due to ACLs (e.g. on HPFS386), an" << endl;
            fstreamLogFile << "incorrect/outdated file system driver (e.g CD-ROM) or file system corruption." << endl;
            }
        }
    if((iCountProblemChkSum!=0) || (iCountProblemChkSum2Much!=0))
        {
        cout << MSG_PROBLEMCHKSUM ": Calculated Checksum of " << iCountProblemChkSum << " files(s) did not match Checksum or filename" << endl;
        cout << "recorded in Checksum file. " << iCountProblemChkSum2Much << " file(s) could be found in Checksum file, but no" << endl;
        cout << "longer in the filesystem. This may be caused by changes to the directories" << endl;
        cout << "and/or file(s), or data corruption on the media the file resides on (e.g. on" << endl;
        cout << "CR-ROM medias)." << endl;
        if(iStatusFlag & XCOMP_STATUS_LOG)
            {
            fstreamLogFile << MSG_PROBLEMCHKSUM ": Calculated Checksum of " << iCountProblemChkSum << " files(s) did not match Checksum or filename" << endl;
            fstreamLogFile << "recorded in Checksum file. " << iCountProblemChkSum2Much << " file(s) could be found in Checksum file, but no" << endl;
            fstreamLogFile << "longer in the filesystem. This may be caused by changes to the directories" << endl;
            fstreamLogFile << "and/or file(s), or data corruption on the media the file resides on (e.g. on" << endl;
            fstreamLogFile << "CR-ROM medias)." << endl;
            }
        }
    if(iStatusFlag & XCOMP_STATUS_SOURCE_ONLY)
        cout << MSG_COMPARETOTAL ": Checksum calculated for " << (iCountProblemNone-iCountProblemOpen-iCountProblemCompare-iCountProblemChkSum) << " file(s) "
            "successfully, calculated for\n"<< iCountProblemNone << " file(s) totally." << endl;
    else
        cout << MSG_COMPARETOTAL ": " << (iCountProblemNone-iCountProblemOpen-iCountProblemCompare-iCountProblemChkSum) << " file(s) compared "
            "successfully, "<< iCountProblemNone << " file(s) compared totally." << endl;
    if(iStatusFlag & XCOMP_STATUS_LOG)
        {
        if(iStatusFlag & XCOMP_STATUS_SOURCE_ONLY)
            fstreamLogFile << MSG_COMPARETOTAL ": Checksum calculated for " << (iCountProblemNone-iCountProblemOpen-iCountProblemCompare-iCountProblemChkSum) << " file(s) "
                "successfully, calculated for\n"<< iCountProblemNone << " file(s) totally." << endl;
        else
            fstreamLogFile << MSG_COMPARETOTAL ": " << (iCountProblemNone-iCountProblemOpen-iCountProblemCompare-iCountProblemChkSum) << " file(s) compared "
                "successfully, "<< iCountProblemNone << " file(s) compared totally." << endl;
        }
    if(iStatusFlag & XCOMP_STATUS_SIMULATE)
        {
        cout << MSG_SIMULATION ": " << "Warning! This was a simulation run, no actual comparisons were done!" << endl;
        if(iStatusFlag & XCOMP_STATUS_LOG)
             fstreamLogFile << MSG_SIMULATION ": " << "Warning! This was a simulation run, no actual comparisons were done!" << endl;
        }
                                        /* If requested, stop on exit */
    if(iStatusFlag & XCOMP_STATUS_PAUSE_ONEXIT)
        getKey(TRUE, FALSE);
                                        /* Cleanup */
    if(pbSourceData!=0)
        delete pbSourceData;
    if(pbTargetData!=0)
        delete pbTargetData;
    if(puProfilingSource!=0)
        delete puProfilingSource;
    if(puProfilingTarget!=0)
        delete puProfilingTarget;
    if(puProfilingTotal!=0)
        delete puProfilingTotal;
#ifdef  __OS2__
    if(hmtxThread1!=NULLHANDLE)
        DosCloseMutexSem(hmtxThread1);
    if(hmtxThread2!=NULLHANDLE)
        DosCloseMutexSem(hmtxThread2);
    if(hmtxThread3!=NULLHANDLE)
        DosCloseMutexSem(hmtxThread3);
#endif  /* __OS2__ */
#ifdef  __WIN32__
    if(hmtxThread1!=NULL)
        CloseHandle(hmtxThread1);
    if(hmtxThread2!=NULL)
        CloseHandle(hmtxThread2);
    if(hmtxThread3!=NULL)
        CloseHandle(hmtxThread3);
#endif  /* __WIN32__ */
    if(iStatusFlag & XCOMP_STATUS_LOG)
        fstreamLogFile.close();
}

void        XCOMP::initialize(void)
{
#ifdef  __OS2__
    TIB    *ptib=0;
    PIB    *ppib=0;
#endif  /* __OS2__ */
    int     iArgument;
    APIRET  apiretRc;

    memset(acExecutableFile, '\0', sizeof(acExecutableFile));
    memset(acLogFile, '\0', sizeof(acLogFile));
    memset(acChkSumFile, '\0', sizeof(acChkSumFile));
    memset(acSourcePath, '\0', sizeof(acSourcePath));
    memset(acSourceFiles, '\0', sizeof(acSourceFiles));
    memset(acTargetPath, '\0', sizeof(acTargetPath));
                                        /* Suspend system error handler (so that
                                           errors are reported to the caller directly
                                           via Returncodes */
#ifdef  __OS2__
    DosError(FERR_DISABLEHARDERR);
#endif  /* __OS2__ */
                                        /* Get fully qualified path to this executable */
#ifdef  __OS2__
    DosGetInfoBlocks(&ptib, &ppib);
    if(ppib!=0)
        DosQueryModuleName(ppib->pib_hmte, sizeof(acExecutableFile), acExecutableFile);
#endif  /* __OS2__ */
#ifdef  __WIN32__
    GetModuleFileName(NULL, acExecutableFile, sizeof(acExecutableFile));
#endif  /* __WIN32__ */
    if(strlen(acExecutableFile)==0)
        throw(UEXCP(APPLICATION_PREFIX, ERR_PIB, "Invalid process information block"));

                                        /* Check commandline options */
    cout << endl;
    if(argc<2)
        throw(UEXCP(APPLICATION_PREFIX, ERR_ARGUMENTS, "Too few commandline arguments specified"));
    if(strlen(argv[1])>CCHMAXPATH)
        throw(UEXCP(APPLICATION_PREFIX, ERR_SOURCEPATH, "Source path too long"));
    if((argc>=3) && (strlen(argv[2])>CCHMAXPATH))
        throw(UEXCP(APPLICATION_PREFIX, ERR_TARGETPATH, "Target path too long"));
    strcpy((char *)acSourcePath, argv[1]);
    if((argc>=3) && (argv[2][0]!='/') && (argv[2][0]!='-'))
        strcpy((char *)acTargetPath, argv[2]);
    else
        iStatusFlag|=XCOMP_STATUS_SOURCE_ONLY;
                                        /* Check for additional commandline arguments */
    for(iArgument=2; iArgument<argc; iArgument++)
        {
        strupr(argv[iArgument]);
                                        /* Verbose */
        if((strstr(argv[iArgument], "-VERBOSE")) ||
            (strstr(argv[iArgument], "/VERBOSE")))
            iStatusFlag|=XCOMP_STATUS_VERBOSE;
                                        /* More verbose */
        if((strstr(argv[iArgument], "--VERBOSE")) ||
            (strstr(argv[iArgument], "//VERBOSE")))
            iStatusFlag|=XCOMP_STATUS_VERBOSE2;
                                        /* Line number information for non-fatal messages */
        if((strstr(argv[iArgument], "-LINE")) ||
            (strstr(argv[iArgument], "/LINE")))
            iStatusFlag|=XCOMP_STATUS_LINENUMBER;
                                        /* Recursive file-search only, without any comparisons */
        if((strstr(argv[iArgument], "-SIMULATE")) ||
            (strstr(argv[iArgument], "/SIMULATE")))
            iStatusFlag|=XCOMP_STATUS_SIMULATE;
                                        /* Multithread option */
        if((strstr(argv[iArgument], "-MP")) ||
            (strstr(argv[iArgument], "/MP")))
            {
            iStatusFlag|=XCOMP_STATUS_MP;
#ifdef  __OS2__
            if(DosCreateMutexSem(NULL, &hmtxThread1, 0, TRUE)!=NO_ERROR)
                iStatusFlag&=(~XCOMP_STATUS_MP);
            if(DosCreateMutexSem(NULL, &hmtxThread2, 0, FALSE)!=NO_ERROR)
                iStatusFlag&=(~XCOMP_STATUS_MP);
            if(DosCreateMutexSem(NULL, &hmtxThread3, 0, TRUE)!=NO_ERROR)
                iStatusFlag&=(~XCOMP_STATUS_MP);
#endif  /* __OS2__ */
#ifdef  __WIN32__
            hmtxThread1=CreateMutex(NULL, TRUE, NULL);
            if(GetLastError()!=NO_ERROR)
                iStatusFlag&=(~XCOMP_STATUS_MP);
            hmtxThread2=CreateMutex(NULL, FALSE, NULL);
            if(GetLastError()!=NO_ERROR)
                iStatusFlag&=(~XCOMP_STATUS_MP);
            hmtxThread3=CreateMutex(NULL, TRUE, NULL);
            if(GetLastError()!=NO_ERROR)
                iStatusFlag&=(~XCOMP_STATUS_MP);
#endif  /* __WIN32__ */
            if(iStatusFlag & XCOMP_STATUS_MP)
                _beginthread((void(_Optlink *)(void *))xcompThread, 0, 65536, (void *)this);
            SLEEP(0);
            }
                                        /* Log option */
        if((strstr(argv[iArgument], "-LOG")) ||
            (strstr(argv[iArgument], "/LOG")))
            {
            char   *pcExtension;

            iStatusFlag|=XCOMP_STATUS_LOG;
            if((argv[iArgument][4]==':') && (argv[iArgument][5]!='\0'))
                {
                strcpy(acLogFile, &argv[iArgument][5]);
                }
            else
                {
                if(getenv("XCOMP"))
                    {
                    strcpy(acLogFile, getenv("XCOMP"));
                    }
                else
                    {
                    strcpy(acLogFile, acExecutableFile);
                    pcExtension=strrchr(acLogFile, '.');
              if(pcExtension)
                        strcpy(pcExtension, ".Log");
                    else
                        strcat(acLogFile, ".Log");
                    }
                }
            fstreamLogFile.open(acLogFile, ios::out);
            if(!fstreamLogFile)
                {
                UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_LOGFILEOPEN, "", __LINE__);

                *puexcpError << "Opening logfile " << acLogFile << " failed";
                throw(*puexcpError);
                }
            }
                                        /* CHK option */
        if((strstr(argv[iArgument], "-CHK")) ||
            (strstr(argv[iArgument], "/CHK")))
            {
            char    acChkSumFileCopy[CCHMAXPATH];
            char   *pcExtension=0;

            iStatusFlag|=XCOMP_STATUS_CHKSUM;
            if((argv[iArgument][4]==':') && (argv[iArgument][5]!='\0'))
                {
                memset(acChkSumFileCopy, '\0', sizeof(acChkSumFileCopy));
                strcpy(acChkSumFile, &argv[iArgument][5]);
#ifdef  __OS2__
                apiretRc=DosQueryPathInfo(acChkSumFile, FIL_QUERYFULLNAME, acChkSumFileCopy, sizeof(acChkSumFileCopy));
#endif  /* __OS2__ */
#ifdef  __WIN32__
                SetLastError(NO_ERROR);
                apiretRc=GetFullPathName(acChkSumFile, sizeof(acChkSumFileCopy), acChkSumFileCopy, NULL);
                apiretRc=GetLastError();
#endif  /* __WIN32__ */
                                        /* If possible, try to get the fully qualified path
                                           to the Checksum file */
                if(apiretRc==NO_ERROR)
                    strcpy(acChkSumFile, acChkSumFileCopy);
                                        /* Check if the Checksum file has the extension .MD5,
                                           we then run in (Linux's) MD5SUM compatibility mode */
                pcExtension=strrchr(acChkSumFile, '.');
                if((pcExtension!=0) && !stricmp(pcExtension, ".MD5"))
                    iStatusFlag|=XCOMP_STATUS_MD5_MD5SUM;
                }
                                        /* First, try to open the Checksum file in read
                                           mode and read it. If that fails we know we have 
                                           to write it first */
            fstreamChkSumFile.open(acChkSumFile, ios::in);
            if(!fstreamChkSumFile)
                {
                fstreamChkSumFile.open(acChkSumFile, ios::out);
                if(!fstreamChkSumFile)
                    {
                    UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_CHKSUMFILEOPEN, "", __LINE__);

                    *puexcpError << "Opening Checksum file \"" << acChkSumFile << "\" (/CHK option) failed";
                    throw(*puexcpError);
                    }
                iStatusFlag|=XCOMP_STATUS_CHKSUM_LOG;
                }
            }
                                        /* No pause after miscompares option */
        if((strstr(argv[iArgument], "-/!C")) ||
            (strstr(argv[iArgument], "/!C")))
            iStatusFlag|=XCOMP_STATUS_NOPAUSEERRCOMP;
                                        /* No pause after destination file not found option */
        if((strstr(argv[iArgument], "-/!F")) ||
            (strstr(argv[iArgument], "/!F")))
            iStatusFlag|=XCOMP_STATUS_NOPAUSEERRFIND;
                                        /* No recursion into subdirectories */
        if((strstr(argv[iArgument], "-/!S")) ||
            (strstr(argv[iArgument], "/!S")))
            iStatusFlag|=XCOMP_STATUS_NORECURSION;
                                        /* No beep for fatal errors */
        if((strstr(argv[iArgument], "-/!B")) ||
            (strstr(argv[iArgument], "/!B")))
            iStatusFlag|=XCOMP_STATUS_NOBEEP;
                                        /* Pause on exit */
        if((strstr(argv[iArgument], "-/P")) ||
            (strstr(argv[iArgument], "/P")))
            iStatusFlag|=XCOMP_STATUS_PAUSE_ONEXIT;
                                        /* Small buffer */
        if((strstr(argv[iArgument], "-TINY")) ||
            (strstr(argv[iArgument], "/TINY")))
            iStatusFlag|=XCOMP_STATUS_SMALLBUFFER;
        }
    iStatusFlag&=(~XCOMP_STATUS_ABORT);
}

void        XCOMP::process(void)
{
    ULONG           ulMemorySize;
#ifdef  __WIN32__
    MEMORYSTATUS    msMemoryStatus;
#endif  /* __WIN32__ */

                                        /* Allocate timers */
    puProfilingSource=new UProfiling();
    puProfilingTarget=new UProfiling();
    puProfilingTotal=new UProfiling();
                                        /* Get available memory size */
    iCompareBufferSize=XCOMPDATASIZE;
#ifdef  __OS2__
    if(DosQuerySysInfo(QSV_TOTPHYSMEM, QSV_TOTPHYSMEM, &ulMemorySize, sizeof(ulMemorySize))==NO_ERROR)
#endif  /* __OS2__ */
#ifdef  __WIN32__
    GlobalMemoryStatus(&msMemoryStatus);
    ulMemorySize=msMemoryStatus.dwTotalPhys;
    if(ulMemorySize!=0);
#endif  /* __WIN32__ */
        {
                                        /* As the returned total physical memory for e.g. a 128MB
                                           SIMM is less than 128MB due to the memory hole between
                                           640kB and 1MB, just add 1MB (as a rule of thumb) */
        ulMemorySize+=(1<<20);
                                        /* Starting with 16MB, double buffer size whenever
                                           RAM doubles */
        if(ulMemorySize>=(1<<24))
            iCompareBufferSize<<=1;
        if(ulMemorySize>=(1<<25))
            iCompareBufferSize<<=1;
        if(ulMemorySize>=(1<<26))
            iCompareBufferSize<<=1;
        if(ulMemorySize>=(1<<27))
            iCompareBufferSize<<=1;
        if(ulMemorySize>=(1<<28))
            iCompareBufferSize<<=1;
        if(ulMemorySize>=(1<<29))
            iCompareBufferSize<<=1;
        if(ulMemorySize>=(1<<30))
            iCompareBufferSize<<=1;
        if(ulMemorySize>=(1<<31))
            iCompareBufferSize<<=1;
        }
                                        /* When requested, just use 2 64kB buffers */
    if(iStatusFlag & XCOMP_STATUS_SMALLBUFFER)
        iCompareBufferSize=(1<<16);
    pbSourceData=new BYTE[iCompareBufferSize];
    pbTargetData=new BYTE[iCompareBufferSize];
    iCountProblemFindFiles=iCountProblemFindDirs=iCountProblemOpen=iCountProblemCompare=0;
    verifySourcePath();
    verifyTargetPath();

    if(iStatusFlag & XCOMP_STATUS_SOURCE_ONLY)
        cout << "Calculating CRC32 and MD5 checksums qualified by " << acSourceFiles << endl;
    else
        cout << "Comparing files qualified by " << acSourceFiles << endl;
    cout << "  at Source path             " << acSourcePath << endl;
    if(!(iStatusFlag & XCOMP_STATUS_SOURCE_ONLY))
        cout << "  with Target path           " << acTargetPath << endl;
    if(iStatusFlag & XCOMP_STATUS_LOG)
        {
        time_t      timetCurrent;
        struct tm  *ptmCurrent;
        char        acTimeCurrent[81];

        fstreamLogFile << COPYRIGHT_1 << endl;
        fstreamLogFile << COPYRIGHT_2 << endl;
        fstreamLogFile << COPYRIGHT_3 << endl;
        time(&timetCurrent);
        ptmCurrent=localtime(&timetCurrent);
        strftime(acTimeCurrent, sizeof(acTimeCurrent)-1,
            "Execution from %a. %d.%b.%Y %H:%M", ptmCurrent);
        fstreamLogFile << acTimeCurrent << endl << endl;
        if(iStatusFlag & XCOMP_STATUS_SOURCE_ONLY)
            fstreamLogFile << "Calculating CRC32 and MD5 checksums" << endl;
        else
            fstreamLogFile << "Comparing files qualified by " << acSourceFiles << endl;
        fstreamLogFile << "  at Source path             " << acSourcePath << endl;
        if(!(iStatusFlag & XCOMP_STATUS_SOURCE_ONLY))
            fstreamLogFile << "  with Target path           " << acTargetPath << endl;
        }
    if(iStatusFlag & XCOMP_STATUS_CHKSUM)
        {
        if(iStatusFlag & XCOMP_STATUS_CHKSUM_LOG)
            {
            cout << "  writing Checksum file into " << acChkSumFile << endl;
            if(iStatusFlag & XCOMP_STATUS_LOG)
                fstreamLogFile << "  writing Checksum file into " << acChkSumFile << endl;
            }
        else
            {
            cout << "  reading Checksum file from " << acChkSumFile << endl;
            if(iStatusFlag & XCOMP_STATUS_LOG)
                fstreamLogFile << "  reading Checksum file from " << acChkSumFile << endl;
            }
        }
    if(iStatusFlag & XCOMP_STATUS_NORECURSION)
        {
        cout << "without recursion into subdirectories" << endl;
        if(iStatusFlag & XCOMP_STATUS_LOG)
            fstreamLogFile << "without recursion into subdirectories" << endl;
        }
    if(iStatusFlag & XCOMP_STATUS_MD5_MD5SUM)
        {
        cout << "running in MD5SUM compatibility mode" << endl;
        if(iStatusFlag & XCOMP_STATUS_LOG)
            fstreamLogFile << "running in MD5SUM compatibility mode" << endl;
        }
    cout << "using " << iCompareBufferSize << " bytes buffer size" << endl;
    if(iStatusFlag & XCOMP_STATUS_LOG)
        cout << "and logging into " << acLogFile << endl;
    cout << endl << endl;
    if(iStatusFlag & XCOMP_STATUS_LOG)
        fstreamLogFile << "using " << iCompareBufferSize << " bytes buffer size\n\n" << endl;
                                        /* Read the Checksum file if available */
    if(!(iStatusFlag & XCOMP_STATUS_CHKSUM_LOG))
        {
        readChkSumFile();
        iStatusFlag|=XCOMP_STATUS_CHKSUM_READ;
        }
                                        /* Do the matching */
    puProfilingTotal->start();
    searchFiles(acSourcePath, acSourceFiles, acTargetPath);
    puProfilingTotal->stop();
                                        /* Inform thread to shutdown */
    iStatusFlag|=XCOMP_STATUS_SHUTDOWN;
    cout << "                                                                               " << endl;
                                        /* Delete Checksum file read and report inconsistencies */
    deleteChkSumEntry(pChkSumEntryRoot);
}

void        XCOMP::usage(void)
{
    cout << COPYRIGHT_1 << endl;
    cout << COPYRIGHT_2 << endl;
    cout << COPYRIGHT_3 << endl;
    cout << "Use the XCOMP command to selectively compare groups of files located in two" << endl;
    cout << "directories, including all subdirectories.\n" << endl;
    cout << "Syntax:" << endl;
    cout << "  XCOMP [drive:\\|\\\\server\\][path\\]filename(s) [[drive:\\|\\\\server\\]path] [/!MP]" << endl;
    cout << "        [/LOG[:[drive:\\|\\\\server\\][path\\]file]] [/!C] [/!F] [/!S] [/!B] [/P]" << endl;
    cout << "        [/TINY] [/LINE] [/SIMULATE] [/CHK:[drive:\\|\\\\server\\][path\\]file]" << endl;
    cout << "Where:" << endl;
    cout << "  [drive:\\|\\\\server\\][path\\]filename(s)" << endl;
    cout << "                 Specifies the location and name of the Source file(s) to" << endl;
    cout << "                 compare from. You may specify a fully qualified path or" << endl;
    cout << "                 UNC path." << endl;
    cout << "  [[drive:\\|\\\\server\\]path]" << endl;
    cout << "                 Specifies the location of the Target path to compare with." << endl;
    cout << "                 You may specify a fully qualified path or UNC path." << endl;
    cout << "  [/MP]" << endl;
    cout << "                 Specifies that 1 thread reads the source and 1 thread reads" << endl;
    cout << "                 the target file. This improves througput when comparing from 2" << endl;
    cout << "                 different physical drives (e.g. CD-ROM and Harddisk)." << endl;
    cout << "  [/LOG[:[drive:\\|\\\\server\\][path\\]file]]" << endl;
    cout << "                 Specifies that XCOMP/2 logs all problems into a file specified" << endl;
    cout << "                 either by this parameter, or by the XCOMP environment variable" << endl;
    cout << "                 or into XCOMP.LOG (put into the directory XCOMP/2 was" << endl;
    cout << "                 installed into) otherwise." << endl;
    cout << "  [/!C]" << endl;
    cout << "                 By default, XCOMP/2 pauses at all mismatches. Specifying this" << endl;
    cout << "                 option allows XCOMP/2 just display the mismatch and continue" << endl;
    cout << "                 the comparison without a pause (e.g. useful when using the" << endl;
    cout << "                 /LOG option or output redirection)." << endl;
    cout << "  [/!F]" << endl;
    cout << "                 By default, XCOMP/2 pauses for files in the source location" << endl;
    cout << "                 that can't be found at the target location. Specifying this" << endl;
    cout << "                 this option allows XCOMP/2 just display the miss and continue" << endl;
    cout << "                 the comparison without a pause (e.g. useful when using the" << endl;
    cout << "                 /LOG option or output redirection)." << endl;
    cout << "  [/!S]" << endl;
    cout << "                 By default, XCOMP/2 recurses into all subdirectories it finds," << endl;
    cout << "                 specifying this option prevents XCOMP/2 doing that." << endl;
    cout << "  [/!B]" << endl;
    cout << "                 By default, XCOMP/2 will beep, when a severe error occurs" << endl;
    cout << "                 accessing a file at the source or target location. Specifying" << endl;
    cout << "                 this option will silence XCOMP/2 (e.g. useful when using the" << endl;
    cout << "                 /LOG option or output redirection)." << endl;
    cout << "  [/P]" << endl;
    cout << "                 Request XCOMP/2 to pause when it has finished."<< endl;
    cout << "  [/TINY]" << endl;
    cout << "                 2 64kB buffers are used instead of a percentage of total RAM." << endl;
    cout << "  [/LINE]" << endl;
    cout << "                 Display line number information for messages (useful for e.g." << endl;
    cout << "                 debugging)" << endl;
    cout << "  [/SIMULATE]" << endl;
    cout << "                 Does not compare the files (useful for e.g. just to list what" << endl;
    cout << "                 files would be compared by checking their existance)" << endl;
    cout << "  [/CHK:[drive:\\|\\\\server\\][path\\]file]" << endl;
    cout << "                 Specifies that XCOMP/2 uses a checksum file to ensure data" << endl;
    cout << "                 integrity. If the checksum file does not exist, it will be" << endl;
    cout << "                 created, otherwise compared with the checksum calculated" << endl;
    cout << "                 from the data read from the Source." << endl;
    cout << "                 When using the extension \".MD5\" is used, the checksum file" << endl;
    cout << "                 will be compatible to the MD5SUM utility. You may need the" << endl;
    cout << "                 option /!S additionally, as MD5SUM ignores subdirectories." << endl;
    cout << "Returns:" << endl;
    cout << "  0              Successful completion" << endl;
    cout << "  1              Files could not be opened to compare (possibly 0-length,"  << endl;
    cout << "                 locked or not existing)" << endl;
    cout << "  2              Directories could not be opened to search for files (possibly" << endl;
    cout << "                 access right or file system problems)" << endl;
    cout << "  3              Directories could not be opened to search for directories" << endl;
    cout << "                 possibly access right or file system problems)" << endl;
    cout << "  4              A mismatch between at least 1 file was detected" << endl;
    cout << "  5              A mismatch between the calculated Checksum and the recorded" << endl;
    cout << "                 one in the Checksum file of at least 1 file was detected" << endl;
    cout << "  100+           Fatal, unrecoverable exceptions\n" << endl;
}

void        XCOMP::verifySourcePath(void)
{
    char    acSourcePathCopy[CCHMAXPATH];
    ULONG   ulDiskNum;
    ULONG   ulTemp;
    int     iUNCFilename;
    char   *pcFilename;
    APIRET  apiretRc;

    memset(acSourcePathCopy, '\0', sizeof(acSourcePathCopy));
                                        /* If just a drive letter was specified, add the
                                           current directory of that drive (note that
                                           DosQueryCurrentDir() does not return the
                                           drive letter) */
    if((acSourcePath[1]==':') && (acSourcePath[2]=='\0'))
        {
        ulDiskNum=(ULONG)acSourcePath[0];
        if((ulDiskNum>='a') && (ulDiskNum<='z'))
            ulDiskNum-=((ULONG)'a'-1);
        if((ulDiskNum>='A') && (ulDiskNum<='Z'))
            ulDiskNum-=((ULONG)'A'-1);
        ulTemp=sizeof(acSourcePathCopy);
#ifdef  __OS2__
        apiretRc=DosQueryCurrentDir(ulDiskNum, acSourcePathCopy, &ulTemp);
        if(apiretRc==NO_ERROR)
            {
            if(strlen(acSourcePathCopy))
                {
                strcat(acSourcePath, "\\");
                strcat(acSourcePath, acSourcePathCopy);
                }
            strcat(acSourcePath, "\\*");
            }
#endif  /* __OS2__ */
#ifdef  __WIN32__
        SetLastError(NO_ERROR);
        apiretRc=GetFullPathName(acSourcePath, sizeof(acSourcePathCopy), acSourcePathCopy, NULL);
        apiretRc=GetLastError();
        if(apiretRc==NO_ERROR)
            {
            if(strlen(acSourcePathCopy))
                {
                strcpy(acSourcePath, acSourcePathCopy);
                }
            strcat(acSourcePath, "\\*");
            }
#endif  /* __WIN32__ */
        }
                                        /* Check for trailing backslash, which we replace
                                           (DosQueryPathInfo() doesn't like them) */
    iSourcePathRootLen=strlen(acSourcePath);
    if(acSourcePath[iSourcePathRootLen-1]=='\\')
        strcat(acSourcePath, "*");
                                        /* If the user has specified "\", ... everything
                                           that follows must be remembered, as DosQueryPathInfo()
                                           just expands "." and ".." while keeping the rest
                                           Q:\Test\* -> *
                                           .\* -> *
                                           \\Server\Alias\* -> *
                                           . -> n/a
                                           .. -> n/a */
    pcFilename=strrchr(acSourcePath, '\\');
    if(pcFilename==strchr(acSourcePath, '\\')+1)
        pcFilename=0;
    if(pcFilename==0)
        pcFilename=strrchr(acSourcePath, '.');
    if(pcFilename==0)
        pcFilename=strrchr(acSourcePath, ':');
    if(pcFilename!=0)
        {
        pcFilename++;
        if(*pcFilename=='\0')
            pcFilename=0;
        }
                                        /* Now let's OS/2 try to expand the path to a fully
                                           qualified name. We then compare the expanded to the
                                           original qualified path name to guess what the user
                                           might have meant, e.g when residing at Q:\Test:
                                           . -> Q:\Test\*
                                           .\* -> Q:\Test\*
                                           Q:* -> Q:\Test\*
                                           ..\Test\* -> Q:\Test\*
                                           Q:\Test\* -> Q:\Test\*
                                           Q:\Test\ -> Q:\Test\* */
#ifdef  __OS2__
    apiretRc=DosQueryPathInfo(acSourcePath, FIL_QUERYFULLNAME, acSourcePathCopy, sizeof(acSourcePathCopy));
#endif  /* __OS2__ */
#ifdef  __WIN32__
    SetLastError(NO_ERROR);
    apiretRc=GetFullPathName(acSourcePath, sizeof(acSourcePathCopy), acSourcePathCopy, NULL);
    apiretRc=GetLastError();
#endif  /* __WIN32__ */
    if(apiretRc!=NO_ERROR)
        {
        UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_DOSQUERYSOURCEPATH, "", __LINE__);

        *puexcpError << "Error SYS" << apiretRc << " returned by DosQueryPathInfo()";
        throw(*puexcpError);
        }
    if(strcmpi(acSourcePath, acSourcePathCopy))
        {
        if(pcFilename==0)
            {
            pcFilename=strstr(acSourcePathCopy, acSourcePath);
            if((pcFilename!=0) && (!strcmpi(pcFilename, acSourcePath)))
                strcpy(acSourcePath, acSourcePathCopy);
            else
                {
                strcpy(acSourcePath, acSourcePathCopy);
                if(acSourcePath[strlen(acSourcePath)-1]=='\\')
                    strcat(acSourcePath, "*");
                else
                    strcat(acSourcePath, "\\*");
                }
            }
        else
            strcpy(acSourcePath, acSourcePathCopy);
        }
                                        /* Test for an UNC-name */
    iUNCFilename=FALSE;
    if(!strncmp(acSourcePath, "\\\\", sizeof("\\\\")-1))
        iUNCFilename=TRUE;
                                        /* Test for a backslash, as everything that follows must
                                           be the filename(s) we want to compare from */
    pcFilename=strrchr(acSourcePath, '\\');
    if(pcFilename!=0)
        {
                                        /* If we found the backslash, construct path and filename(s)
                                           info, but take care for UNC filenames */
        if((iUNCFilename==TRUE) && (pcFilename<=(acSourcePath+1)))
            {
            throw(UEXCP(APPLICATION_PREFIX, ERR_UNCSOURCEPATH, "Invalid UNC name specified for source path"));
            }
        pcFilename++;
        if(*pcFilename!='\0')
            strcpy(acSourceFiles, pcFilename);
        *pcFilename='\0';
        iSourcePathRootLen=strlen(acSourcePath);
        }
    else
        {
        ULONG   ulDriveNum;
        ULONG   ulDriveMap;
        ULONG   ulLength;

                                        /* We need the current drive anyway */
#ifdef  __OS2__
        apiretRc=DosQueryCurrentDisk(&ulDriveNum, &ulDriveMap);
#endif  /* __OS2__ */
#ifdef  __WIN32__
        SetLastError(NO_ERROR);
        apiretRc=GetCurrentDirectory(sizeof(acSourcePathCopy), acSourcePathCopy);
        apiretRc=GetLastError();
#endif  /* __WIN32__ */
        if(apiretRc!=NO_ERROR)
            {
            UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_CURRENTDISK, "", __LINE__);

            *puexcpError << "Error SYS" << apiretRc << " returned by DosQueryCurrentDisk()";
            throw(*puexcpError);
            }
                                        /* If we didn't find a backslash, we may either have:
                                             - just the filename(s)
                                             - just the drive letter */
        pcFilename=strchr(acSourcePath, ':');
        if(pcFilename==acSourcePath)
            {
            throw(UEXCP(APPLICATION_PREFIX, ERR_SOURCEDRIVE, "Drive letter not specified for source path"));
            }
        if(pcFilename==0)
            {
                                        /* We didn't find a drive letter, so let's use the
                                           current drive and directory */
            strcpy(acSourceFiles, acSourcePath);
            strcpy(acSourcePath, "C:\\");
            ulLength=CCHMAXPATH-strlen(acSourcePath);
            acSourcePath[0]=(char)((ulDriveNum-1)+(ULONG)'a');
#ifdef  __OS2__
            apiretRc=DosQueryCurrentDir(ulDriveNum, &acSourcePath[strlen(acSourcePath)], &ulLength);
#endif  /* __OS2__ */
#ifdef  __WIN32__
            SetLastError(NO_ERROR);
            apiretRc=GetCurrentDirectory(ulLength, &acSourcePath[strlen(acSourcePath)]);
            apiretRc=GetLastError();
#endif  /* __WIN32__ */
            if(apiretRc!=NO_ERROR)
                {
                UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_CURRENTDIR, "", __LINE__);

                *puexcpError << "Error SYS" << apiretRc << " returned by DosQueryCurrentDir()";
                throw(*puexcpError);
                }
            }
        else if(*pcFilename==':')
            {
                                        /* We found a drive letter, which has to be enhanced with
                                           the current directory on that drive */
            strcpy(acSourceFiles, pcFilename+1);
            strcpy(pcFilename+1, "\\");
            ulLength=CCHMAXPATH-strlen(acSourcePath);
            ulDriveNum=((ULONG)*(pcFilename-1) | 0x00000020)-(ULONG)'a';
            ulDriveNum++;
#ifdef  __OS2__
            apiretRc=DosQueryCurrentDir(ulDriveNum, &acSourcePath[strlen(acSourcePath)], &ulLength);
#endif  /* __OS2__ */
#ifdef  __WIN32__
            SetLastError(NO_ERROR);
            apiretRc=GetCurrentDirectory(ulLength, &acSourcePath[strlen(acSourcePath)]);
            apiretRc=GetLastError();
#endif  /* __WIN32__ */
            if(apiretRc!=NO_ERROR)
                {
                UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_CURRENTDIR1, "", __LINE__);

                *puexcpError << "Error SYS" << apiretRc << " returned by DosQueryCurrentDir()";
                throw(*puexcpError);
                }
            }
        else
            {
                                        /* As we didn't find any drive letter, the user must have
                                           specified the filename(s) he's looking for, so get the
                                           current directory and the current drive */
            strcpy(acSourceFiles, acSourcePath);
            acSourcePath[0]=(char)((ulDriveNum-1)+(ULONG)'a');
            strcpy(&acSourcePath[1], ":\\");
            ulLength=CCHMAXPATH-strlen(acSourcePath);
#ifdef  __OS2__
            apiretRc=DosQueryCurrentDir(ulDriveNum, &acSourcePath[strlen(acSourcePath)], &ulLength);
#endif  /* __OS2__ */
#ifdef  __WIN32__
            SetLastError(NO_ERROR);
            apiretRc=GetCurrentDirectory(ulLength, &acSourcePath[strlen(acSourcePath)]);
            apiretRc=GetLastError();
#endif  /* __WIN32__ */
            if(apiretRc!=NO_ERROR)
                {
                UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_CURRENTDIR, "", __LINE__);

                *puexcpError << "Error SYS" << apiretRc << " returned by DosQueryCurrentDir()";
                throw(*puexcpError);
                }
            }
        iSourcePathRootLen=strlen(acSourcePath);
        if(acSourcePath[iSourcePathRootLen-1]!='\\')
            {
            strcat(acSourcePath, "\\");
            iSourcePathRootLen++;
            }
        }
                                        /* Check for trailing backslash, if nothing follows then
                                           just assume user wanted to specify "*" */
    if(!strlen(acSourceFiles))
        strcpy(acSourceFiles, "*");
}

void        XCOMP::verifyTargetPath(void)
{
    char    acTargetPathCopy[CCHMAXPATH];
    ULONG   ulDiskNum;
    ULONG   ulTemp;
    int     iUNCFilename;
    char   *pcPath;
    APIRET  apiretRc;

    memset(acTargetPathCopy, '\0', sizeof(acTargetPathCopy));
                                        /* If just a drive letter was specified, add the
                                           current directory of that drive (note that
                                           DosQueryCurrentDir() does not return the
                                           drive letter) */
#ifdef  __OS2__
    if((acTargetPath[1]==':') && (acTargetPath[2]=='\0'))
        {
        ulDiskNum=(ULONG)acTargetPath[0];
        if((ulDiskNum>='a') && (ulDiskNum<='z'))
            ulDiskNum-=((ULONG)'a'-1);
        if((ulDiskNum>='A') && (ulDiskNum<='Z'))
            ulDiskNum-=((ULONG)'A'-1);
        ulTemp=sizeof(acTargetPathCopy);
        apiretRc=DosQueryCurrentDir(ulDiskNum, acTargetPathCopy, &ulTemp);
        if(apiretRc==NO_ERROR)
            {
            strcat(acTargetPath, "\\");
            strcat(acTargetPath, acTargetPathCopy);
            }
        }
                                        /* Check for trailing backslash, which we remove */
    iTargetPathRootLen=strlen(acTargetPath);
    if(iTargetPathRootLen>1)
        {
        if(acTargetPath[iTargetPathRootLen-1]=='\\')
            acTargetPath[iTargetPathRootLen-1]='\0';
        }
                                        /* Now let's OS/2 try to expand the path to a fully
                                           qualified name (if possible, that is we ignore an
                                           error here as we'll find it later), and add a trailing
                                           backslash */
    apiretRc=DosQueryPathInfo(acTargetPath, FIL_QUERYFULLNAME, acTargetPath, sizeof(acTargetPath));
#endif  /* __OS2__ */
#ifdef  __WIN32__
    SetLastError(NO_ERROR);
    apiretRc=GetFullPathName(acTargetPath, sizeof(acTargetPathCopy), acTargetPathCopy, NULL);
    apiretRc=GetLastError();
    strcpy(acTargetPath, acTargetPathCopy);
#endif  /* __WIN32__ */
    iTargetPathRootLen=strlen(acTargetPath);
    if(acTargetPath[iTargetPathRootLen-1]!='\\')
        strcat(acTargetPath, "\\");
                                        /* Test for an UNC-name */
    iUNCFilename=FALSE;
    if(!strncmp(acTargetPath, "\\\\", sizeof("\\\\")-1))
        iUNCFilename=TRUE;
                                        /* Test for a backslash, as everything that follows must
                                           be the path we want to compare to */
    pcPath=strrchr(acTargetPath, '\\');
    if(pcPath!=0)
        {
                                        /* If we found the backslash, construct path info, but
                                           take care for UNC filenames */
        if(iUNCFilename==TRUE)
            {
            if(pcPath<=(acTargetPath+1))
                {
                throw(UEXCP(APPLICATION_PREFIX, ERR_UNCTARGETPATH, "Invalid UNC name specified for target path"));
                }
            if((*(pcPath+1)!='\0') || (strchr(acTargetPath+2, '\\')>=pcPath))
                {
                throw(UEXCP(APPLICATION_PREFIX, ERR_UNCTARGETPATH1, "Invalid UNC name specified for target path"));
                }
            }
        iTargetPathRootLen=strlen(acTargetPath);
        }
    else
        {
        ULONG   ulDriveNum;
        ULONG   ulDriveMap;
        ULONG   ulLength;

                                        /* If we didn't find a backslash, we may either have:
                                             - just the drive letter */
        pcPath=strchr(acTargetPath, ':');
        if(pcPath==acTargetPath)
            {
            throw(UEXCP(APPLICATION_PREFIX, ERR_TARGETDRIVE, "Drive letter not specified for target path"));
            }
        if(pcPath!=NULL)
            {
                                        /* We found a drive letter, which has to be enhanced with
                                           the current directory on that drive */
            strcpy(pcPath+1, "\\");
            ulLength=CCHMAXPATH-strlen(acTargetPath);
            ulDriveNum=((ULONG)*(pcPath-1) | 0x00000020) - (ULONG)'a';
            ulDriveNum++;
#ifdef  __OS2__
            apiretRc=DosQueryCurrentDir(ulDriveNum, &acTargetPath[strlen(acTargetPath)], &ulLength);
#endif  /* __OS2__ */
#ifdef  __WIN32__
            SetLastError(NO_ERROR);
            apiretRc=GetCurrentDirectory(sizeof(acTargetPathCopy), acTargetPathCopy);
            apiretRc=GetLastError();
#endif  /* __WIN32__ */
            if(apiretRc!=NO_ERROR)
                {
                UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_CURRENTDIR4, "", __LINE__);

                *puexcpError << "Error SYS" << apiretRc << " returned by DosQueryCurrentDir()";
                throw(*puexcpError);
                }
            }
        else
            {
            throw(UEXCP(APPLICATION_PREFIX, ERR_TARGETPATHINVALID, "Invalid target path specified"));
            }
        }
    iTargetPathRootLen=strlen(acTargetPath);
    if(acTargetPath[iTargetPathRootLen-1]!='\\')
        {
        strcat(acTargetPath, "\\");
        iTargetPathRootLen++;
        }
}

void        XCOMP::searchFiles(char *pcSourcePath, char *pcSourceFiles, char *pcTargetPath)
{
    char            acSourceFiles[CCHMAXPATH];
    char            acTargetFiles[CCHMAXPATH];
    char            acSourceDirectory[CCHMAXPATH];
    char            acSourceFileCurrent[CCHMAXPATH];
    HDIR            hdirSourceFiles;
#ifdef  __OS2__
    FILEFINDBUF3    ffb3SourceFiles;
    ULONG           ulffb3Length;
    ULONG           ulFindCount;
#endif  /* __OS2__ */
#ifdef  __WIN32__
    WIN32_FIND_DATA fdSourceFiles;
#endif  /* __WIN32__ */
    APIRET          apiretRc;
    char           *pcSourceDirectories;
    char           *pcTargetDirectories;

    strcpy(acSourceFiles, pcSourcePath);
    strcat(acSourceFiles, pcSourceFiles);
                                        /* Say what we're going to compare */
    strcpy(acSourceDirectory, &acSourceFiles[iSourcePathRootLen]);
    pcSourceDirectories=strrchr(acSourceDirectory, '\\');
    if(pcSourceDirectories==0)
        {
        acSourceDirectory[0]='\\';
        acSourceDirectory[1]='\0';
        }
    else
        *(pcSourceDirectories+1)='\0';
    cout << acSourceDirectory << endl;
    cout.flush();
                                        /* Open files enumeration */
#ifdef  __OS2__
    memset(&ffb3SourceFiles, 0, sizeof(ffb3SourceFiles));
    hdirSourceFiles=HDIR_CREATE;
    ulffb3Length=sizeof(ffb3SourceFiles);
    ulFindCount=1;
    apiretRc=DosFindFirst(acSourceFiles,
        &hdirSourceFiles,
        FILE_ARCHIVED | FILE_SYSTEM | FILE_HIDDEN | FILE_READONLY,
        &ffb3SourceFiles,
        ulffb3Length,
        &ulFindCount,
        FIL_STANDARD);
    if((apiretRc==ERROR_SEEK) || (apiretRc==ERROR_PATH_NOT_FOUND))
#endif  /* __OS2__ */
#ifdef  __WIN32__
    memset(&fdSourceFiles, 0, sizeof(fdSourceFiles));
                                        /* This call will return both files and
                                           directories which is not the way we
                                           want it. Thus we need to filter the
                                           directories away */
    SetLastError(NO_ERROR);
    hdirSourceFiles=FindFirstFile(acSourceFiles, &fdSourceFiles);
    apiretRc=GetLastError();
    if((apiretRc==NO_ERROR) && (fdSourceFiles.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        goto WindowsSkipDirectories;
    if(apiretRc==ERROR_FILE_NOT_FOUND)
        apiretRc=ERROR_NO_MORE_FILES;
    if(apiretRc!=NO_ERROR && (apiretRc!=ERROR_NO_MORE_FILES))
#endif  /* __WIN32__ */
        {
        iCountProblemFindFiles++;
        cout << MSG_DOSFINDFIRSTSOURCE ": Error SYS" << apiretRc << " returned by DosFindFirst()";
        if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
            cout << " at Line " << (int)__LINE__;
        cout << endl;
        if(iStatusFlag & XCOMP_STATUS_LOG)
            {
            fstreamLogFile << MSG_DOSFINDFIRSTSOURCE ": Error SYS" << apiretRc << " returned by DosFindFirst() at Line " << (int)__LINE__ << endl;
            }
        if(iStatusFlag & XCOMP_STATUS_NOPAUSEERRCOMP)
            getKey(FALSE);
        else
            getKey();
        }
    else if((apiretRc!=NO_ERROR) && (apiretRc!=ERROR_NO_MORE_FILES))
        {
        UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_DOSFINDFIRSTSOURCE, "", __LINE__);

        *puexcpError << "Error SYS" << apiretRc << " returned by DosFindFirst()";
        throw(*puexcpError);
        }
                                        /* Check if the current file found is our Checksum
                                           file, which we'll exclude */
    strcpy(acSourceFileCurrent, pcSourcePath);
#ifdef  __OS2__
    strcat(acSourceFileCurrent, ffb3SourceFiles.achName);
#endif  /* __OS2__ */
#ifdef  __WIN32__
    strcat(acSourceFileCurrent, fdSourceFiles.cFileName);
#endif  /* __WIN32__ */
    if(!stricmp(acSourceFileCurrent, acChkSumFile))
        {
        cout << MSG_SKIPCHECKSUMFILE ": Skipping specified Checksum file";
        if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
            cout << " at Line " << (int)__LINE__;
        cout << endl;
        if(iStatusFlag & XCOMP_STATUS_LOG)
            {
            fstreamLogFile << MSG_SKIPCHECKSUMFILE ": Skipping specified Checksum file" << " at Line " << (int)__LINE__ << endl;
            }
        goto SkipCheckSumFileItself;
        }
    if(iStatusFlag & XCOMP_STATUS_VERBOSE2)
        {
#ifdef  __OS2__
        if(apiretRc==NO_ERROR)
            cerr << endl << "  Line " << __LINE__ << ": DosFindFirst() found source file " << ffb3SourceFiles.achName << endl;
#endif  /* __OS2__ */
#ifdef  __WIN32__
        if(apiretRc==NO_ERROR)
            cerr << endl << "  Line " << __LINE__ << ": DosFindFirst() found source file " << fdSourceFiles.cFileName << endl;
#endif  /* __WIN32__ */
        }
                                        /* Enumerate files */
    if(apiretRc==ERROR_PATH_NOT_FOUND)
        return;
    while(apiretRc!=ERROR_NO_MORE_FILES)
        {
#ifdef  __OS2__
        if(iStatusFlag & XCOMP_STATUS_SOURCE_ONLY)
            readFiles(pcSourcePath, ffb3SourceFiles.achName);
        else
            compareFiles(pcSourcePath, ffb3SourceFiles.achName, pcTargetPath);
#endif  /* __OS2__ */
#ifdef  __WIN32__
        if(iStatusFlag & XCOMP_STATUS_SOURCE_ONLY)
            readFiles(pcSourcePath, fdSourceFiles.cFileName);
        else
            compareFiles(pcSourcePath, fdSourceFiles.cFileName, pcTargetPath);
#endif  /* __WIN32__ */
                                        /* Checksum result processing (if we had success
                                           reading or comparing the file completely) */
        if((iStatusFlag & (XCOMP_STATUS_CHKSUM|XCOMP_STATUS_CRC32_DONE|XCOMP_STATUS_MD5_DONE))==
            (XCOMP_STATUS_CHKSUM|XCOMP_STATUS_CRC32_DONE|XCOMP_STATUS_MD5_DONE))
            {
            if(iStatusFlag & XCOMP_STATUS_CHKSUM_LOG)
                {
                if(iStatusFlag & XCOMP_STATUS_MD5_MD5SUM)
                    {
                                        /* MD5SUM compatibility mode */
                    strlwr((char *)aucMd5Digest);
                    fstreamChkSumFile << aucMd5Digest << " *" <<
#ifdef  __OS2__
                        &pcSourcePath[iSourcePathRootLen] << ffb3SourceFiles.achName <<  endl;
#endif  /* __OS2__ */
#ifdef  __WIN32__
                        &pcSourcePath[iSourcePathRootLen] << fdSourceFiles.cFileName <<  endl;
#endif  /* __WIN32__ */
                    }
                else
                    {
                                        /* Our own Checksum file format */
                    fstreamChkSumFile << "CRC32: " << aucCrc32 << " MD5: " << aucMd5Digest <<
#ifdef  __OS2__
                        " Path: \\" << &pcSourcePath[iSourcePathRootLen] << ffb3SourceFiles.achName <<  endl;
#endif  /* __OS2__ */
#ifdef  __WIN32__
                        " Path: \\" << &pcSourcePath[iSourcePathRootLen] << fdSourceFiles.cFileName <<  endl;
#endif  /* __WIN32__ */
                    }
                }
            else
                {
                                        /* Entry read from Checksum file that matches with
                                           the current relative path */
                CHKSUM_ENTRY
                           *pChkSumEntryMatch=0;
                UCHAR       aucRelativeSourcePath[CCHMAXPATH];
#define     MISMATCH_NONE               0x00000000
#define     MISMATCH_CRC32              0x00000001
#define     MISMATCH_MD5DIGEST          0x00000002
#define     MISMATCH_FILENAME           0x00000004
                int         iMisMatch=MISMATCH_NONE;
                UCHAR       aucCrc32Read[UCRC32_CRC_LENGTH_STRG];
                UCHAR       aucMd5DigestRead[UMD5_DIGEST_LENGTH_STRG];


                memset(&aucRelativeSourcePath, '\0', sizeof(aucRelativeSourcePath));
                memset(&aucCrc32Read, '\0', sizeof(aucCrc32Read));
                memset(&aucMd5DigestRead, '\0', sizeof(aucMd5DigestRead));
                if(pcSourcePath[iSourcePathRootLen]!='\\')
                    strcat((char *)aucRelativeSourcePath, "\\");
                                        /* Now check if the path can be found in the Checksum 
                                           file we read earlier is a case insensitive part 
                                           in the filesystem tree */
                strcat((char *)aucRelativeSourcePath, &pcSourcePath[iSourcePathRootLen]);
#ifdef  __OS2__
                strcat((char *)aucRelativeSourcePath, ffb3SourceFiles.achName);
#endif  /* __OS2__ */
#ifdef  __WIN32__
                strcat((char *)aucRelativeSourcePath, fdSourceFiles.cFileName);
#endif  /* __WIN32__ */
                pChkSumEntryMatch=findChkSumEntry(pChkSumEntryRoot, aucRelativeSourcePath);
                if(pChkSumEntryMatch)
                    {
                    pChkSumEntryMatch->usStatus|=CHKSUM_STATUS_FILEFOUND;
                                        /* If we could find an entry for the current
                                           file in the Checksum file, then check for matching
                                           Checksums */
                    if(iStatusFlag & XCOMP_STATUS_MD5_MD5SUM)
                        {
                        if(memcmp(abMd5Digest, pChkSumEntryMatch->File.pbMd5Digest, sizeof(abMd5Digest)))
                            iMisMatch|=MISMATCH_MD5DIGEST;
                        }
                    else
                        {
                        if(ulCrc32!=pChkSumEntryMatch->File.ulCrc32)
                            iMisMatch|=MISMATCH_CRC32;
                        if(memcmp(abMd5Digest, pChkSumEntryMatch->File.pbMd5Digest, sizeof(abMd5Digest)))
                            iMisMatch|=MISMATCH_MD5DIGEST;
                        }
                    }
                else
                    {
                                        /* If we couldn't find an entry for the current
                                           file in the Checksum file, then we have a sequence 
                                           error */
                    iMisMatch|=MISMATCH_FILENAME;
                    }
                if(iMisMatch!=MISMATCH_NONE)
                    {
                    iCountProblemChkSum++;
                    if(iStatusFlag & XCOMP_STATUS_LOG)
                        {
#ifdef  __OS2__
                        fstreamLogFile << &pcSourcePath[iSourcePathRootLen] << ffb3SourceFiles.achName << endl;
#endif  /* __OS2__ */
#ifdef  __WIN32__
                        fstreamLogFile << &pcSourcePath[iSourcePathRootLen] << fdSourceFiles.cFileName << endl;
#endif  /* __WIN32__ */
                        }
                    if(iMisMatch & MISMATCH_FILENAME)
                        {
                                        /* If the Checksum file does not contain the filename
                                           we currently process, then current file has been
                                           removed since the Checksum file was generated last */
                        cout << MSG_CHKSUMFILENAME ": File missing in Checksum file";
                        if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
                            cout << " at Line " << (int)__LINE__;
                        cout << endl;
                        if(iStatusFlag & XCOMP_STATUS_LOG)
                            fstreamLogFile << MSG_CHKSUMFILENAME ": File missing in Checksum file at Line " << (int)__LINE__ << endl;
                        if(!(iStatusFlag & XCOMP_STATUS_NOPAUSEERRCOMP))
                            getKey();
                        }
                    else
                        {
                        if(pChkSumEntryMatch)
                            {
                            UCRC32::convert2String(pChkSumEntryMatch->File.ulCrc32, aucCrc32Read);
                            UMD5::convert2String(pChkSumEntryMatch->File.pbMd5Digest, aucMd5DigestRead);
                            cout << MSG_CHKSUMMISMATCH ": Checksum(s) mismatch";
                            if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
                                cout << " at Line " << (int)__LINE__;
                            cout << ". Checksum file vs. calculated:\n";
                            if(iStatusFlag & XCOMP_STATUS_MD5_MD5SUM)
                                {
                                strlwr((char *)aucMd5Digest);
                                }
                            else
                                {
                                if(iMisMatch & MISMATCH_CRC32)
                                    {
                                    cout << "  CRC: " << (char *)aucCrc32Read <<
                                        "                         vs: " << aucCrc32 << "\n";
                                    }
                                }
                            if(iMisMatch & MISMATCH_MD5DIGEST)
                                {
                                cout << "  MD5: " << (char *)aucMd5DigestRead <<
                                    " vs: " << aucMd5Digest << "\n";
                                }
                            if(iStatusFlag & XCOMP_STATUS_LOG)
                                {
                                fstreamLogFile << MSG_CHKSUMMISMATCH ": Checksum(s) mismatch at Line " << (int)__LINE__;
                                fstreamLogFile << ". Checksum file vs. calculated:\n";
                                if(!(iStatusFlag & XCOMP_STATUS_MD5_MD5SUM))
                                    {
                                    if(iMisMatch & MISMATCH_CRC32)
                                        {
                                        fstreamLogFile << "  CRC32: " << (char *)aucCrc32Read <<
                                            "                         vs: " << aucCrc32 << "\n";
                                        }
                                    }
                                if(iMisMatch & MISMATCH_MD5DIGEST)
                                    {
                                    fstreamLogFile << "  MD5    " << (char *)aucMd5DigestRead <<
                                        " vs: " << aucMd5Digest << "\n";
                                    }
                                }
                            }
                        else
                            {
                            cout << MSG_CHKSUMMISMATCH ": Checksum(s) missing";
                            if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
                                cout << " at Line " << (int)__LINE__;
                            cout << ".\n";
                            if(iStatusFlag & XCOMP_STATUS_LOG)
                                {
                                fstreamLogFile << MSG_CHKSUMMISMATCH ": Checksum(s) mismatch at Line " << (int)__LINE__;
                                fstreamLogFile << ".\n";
                                }
                            }
                        if(!(iStatusFlag & XCOMP_STATUS_NOPAUSEERRCOMP))
                            getKey();
                        }
                    fstreamLogFile.flush();
                    }
                }
            }
SkipCheckSumFileItself:
#ifdef  __OS2__
        memset(&ffb3SourceFiles, 0, sizeof(ffb3SourceFiles));
        ulFindCount=1;
        apiretRc=DosFindNext(hdirSourceFiles,
            &ffb3SourceFiles,
            ulffb3Length,
            &ulFindCount);
#endif  /* __OS2__ */
#ifdef  __WIN32__
WindowsSkipDirectories:
        memset(&fdSourceFiles, 0, sizeof(fdSourceFiles));
                                        /* This call will return both files and
                                           directories which is not the way we
                                           want it. Thus we need to filter the
                                           directories away */
        SetLastError(NO_ERROR);
        apiretRc=FindNextFile(hdirSourceFiles, &fdSourceFiles);
        apiretRc=GetLastError();
        if((apiretRc==NO_ERROR) && (fdSourceFiles.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            goto WindowsSkipDirectories;
#endif  /* __WIN32__ */
        if((apiretRc==ERROR_SEEK) || (apiretRc==ERROR_PATH_NOT_FOUND))
            {
            iCountProblemFindFiles++;
            cout << MSG_DOSFINDNEXTSOURCE ": Error SYS" << apiretRc << " returned by DosFindNext()";
            if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
                cout << " at Line " << (int)__LINE__;
            cout << endl;
            if(iStatusFlag & XCOMP_STATUS_LOG)
                {
                fstreamLogFile << MSG_DOSFINDNEXTSOURCE ": Error SYS" << apiretRc << " returned by DosFindNext() at Line " << (int)__LINE__ << endl;
                }
            if(iStatusFlag & XCOMP_STATUS_NOPAUSEERRCOMP)
                getKey(FALSE);
            else
                getKey();
            }
        else if((apiretRc!=NO_ERROR) && (apiretRc!=ERROR_NO_MORE_FILES))
            {
            UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_DOSFINDNEXTSOURCE, "", __LINE__);

            *puexcpError << "Error SYS" << apiretRc << " returned by DosFindNext()";
            throw(*puexcpError);
            }
        if(iStatusFlag & XCOMP_STATUS_VERBOSE2)
            {
#ifdef  __OS2__
            if(apiretRc==NO_ERROR)
                cerr << "  Line " << __LINE__ << ": DosFindNext() found source file " << ffb3SourceFiles.achName << endl;
#endif  /* __OS2__ */
#ifdef  __WIN32__
            if(apiretRc==NO_ERROR)
                cerr << "  Line " << __LINE__ << ": DosFindNext() found source file " << fdSourceFiles.cFileName << endl;
#endif  /* __WIN32__ */
            }
                                        /* Check if the current file found is our Checksum
                                           file, which we'll exclude */
        strcpy(acSourceFileCurrent, pcSourcePath);
#ifdef  __OS2__
        strcat(acSourceFileCurrent, ffb3SourceFiles.achName);
#endif  /* __OS2__ */
#ifdef  __WIN32__
        strcat(acSourceFileCurrent, fdSourceFiles.cFileName);
#endif  /* __WIN32__ */
        if(!stricmp(acSourceFileCurrent, acChkSumFile))
            {
            cout << MSG_SKIPCHECKSUMFILE ": Skipping specified Checksum file";
            if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
                cout << " at Line " << (int)__LINE__;
            cout << endl;
            if(iStatusFlag & XCOMP_STATUS_LOG)
                {
                fstreamLogFile << MSG_SKIPCHECKSUMFILE ": Skipping specified Checksum file" << " at Line " << (int)__LINE__ << endl;
                }
            goto SkipCheckSumFileItself;
            }
        }
                                        /* Close file enumeration, if not a single file
                                           was found. DosFindFirst() returns a handle which
                                           DosFindClose() finds invalid (I'm not sure if this
                                           isn't a bug) */
#ifdef  __WIN32__
    SetLastError(NO_ERROR);
#endif  /* __WIN32__ */
    apiretRc=FINDCLOSE(hdirSourceFiles);
    hdirSourceFiles=(HDIR)-1;
#ifdef  __WIN32__
    apiretRc=GetLastError();
#endif  /* __WIN32__ */
    if((apiretRc!=NO_ERROR) && (apiretRc!=ERROR_INVALID_HANDLE))
        {
        UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_DOSFINDCLOSESOURCE, "", __LINE__);

        *puexcpError << "Error SYS" << apiretRc << " returned by DosFindClose()";
        throw(*puexcpError);
        }
                                        /* If no recursion into subdirectories is
                                           wanted, do not search for directories */
    if(iStatusFlag & XCOMP_STATUS_NORECURSION)
        return;
                                        /* Open directories enumeration */
    strcpy(acSourceFiles, pcSourcePath);
    pcSourceDirectories=strchr(acSourceFiles, '\0');
    strcpy(acTargetFiles, pcTargetPath);
    pcTargetDirectories=strchr(acTargetFiles, '\0');
    strcat(acSourceFiles, "*");
#ifdef  __OS2__
    memset(&ffb3SourceFiles, 0, sizeof(ffb3SourceFiles));
    hdirSourceFiles=HDIR_CREATE;
    ulffb3Length=sizeof(ffb3SourceFiles);
    ulFindCount=1;
    apiretRc=DosFindFirst(acSourceFiles,
        &hdirSourceFiles,
        MUST_HAVE_DIRECTORY | FILE_ARCHIVED | FILE_SYSTEM | FILE_HIDDEN | FILE_READONLY,
        &ffb3SourceFiles,
        ulffb3Length,
        &ulFindCount,
        FIL_STANDARD);
//@@@@@@@ Error path not found gets returned on Windows VFAT drives
//    if((apiretRc==ERROR_SEEK) || (apiretRc==ERROR_PATH_NOT_FOUND))
    if(apiretRc==ERROR_SEEK)
#endif  /* __OS2__ */
#ifdef  __WIN32__
    memset(&fdSourceFiles, 0, sizeof(fdSourceFiles));
                                        /* This call will return both files and
                                           directories which is not the way we
                                           want it. Thus we need to filter the
                                           files away */
    SetLastError(NO_ERROR);
    hdirSourceFiles=FindFirstFile(acSourceFiles, &fdSourceFiles);
    apiretRc=GetLastError();
    if((apiretRc==NO_ERROR) && !(fdSourceFiles.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        goto WindowsSkipFiles;
    if(apiretRc==ERROR_FILE_NOT_FOUND)
        apiretRc=ERROR_NO_MORE_FILES;
    if(apiretRc!=NO_ERROR && (apiretRc!=ERROR_NO_MORE_FILES))
#endif  /* __WIN32__ */
        {
        iCountProblemFindDirs++;
        cout << MSG_DOSFINDFIRSTDIR ": Error SYS" << apiretRc << " returned by DosFindFirst()";
        if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
            cout << " at Line " << (int)__LINE__;
        cout << endl;
        if(iStatusFlag & XCOMP_STATUS_LOG)
            {
            fstreamLogFile << MSG_DOSFINDFIRSTDIR ": Error SYS" << apiretRc << " returned by DosFindFirst() at Line " << (int)__LINE__ << endl;
            }
        if(iStatusFlag & XCOMP_STATUS_NOPAUSEERRCOMP)
            getKey(FALSE);
        else
            getKey();
        }
//@@@@@@ Result from above problem
//    else if((apiretRc!=NO_ERROR) && (apiretRc!=ERROR_NO_MORE_FILES))
    else if((apiretRc!=NO_ERROR) && (apiretRc!=ERROR_NO_MORE_FILES) && (apiretRc!=ERROR_PATH_NOT_FOUND))
        {
        UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_DOSFINDFIRSTDIR, "", __LINE__);

        *puexcpError << "Error SYS" << apiretRc << " returned by DosFindFirst()";
        throw(*puexcpError);
        }
    if(iStatusFlag & XCOMP_STATUS_VERBOSE2)
        {
#ifdef  __OS2__
        if(apiretRc==NO_ERROR)
            cerr << "  Line " << __LINE__ << ": DosFindFirst() found source dir " << ffb3SourceFiles.achName << endl;
#endif  /* __OS2__ */
#ifdef  __WIN32__
        if(apiretRc==NO_ERROR)
            cerr << "  Line " << __LINE__ << ": DosFindFirst() found source dir " << fdSourceFiles.cFileName << endl;
#endif  /* __WIN32__ */
        }
                                        /* Enumerate directories, but skip . and .., also skip
                                           files (only directories must be returned for this
                                           DosFind*() handle as requested by MUST_HAVE_DIRECTORY, but
                                           TVFS seems to have a bug here and also returns files) */
    while((apiretRc!=ERROR_NO_MORE_FILES) && (apiretRc!=ERROR_PATH_NOT_FOUND))
        {
#ifdef  __OS2__
        if(ffb3SourceFiles.attrFile & FILE_DIRECTORY)
            {
            if((strcmp(ffb3SourceFiles.achName, ".")) && (strcmp(ffb3SourceFiles.achName, "..")))
                {
                strcpy(pcSourceDirectories, ffb3SourceFiles.achName);
                strcat(pcSourceDirectories, "\\");
                strcpy(pcTargetDirectories, ffb3SourceFiles.achName);
                strcat(pcTargetDirectories, "\\");
                searchFiles(acSourceFiles, pcSourceFiles, acTargetFiles);
                }
            }
#endif  /* __OS2__ */
#ifdef  __WIN32__
            {
            if((strcmp(fdSourceFiles.cFileName, ".")) && (strcmp(fdSourceFiles.cFileName, "..")))
                {
                strcpy(pcSourceDirectories, fdSourceFiles.cFileName);
                strcat(pcSourceDirectories, "\\");
                strcpy(pcTargetDirectories, fdSourceFiles.cFileName);
                strcat(pcTargetDirectories, "\\");
                searchFiles(acSourceFiles, pcSourceFiles, acTargetFiles);
                }
            }
#endif  /* __WIN32__ */
#ifdef  __OS2__
        memset(&ffb3SourceFiles, 0, sizeof(ffb3SourceFiles));
        ulFindCount=1;
        apiretRc=DosFindNext(hdirSourceFiles,
            &ffb3SourceFiles,
            ulffb3Length,
            &ulFindCount);
        if((apiretRc==ERROR_SEEK) || (apiretRc==ERROR_PATH_NOT_FOUND))
#endif  /* __OS2__ */
#ifdef  __WIN32__
WindowsSkipFiles:
        memset(&fdSourceFiles, 0, sizeof(fdSourceFiles));
                                        /* This call will return both files and
                                           directories which is not the way we
                                           want it. Thus we need to filter the
                                           files away */
        SetLastError(NO_ERROR);
        apiretRc=FindNextFile(hdirSourceFiles, &fdSourceFiles);
        apiretRc=GetLastError();
        if((apiretRc==NO_ERROR) && !(fdSourceFiles.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            goto WindowsSkipFiles;
        if((apiretRc!=NO_ERROR) && (apiretRc!=ERROR_NO_MORE_FILES))
#endif  /* __WIN32__ */
            {
            iCountProblemFindDirs++;
            cout << MSG_DOSFINDNEXTDIR ": Error SYS" << apiretRc << " returned by DosFindNext()";
            if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
                cout << " at Line " << (int)__LINE__;
            cout << endl;
            if(iStatusFlag & XCOMP_STATUS_LOG)
                {
                fstreamLogFile << MSG_DOSFINDNEXTDIR ": Error SYS" << apiretRc << " returned by DosFindNext() at Line " << (int)__LINE__ << endl;
                }
            if(iStatusFlag & XCOMP_STATUS_NOPAUSEERRCOMP)
                getKey(FALSE);
            else
                getKey();
            }
        else if((apiretRc!=NO_ERROR) && (apiretRc!=ERROR_NO_MORE_FILES))
            {
            UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_DOSFINDNEXTDIR, "", __LINE__);

            *puexcpError << "Error SYS" << apiretRc << " returned by DosFindNext()";
            throw(*puexcpError);
            }
        if(iStatusFlag & XCOMP_STATUS_VERBOSE2)
            {
#ifdef  __OS2__
            if(apiretRc==NO_ERROR)
                cerr << "  Line " << __LINE__ << ": DosFindNext() found source dir " << ffb3SourceFiles.achName << endl;
#endif  /* __OS2__ */
#ifdef  __WIN32__
            if(apiretRc==NO_ERROR)
                cerr << "  Line " << __LINE__ << ": DosFindNext() found source dir " << fdSourceFiles.cFileName << endl;
#endif  /* __WIN32__ */
            }
        }
                                        /* Close file enumeration */
#ifdef  __WIN32__
    SetLastError(NO_ERROR);
#endif  /* __WIN32__ */
    apiretRc=FINDCLOSE(hdirSourceFiles);
#ifdef  __WIN32__
    apiretRc=GetLastError();
#endif  /* __WIN32__ */
    hdirSourceFiles=(HDIR)-1;
    if((apiretRc!=NO_ERROR) && (apiretRc!=ERROR_INVALID_HANDLE))
        {
        UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_DOSFINDCLOSETARGET, "", __LINE__);

        *puexcpError << "Error SYS" << apiretRc << " returned by DosFindClose()";
        throw(*puexcpError);
        }
}

int         XCOMP::readFiles(char *pcSourcePath, char *pcSourceFile)
{
    char            acSourceFile[CCHMAXPATH];
    ULONG           ulActionSourceFile;
    ULONG           ulErrorFound;
#ifdef  __OS2__
    FILESTATUS3     filestatus3SourceFile;
#endif  /* __OS2__ */
#ifdef  __WIN32__
    DWORD           dwSizeSourceFile;
#endif  /* __WIN32__ */
    ULONG           ulSizeSourceFile;
    ULONG           ulRetryCount;
    ULONG           ulBytesReadAccumulated;
    ULONG           ulBytesReadExpected;
    ULONG           ulPercentageCompleted;
    ULONG           ulPercentageCompletedPrevious;
    ULONG           ulPercentageUsed=FALSE;
    ULONG           ulFilePosSourceFile;
    ULONG           ulBlockCurrent;
    ULONG           ulBlockFinished;
    int             iKey;
    APIRET          apiretRc;

    strcpy(acSourceFile, pcSourcePath);
    strcat(acSourceFile, pcSourceFile);
    iCountProblemNone++;
    ulRetryCount=0;
    ulBlockFinished=0;
                                        /* Say what we're going to read */
    cout << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << "\r";
    cout.flush();
RetryReadingFromBeginning:
                                        /* Clean CRC32 and MD5 checksum calculations */
    iStatusFlag&=~(XCOMP_STATUS_CRC32_DONE|XCOMP_STATUS_MD5_DONE);
    memset(aucCrc32, '\0', sizeof(aucCrc32));
    ulCrc32=0;
    memset(aucMd5Digest, '\0', sizeof(aucMd5Digest));
    memset(abMd5Digest, '\0', sizeof(abMd5Digest));
    ucrc32Source.init();
    umd5Source.init();
                                        /* Start with first block of file */
    ulBlockCurrent=0;
    ulErrorFound=FALSE;
    if(iStatusFlag & XCOMP_STATUS_VERBOSE)
        {
        cout << endl;
        cerr << "  Line " << __LINE__ << ": Opening source file " << acSourceFile << endl;
        }
#ifdef  __OS2__
    apiretRcSource=DosOpen(acSourceFile,
        &hfileSourceFile,
        &ulActionSourceFile,
        0,
        FILE_NORMAL,
        OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
        OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_SEQUENTIAL | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY,
        0);
    if(apiretRcSource!=NO_ERROR)
#endif  /* __OS2__ */
#ifdef  __WIN32__
    SetLastError(NO_ERROR);
    hfileSourceFile=CreateFile(acSourceFile,
        GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);
    apiretRcSource=GetLastError();
    if(apiretRcSource!=NO_ERROR)
#endif  /* __WIN32__ */
        {
        ulErrorFound=TRUE;
        iCountProblemOpen++;
        cout << endl << MSG_DOSOPENSOURCE ": Error SYS" << apiretRcSource << " opening source file returned by DosOpen()";
        if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
            cout << " at Line " << (int)__LINE__;
        cout << endl;
        if(iStatusFlag & XCOMP_STATUS_LOG)
            {
            fstreamLogFile << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << endl;
            fstreamLogFile << MSG_DOSOPENSOURCE ": Error SYS" << apiretRcSource << " opening source file returned by DosOpen() at Line " << (int)__LINE__ << endl;
            }
        if(apiretRcSource==ERROR_SHARING_VIOLATION)
            {
            if(!(iStatusFlag & XCOMP_STATUS_NOPAUSEERRFIND))
                getKey();
            }
        CLOSEFILE(hfileSourceFile);
        return(apiretRcSource);
        }
                                        /* Query soure file size */
#ifdef  __OS2__
    apiretRcSource=DosQueryFileInfo(hfileSourceFile,
        FIL_STANDARD,
        &filestatus3SourceFile,
        sizeof(filestatus3SourceFile));
    ulSizeSourceFile=filestatus3SourceFile.cbFile;
#endif  /* __OS2__ */
#ifdef  __WIN32__
    SetLastError(NO_ERROR);
    ulSizeSourceFile=GetFileSize(hfileSourceFile, &dwSizeSourceFile);
    apiretRcSource=GetLastError();
#endif  /* __WIN32__ */
    if(apiretRcSource!=NO_ERROR)
        {
        ulErrorFound=TRUE;
        cout << endl << MSG_DOSFILEINFOSOURCE ": Error SYS" << apiretRcSource << " during reading source file returned by DosQueryFileInfo()";
        if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
            cout << " at Line " << (int)__LINE__;
        cout << endl;
        if(iStatusFlag & XCOMP_STATUS_VERBOSE2)
            {
            cerr << "  Line " << __LINE__ << ": DosQueryFileInfo() found source " << ulSizeSourceFile << " bytes in size" << endl;
            }
        if(iStatusFlag & XCOMP_STATUS_LOG)
            {
            fstreamLogFile << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << endl;
            fstreamLogFile << MSG_DOSFILEINFOSOURCE ": Error SYS" << apiretRcSource << " reading source file info returned by DosQueryFileInfo() at Line " << (int)__LINE__ << endl;
            }
        }
                                        /* If we just look for files recursively wihtout
                                           doing actual comparisons, return it's ok */
    if(iStatusFlag & XCOMP_STATUS_SIMULATE)
        {
        cout << endl;
        ulRetryCount=RETRY_COUNT;
        goto FileCompareOnlySimulated;
        }
                                        /* Initialize */
    ulBytesReadAccumulated=0;
    ulBytesReadExpected=0;
    ulPercentageCompleted=0;
    ulPercentageCompletedPrevious=(ULONG)-1;
                                        /* Calculate one percent of the file's size (if it
                                           has at least 100 bytes) */
    if(ulSizeSourceFile>=100);
        ulBytesReadExpected=(ulSizeSourceFile/100);
                                        /* If file is larger than buffer, start displaying a
                                           percentage base progress indicator, else just
                                           display the filename */
    if(ulSizeSourceFile>iCompareBufferSize)
        {
        ulPercentageUsed=TRUE;
        if(ulPercentageCompleted!=ulPercentageCompletedPrevious)
            cout << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << " (" << ulPercentageCompleted << "% done)";
        }
    else
        cout << &pcSourcePath[iSourcePathRootLen] << pcSourceFile;
    if(ulRetryCount>0)
        cout << " Retry " << ulRetryCount;
    cout << "  \r";
    cout.flush();
                                        /* Loop until all blocks of the files have been
                                           read */
    while (TRUE)
        {
        ulBytesReadSourceFile=0;
                                        /* Consistency check by comparing the file pointer
                                           position of both files */
#ifdef  __OS2__
        apiretRc=DosSetFilePtr(hfileSourceFile, 0L, FILE_CURRENT, &ulFilePosSourceFile);
#endif  /* __OS2__ */
#ifdef  __WIN32__
        SetLastError(NO_ERROR);
        ulFilePosSourceFile=SetFilePointer(hfileSourceFile, 0, NULL, FILE_CURRENT);
        apiretRc=GetLastError();
#endif  /* __WIN32__ */
        if(apiretRc!=NO_ERROR)
            {
            UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_DOSFILEPTRSOURCE, "", __LINE__);

            *puexcpError << "Error SYS" << apiretRc << " returned by DosSetFilePtr()";
            throw(*puexcpError);
            }
                                        /* Read iCompareBufferSize sized blocks of data from
                                           the source. If iCompareBufferSize bytes couldn't be
                                           read anymore, we have read the last block and can
                                           exit the loop. As we read only the source files
                                           run singlethreaded */
        apiretRcSource=readSourceFile();
        if(apiretRcSource!=NO_ERROR)
            {
            ulErrorFound=TRUE;
            if(ulRetryCount>=RETRY_COUNT)
                {
                iCountProblemCompare++;
                cout << endl << MSG_READSOURCE ": Error SYS" << apiretRcSource << " reading source file returned by DosRead()";
                if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
                    cout << " at Line " << (int)__LINE__;
                cout << endl;
                if((apiretRcSource==ERROR_CRC) || (apiretRcSource==ERROR_SECTOR_NOT_FOUND))
                    cout << "          Source media may contain weak bits or may be defective!" << endl;
                if(iStatusFlag & XCOMP_STATUS_LOG)
                    {
                    fstreamLogFile << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << endl;
                    fstreamLogFile << MSG_READSOURCE ": Error SYS" << apiretRcSource << " reading source file returned by DosRead() at Line " << (int)__LINE__ << endl;
                    if((apiretRcSource==ERROR_CRC) || (apiretRcSource==ERROR_SECTOR_NOT_FOUND))
                        fstreamLogFile << "          Source media may contain weak bits or may be defective!" << endl;
                    }
                if(iStatusFlag & XCOMP_STATUS_NOPAUSEERRCOMP)
                    getKey(FALSE);
                else
                    getKey();
                }
            break;
            }
        else
            {
            ullSourceTotalRead+=ulBytesReadSourceFile;
            ullSourceTotalMS+=puProfilingSource->elapsedMilliSec();
            }
                                        /* Calculate CRC for current buffer (and add to
                                           CRC of previous buffer) */
        ucrc32Source.update(pbSourceData, ulBytesReadSourceFile);
                                        /* Update MD5 digest for current buffer (added to
                                           state from previous buffer) */
        umd5Source.update(pbSourceData, ulBytesReadSourceFile);
                                        /* If our buffer couldn't be filled any longer
                                           completely, we've read the last blocks of the
                                           files */
        if(ulBytesReadSourceFile!=iCompareBufferSize)
            break;
        ulBytesReadAccumulated+=ulBytesReadSourceFile;
        if(ulBytesReadExpected!=0)
            {
            ulPercentageCompleted=ulBytesReadAccumulated/ulBytesReadExpected;
            if(ulPercentageCompleted<100)
                if(ulPercentageCompleted!=ulPercentageCompletedPrevious)
                    cout << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << " (" << ulPercentageCompleted << "% done)";
            if(ulRetryCount>0)
                if(ulPercentageCompleted!=ulPercentageCompletedPrevious)
                    cout << " Retry " << ulRetryCount;
            if(ulPercentageCompleted!=ulPercentageCompletedPrevious)
                {
                cout << "  \r";
                cout.flush();
                }
            }
        ulPercentageCompletedPrevious=ulPercentageCompleted;
                                        /* We've successfully read the last block
                                           so reset retry counter so that next block
                                           starts again with the first retry.
                                           However, if a previous block that we've already
                                           read successfully will now fail, that fail will
                                           increment the retry counter (and thus may
                                           overflow the retry counter then), as we assume
                                           anything we have already read successful must
                                           still be readable */
        if(ulBlockCurrent==ulBlockFinished)
            ulRetryCount=0;
                                        /* We count the blocks successfully read
                                           (even when a retries were required) */
        if(ulBlockCurrent==ulBlockFinished)
            ulBlockFinished++;
        ulBlockCurrent++;
        }
                                        /* Blank out completion percentage and retry count */
    if(ulErrorFound==FALSE)
        {
        if(!((iStatusFlag & XCOMP_STATUS_VERBOSE) && (ulPercentageUsed==FALSE)))
            {
            if(ulPercentageUsed==TRUE)
                cout << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << "            ";
            if(ulRetryCount)
                cout << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << "        ";
            cout << endl;
            }
        cout.flush();
                                        /* We're done, thus set retry count to
                                           allow exiting this file */
        ulRetryCount=RETRY_COUNT;
        }
FileCompareOnlySimulated:
FileSizeDiffers:
#ifdef  __OS2__
    apiretRcSource=CLOSEFILE(hfileSourceFile);
#endif  /* __OS2__ */
#ifdef  __WIN32__
    SetLastError(NO_ERROR);
    CLOSEFILE(hfileSourceFile);
    apiretRcSource=GetLastError();
#endif  /* __WIN32__ */
    if(apiretRcSource!=NO_ERROR)
        {
        cout << MSG_DOSCLOSESOURCE ": Error SYS" << apiretRcSource << " closing source file returned by DosClose()";
        if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
            cout << " at Line " << (int)__LINE__;
        cout << endl;
        if(iStatusFlag & XCOMP_STATUS_LOG)
            {
            fstreamLogFile << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << endl;
            fstreamLogFile << MSG_DOSCLOSESOURCE ": Error SYS" << apiretRcSource << " closing source file returned by DosClose() at Line " << (int)__LINE__ << endl;
            }
        }
                                        /* Check for retry required */
    if(ulRetryCount<RETRY_COUNT)
        {
        ulRetryCount++;
        goto RetryReadingFromBeginning;
        }
                                        /* Get CRC and MD5 in binary and in ASCII C-string 
                                           format */
    if(ucrc32Source.final(aucCrc32, sizeof(aucCrc32))==TRUE)
        {
        UCRC32::convert2ULong(aucCrc32, &ulCrc32);
        iStatusFlag|=XCOMP_STATUS_CRC32_DONE;
        }
    if(umd5Source.final(aucMd5Digest, sizeof(aucMd5Digest))==TRUE)
        {
        UMD5::convert2ByteArray(aucMd5Digest, abMd5Digest);
        iStatusFlag|=XCOMP_STATUS_MD5_DONE;
        }
    return(NO_ERROR);
}

int         XCOMP::compareFiles(char *pcSourcePath, char *pcSourceFile, char *pcTargetPath)
{
    char            acSourceFile[CCHMAXPATH];
    char            acTargetFile[CCHMAXPATH];
    ULONG           ulActionSourceFile;
    ULONG           ulActionTargetFile;
    ULONG           ulErrorFound;
#ifdef  __OS2__
    FILESTATUS3     filestatus3SourceFile;
    FILESTATUS3     filestatus3TargetFile;
#endif  /* __OS2__ */
#ifdef  __WIN32__
    DWORD           dwSizeSourceFile;
    DWORD           dwSizeTargetFile;
#endif  /* __WIN32__ */
    ULONG           ulSizeSourceFile;
    ULONG           ulSizeTargetFile;
    ULONG           ulRetryCount;
    ULONG           ulBytesReadAccumulated;
    ULONG           ulBytesReadExpected;
    ULONG           ulPercentageCompleted;
    ULONG           ulPercentageCompletedPrevious;
    ULONG           ulPercentageUsed=FALSE;
    ULONG           ulFilePosSourceFile;
    ULONG           ulFilePosTargetFile;
    ULONG           ulBlockCurrent;
    ULONG           ulBlockFinished;
    int             iKey;
    APIRET          apiretRc;

    strcpy(acSourceFile, pcSourcePath);
    strcat(acSourceFile, pcSourceFile);
    strcpy(acTargetFile, pcTargetPath);
    strcat(acTargetFile, pcSourceFile);
    iCountProblemNone++;
    ulRetryCount=0;
    ulBlockFinished=0;
                                        /* Say what we're going to compare */
    cout << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << "\r";
    cout.flush();
RetryReadingFromBeginning:
                                        /* Clean CRC32 and MD5 checksum calculations */
    iStatusFlag&=~(XCOMP_STATUS_CRC32_DONE|XCOMP_STATUS_MD5_DONE);
    ucrc32Source.init();
    umd5Source.init();
                                        /* Start with first block of file */
    ulBlockCurrent=0;
    ulErrorFound=FALSE;
    if(iStatusFlag & XCOMP_STATUS_VERBOSE)
        {
        cout << endl;
        cerr << "  Line " << __LINE__ << ": Opening source file " << acSourceFile << endl;
        }
#ifdef  __OS2__
    apiretRcSource=DosOpen(acSourceFile,
        &hfileSourceFile,
        &ulActionSourceFile,
        0,
        FILE_NORMAL,
        OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
        OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_SEQUENTIAL | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY,
        0);
    if(apiretRcSource!=NO_ERROR)
#endif  /* __OS2__ */
#ifdef  __WIN32__
    SetLastError(NO_ERROR);
    hfileSourceFile=CreateFile(acSourceFile,
        GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);
    apiretRcSource=GetLastError();
    if(apiretRcSource!=NO_ERROR)
#endif  /* __WIN32__ */
        {
        ulErrorFound=TRUE;
        iCountProblemOpen++;
        cout << endl << MSG_DOSOPENSOURCE ": Error SYS" << apiretRcSource << " opening source file returned by DosOpen()";
        if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
            cout << " at Line " << (int)__LINE__;
        cout << endl;
        if(iStatusFlag & XCOMP_STATUS_LOG)
            {
            fstreamLogFile << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << endl;
            fstreamLogFile << MSG_DOSOPENSOURCE ": Error SYS" << apiretRcSource << " opening source file returned by DosOpen() at Line " << (int)__LINE__ << endl;
            }
        if((apiretRcSource==ERROR_SHARING_VIOLATION) || (apiretRcSource==ERROR_ACCESS_DENIED))
            {
            if(!(iStatusFlag & XCOMP_STATUS_NOPAUSEERRFIND))
                getKey();
            }
        CLOSEFILE(hfileSourceFile);
        return(apiretRcSource);
        }
    if(iStatusFlag & XCOMP_STATUS_VERBOSE)
        {
        cerr << "  Line " << __LINE__ << ": Opening target file " << acTargetFile << endl;
        }
#ifdef  __OS2__
    apiretRcTarget=DosOpen(acTargetFile,
        &hfileTargetFile,
        &ulActionTargetFile,
        0,
        FILE_NORMAL,
        OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
        OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_SEQUENTIAL | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY,
        0);
    if(apiretRcTarget!=NO_ERROR)
#endif  /* __OS2__ */
#ifdef  __WIN32__
    SetLastError(NO_ERROR);
    hfileTargetFile=CreateFile(acTargetFile,
        GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);
    apiretRcTarget=GetLastError();
    if(apiretRcTarget!=NO_ERROR)
#endif  /* __WIN32__ */
        {
        ulErrorFound=TRUE;
        iCountProblemOpen++;
#ifdef  __OS2__
        if((apiretRcTarget==ERROR_OPEN_FAILED) ||
            (apiretRcTarget==ERROR_INVALID_NAME) ||
            (apiretRcTarget==ERROR_FILENAME_EXCED_RANGE))
#endif  /* __OS2__ */
#ifdef  __WIN32__
        if((HANDLE)apiretRcTarget==INVALID_HANDLE_VALUE)
#endif  /* __WIN32__ */
            {
            cout << endl << MSG_DOSOPENTARGET ": Error SYS" << apiretRcTarget << " target file not found by DosOpen()";
            if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
                cout << " at Line " << (int)__LINE__;
            cout << endl;
            }
        else
            {
            cout << endl << MSG_DOSOPENTARGET ": Error SYS" << apiretRcTarget << " opening target file returned by DosOpen()";
            if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
                cout << " at Line " << (int)__LINE__;
            cout << endl;
            }
        if(iStatusFlag & XCOMP_STATUS_LOG)
            {
            fstreamLogFile << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << endl;
#ifdef  __OS2__
            if((apiretRcTarget==ERROR_OPEN_FAILED) ||
                (apiretRcTarget==ERROR_INVALID_NAME) ||
                (apiretRcTarget==ERROR_FILENAME_EXCED_RANGE))
#endif  /* __OS2__ */
#ifdef  __WIN32__
            if((HANDLE)apiretRcTarget==INVALID_HANDLE_VALUE)
#endif  /* __WIN32__ */
                {
                fstreamLogFile << MSG_DOSOPENTARGET ": Error SYS" << apiretRcTarget << " target file not found by DosOpen() at Line " << (int)__LINE__ << endl;
                }
            else
                {
                fstreamLogFile << MSG_DOSOPENTARGET ": Error SYS" << apiretRcTarget << " opening target file returned by DosOpen() at Line " << (int)__LINE__ << endl;
                }
            }
        if(!(iStatusFlag & XCOMP_STATUS_NOPAUSEERRFIND))
            getKey();
        CLOSEFILE(hfileSourceFile);
        CLOSEFILE(hfileTargetFile);
        return(apiretRcTarget);
        }
    else
        {
                                        /* Query soure file size */
#ifdef  __OS2__
        apiretRcSource=DosQueryFileInfo(hfileSourceFile,
            FIL_STANDARD,
            &filestatus3SourceFile,
            sizeof(filestatus3SourceFile));
        ulSizeSourceFile=filestatus3SourceFile.cbFile;
#endif  /* __OS2__ */
#ifdef  __WIN32__
        SetLastError(NO_ERROR);
        ulSizeSourceFile=GetFileSize(hfileSourceFile, &dwSizeSourceFile);
        apiretRcSource=GetLastError();
#endif  /* __WIN32__ */
        if(apiretRcSource!=NO_ERROR)
            {
            ulErrorFound=TRUE;
            cout << endl << MSG_DOSFILEINFOSOURCE ": Error SYS" << apiretRcSource << " during reading source file returned by DosQueryFileInfo()";
            if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
                cout << " at Line " << (int)__LINE__;
            cout << endl;
            if(iStatusFlag & XCOMP_STATUS_VERBOSE2)
                {
                cerr << "  Line " << __LINE__ << ": DosQueryFileInfo() found source " << ulSizeSourceFile << " bytes in size" << endl;
                }
            if(iStatusFlag & XCOMP_STATUS_LOG)
                {
                fstreamLogFile << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << endl;
                fstreamLogFile << MSG_DOSFILEINFOSOURCE ": Error SYS" << apiretRcSource << " reading source file info returned by DosQueryFileInfo() at Line " << (int)__LINE__ << endl;
                }
            }
                                        /* Query target file size */
#ifdef  __OS2__
        apiretRcTarget=DosQueryFileInfo(hfileTargetFile,
            FIL_STANDARD,
            &filestatus3TargetFile,
            sizeof(filestatus3TargetFile));
        ulSizeTargetFile=filestatus3TargetFile.cbFile;
#endif  /* __OS2__ */
#ifdef  __WIN32__
        SetLastError(NO_ERROR);
        ulSizeTargetFile=GetFileSize(hfileTargetFile, &dwSizeTargetFile);
        apiretRcTarget=GetLastError();
#endif  /* __WIN32__ */
        if(apiretRcTarget!=NO_ERROR)
            {
            ulErrorFound=TRUE;
            cout << endl << MSG_DOSFILEINFOTARGET ": Error SYS" << apiretRcTarget << " during reading target file returned by DosQueryFileInfo()";
            if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
                cout << " at Line " << (int)__LINE__;
            cout << endl;
            if(iStatusFlag & XCOMP_STATUS_VERBOSE2)
                {
                cerr << "  Line " << __LINE__ << ": DosQueryFileInfo() found target " << ulSizeTargetFile << " bytes in size" << endl;
                }
            if(iStatusFlag & XCOMP_STATUS_LOG)
                {
                fstreamLogFile << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << endl;
                fstreamLogFile << MSG_DOSFILEINFOTARGET ": Error SYS" << apiretRcTarget << " reading target file info returned by DosQueryFileInfo() at Line " << (int)__LINE__ << endl;
                }
            }
                                        /* If files differ in size, we don't need to do any
                                           comparison at all */
        if(ulSizeSourceFile!=ulSizeTargetFile)
            {
            iCountProblemCompare++;
            cout << endl << MSG_FILESIZEDIFFERS ": Source (" << ulSizeSourceFile << ") and target (" << ulSizeTargetFile << ") file sizes differ";
            if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
                cout << " at Line " << (int)__LINE__;
            cout << endl;
            if(iStatusFlag & XCOMP_STATUS_LOG)
                {
                fstreamLogFile << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << endl;
                fstreamLogFile << MSG_FILESIZEDIFFERS ": Source (" << ulSizeSourceFile << ") and target (" << ulSizeTargetFile << ") file sizes differ at Line " << (int)__LINE__ << endl;
                }
            if(iStatusFlag & XCOMP_STATUS_NOPAUSEERRCOMP)
                getKey(FALSE);
            else
                getKey();
            ulRetryCount=RETRY_COUNT;
            goto FileSizeDiffers;
            }
                                        /* If we just look for files recursively wihtout
                                           doing actual comparisons, return it's ok */
        if(iStatusFlag & XCOMP_STATUS_SIMULATE)
            {
            cout << endl;
            ulRetryCount=RETRY_COUNT;
            goto FileCompareOnlySimulated;
            }
                                        /* Initialize */
        ulBytesReadAccumulated=0;
        ulBytesReadExpected=0;
        ulPercentageCompleted=0;
        ulPercentageCompletedPrevious=(ULONG)-1;
                                        /* Calculate one percent of the file's size (if it
                                           has at least 100 bytes) */
        if(ulSizeSourceFile>=100);
            ulBytesReadExpected=(ulSizeSourceFile/100);
                                        /* If file is larger than buffer, start displaying a
                                           percentage base progress indicator, else just
                                           display the filename */
        if(ulSizeSourceFile>iCompareBufferSize)
            {
            ulPercentageUsed=TRUE;
            cout << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << " (" << ulPercentageCompleted << "% done)";
            }
        else
            cout << &pcSourcePath[iSourcePathRootLen] << pcSourceFile;
        if(ulRetryCount>0)
            cout << " Retry " << ulRetryCount;
        cout << "  \r";
        cout.flush();
                                        /* Loop until all blocks of the files have been
                                           compared */
        while (TRUE)
            {
            ulBytesReadSourceFile=ulBytesReadTargetFile=0;
                                        /* Consistency check by comparing the file pointer
                                           position of both files */
#ifdef  __OS2__
            apiretRc=DosSetFilePtr(hfileSourceFile, 0L, FILE_CURRENT, &ulFilePosSourceFile);
#endif  /* __OS2__ */
#ifdef  __WIN32__
            SetLastError(NO_ERROR);
            ulFilePosSourceFile=SetFilePointer(hfileSourceFile, 0, NULL, FILE_CURRENT);
            apiretRc=GetLastError();
#endif  /* __WIN32__ */
            if(apiretRc!=NO_ERROR)
                {
                UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_DOSFILEPTRSOURCE, "", __LINE__);

                *puexcpError << "Error SYS" << apiretRc << " returned by DosSetFilePtr()";
                throw(*puexcpError);
                }
#ifdef  __OS2__
            apiretRc=DosSetFilePtr(hfileTargetFile, 0L, FILE_CURRENT, &ulFilePosTargetFile);
#endif  /* __OS2__ */
#ifdef  __WIN32__
            SetLastError(NO_ERROR);
            ulFilePosTargetFile=SetFilePointer(hfileTargetFile, 0, NULL, FILE_CURRENT);
            apiretRc=GetLastError();
#endif  /* __WIN32__ */
            if(apiretRc!=NO_ERROR)
                {
                UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_DOSFILEPTRTARGET, "", __LINE__);

                *puexcpError << "Error SYS" << apiretRc << " returned by DosSetFilePtr()";
                throw(*puexcpError);
                }
            if(ulFilePosSourceFile!=ulFilePosTargetFile)
                {
                ulErrorFound=TRUE;
                iCountProblemCompare++;
                cout << endl << MSG_INTERNALERROR ": Internal error (" << ulFilePosSourceFile << ":" << ulFilePosTargetFile <<")";
                if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
                    cout << " at Line " << (int)__LINE__;
                cout << endl;
                if(iStatusFlag & XCOMP_STATUS_LOG)
                    {
                    fstreamLogFile << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << endl;
                    fstreamLogFile << MSG_INTERNALERROR ": Internal error (" << ulFilePosSourceFile << ":" << ulFilePosTargetFile <<") at Line " << (int)__LINE__ << endl;
                    }
                if(iStatusFlag & XCOMP_STATUS_NOPAUSEERRCOMP)
                    getKey(FALSE);
                else
                    getKey();
                break;
                }
                                        /* Read iCompareBufferSize sized blocks of data from both
                                           the source and target file and compare them. If
                                           iCompareBufferSize bytes couldn't be read anymore, we
                                           have read the last block and can exit the loop */
            if(iStatusFlag & XCOMP_STATUS_MP)
                {
#ifdef  DEBUG
                WRITESTDIO("\r\nA1+2-3+ ");
                FLUSHSTDIO();
#endif  /* DEBUG */
                                        /* If we're running multithreaded, release sempahore
                                           to let thread read target file while we read the
                                           source file and wait until the thread has finished */
#ifdef  __WIN32__
            SetLastError(NO_ERROR);
#endif  /* __WIN32__ */
                                        /* Under WIN32, requesting a semaphore returns 0 which
                                           means that the object 0 waited for has been released,
                                           which is the same as NO_ERROR (Note! Correct only
                                           when waiting for 1 Mutex as we do here) */
                apiretRc=RELEASEMUTEXSEM(hmtxThread1);
                apiretRcSource=readSourceFile();
                if(iStatusFlag & XCOMP_STATUS_VERBOSE2)
                    BEEP(500, 10);
#ifdef  DEBUG
                WRITESTDIO("A1-2-3+ ");
                FLUSHSTDIO();
#endif  /* DEBUG */
                do  {
                    apiretRc=REQUESTMUTEXSEM(hmtxThread2, MUTEX_TIMEOUT);
#ifdef  __WIN32__
                    if((apiretRc!=NO_ERROR) && (apiretRc!=WAIT_TIMEOUT))
                        apiretRc=GetLastError();
#endif  /* __WIN32__ */
                    } while(apiretRc!=NO_ERROR);
#ifdef  DEBUG
                WRITESTDIO("A1-2+3+ ");
                FLUSHSTDIO();
#endif  /* DEBUG */
                apiretRc=RELEASEMUTEXSEM(hmtxThread3);
                do  {
                    apiretRc=REQUESTMUTEXSEM(hmtxThread1, MUTEX_TIMEOUT);
#ifdef  __WIN32__
                    if((apiretRc!=NO_ERROR) && (apiretRc!=WAIT_TIMEOUT))
                        apiretRc=GetLastError();
#endif  /* __WIN32__ */
                    } while(apiretRc!=NO_ERROR);
#ifdef  DEBUG
                WRITESTDIO("A1+2+3- ");
                FLUSHSTDIO();
#endif  /* DEBUG */
                apiretRc=RELEASEMUTEXSEM(hmtxThread2);
                do  {
                    apiretRc=REQUESTMUTEXSEM(hmtxThread3, MUTEX_TIMEOUT);
#ifdef  __WIN32__
                    if((apiretRc!=NO_ERROR) && (apiretRc!=WAIT_TIMEOUT))
                        apiretRc=GetLastError();
#endif  /* __WIN32__ */
                    } while(apiretRc!=NO_ERROR);
#ifdef  DEBUG
                WRITESTDIO("A1+2-3+");
                FLUSHSTDIO();
#endif  /* DEBUG */
                }
            else
                {
                                        /* If we're running singlethreaded, read source and then
                                           target file */
                apiretRcSource=readSourceFile();
                apiretRcTarget=readTargetFile();
                }
            if(apiretRcSource!=NO_ERROR)
                {
                ulErrorFound=TRUE;
                if(ulRetryCount>=RETRY_COUNT)
                    {
                    iCountProblemCompare++;
                    cout << endl << MSG_READSOURCE ": Error SYS" << apiretRcSource << " reading source file returned by DosRead()";
                    if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
                        cout << " at Line " << (int)__LINE__;
                    cout << endl;
                    if((apiretRcSource==ERROR_CRC) || (apiretRcSource==ERROR_SECTOR_NOT_FOUND))
                        cout << "          Source media may contain weak bits or may be defective!" << endl;
                    if(iStatusFlag & XCOMP_STATUS_LOG)
                        {
                        fstreamLogFile << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << endl;
                        fstreamLogFile << MSG_READSOURCE ": Error SYS" << apiretRcSource << " reading source file returned by DosRead() at Line " << (int)__LINE__ << endl;
                        if((apiretRcSource==ERROR_CRC) || (apiretRcSource==ERROR_SECTOR_NOT_FOUND))
                            fstreamLogFile << "          Source media may contain weak bits or may be defective!" << endl;
                        }
                    if(iStatusFlag & XCOMP_STATUS_NOPAUSEERRCOMP)
                        getKey(FALSE);
                    else
                        getKey();
                    }
                break;
                }
            else
                {
                ullSourceTotalRead+=ulBytesReadSourceFile;
                ullSourceTotalMS+=puProfilingSource->elapsedMilliSec();
                }
            if(apiretRcTarget!=NO_ERROR)
                {
                ulErrorFound=TRUE;
                if(ulRetryCount>=RETRY_COUNT)
                    {
                    iCountProblemCompare++;
                    cout << endl << MSG_READTARGET ": Error SYS" << apiretRcTarget << " reading source file returned by DosRead()";
                    if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
                        cout << " at Line " << (int)__LINE__;
                    cout << endl;
                    if((apiretRcTarget==ERROR_CRC) || (apiretRcTarget==ERROR_SECTOR_NOT_FOUND))
                        cout << "          Target media may contain weak bits or may be defective!" << endl;
                    if(iStatusFlag & XCOMP_STATUS_LOG)
                        {
                        fstreamLogFile << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << endl;
                        fstreamLogFile << MSG_READTARGET ": Error SYS" << apiretRcTarget << " reading target file returned by DosRead() at Line " << (int)__LINE__ << endl;
                        if((apiretRcTarget==ERROR_CRC) || (apiretRcTarget==ERROR_SECTOR_NOT_FOUND))
                            fstreamLogFile << "          Target media may contain weak bits or may be defective!" << endl;
                        }
                    if(iStatusFlag & XCOMP_STATUS_NOPAUSEERRCOMP)
                        getKey(FALSE);
                    else
                        getKey();
                    }
                break;
                }
            else
                {
                ullTargetTotalRead+=ulBytesReadTargetFile;
                ullTargetTotalMS+=puProfilingTarget->elapsedMilliSec();
                }
            if(ulBytesReadSourceFile!=ulBytesReadTargetFile)
                {
#ifdef  DEBUG
                if(ulRetryCount<=RETRY_COUNT)
                    {
                    cout << endl << MSG_RETRYCOUNT ": Retry number " << ulRetryCount << endl;
                    getKey();
                    }
#endif  /* DEBUG */
                ulErrorFound=TRUE;
                if(ulRetryCount<RETRY_COUNT)
                    break;
                iCountProblemCompare++;
                cout << endl << MSG_COMPARESTATS ": Read for source (" << ulBytesReadSourceFile << ") and target (" << ulBytesReadTargetFile << ") different length";
                if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
                    cout << " at Line " << (int)__LINE__;
                cout << endl;
                if(iStatusFlag & XCOMP_STATUS_LOG)
                    {
                    fstreamLogFile << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << endl;
                    fstreamLogFile << MSG_COMPARESTATS ": Read for source (" << ulBytesReadSourceFile << ") and target (" << ulBytesReadTargetFile << ") different length at Line " << (int)__LINE__ << endl;
                    }
                if(iStatusFlag & XCOMP_STATUS_NOPAUSEERRCOMP)
                    getKey(FALSE);
                else
                    getKey();
                break;
                }
                                        /* Compare what we have read, and in
                                           case of an error, cause a retry again */
            if(memcmp(pbSourceData, pbTargetData, ulBytesReadSourceFile))
                {
#ifdef  DEBUG
                if(ulRetryCount<=RETRY_COUNT)
                    {
                    cout << endl << MSG_RETRYEXCEEDED ": Retry number " << ulRetryCount << endl;
                    getKey();
                    }
#endif  /* DEBUG */
                ulErrorFound=TRUE;
                if(ulRetryCount<RETRY_COUNT)
                    break;
                                        /* If we retried and still haven't found
                                           a match, then we have an unrecoverable
                                           miscompare */
                iCountProblemCompare++;
                cout << endl << MSG_DIFFERENCE ": Contents of source and target file differ";
                if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
                    cout << " at Line " << (int)__LINE__;
                cout << endl;
                if(iStatusFlag & XCOMP_STATUS_LOG)
                    {
                    fstreamLogFile << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << endl;
                    fstreamLogFile << MSG_DIFFERENCE ": Contents of source and target file differ at Line " << (int)__LINE__ << endl;
                    }
                if(iStatusFlag & XCOMP_STATUS_NOPAUSEERRCOMP)
                    getKey(FALSE);
                else
                    getKey();
                                        /* Even during an error calculate Checksums */
                ucrc32Source.update(pbSourceData, ulBytesReadSourceFile);
                umd5Source.update(pbSourceData, ulBytesReadSourceFile);
                break;
                }
                                        /* Calculate CRC for current buffer (and add to
                                           CRC of previous buffer) */
            ucrc32Source.update(pbSourceData, ulBytesReadSourceFile);
                                        /* Update MD5 digest for current buffer (added to
                                           state from previous buffer) */
            umd5Source.update(pbSourceData, ulBytesReadSourceFile);
                                        /* If our buffer couldn't be filled any longer
                                           completely, we've read the last blocks of the
                                           files */
            if(ulBytesReadSourceFile!=iCompareBufferSize)
                break;
            ulBytesReadAccumulated+=ulBytesReadSourceFile;
            if(ulBytesReadExpected!=0)
                {
                ulPercentageCompleted=ulBytesReadAccumulated/ulBytesReadExpected;
                if(ulPercentageCompleted<100)
                    if(ulPercentageCompleted!=ulPercentageCompletedPrevious)
                        cout << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << " (" << ulPercentageCompleted << "% done)";
                if(ulRetryCount>0)
                    if(ulPercentageCompleted!=ulPercentageCompletedPrevious)
                        cout << " Retry " << ulRetryCount;
                if(ulPercentageCompleted!=ulPercentageCompletedPrevious)
                    {
                    cout << "  \r";
                    cout.flush();
                    }
                }
            ulPercentageCompletedPrevious=ulPercentageCompleted;
                                        /* We've successfully read the last block
                                           so reset retry counter so that next block
                                           starts again with the first retry.
                                           However, if a previous block that we've already
                                           read successfully will now fail, that fail will
                                           increment the retry counter (and thus may
                                           overflow the retry counter then), as we assume
                                           anything we have already read successful must
                                           still be readable */
            if(ulBlockCurrent==ulBlockFinished)
                ulRetryCount=0;
                                        /* We count the blocks successfully read
                                           (even when a retries were required) */
            if(ulBlockCurrent==ulBlockFinished)
                ulBlockFinished++;
            ulBlockCurrent++;
            }
                                        /* Blank out completion percentage and retry count */
        if(ulErrorFound==FALSE)
            {
            if(!((iStatusFlag & XCOMP_STATUS_VERBOSE) && (ulPercentageUsed==FALSE)))
                {
                if(ulPercentageUsed==TRUE)
                    cout << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << "            ";
                if(ulRetryCount)
                    cout << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << "        ";
                cout << endl;
                }
            cout.flush();
                                        /* We're done, thus set retry count to
                                           allow exiting this file */
            ulRetryCount=RETRY_COUNT;
            }
        }
FileCompareOnlySimulated:
FileSizeDiffers:
#ifdef  __OS2__
    apiretRcSource=CLOSEFILE(hfileSourceFile);
#endif  /* __OS2__ */
#ifdef  __WIN32__
    SetLastError(NO_ERROR);
    CLOSEFILE(hfileSourceFile);
    apiretRcSource=GetLastError();
#endif  /* __WIN32__ */
    if(apiretRcSource!=NO_ERROR)
        {
        cout << MSG_DOSCLOSESOURCE ": Error SYS" << apiretRcSource << " closing source file returned by DosClose()";
        if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
            cout << " at Line " << (int)__LINE__;
        cout << endl;
        if(iStatusFlag & XCOMP_STATUS_LOG)
            {
            fstreamLogFile << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << endl;
            fstreamLogFile << MSG_DOSCLOSESOURCE ": Error SYS" << apiretRcSource << " closing source file returned by DosClose() at Line " << (int)__LINE__ << endl;
            }
        }
#ifdef  __OS2__
    apiretRcTarget=CLOSEFILE(hfileTargetFile);
#endif  /* __OS2__ */
#ifdef  __WIN32__
    SetLastError(NO_ERROR);
    CLOSEFILE(hfileTargetFile);
    apiretRcTarget=GetLastError();
#endif  /* __WIN32__ */
    if(apiretRcTarget!=NO_ERROR)
        {
        cout << MSG_DOSCLOSETARGET ": Error SYS" << apiretRcTarget << " closing target file returned by DosClose()";
        if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
            cout << " at Line " << (int)__LINE__;
        cout << endl;
        if(iStatusFlag & XCOMP_STATUS_LOG)
            {
            fstreamLogFile << &pcSourcePath[iSourcePathRootLen] << pcSourceFile << endl;
            fstreamLogFile << MSG_DOSCLOSETARGET ": Error SYS" << apiretRcTarget << " closing target file returned by DosClose() at Line " << (int)__LINE__ << endl;
            }
        }
                                        /* Check for retry required */
    if(ulRetryCount<RETRY_COUNT)
        {
        ulRetryCount++;
        goto RetryReadingFromBeginning;
        }
                                        /* Get CRC and MD5 in binary and in ASCII C-string 
                                           format */
    if(ucrc32Source.final(aucCrc32, sizeof(aucCrc32))==TRUE)
        {
        UCRC32::convert2ULong(aucCrc32, &ulCrc32);
        iStatusFlag|=XCOMP_STATUS_CRC32_DONE;
        }
    if(umd5Source.final(aucMd5Digest, sizeof(aucMd5Digest))==TRUE)
        {
        UMD5::convert2ByteArray(aucMd5Digest, abMd5Digest);
        iStatusFlag|=XCOMP_STATUS_MD5_DONE;
        }
    return(NO_ERROR);
}

APIRET      XCOMP::readSourceFile(void)
{
    APIRET          apiretRc=NO_ERROR;

                                        /* Read a block from the source file and
                                           profile that */
    puProfilingSource->start();
#ifdef  __OS2__
    apiretRc=DosRead(hfileSourceFile,
        pbSourceData,
        iCompareBufferSize,
        &ulBytesReadSourceFile);
#endif  /* __OS2__ */
#ifdef  __WIN32__
    SetLastError(NO_ERROR);
    apiretRc=ReadFile(hfileSourceFile,
        pbSourceData,
        iCompareBufferSize,
        &ulBytesReadSourceFile,
        NULL);
    apiretRc=GetLastError();
#endif  /* __WIN32__ */
    puProfilingSource->stop();
    return(apiretRc);
}

APIRET      XCOMP::readTargetFile(void)
{
    APIRET          apiretRc=NO_ERROR;

                                        /* Read a block from the target file and
                                           profile that */
    puProfilingTarget->start();
#ifdef  __OS2__
    apiretRc=DosRead(hfileTargetFile,
        pbTargetData,
        iCompareBufferSize,
        &ulBytesReadTargetFile);
#endif  /* __OS2__ */
#ifdef  __WIN32__
    SetLastError(NO_ERROR);
    apiretRc=ReadFile(hfileTargetFile,
        pbTargetData,
        iCompareBufferSize,
        &ulBytesReadTargetFile,
        NULL);
    apiretRc=GetLastError();
#endif  /* __WIN32__ */
    puProfilingTarget->stop();
    return(apiretRc);
}

APIRET      XCOMP::getKey(int iGetKey, int iDoBeep)
{
    APIRET          apiretRc=NO_ERROR;
    int             iKey;

    if(iGetKey==TRUE)
        {
        cout << "          ==> Press Enter to continue ";
        cout.flush();
        }
    if((!(iStatusFlag & XCOMP_STATUS_NOBEEP)) && (iDoBeep==TRUE))
        BEEP(800, 200);
    if(iGetKey==TRUE)
        {
        iKey=_getch();
        if((iKey==0) || (iKey==EXTENDED_KEY))
            iKey=_getch();
        cout << endl;
        }
    return(apiretRc);
}

void        XCOMP::processThread(void)
{
    APIRET          apiretRc;

#ifdef  __WIN32__
    SetLastError(NO_ERROR);
#endif  /* __WIN32__ */
                                        /* Request our semaphore */
    do  {
        apiretRc=REQUESTMUTEXSEM(hmtxThread2, MUTEX_TIMEOUT);
#ifdef  __WIN32__
        if(apiretRc!=WAIT_TIMEOUT)
            apiretRc=GetLastError();
#endif  /* __WIN32__ */
        } while((apiretRc!=NO_ERROR) && !(iStatusFlag & XCOMP_STATUS_SHUTDOWN));
                                        /* Keep thread running as long as XCOMP is
                                           running */
    while(!(iStatusFlag & XCOMP_STATUS_SHUTDOWN))
        {
#ifdef  __WIN32__
        SetLastError(NO_ERROR);
#endif  /* __WIN32__ */
                                        /* Wait until the semaphore is posted or 1
                                           timeout. In the first case we can read the
                                           target file, in the second we just continue
                                           the loop */
        do  {
            apiretRc=REQUESTMUTEXSEM(hmtxThread1, MUTEX_TIMEOUT);
            } while((apiretRc!=NO_ERROR) && !(iStatusFlag & XCOMP_STATUS_SHUTDOWN));
        if(apiretRc==NO_ERROR)
            {
#ifdef  DEBUG
            WRITESTDIO("B1+2+3- ");
            FLUSHSTDIO();
#endif  /* DEBUG */
                                        /* If semaphore was posted, the source file is
                                           just going to be read and we are going to read
                                           the target file. Once we finish, release the
                                           semaphore so that the source and target buffers
                                           can be compared */
            apiretRcTarget=readTargetFile();
            if(iStatusFlag & XCOMP_STATUS_VERBOSE2)
                BEEP(500, 10);
            apiretRc=RELEASEMUTEXSEM(hmtxThread2);
            do  {
                apiretRc=REQUESTMUTEXSEM(hmtxThread3, MUTEX_TIMEOUT);
#ifdef  __WIN32__
                if((apiretRc!=NO_ERROR) && (apiretRc!=WAIT_TIMEOUT))
                     apiretRc=GetLastError();
#endif  /* __WIN32__ */
                } while((apiretRc!=NO_ERROR) && !(iStatusFlag & XCOMP_STATUS_SHUTDOWN));
#ifdef  DEBUG
            WRITESTDIO("B1+2-3+ ");
            FLUSHSTDIO();
#endif  /* DEBUG */
            RELEASEMUTEXSEM(hmtxThread1);
            do  {
                apiretRc=REQUESTMUTEXSEM(hmtxThread2, MUTEX_TIMEOUT);
#ifdef  __WIN32__
                if((apiretRc!=NO_ERROR) && (apiretRc!=WAIT_TIMEOUT))
                    apiretRc=GetLastError();
#endif  /* __WIN32__ */
                } while((apiretRc!=NO_ERROR) && !(iStatusFlag & XCOMP_STATUS_SHUTDOWN));
#ifdef  DEBUG
            WRITESTDIO("B1-2+3+ ");
            FLUSHSTDIO();
#endif  /* DEBUG */
            apiretRc=RELEASEMUTEXSEM(hmtxThread3);
#ifdef  DEBUG
            WRITESTDIO("B1-2+3- ");
            FLUSHSTDIO();
#endif  /* DEBUG */
            }
        else
            {
            continue;
            }
        }
}

APIRET      XCOMP::readChkSumFile(void)
{
                                        /* Filled from a line read from the Checksum file */
    UCHAR           aucCrc32Read[UCRC32_CRC_LENGTH_STRG];
    UCHAR           aucMd5DigestRead[UMD5_DIGEST_LENGTH_STRG];
    UCHAR           aucPathRead[CCHMAXPATH];

    cout << "Reading Checksum file into memory ...";
    cout.flush();
    do  
        {
                                        /* Read next line from Checksum file */
        memset(&aucCrc32Read, '\0', sizeof(aucCrc32Read));
        memset(&aucMd5DigestRead, '\0', sizeof(aucMd5DigestRead));
        memset(&aucPathRead, '\0', sizeof(aucPathRead));
        if(iStatusFlag & XCOMP_STATUS_MD5_MD5SUM)
            {
                                        /* MD5SUM compatibility mode (the filenames in the
                                           Checksum file do not have a leading backslash) */
            MD5SUM      md5sumFile;
    
            memset(&md5sumFile, '\0', sizeof(md5sumFile));
            fstreamChkSumFile.getline((char *)&md5sumFile, sizeof(md5sumFile));
            memcpy(aucMd5DigestRead, md5sumFile.aucMd5Digest, sizeof(md5sumFile.aucMd5Digest));
            aucPathRead[0]='\\';
            strcpy((char *)&aucPathRead[1], (char *)md5sumFile.aucPath);
            }
        else
            {
                                        /* Our own Checksum file format */
            CHKSUM      chksumFile;
    
            memset(&chksumFile, '\0', sizeof(chksumFile));
            fstreamChkSumFile.getline((char *)&chksumFile, sizeof(chksumFile));
            memcpy(aucMd5DigestRead, chksumFile.aucMd5Digest, sizeof(chksumFile.aucMd5Digest));
            memcpy(aucCrc32Read, chksumFile.aucCrc32, sizeof(chksumFile.aucCrc32));
            strcpy((char *)aucPathRead, (char *)chksumFile.aucPath);
            }
        if(fstreamChkSumFile.eof())
            break;
        addChkSumEntry(aucCrc32Read, aucMd5DigestRead, aucPathRead);
        } while(TRUE);
    cout << "\r";
    cout << "                                     ";
    cout << "\r";
    cout.flush();
    return(NO_ERROR);
}

APIRET      XCOMP::addChkSumEntry(UCHAR *pucCrc32, UCHAR *pucMD5, UCHAR *pucPath)
{
    CHKSUM_ENTRY   *pChkSumEntry;

    pChkSumEntry=findChkSumEntry(pChkSumEntryRoot, pucPath);
    if(pChkSumEntry!=NULL)
        {
        if(!(iStatusFlag & XCOMP_STATUS_MD5_MD5SUM))
            UCRC32::convert2ULong(pucCrc32, &pChkSumEntry->File.ulCrc32);
        pChkSumEntry->File.pbMd5Digest=new BYTE[UMD5_DIGEST_LENGTH];
        UMD5::convert2ByteArray(pucMD5, pChkSumEntry->File.pbMd5Digest);
        }
    return(NO_ERROR);
}

CHKSUM_ENTRY
           *XCOMP::findChkSumEntry(CHKSUM_ENTRY *pChkSumEntry, UCHAR *pucPath)
{
    char           *pcBackslashBegin;
    char           *pcBackslashEnd;
    CHKSUM_ENTRY
                   *pChkSumEntryTemp;
    int             iSubDirLength;
    int             iEntry;

    pcBackslashBegin=strchr((char *)pucPath, '\\');
    if(pcBackslashBegin==0)
        return(0);
    pcBackslashEnd=strchr((char *)pcBackslashBegin+1, '\\');
    if(pcBackslashEnd==0)
        iSubDirLength=0;
    else
        iSubDirLength=(pcBackslashEnd-pcBackslashBegin);
                                        /* If this is the first entry, we have to 
                                           allocate an array, starting with a default size
                                           assumption of 2**CHKSUM_ENTRY_SIZEINITIAL */
    if(pChkSumEntryRoot==0)
        {
        pChkSumEntryRoot=(CHKSUM_ENTRY *)new CHKSUM_ENTRY[1<<CHKSUM_ENTRY_SIZEINITIAL];
        memset(pChkSumEntryRoot, 0, sizeof(CHKSUM_ENTRY)*(1<<CHKSUM_ENTRY_SIZEINITIAL));
        pChkSumEntryRoot->usStatus=CHKSUM_STATUS_ENTRYHDR;
        pChkSumEntryRoot->usSize=CHKSUM_ENTRY_SIZEINITIAL;
        pChkSumEntry=pChkSumEntryRoot;
        }
                                        /* Enumerate all entries within the array */
    for(iEntry=1; iEntry<(1<<pChkSumEntry->usSize); iEntry++)
        {
        if(iSubDirLength)
            {
                                        /* If we find the current directory level and                                          it is followed by another sublevel, then recurse 
                                           for that level to search for the subdirectory */
            if(pChkSumEntry[iEntry].usStatus & CHKSUM_STATUS_ENTRYDIR)
                {
                if((!strnicmp((char *)pChkSumEntry[iEntry].pucName, pcBackslashBegin, iSubDirLength)) &&
                    (strlen((char *)pChkSumEntry[iEntry].pucName)==iSubDirLength))
                    {
                    if(pChkSumEntry[iEntry].Dir.pSubDir==0)
                        {
                        pChkSumEntry[iEntry].Dir.pSubDir=(CHKSUM_ENTRY *)new CHKSUM_ENTRY[1<<CHKSUM_ENTRY_SIZEINITIAL];
                        memset(pChkSumEntry[iEntry].Dir.pSubDir, 0, sizeof(CHKSUM_ENTRY)*(1<<CHKSUM_ENTRY_SIZEINITIAL));
                        pChkSumEntry[iEntry].Dir.pSubDir->usStatus=CHKSUM_STATUS_ENTRYHDR;
                        pChkSumEntry[iEntry].Dir.pSubDir->usSize=CHKSUM_ENTRY_SIZEINITIAL;
                        pChkSumEntry[iEntry].Dir.pSubDir->Hdr.pParentDir=pChkSumEntry;
                        pChkSumEntry[iEntry].Dir.pSubDir->Hdr.ulIndex=iEntry;
                        }
                    return(findChkSumEntry(pChkSumEntry[iEntry].Dir.pSubDir, (UCHAR *)pcBackslashEnd));
                    }
                }
            }
        else
            {
                                            /* If we find the current file, then we're done */ 
            if(pChkSumEntry[iEntry].usStatus & CHKSUM_STATUS_ENTRYFILE)
                {
                if(!stricmp((char *)pChkSumEntry[iEntry].pucName, pcBackslashBegin))
                    return(&pChkSumEntry[iEntry]);
                }
            }
        if(pChkSumEntry[iEntry].usStatus==CHKSUM_STATUS_ENTRYEMPTY)
            break;
        }
                                        /* If we should only locate but not update the
                                           currenty entry in the Checksum file, then
                                           return if we couldn't locate it */
    if(iStatusFlag & XCOMP_STATUS_CHKSUM_READ)
        return(NULL);
                                        /* If we got over our estimated array size, the
                                           we have to reallocate a larger array (and we
                                           know by getting here, that no match was found) */
    if(iEntry>=(1<<pChkSumEntry->usSize))
        {
        pChkSumEntryTemp=(CHKSUM_ENTRY *)new CHKSUM_ENTRY[1<<(pChkSumEntry->usSize+1)];
        memset(pChkSumEntryTemp, 0, sizeof(CHKSUM_ENTRY)*(1<<(pChkSumEntry->usSize+1)));
        memcpy(pChkSumEntryTemp, pChkSumEntry, 
            sizeof(CHKSUM_ENTRY)*(1<<pChkSumEntry->usSize));
        pChkSumEntryTemp->usSize=pChkSumEntry->usSize+1;
        if(pChkSumEntry!=pChkSumEntryRoot)
            {
                                        /* Link the reallocated array into our parent
                                           array (unless we are at the root level, where
                                           there is no parent) */
            (pChkSumEntryTemp->Hdr.pParentDir[pChkSumEntryTemp->Hdr.ulIndex]).Dir.pSubDir=pChkSumEntryTemp;
                                        /* For all subdirectory links in our array,
                                           relink the subdirectories to our reallocated
                                           array */
            for(iEntry=1; iEntry<(1<<pChkSumEntryTemp->usSize); iEntry++)
                {
                if(pChkSumEntryTemp[iEntry].usStatus & CHKSUM_STATUS_ENTRYDIR)
                    {
                    pChkSumEntryTemp[iEntry].Dir.pSubDir->Hdr.pParentDir=pChkSumEntryTemp;
                    }
                if(pChkSumEntryTemp[iEntry].usStatus==CHKSUM_STATUS_ENTRYEMPTY)
                    break;
                }
            }
        delete pChkSumEntry;
        if(pChkSumEntryRoot==pChkSumEntry)
            pChkSumEntryRoot=pChkSumEntryTemp;
        pChkSumEntry=pChkSumEntryTemp;
        }
                                        /* Finally, add the current entry into the array */
    if(iSubDirLength)
        {
                                        /* We have to add another subdirectory. But as that new
                                           subdirectory was followed by another subdirectory or file,
                                           we have to add that recursively too */
        pChkSumEntry[iEntry].usStatus|=CHKSUM_STATUS_ENTRYDIR;
        pChkSumEntry[iEntry].pucName=(UCHAR *)new UCHAR[iSubDirLength+1];
        memset(pChkSumEntry[iEntry].pucName, '\0', (iSubDirLength+1));
        strncpy((char *)pChkSumEntry[iEntry].pucName, pcBackslashBegin, iSubDirLength);
        return(findChkSumEntry(pChkSumEntry, (UCHAR *)pcBackslashBegin));
        }
    else
        {
                                        /* We have to add another file. As a file is the endpoint
                                           in a path, we're done */
        iSubDirLength=strlen(pcBackslashBegin);
        pChkSumEntry[iEntry].usStatus|=CHKSUM_STATUS_ENTRYFILE;
        pChkSumEntry[iEntry].pucName=(UCHAR *)new UCHAR[iSubDirLength+1];
        memset(pChkSumEntry[iEntry].pucName, '\0', (iSubDirLength+1));
        strncpy((char *)pChkSumEntry[iEntry].pucName, pcBackslashBegin, iSubDirLength);
        return(&pChkSumEntry[iEntry]);
        }
}

CHKSUM_ENTRY
           *XCOMP::deleteChkSumEntry(CHKSUM_ENTRY *pChkSumEntry)
{
    static char     acPath[CCHMAXPATH];
    int             iEntry;

                                        /* Enumerate all entries within the array. If an
                                           entry was not yet located once before, then
                                           it is excessive in the Checksum file and that's
                                           a problem (that is the files are no longer
                                           consistent to the Checksum file) */
    if(pChkSumEntry!=0)
        {
        for(iEntry=1; iEntry<(1<<pChkSumEntry->usSize); iEntry++)
            {
            if(pChkSumEntry==pChkSumEntryRoot)
                memset(acPath, '\0', sizeof(acPath));
            if(pChkSumEntry[iEntry].usStatus & CHKSUM_STATUS_ENTRYDIR)
                {
                strcat(acPath, (char *)pChkSumEntry[iEntry].pucName);
                deleteChkSumEntry(pChkSumEntry[iEntry].Dir.pSubDir);
                }
            if(pChkSumEntry[iEntry].usStatus & CHKSUM_STATUS_ENTRYFILE)
                {
                if(!(pChkSumEntry[iEntry].usStatus & CHKSUM_STATUS_FILEFOUND))
                    {
                    iCountProblemChkSum2Much++;
                    if(iCountProblemChkSum2Much==1)
                        {
                        cout << MSG_CHKSUM2MUCH ": File(s) in Checksum file without match in filesystem";
                        if(iStatusFlag & XCOMP_STATUS_LINENUMBER)
                            cout << " at Line " << (int)__LINE__;
                        cout << ":\n";
                        if(iStatusFlag & XCOMP_STATUS_LOG)
                            {
                            fstreamLogFile << endl;
                            fstreamLogFile << MSG_CHKSUM2MUCH ": File(s) in Checksum file without match in filesystem at line " << (int)__LINE__;
                            fstreamLogFile << ":\n";
                            }
                        }
                    cout << acPath << pChkSumEntry[iEntry].pucName << endl;
                    if(iStatusFlag & XCOMP_STATUS_LOG)
                        fstreamLogFile << acPath << pChkSumEntry[iEntry].pucName << endl;
                    }
                delete pChkSumEntry[iEntry].File.pbMd5Digest;
                }
            delete pChkSumEntry[iEntry].pucName;
            }
        delete pChkSumEntry;
        }
    if(pChkSumEntry==pChkSumEntryRoot)
        {
        pChkSumEntryRoot=pChkSumEntry=0;
        if(iCountProblemChkSum2Much)
            cout << endl;
        }
    return(pChkSumEntryRoot);
}


int         XCOMP::getProblemFindFilesCount(void)
{
    return(iCountProblemFindFiles);
}

int         XCOMP::getProblemFindDirsCount(void)
{
    return(iCountProblemFindDirs);
}

int         XCOMP::getProblemOpenCount(void)
{
    return(iCountProblemOpen);
}

int         XCOMP::getProblemCompareCount(void)
{
    return(iCountProblemCompare);
}

int         XCOMP::getProblemChkSumCount(void)
{
    return(iCountProblemChkSum+iCountProblemChkSum2Much);
}

void        _Optlink xcompThread(XCOMP *pXCOMP)
{
    pXCOMP->processThread();
    _endthread();
}

int     main(int argc, char *argv[])
{
    int     iReturnCode=0;
    XCOMP  *pxcompInstance=0;

    try
        {
        pxcompInstance=new XCOMP(argc, argv);
        pxcompInstance->initialize();
        try
            {
            pxcompInstance->process();
            }
        catch(UEXCP& ruexcpError)
            {
            pxcompInstance->usage();
            ruexcpError.print();
            BEEP(800, 200);
            iReturnCode=ruexcpError.getError();
            }
                                        /* If no fatal exception happened, set return code
                                           depending on criticalness of recoverable errors */
        if((iReturnCode==0) && (pxcompInstance->getProblemOpenCount()))
            iReturnCode=1;
        if(pxcompInstance->getProblemFindFilesCount())
            iReturnCode=2;
        if(pxcompInstance->getProblemFindDirsCount())
            iReturnCode=3;
        if(pxcompInstance->getProblemCompareCount())
            iReturnCode=4;
        if(pxcompInstance->getProblemChkSumCount())
            iReturnCode=5;
        }
    catch(UEXCP& ruexcpError)
        {
        if(pxcompInstance!=0)
            pxcompInstance->usage();
        BEEP(800, 200);
        ruexcpError.print();
        iReturnCode=ruexcpError.getError();
        }
                                        /* Inform user we're done as it makes sense
                                           to run XCOMP/2 in the background */
    BEEP(800, 200);
    SLEEP(25);
    BEEP(1200, 200);
    SLEEP(25);
    BEEP(800, 200);
    SLEEP(25);
                                        /* Cleanup */
    if(pxcompInstance!=0)
        delete pxcompInstance;
#ifdef  __DEBUG_ALLOC__
    _dump_allocated(32);                /* Display memory leaks */
#endif  /* __DEBUG_ALLOC__ */
    return(iReturnCode);
}

