/***********************************************************************\
 *                              IsoComp/2                              *
 *              Copyright (C) by Stangl Roman, 2001, 2002              *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
 * IsoComp.cpp  IsoComp/2 main code.                                   *
 *                                                                     *
\***********************************************************************/

/***********************************************************************\
 *                                                                     *
 * Dualthreaded compare design:                                        *
 *                                                                     *
 * Not usinng the /!MP commandline option performs a dualthreaded      *
 * comparison that is thread 1 is responsible reading the source file  *
 * and thread 2 is responsible reading the target file. Thread 1 then  *
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

#include    "IsoComp.hpp"

            ISOCOMP::ISOCOMP(int argc, char *argv[]) : argc(argc),
                                                       argv(argv),
                                                       iCompareBufferSize(0),
                                                       iCompareBufferSectors(0),
                                                       iSourcePathRootLen(0),
                                                       iTargetPathRootLen(0),
                                                       hfileSourceFile(0),
                                                       hfileTargetFile(0),
                                                       ulBytesReadSource(0),
                                                       ulBytesReadTarget(0),
                                                       iSourceSectorsUsed(0),
                                                       iTargetSectorsUsed(0),
                                                       iSourceSectorSize(0),
                                                       pbSourceData(0),
                                                       pbTargetData(0),
                                                       iCountProblem(0),
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
                                                       iStatusFlag(ISOCOMP_STATUS_ABORT)
{
}

            ISOCOMP::~ISOCOMP(void)
{
    delete pbSourceData;
    delete pbTargetData;
    pbSourceData=pbTargetData=0;
    if(iStatusFlag & ISOCOMP_STATUS_ABORT)
        return;
    if(hfileSourceFile)
        CLOSEFILE(hfileSourceFile);
    if(hfileTargetFile)
        CLOSEFILE(hfileTargetFile);
    if(iStatusFlag & ISOCOMP_STATUS_CHKSUM)
        fstreamChkSumFile.close();
                                        /* Report success */
    if(getProblemCount()==0)
        {
        if(iStatusFlag & ISOCOMP_STATUS_SOURCE_ONLY)
            cout << MSG_SUCCESS ": Checksum calculations ended successfully";
        else if(iStatusFlag & ISOCOMP_STATUS_TARGET_WRITE)
            cout << MSG_SUCCESS ": ISO image created successfully";
        else
            cout << MSG_SUCCESS ": Comparison ended successfully";
        cout << endl;
        if(iStatusFlag & ISOCOMP_STATUS_LOG)
            {
            if(iStatusFlag & ISOCOMP_STATUS_SOURCE_ONLY)
                fstreamLogFile << MSG_SUCCESS ": Checksum calculations ended successfully";
            else if(iStatusFlag & ISOCOMP_STATUS_TARGET_WRITE)
                fstreamLogFile << endl << MSG_SUCCESS ": ISO image created successfully";
            else
                fstreamLogFile << endl << MSG_SUCCESS ": Comparison ended successfully";
            }
        }
    else
        {
        cout << endl;
        fstreamLogFile << endl;
                                        /* On error, delete empty checksum file */
        if((iStatusFlag & (ISOCOMP_STATUS_CHKSUM|ISOCOMP_STATUS_CHKSUM_LOG))==
            (ISOCOMP_STATUS_CHKSUM|ISOCOMP_STATUS_CHKSUM_LOG))
            remove(acChkSumFile);
        }
    if(iStatusFlag & ISOCOMP_STATUS_LOG)
        fstreamLogFile << endl;
                                        /* Report performance data */
    ullTotalMS=puProfilingTotal->elapsedMilliSec();
                                        /* To calculate the throughput we take the number of bytes
                                           read, multiply that by 1000 and divide it by the time in
                                           milliseconds resulting in bytes/second. We then divide that
                                           by 1024 (>>10) to calculate kB out of bytes */
    if((iStatusFlag & ISOCOMP_STATUS_SOURCE_ONLY) && (ullSourceTotalMS!=0))
        {
        cout << MSG_THROUGHPUT ": Throughput Source "
            << ((ullSourceTotalRead/ullSourceTotalMS*1000)>>10)
            << "kB/s"
            << endl;
        if(iStatusFlag & ISOCOMP_STATUS_LOG)
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
        if(iStatusFlag & ISOCOMP_STATUS_LOG)
            {
            fstreamLogFile << MSG_THROUGHPUT ": Throughput Source " << ((ullSourceTotalRead>>10)*1000/ullSourceTotalMS) << "kB/s, Destination " <<
                ((ullTargetTotalRead>>10)*1000/ullTargetTotalMS) << "kB/s, Total " <<
                (((ullSourceTotalRead+ullTargetTotalRead)>>10)*1000/ullTotalMS) << "kB/s" << endl;
            }
        }
    if(iStatusFlag & ISOCOMP_STATUS_CRC32_DONE)
        cout << MSG_CRC ": Source CRC32 checksum: " << aucCrc32 << endl;
    if(iStatusFlag & ISOCOMP_STATUS_MD5_DONE)
        cout << MSG_MD5 ": Source MD5 checksum  : " << aucMd5Digest << endl;
    if(iStatusFlag & ISOCOMP_STATUS_LOG)
        {
        if(iStatusFlag & ISOCOMP_STATUS_CRC32_DONE)
            fstreamLogFile << MSG_CRC ": Source CRC32 checksum: " << aucCrc32 << endl;
        if(iStatusFlag & ISOCOMP_STATUS_MD5_DONE)
            fstreamLogFile << MSG_MD5 ": Source MD5 checksum  : " << aucMd5Digest << endl;
        }
                                        /* If requested, stop on exit */
    if(iStatusFlag & ISOCOMP_STATUS_PAUSE_ONEXIT)
        getKey(TRUE, FALSE);
                                        /* Cleanup */
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
    if(iStatusFlag & ISOCOMP_STATUS_LOG)
        fstreamLogFile.close();
}

void        ISOCOMP::initialize(void)
{
#ifdef  __OS2__
    TIB            *ptib=0;
    PIB            *ppib=0;
#endif  /* __OS2__ */
    int             iArgument;
    APIRET          apiretRc;

    memset(acSourcePath, '\0', sizeof(acSourcePath));
    memset(acSourceFile, '\0', sizeof(acSourceFile));
    memset(acTargetPath, '\0', sizeof(acTargetPath));
    memset(acTargetFile, '\0', sizeof(acTargetFile));
    memset(acSourceFilesystem, '\0', sizeof(acSourceFilesystem));
    memset(acTargetFilesystem, '\0', sizeof(acTargetFilesystem));
    memset(acExecutableFile, '\0', sizeof(acExecutableFile));
    memset(acLogFile, '\0', sizeof(acLogFile));
    memset(acChkSumFile, '\0', sizeof(acChkSumFile));
    memset(aucCrc32, '\0', sizeof(aucCrc32));
    memset(aucMd5Digest, '\0', sizeof(aucMd5Digest));
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
    if((argv[1][0]!='/') && (argv[1][0]!='-'))
        strcpy((char *)acSourcePath, argv[1]);
    if((argc>=3) && (argv[2][0]!='/') && (argv[2][0]!='-'))
        strcpy((char *)acTargetPath, argv[2]);
    else
        iStatusFlag|=ISOCOMP_STATUS_SOURCE_ONLY;
                                        /* Check for additional commandline arguments */
    iStatusFlag|=ISOCOMP_STATUS_MP;
    for(iArgument=2; iArgument<argc; iArgument++)
        {
        strupr(argv[iArgument]);
                                        /* Multithread option */
        if((strstr(argv[iArgument], "-!MP")) ||
            (strstr(argv[iArgument], "/!MP")))
            {
            iStatusFlag&=(~ISOCOMP_STATUS_MP);
            }
                                        /* Log option */
        if((strstr(argv[iArgument], "-LOG")) ||
            (strstr(argv[iArgument], "/LOG")))
            {
            char   *pcExtension;

            iStatusFlag|=ISOCOMP_STATUS_LOG;
            if((argv[iArgument][4]==':') && (argv[iArgument][5]!='\0'))
                {
                strcpy(acLogFile, &argv[iArgument][5]);
                }
            else
                {
                if(getenv("ISOCOMP"))
                    {
                    strcpy(acLogFile, getenv("ISOCOMP"));
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
            iStatusFlag|=ISOCOMP_STATUS_CHKSUM;
            if((argv[iArgument][4]==':') && (argv[iArgument][5]!='\0'))
                strcpy(acChkSumFile, &argv[iArgument][5]);
                                        /* First, try to open the Checksum file in read
                                           mode, if that fails we know we have to write
                                           it first */
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
                iStatusFlag|=ISOCOMP_STATUS_CHKSUM_LOG;
                }
            }
                                        /* No beep for fatal errors */
        if((strstr(argv[iArgument], "-/!B")) ||
            (strstr(argv[iArgument], "/!B")))
            iStatusFlag|=ISOCOMP_STATUS_NOBEEP;
                                        /* Pause on exit */
        if((strstr(argv[iArgument], "-/P")) ||
            (strstr(argv[iArgument], "/P")))
            iStatusFlag|=ISOCOMP_STATUS_PAUSE_ONEXIT;
                                        /* Small buffer */
        if((strstr(argv[iArgument], "-TINY")) ||
            (strstr(argv[iArgument], "/TINY")))
            iStatusFlag|=ISOCOMP_STATUS_SMALLBUFFER;
        }
    if(iStatusFlag & ISOCOMP_STATUS_MP)
        {
#ifdef  __OS2__
        if(DosCreateMutexSem(NULL, &hmtxThread1, 0, TRUE)!=NO_ERROR)
            iStatusFlag&=(~ISOCOMP_STATUS_MP);
        if(DosCreateMutexSem(NULL, &hmtxThread2, 0, FALSE)!=NO_ERROR)
            iStatusFlag&=(~ISOCOMP_STATUS_MP);
        if(DosCreateMutexSem(NULL, &hmtxThread3, 0, TRUE)!=NO_ERROR)
            iStatusFlag&=(~ISOCOMP_STATUS_MP);
#endif  /* __OS2__ */
#ifdef  __WIN32__
        SetLastError(NO_ERROR);
        hmtxThread1=CreateMutex(NULL, TRUE, NULL);
        if(GetLastError()!=NO_ERROR)
            iStatusFlag&=(~ISOCOMP_STATUS_MP);
        SetLastError(NO_ERROR);
        hmtxThread2=CreateMutex(NULL, FALSE, NULL);
        if(GetLastError()!=NO_ERROR)
            iStatusFlag&=(~ISOCOMP_STATUS_MP);
        SetLastError(NO_ERROR);
        hmtxThread3=CreateMutex(NULL, TRUE, NULL);
        if(GetLastError()!=NO_ERROR)
            iStatusFlag&=(~ISOCOMP_STATUS_MP);
#endif  /* __WIN32__ */
        if(iStatusFlag & ISOCOMP_STATUS_MP)
            _beginthread((void(_Optlink *)(void *))isocompThread, 0, 65536, (void *)this);
        SLEEP(0);
        }
    iStatusFlag&=(~ISOCOMP_STATUS_ABORT);
}

void        ISOCOMP::process(void)
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
    iCompareBufferSize=ISOCOMPDATASIZE;
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
                                        /* Starting with 32MB, double buffer size whenever
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
    if(iStatusFlag & ISOCOMP_STATUS_SMALLBUFFER)
        iCompareBufferSize=(1<<16);
    iCompareBufferSectors=iCompareBufferSize/CDROM_SECTORSIZE_ISO;
    pbSourceData=new BYTE[(iCompareBufferSize/CDROM_SECTORSIZE_ISO)*CDROM_SECTORSIZE_RAW];
    pbTargetData=new BYTE[(iCompareBufferSize/CDROM_SECTORSIZE_ISO)*CDROM_SECTORSIZE_RAW];
                                        /* Verify that the Source path specified makes 
                                           sense */
    if(strlen(acSourcePath)<=2)
        verifySourceDrive();
    else
        verifySourceFile();
                                        /* Verify that the Target path specified makes
                                           sense. We expect either a drive letter or
                                           fully qualified path to a filename */
    if(!(iStatusFlag & ISOCOMP_STATUS_SOURCE_ONLY))
        {
        if(strlen(acTargetPath)<=2)
            verifyTargetDrive();
        else
            verifyTargetFile();
        }
    if(iStatusFlag & ISOCOMP_STATUS_TARGET_WRITE)
        {
        if(iStatusFlag & ISOCOMP_STATUS_SOURCE_ONLY)
            cout << "Calculating CRC32 and MD5 checksums" << endl;
        else if(iStatusFlag & ISOCOMP_STATUS_TARGET_RSJ)
            cout << "Creating RSJ track image file" << endl;
        else
            cout << "Creating ISO image file" << endl;
        if(iStatusFlag & ISOCOMP_STATUS_SOURCE_FILE)
            {
            cout << "  from Source file   "; 
            if(iStatusFlag & ISOCOMP_STATUS_SOURCE_RAW)
                cout << "(2352 Bytes/Sector) ";
            else
                cout << "(2048 Bytes/Sector) ";
            cout << acSourcePath << acSourceFile << endl;
            }
        else
            {
            cout << "  from Source CD-ROM (2048 Bytes/Sector) " << acSourcePath << endl;
            }
        cout << "  into Target file   (2048 Bytes/Sector) " << acTargetPath << acTargetFile << endl;
        }
    else
        {
        if(iStatusFlag & ISOCOMP_STATUS_SOURCE_ONLY)
            cout << "Calculating CRC32 and MD5 checksums" << endl;
        else if(iStatusFlag & ISOCOMP_STATUS_SOURCE_RSJ)
            cout << "Comparing RSJ track image (ISO image contents)" << endl;
        else
            cout << "Comparing ISO image" << endl;
        if(iStatusFlag & ISOCOMP_STATUS_SOURCE_FILE)
            {
            cout << "  from Source file   ";
            if(iStatusFlag & ISOCOMP_STATUS_SOURCE_RAW)
                cout << "(2352 Bytes/Sector) ";
            else
                cout << "(2048 Bytes/Sector) ";
            cout << acSourcePath << acSourceFile <<  endl;
            }
        else
            {
            cout << "  from Source CD-ROM (2048 Bytes/Sector) " << acSourcePath << endl;
            }
        if(!(iStatusFlag & ISOCOMP_STATUS_SOURCE_ONLY))
            {
            if(iStatusFlag & ISOCOMP_STATUS_TARGET_CDROM)
                {
                cout << "  with Target CD-ROM (2048 Bytes/Sector) " << acTargetPath << acTargetFile << endl;
                }
            else
                {
                cout << "  with Target file   ";
                if(iStatusFlag & ISOCOMP_STATUS_TARGET_RAW)
                    cout << "(2352 Bytes/Sector) ";
                else
                    cout << "(2048 Bytes/Sector) ";
                cout << acTargetPath << acTargetFile << endl;
                }
            }
        }
    if(iStatusFlag & ISOCOMP_STATUS_LOG)
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
        if(iStatusFlag & ISOCOMP_STATUS_TARGET_WRITE)
            {
            if(iStatusFlag & ISOCOMP_STATUS_TARGET_RSJ)
                fstreamLogFile << "Creating RSJ track image file" << endl;
            else
                fstreamLogFile << "Creating ISO image file" << endl;
            if(iStatusFlag & ISOCOMP_STATUS_SOURCE_FILE)
                {
                fstreamLogFile << "  from Source file   ";
                if(iStatusFlag & ISOCOMP_STATUS_SOURCE_RAW)
                    fstreamLogFile << "(2352 Bytes/Sector) ";
                else
                    fstreamLogFile << "(2048 Bytes/Sector) ";
                cout << acSourcePath << acSourceFile << endl;
                }
            else
                {
                fstreamLogFile << "  from Source CD-ROM (2048 Bytes/Sector) " << acSourcePath << endl;
                }
            fstreamLogFile << "  into Target file (2048 Bytes/Sector) " << acTargetPath << acTargetFile << endl;
            }
        else
            {
            if(iStatusFlag & ISOCOMP_STATUS_SOURCE_ONLY)
                fstreamLogFile << "Calculating CRC32 and MD5 checksums" << endl;
            else if(iStatusFlag & ISOCOMP_STATUS_SOURCE_RSJ)
                fstreamLogFile << "Comparing RSJ track image (ISO image contents)" << endl;
            else
                fstreamLogFile << "Comparing ISO image" << endl;
            if(iStatusFlag & ISOCOMP_STATUS_SOURCE_FILE)
                {
                fstreamLogFile << "  from Source file   ";
                if(iStatusFlag & ISOCOMP_STATUS_SOURCE_RAW)
                    fstreamLogFile << "(2352 Bytes/Sector) ";
                else
                    fstreamLogFile << "(2048 Bytes/Sector) ";
                fstreamLogFile << acSourcePath << acSourceFile << endl;
                }
            else
                {
                fstreamLogFile << "  from Source CD-ROM (2048 Bytes/Sector) " << acSourcePath << endl;
                }
            if(!(iStatusFlag & ISOCOMP_STATUS_SOURCE_ONLY))
                {
                if(iStatusFlag & ISOCOMP_STATUS_TARGET_CDROM)
                    {
                    fstreamLogFile << "  with Target CD-ROM (2048 Bytes/Sector) " << acTargetPath << acTargetFile << endl;
                    }
                else
                    {
                    fstreamLogFile << "  with Target file   ";
                    if(iStatusFlag & ISOCOMP_STATUS_TARGET_RAW)
                        fstreamLogFile << "(2352 Bytes/Sector) ";
                    else
                        fstreamLogFile << "(2048 Bytes/Sector) ";
                    fstreamLogFile << acTargetPath << acTargetFile << endl;
                    }
                }
            }
        }
    if(iStatusFlag & ISOCOMP_STATUS_CHKSUM)
        {
        if(iStatusFlag & ISOCOMP_STATUS_CHKSUM_LOG)
            {
            cout << "  writing checksums into file " << acChkSumFile << endl;
            if(iStatusFlag & ISOCOMP_STATUS_LOG)
                fstreamLogFile << "  writing checksums into file " << acChkSumFile << endl;
            }
        else
            {
            cout << "  reading checksums from file " << acChkSumFile << endl;
            if(iStatusFlag & ISOCOMP_STATUS_LOG)
                fstreamLogFile << "  reading checksums from file " << acChkSumFile << endl;
            }
        }
    cout << "using " << iCompareBufferSize << " bytes buffer size (" << iCompareBufferSectors << " CD-ROM sectors)" << endl;
    if(iStatusFlag & ISOCOMP_STATUS_LOG)
        cout << "and logging into " << acLogFile << endl;
    cout << endl << endl;
    if(iStatusFlag & ISOCOMP_STATUS_LOG)
        fstreamLogFile << "using " << iCompareBufferSize << " bytes buffer size (" << iCompareBufferSectors << " CD-ROM sectors)\n\n" << endl;
                                        /* If the size of the Source and Target files
                                           differ, it doesn't make sense to compare them.
                                           Also, when writing the Target file we can't
                                           compare file sizes */
    if(!(iStatusFlag & ISOCOMP_STATUS_SOURCE_ONLY) &&
        !(iStatusFlag & ISOCOMP_STATUS_TARGET_WRITE) &&
        (iSourceSectorsUsed!=iTargetSectorsUsed))
        {
        UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_FILESIZEDIFFERS, "", __LINE__);

        *puexcpError << "Source and Target size (" << iSourceSectorsUsed << ":" << iTargetSectorsUsed << " sectors) differs";
        if(iStatusFlag & ISOCOMP_STATUS_LOG)
            fstreamLogFile << MSG_FILESIZEDIFFERS ": Source and Target size (" << iSourceSectorsUsed << ":" << iTargetSectorsUsed << " sectors) differs at line " << (int)__LINE__ << endl;
        iCountProblem++;
        throw(*puexcpError);
        }
                                        /* Do the matching */
    ucrc32Source.init();
    umd5Source.init();
    puProfilingTotal->start();
    if(iStatusFlag & ISOCOMP_STATUS_SOURCE_ONLY)
        readImage();
    else
        compareImage();
    puProfilingTotal->stop();
                                        /* Inform thread to shutdown */
    iStatusFlag|=ISOCOMP_STATUS_SHUTDOWN;
                                        /* Get CRC and MD5 in ASCII C-string format */
    if(ucrc32Source.final(aucCrc32, sizeof(aucCrc32))==TRUE)
        iStatusFlag|=ISOCOMP_STATUS_CRC32_DONE;
    if(umd5Source.final(aucMd5Digest, sizeof(aucMd5Digest))==TRUE)
        iStatusFlag|=ISOCOMP_STATUS_MD5_DONE;
                                        /* Checksum result processing */
    if(iStatusFlag & ISOCOMP_STATUS_CHKSUM)
        {
        if(iStatusFlag & ISOCOMP_STATUS_CHKSUM_LOG)
            {
#ifdef  __OS2__
            fstreamChkSumFile << "CRC32: " << aucCrc32 << " MD5: " << aucMd5Digest << 
                " Path: " << acSourcePath << acSourceFile <<  endl;
#endif  /* __OS2__ */
#ifdef  __WIN32__
            if(iStatusFlag & ISOCOMP_STATUS_SOURCE_FILE)
                fstreamChkSumFile << "CRC32: " << aucCrc32 << " MD5: " << aucMd5Digest << 
                    " Path: " << acSourcePath << acSourceFile <<  endl;
            else
                fstreamChkSumFile << "CRC32: " << aucCrc32 << " MD5: " << aucMd5Digest << 
                    " Path: " << &acSourcePath[4] << acSourceFile <<  endl;
#endif  /* __WIN32__ */
            }
        else
            {
            CHKSUM      chksumFile;
    
            memset(&chksumFile, '\0', sizeof(chksumFile));
            fstreamChkSumFile.getline((char *)&chksumFile, sizeof(chksumFile));
            if(strnicmp((char *)chksumFile.aucCrc32, (char *)aucCrc32, sizeof(chksumFile.aucCrc32)) ||
                strnicmp((char *)chksumFile.aucMd5Digest, (char *)aucMd5Digest, sizeof(chksumFile.aucMd5Digest)))
                {
                UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_CHKSUMMISMATCH, "", __LINE__);
    
                iCountProblem++;
                *puexcpError << "Checksum(s) did not match! " << acChkSumFile << " contains:\n";
                chksumFile.aucTextMD5[0]='\0';
                chksumFile.aucTextPath[0]='\0';
                if(strnicmp((char *)chksumFile.aucCrc32, (char *)aucCrc32, sizeof(chksumFile.aucCrc32)))
                    *puexcpError << APPLICATION_MESSAGE_PREFIX << "File CRC32 checksum:   " << (char *)chksumFile.aucCrc32 << "\n";
                if(strnicmp((char *)chksumFile.aucMd5Digest, (char *)aucMd5Digest, sizeof(chksumFile.aucMd5Digest)))
                    *puexcpError << APPLICATION_MESSAGE_PREFIX << "File MD5 checksum:     " << (char *)chksumFile.aucMd5Digest << "\n";
                *puexcpError << APPLICATION_MESSAGE_PREFIX << "Possible data corruption, please verify!";
                if(iStatusFlag & ISOCOMP_STATUS_LOG)
                    {
                    fstreamLogFile << MSG_CHKSUMMISMATCH << ": " << puexcpError->text() << " at line " << __LINE__;
                    }
                throw(*puexcpError);
                }
            }
        }
}

void        ISOCOMP::usage(void)
{
    cout << COPYRIGHT_1 << endl;
    cout << COPYRIGHT_2 << endl;
    cout << COPYRIGHT_3 << endl;
    cout << "Use the ISOCOMP command to compare the contents of a Source CD-ROM with the" << endl;
    cout << "contents of a Target CD-ROM or with an ISO, RAW or RSJ track image file." << endl;
    cout << "Or you can create an ISO image or RSJ track image file out of a CD-ROM." << endl << endl;
    cout << "Syntax:" << endl;
    cout << "  ISOCOMP Source(CDROM-drive:|[drive:\\|\\\\server\\][path\\]ISO|RAW-file)" << endl;
    cout << "        Target(CDROM-drive:|[drive:\\|\\\\server\\][path\\]ISO|RAW-file)" << endl;
    cout << "        [/!MP] [/LOG[:[drive:\\|\\\\server\\][path\\]file]] [/P] [/TINY]" << endl;
    cout << "        [/CHK:drive:\\|\\\\server\\][path\\]file]" << endl;
    cout << "  ISOCOMP CDROM-drive: | CDROM-drive:|[drive:\\|\\\\server\\][path\\]ISO|RAW-file" << endl;
    cout << "        [/!MP] [/LOG[:[drive:\\|\\\\server\\][path\\]file]] [/P] [/TINY]" << endl;
    cout << "        [/CHK:[drive:\\|\\\\server\\][path\\]file]" << endl;
    cout << "Where:" << endl;
    cout << "  Source(CDROM-drive:|[drive:\\|\\\\server\\][path\\]ISO|RAW-file)" << endl;
    cout << "                 Specifies either the Source drive letter (e.g. Z or Z:) of" << endl;
    cout << "                 your CD-ROM drive containing a data CD-ROM, or the location" << endl;
    cout << "                 and name of a Source ISO, RAW or RSJ track image file, whose" << endl;
    cout << "                 contents you want to compare with a media in your Target" << endl;
    cout << "                 CD-ROM drive or to compare it with or write from an ISO image" << endl;
    cout << "                 or RSJ track image file." << endl;
    cout << "                 You may specify a fully qualified path or UNC path." << endl;
    cout << "  Target(CDROM-drive:|[drive:\\|\\\\server\\][path\\]ISO|RAW-file)" << endl;
    cout << "                 Specifies either the Target drive letter (e.g. Z or Z:) of" << endl;
    cout << "                 your CD-ROM drive containing a data CD-ROM, or the location" << endl;
    cout << "                 and name of a Target ISO, RAW or RSJ track image file. If" << endl;
    cout << "                 the image file does not exist, it will be created out of the" << endl;
    cout << "                 CD-ROM contents, otherwise compared with the CD-ROM contents" << endl;
    cout << "                 (Only ISO or RSJ track image files will be created)." << endl;
    cout << "                 You may specify a fully qualified path or UNC path." << endl;
    cout << "  [/!MP]" << endl;
    cout << "                 Specifies that only 1 thread is used reading the CD-ROM" << endl;
    cout << "                 contents and the image file instead of the default of 2" << endl;
    cout << "                 threads." << endl;
    cout << "  [/LOG[:[drive:\\|\\\\server\\][path\\]file]]" << endl;
    cout << "                 Specifies that ISOCOMP/2 logs all problems into a file" << endl;
    cout << "                 specified either by this parameter, or by the ISOCOMP" << endl;
    cout << "                 environment variable or into ISOCOMP.LOG (put into the" << endl;
    cout << "                 directory ISOCOMP/2 was installed into) otherwise." << endl;
    cout << "  [/P]" << endl;
    cout << "                 Request ISOCOMP/2 to pause when it has finished."<< endl;
    cout << "  [/TINY]" << endl;
    cout << "                 2 64kB buffers are used instead of a percentage of total RAM." << endl;
    cout << "  [/CHK:[drive:\\|\\\\server\\][path\\]file]" << endl;
    cout << "                 Specifies that ISOCOMP/2 uses a checksum file to ensure data" << endl;
    cout << "                 integrity. If the checksum file does not exist, it will be" << endl;
    cout << "                 created, otherwise compared with the checksum calculated" << endl;
    cout << "                 from the data read from the Source." << endl;
    cout << "Returns:" << endl;
    cout << "  0              Successful completion" << endl;
    cout << "  100+           Fatal, unrecoverable exceptions\n" << endl;
}

int         ISOCOMP::setProblemCount(void)
{
    return(++iCountProblem);
}

int         ISOCOMP::getProblemCount(void)
{
    return(iCountProblem);
}

void        ISOCOMP::verifySourceDrive(void)
{
    ULONG           ulBuffLength;
#ifdef  __OS2__
    FSQBUFFER2     *pfsqbuffer2CD;
    FSALLOCATE      fsallocateCD;
#endif  /* __OS2__ */
#ifdef  __WIN32__
    DISK_GEOMETRY   diskgeometryCD;
    ULONG           ulSectorsPerCluster;
    ULONG           ulBytesPerSector;
    ULONG           ulNumberOfFreeClusters;
    ULONG           ulTotalNumberOfClusters;
#endif  /* __WIN32__ */
    ULONG           ulAction;
    APIRET          apiretRc;
    
                                        /* Check if the specified Source path is
                                           a drive letter for a CD-ROM drive */
                                        /* We only accept "drive" or "drive:" */
    if(strlen(acSourcePath)>2)
        throw(UEXCP(APPLICATION_PREFIX, ERR_SOURCEPATH, "Invalid Source drive letter, please specify CD-ROM drive letter"));
    if(acSourcePath[1]=='\0')
        strcat(acSourcePath, ":");
#ifdef  __OS2__
                                        /* Check if the Source drive looks like a
                                           CD-ROM drive, that is formatted with the
                                           CDFS or CDWFS filesystems and having a 
                                           sector size of 2048 bytes */
    ulBuffLength=sizeof(*pfsqbuffer2CD)+3*CCHMAXPATH;
    pfsqbuffer2CD=(FSQBUFFER2 *)new BYTE[ulBuffLength];
    apiretRc=DosQueryFSAttach(acSourcePath, 0, FSAIL_QUERYNAME, pfsqbuffer2CD, &ulBuffLength);
    if(apiretRc!=NO_ERROR)
        {
        UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_SOURCEDOSQUERYFSATTACH, "", __LINE__);

        *puexcpError << "Invalid Source drive letter, please specify CD-ROM drive letter!\n";
        *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosQueryFSAttach()";
        throw(*puexcpError);
        }
    strcpy(acSourceFilesystem, (char *)pfsqbuffer2CD->szName+pfsqbuffer2CD->cbName+1);
    delete pfsqbuffer2CD;
    pfsqbuffer2CD=0;
    if((strcmp(acSourceFilesystem, "CDFS")) && (strcmp(acSourceFilesystem, "CDWFS")))
        throw(UEXCP(APPLICATION_PREFIX, ERR_SOURCEFILESYSTEM, "Source drive filesystem isn't CDFS, please specify a CD-ROM drive"));
    apiretRc=DosQueryFSInfo((acSourcePath[0]&0xDF)-'A'+1, FSIL_ALLOC, &fsallocateCD, sizeof(fsallocateCD));
    if(apiretRc!=NO_ERROR)
        {
        UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_SOURCEDOSQUERYFSINFO, "", __LINE__);

        *puexcpError << "Invalid media, please specify Source CD-ROM drive containing a media!\n";
        *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosQueryFSInfo()";
        throw(*puexcpError);
        }
    iSourceSectorsUsed=fsallocateCD.cUnit;
    iSourceSectorSize=fsallocateCD.cbSector;
    if(iSourceSectorSize!=CDROM_SECTORSIZE_ISO)
        throw(UEXCP(APPLICATION_PREFIX, ERR_SOURCESECTORSIZE, "Source drive filesystem isn't CDFS, please specify a CD-ROM drive"));
                                        /* Open the CD-ROM drive as a DASD media */
    apiretRc=DosOpen(acSourcePath, &hfileSourceFile, &ulAction, 0, FILE_NORMAL,
        OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
        OPEN_FLAGS_DASD | OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_NO_CACHE | 
        OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY, NULL);
    if(apiretRc!=NO_ERROR)
        {
        UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_SOURCEDOSOPEN, "", __LINE__);

        *puexcpError << "Source CD-ROM drive can't be opened. Please insert a data CD!\n";
        *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosOpen()";
        throw(*puexcpError);
        }
#endif  /* __OS2__ */
#ifdef  __WIN32__
                                        /* Get information about the Source media */
    strcat(acSourcePath, "\\");
    SetLastError(NO_ERROR);
    GetDiskFreeSpace(acSourcePath, &ulSectorsPerCluster, &ulBytesPerSector, 
        &ulNumberOfFreeClusters, &ulTotalNumberOfClusters);
    acSourcePath[2]='\0';
    iSourceSectorSize=ulBytesPerSector;
    iSourceSectorsUsed=ulTotalNumberOfClusters;
    apiretRc=GetLastError();
    if(apiretRc!=NO_ERROR)
        {
        UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_SOURCEDOSQUERYFSATTACH, "", __LINE__);

        *puexcpError << "Invalid Source drive letter, please specify CD-ROM drive letter!\n";
        *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosQueryFSAttach()";
        throw(*puexcpError);
        }
    if(iSourceSectorSize!=CDROM_SECTORSIZE_ISO)
        throw(UEXCP(APPLICATION_PREFIX, ERR_SOURCESECTORSIZE, "Source drive filesystem isn't CDFS, please specify a CD-ROM drive"));
                                        /* Open the CD-ROM drive as a DASD media, that
                                           is using the syntax \\.\x: */
    strcpy(&acSourcePath[sizeof("\\\\.\\")-1], acSourcePath);
    memcpy(acSourcePath, "\\\\.\\", sizeof("\\\\.\\")-1);
    SetLastError(NO_ERROR);
    hfileSourceFile=CreateFile(acSourcePath,
        GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    apiretRc=GetLastError();
    if(apiretRc!=NO_ERROR)
        {
        UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_SOURCEDOSOPEN, "", __LINE__);

        *puexcpError << "Source CD-ROM drive can't be opened. Please insert a data CD!\n";
        *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosOpen()";
        throw(*puexcpError);
        }
#endif  /* __WIN32__ */
}

void        ISOCOMP::verifySourceFile(void)
{
    char            acSourcePathCopy[CCHMAXPATH];
    ULONG           ulDiskNum;
    ULONG           ulTemp;
    int             iUNCFilename;
    char           *pcFilename;
    char           *pcExtension;
    ULONG           ulAction;
    ULONG           ulActual;
    APIRET          apiretRc;

    memset(acSourcePathCopy, '\0', sizeof(acSourcePathCopy));
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
    GetFullPathName(acSourcePath, sizeof(acSourcePathCopy), acSourcePathCopy, NULL);
    apiretRc=GetLastError();
#endif  /* __WIN32__ */
    if(apiretRc!=NO_ERROR)
        {
        UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_SOURCEDRIVE, "", __LINE__);

        *puexcpError << "Invalid path or filename for Source ISO image, please correct!\n";
        *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosQueryPathInfo()";
        throw(*puexcpError);
        }
    if(strcmpi(acSourcePath, acSourcePathCopy))
        strcpy(acSourcePath, acSourcePathCopy);
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
            throw(UEXCP(APPLICATION_PREFIX, ERR_SOURCEUNCPATH, "Invalid UNC name specified for Source file"));
            }
        pcFilename++;
        if(*pcFilename!='\0')
            strcpy(acSourceFile, pcFilename);
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
            UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_SOURCECURRENTDISK, "", __LINE__);

            *puexcpError << "Invalid path or filename for Source ISO image, please correct!\n";
            *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosQueryCurrentDisk()";
            throw(*puexcpError);
            }
                                        /* If we didn't find a backslash, we may either have:
                                             - just the filename(s)
                                             - just the drive letter */
        pcFilename=strchr(acSourcePath, ':');
        if(pcFilename==acSourcePath)
            {
            throw(UEXCP(APPLICATION_PREFIX, ERR_SOURCEDRIVE, "Drive letter not specified for Source file"));
            }
        if(pcFilename==0)
            {
                                        /* We didn't find a drive letter, so let's use the
                                           current drive and directory */
            strcpy(acSourceFile, acSourcePath);
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
                UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_SOURCECURRENTDIR, "", __LINE__);

                *puexcpError << "Invalid path or filename for Source ISO image, please correct!\n";
                *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosQueryCurrentDir()";
                throw(*puexcpError);
                }
            }
        else if(*pcFilename==':')
            {
                                        /* We found a drive letter, which has to be enhanced with
                                           the current directory on that drive */
            strcpy(acSourceFile, pcFilename+1);
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
                UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_SOURCECURRENTDIR1, "", __LINE__);

                *puexcpError << "Invalid path or filename for Source ISO image, please correct!\n";
                *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosQueryCurrentDir()";
                throw(*puexcpError);
                }
            }
        else
            {
                                        /* As we didn't find any drive letter, the user must have
                                           specified the filename(s) he's looking for, so get the
                                           current directory and the current drive */
            strcpy(acSourceFile, acSourcePath);
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
                UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_SOURCECURRENTDIR2, "", __LINE__);

                *puexcpError << "Invalid path or filename for Source ISO image, please correct!\n";
                *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosQueryCurrentDir()";
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
                                        /* Check for trailing backslash, if nothing follows 
                                           the ISO image filename is missing */
    if(acSourceFile[0]=='\0')
        throw(UEXCP(APPLICATION_PREFIX, ERR_SOURCEFILENAME, "Only path specified, please specify Source ISO image filename too"));
                                        /* Open the imagefile, if it does exist, we will
                                           use it to compare CD-ROM contents with, if not
                                           we've a problem */
    strcpy(acSourcePathCopy, acSourcePath);
    strcat(acSourcePathCopy, acSourceFile);
#ifdef  __OS2__
    apiretRc=DosOpen(acSourcePathCopy,
        &hfileSourceFile,
        &ulAction,
        0,
        FILE_NORMAL,
        OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
        OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_SEQUENTIAL | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY,
        0);
#endif  /* __OS2__ */
#ifdef  __WIN32__
    SetLastError(NO_ERROR);
    hfileSourceFile=CreateFile(acSourcePathCopy,
        GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    apiretRc=GetLastError();
#endif  /* __WIN32__ */
    if(apiretRc==NO_ERROR)
        {
                                        /* We're comparing the CD-ROM with an ISO image.
                                           Check if that image is a RSJ track image, in
                                           which case we have to ignore the first 40 bytes */
#ifdef  __OS2__
        apiretRc=DosRead(hfileSourceFile, pbSourceData, RSJ_HEADERSIZE, &ulActual);
#endif  /* __OS2__ */
#ifdef  __WIN32__
        SetLastError(NO_ERROR);
        apiretRc=ReadFile(hfileSourceFile,
            pbSourceData,
            RSJ_HEADERSIZE,
            &ulActual,
            NULL);
        apiretRc=GetLastError();
#endif  /* __WIN32__ */
        if(apiretRc==NO_ERROR)
            {
                                        /* Check if we have a RSJ track image */
            while(TRUE)
                {
                if(ulActual<RSJ_HEADERSIZE)
                    break;
                                        /* No signature "RSJ CDWFS TRACK FILE" */
                if(memcmp(((RSJ_HEADER *)pbSourceData)->acSignature, RSJ_SIGNATURE, sizeof(RSJ_SIGNATURE)-1))
                    break;
                                        /* No test did fail, so we'll probably have
                                           an RSJ track image file here */
                iStatusFlag|=ISOCOMP_STATUS_SOURCE_RSJ;
                break;
                }
            }
        else
            {
            UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_SOURCERSJDOSREAD, "", __LINE__);

            *puexcpError << "Invalid contents for specified Source ISO image, please correct!\n";
            *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosRead()";
            throw(*puexcpError);
            }
                                        /* Get file size in bytes, use this as a basis to
                                           calculate size in sectors (to simulate for our
                                           Source file what the CD-ROM would have told us
                                           directly) */
#ifdef  __OS2__
        DosSetFilePtr(hfileSourceFile, 0, FILE_END, &ulActual);
#endif  /* __OS2__ */
#ifdef  __WIN32__
        ulActual=SetFilePointer(hfileSourceFile, 0, NULL, FILE_END);
#endif  /* __WIN32__ */
        iSourceSectorsUsed=ulActual;
                                        /* Reposition back to the beginning of the ISO 
                                           image file contents (that is after the RSJ header
                                           for RSJ track images) */ 
        if(iStatusFlag & ISOCOMP_STATUS_SOURCE_RSJ)
            {
            iSourceSectorsUsed-=RSJ_HEADERSIZE;
#ifdef  __OS2__
            DosSetFilePtr(hfileSourceFile, RSJ_HEADERSIZE, FILE_BEGIN, &ulActual);
#endif  /* __OS2__ */
#ifdef  __WIN32__
            ulActual=SetFilePointer(hfileSourceFile, RSJ_HEADERSIZE, NULL, FILE_BEGIN);
#endif  /* __WIN32__ */
            }
        else
            {
#ifdef  __OS2__
            DosSetFilePtr(hfileSourceFile, 0, FILE_BEGIN, &ulActual);
#endif  /* __OS2__ */
#ifdef  __WIN32__
            ulActual=SetFilePointer(hfileSourceFile, 0, NULL, FILE_BEGIN);
#endif  /* __WIN32__ */
            }
                                        /* If the extension is .RAW or .BIN assume we
                                           read from a RAW track image file */
        pcExtension=strrchr(acSourcePathCopy, '.');
        if(pcExtension!=0)
            {
            if(!(strnicmp(pcExtension, RAW_EXTENSION1, sizeof(RAW_EXTENSION1))) ||                
                !(strnicmp(pcExtension, RAW_EXTENSION2, sizeof(RAW_EXTENSION2))))
                {
                iSourceSectorsUsed/=CDROM_SECTORSIZE_RAW;
                iStatusFlag|=ISOCOMP_STATUS_SOURCE_RAW;
                }
            else
                {
                pcExtension=0;
                }
            }
                                        /* If we didn't find a extension, or the one
                                           we found didn't help, try to determine format
                                           from the file size */
        if(pcExtension==0)
            {
            if(((iSourceSectorsUsed%CDROM_SECTORSIZE_RAW)==0) &&
                ((iSourceSectorsUsed%CDROM_SECTORSIZE_ISO)!=0))
                {
                iSourceSectorsUsed/=CDROM_SECTORSIZE_RAW;
                iStatusFlag|=ISOCOMP_STATUS_SOURCE_RAW;
                }
            else
                {
                iSourceSectorsUsed/=CDROM_SECTORSIZE_ISO;
                }
            }
        }
    else
        {
        UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_SOURCEDOSOPEN, "", __LINE__);

        *puexcpError << "Invalid path or filename for Source CD-ROM or image, please correct!\n";
        *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosOpen()";
        throw(*puexcpError);
        }
                                        /* Our Source is a file not CD-ROM drive */
    iStatusFlag|=ISOCOMP_STATUS_SOURCE_FILE;
}

void        ISOCOMP::verifyTargetDrive(void)
{
    ULONG           ulBuffLength;
#ifdef  __OS2__
    FSQBUFFER2     *pfsqbuffer2CD;
    FSALLOCATE      fsallocateCD;
#endif  /* __OS2__ */
#ifdef  __WIN32__
    ULONG           ulSectorsPerCluster;
    ULONG           ulBytesPerSector;
    ULONG           ulNumberOfFreeClusters;
    ULONG           ulTotalNumberOfClusters;
#endif  /* __WIN32__ */
    ULONG           ulAction;
    APIRET          apiretRc;
    
                                        /* Check if the specified Target path is
                                           a drive letter for a CD-ROM drive */
    if(acTargetPath[1]=='\0')
        strcat(acTargetPath, ":");
#ifdef  __OS2__
                                        /* Check if the Target drive looks like a
                                           CD-ROM drive, that is formatted with the
                                           CDFS or CDWFS filesystems and having a 
                                           sector size of 2048 bytes */
    ulBuffLength=sizeof(*pfsqbuffer2CD)+3*CCHMAXPATH;
    pfsqbuffer2CD=(FSQBUFFER2 *)new BYTE[ulBuffLength];
    apiretRc=DosQueryFSAttach(acTargetPath, 0, FSAIL_QUERYNAME, pfsqbuffer2CD, &ulBuffLength);
    if(apiretRc!=NO_ERROR)
        {
        UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_TARGETDOSQUERYFSATTACH, "", __LINE__);

        *puexcpError << "Invalid Target drive letter, please specify CD-ROM drive letter!\n";
        *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosQueryFSAttach()";
        throw(*puexcpError);
        }
    strcpy(acTargetFilesystem, (char *)pfsqbuffer2CD->szName+pfsqbuffer2CD->cbName+1);
    delete pfsqbuffer2CD;
    pfsqbuffer2CD=0;
    if((strcmp(acTargetFilesystem, "CDFS")) && (strcmp(acTargetFilesystem, "CDWFS")))
        throw(UEXCP(APPLICATION_PREFIX, ERR_TARGETFILESYSTEM, "Target drive filesystem isn't CDFS, please specify a CD-ROM drive"));
    apiretRc=DosQueryFSInfo((acTargetPath[0]&0xDF)-'A'+1, FSIL_ALLOC, &fsallocateCD, sizeof(fsallocateCD));
    if(apiretRc!=NO_ERROR)
        {
        UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_TARGETDOSQUERYFSINFO, "", __LINE__);

        *puexcpError << "Invalid media, please specify Target CD-ROM drive containing a media!\n";
        *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosQueryFSInfo()";
        throw(*puexcpError);
        }
    iTargetSectorsUsed=fsallocateCD.cUnit;
    if(fsallocateCD.cbSector!=CDROM_SECTORSIZE_ISO)
        throw(UEXCP(APPLICATION_PREFIX, ERR_TARGETSECTORSIZE, "Target drive filesystem isn't CDFS, please specify a CD-ROM drive"));
                                        /* Open the CD-ROM drive as a DASD media */
    apiretRc=DosOpen(acTargetPath, &hfileTargetFile, &ulAction, 0, FILE_NORMAL,
        OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
        OPEN_FLAGS_DASD | OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_NO_CACHE | 
        OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY, NULL);
    if(apiretRc!=NO_ERROR)
        {
        UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_TARGETDOSOPEN, "", __LINE__);

        *puexcpError << "Target CD-ROM drive can't be opened. Please insert a data CD!\n";
        *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosOpen()";
        throw(*puexcpError);
        }
#endif  /* __OS2__ */
#ifdef  __WIN32__
                                        /* Get information about the Target media */
    strcat(acTargetPath, "\\");
    SetLastError(NO_ERROR);
    GetDiskFreeSpace(acTargetPath, &ulSectorsPerCluster, &ulBytesPerSector, 
        &ulNumberOfFreeClusters, &ulTotalNumberOfClusters);
    acTargetPath[2]='\0';
    iTargetSectorsUsed=ulTotalNumberOfClusters;
    apiretRc=GetLastError();
    if(apiretRc!=NO_ERROR)
        {
        UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_TARGETDOSQUERYFSATTACH, "", __LINE__);

        *puexcpError << "Invalid Target drive letter, please specify CD-ROM drive letter!\n";
        *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosQueryFSAttach()";
        throw(*puexcpError);
        }
    if(ulBytesPerSector!=CDROM_SECTORSIZE_ISO)
        throw(UEXCP(APPLICATION_PREFIX, ERR_TARGETSECTORSIZE, "Target drive filesystem isn't CDFS, please specify a CD-ROM drive"));
                                        /* Open the CD-ROM drive as a DASD media, that
                                           is using the syntax \\.\x: */
    strcpy(&acTargetPath[sizeof("\\\\.\\")-1], acTargetPath);
    memcpy(acTargetPath, "\\\\.\\", sizeof("\\\\.\\")-1);
    SetLastError(NO_ERROR);
    hfileTargetFile=CreateFile(acTargetPath,
        GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    apiretRc=GetLastError();
    if(apiretRc!=NO_ERROR)
        {
        UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_TARGETDOSOPEN, "", __LINE__);

        *puexcpError << "Target CD-ROM drive can't be opened. Please insert a data CD!\n";
        *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosOpen()";
        throw(*puexcpError);
        }
#endif  /* __WIN32__ */
                                        /* Our Target file is also a CD-ROM drive */
    iStatusFlag|=ISOCOMP_STATUS_TARGET_CDROM;
}

void        ISOCOMP::verifyTargetFile(void)
{
    char            acTargetPathCopy[CCHMAXPATH];
    ULONG           ulDiskNum;
    ULONG           ulTemp;
    int             iUNCFilename;
    char           *pcFilename;
    char           *pcExtension;
    ULONG           ulAction;
    ULONG           ulActual;
    APIRET          apiretRc;

    memset(acTargetPathCopy, '\0', sizeof(acTargetPathCopy));
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
    apiretRc=DosQueryPathInfo(acTargetPath, FIL_QUERYFULLNAME, acTargetPathCopy, sizeof(acTargetPathCopy));
#endif  /* __OS2__ */
#ifdef  __WIN32__
    SetLastError(NO_ERROR);
    GetFullPathName(acTargetPath, sizeof(acTargetPathCopy), acTargetPathCopy, NULL);
    apiretRc=GetLastError();
#endif  /* __WIN32__ */
    if(apiretRc!=NO_ERROR)
        {
        UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_TARGETDRIVE, "", __LINE__);

        *puexcpError << "Invalid path or filename for Target ISO image, please correct!\n";
        *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosQueryPathInfo()";
        throw(*puexcpError);
        }
    if(strcmpi(acTargetPath, acTargetPathCopy))
        strcpy(acTargetPath, acTargetPathCopy);
                                        /* Test for an UNC-name */
    iUNCFilename=FALSE;
    if(!strncmp(acTargetPath, "\\\\", sizeof("\\\\")-1))
        iUNCFilename=TRUE;
                                        /* Test for a backslash, as everything that follows must
                                           be the filename(s) we want to compare from */
    pcFilename=strrchr(acTargetPath, '\\');
    if(pcFilename!=0)
        {
                                        /* If we found the backslash, construct path and filename(s)
                                           info, but take care for UNC filenames */
        if((iUNCFilename==TRUE) && (pcFilename<=(acTargetPath+1)))
            {
            throw(UEXCP(APPLICATION_PREFIX, ERR_TARGETUNCPATH, "Invalid UNC name specified for Target file"));
            }
        pcFilename++;
        if(*pcFilename!='\0')
            strcpy(acTargetFile, pcFilename);
        *pcFilename='\0';
        iTargetPathRootLen=strlen(acTargetPath);
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
        apiretRc=GetCurrentDirectory(sizeof(acTargetPathCopy), acTargetPathCopy);
        apiretRc=GetLastError();
#endif  /* __WIN32__ */
        if(apiretRc!=NO_ERROR)
            {
            UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_TARGETCURRENTDISK, "", __LINE__);

            *puexcpError << "Invalid path or filename for Target ISO image, please correct!\n";
            *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosQueryCurrentDisk()";
            throw(*puexcpError);
            }
                                        /* If we didn't find a backslash, we may either have:
                                             - just the filename(s)
                                             - just the drive letter */
        pcFilename=strchr(acTargetPath, ':');
        if(pcFilename==acTargetPath)
            {
            throw(UEXCP(APPLICATION_PREFIX, ERR_TARGETDRIVE, "Drive letter not specified for Target file"));
            }
        if(pcFilename==0)
            {
                                        /* We didn't find a drive letter, so let's use the
                                           current drive and directory */
            strcpy(acTargetFile, acTargetPath);
            strcpy(acTargetPath, "C:\\");
            ulLength=CCHMAXPATH-strlen(acTargetPath);
            acTargetPath[0]=(char)((ulDriveNum-1)+(ULONG)'a');
#ifdef  __OS2__
            apiretRc=DosQueryCurrentDir(ulDriveNum, &acTargetPath[strlen(acTargetPath)], &ulLength);
#endif  /* __OS2__ */
#ifdef  __WIN32__
            SetLastError(NO_ERROR);
            apiretRc=GetCurrentDirectory(ulLength, &acTargetPath[strlen(acTargetPath)]);
            apiretRc=GetLastError();
#endif  /* __WIN32__ */
            if(apiretRc!=NO_ERROR)
                {
                UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_TARGETCURRENTDIR, "", __LINE__);

                *puexcpError << "Invalid path or filename for Target ISO image, please correct!\n";
                *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosQueryCurrentDir()";
                throw(*puexcpError);
                }
            }
        else if(*pcFilename==':')
            {
                                        /* We found a drive letter, which has to be enhanced with
                                           the current directory on that drive */
            strcpy(acTargetFile, pcFilename+1);
            strcpy(pcFilename+1, "\\");
            ulLength=CCHMAXPATH-strlen(acTargetPath);
            ulDriveNum=((ULONG)*(pcFilename-1) | 0x00000020)-(ULONG)'a';
            ulDriveNum++;
#ifdef  __OS2__
            apiretRc=DosQueryCurrentDir(ulDriveNum, &acTargetPath[strlen(acTargetPath)], &ulLength);
#endif  /* __OS2__ */
#ifdef  __WIN32__
            SetLastError(NO_ERROR);
            apiretRc=GetCurrentDirectory(ulLength, &acTargetPath[strlen(acTargetPath)]);
            apiretRc=GetLastError();
#endif  /* __WIN32__ */
            if(apiretRc!=NO_ERROR)
                {
                UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_TARGETCURRENTDIR1, "", __LINE__);

                *puexcpError << "Invalid path or filename for Target ISO image, please correct!\n";
                *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosQueryCurrentDir()";
                throw(*puexcpError);
                }
            }
        else
            {
                                        /* As we didn't find any drive letter, the user must have
                                           specified the filename(s) he's looking for, so get the
                                           current directory and the current drive */
            strcpy(acTargetFile, acTargetPath);
            acTargetPath[0]=(char)((ulDriveNum-1)+(ULONG)'a');
            strcpy(&acTargetPath[1], ":\\");
            ulLength=CCHMAXPATH-strlen(acTargetPath);
#ifdef  __OS2__
            apiretRc=DosQueryCurrentDir(ulDriveNum, &acTargetPath[strlen(acTargetPath)], &ulLength);
#endif  /* __OS2__ */
#ifdef  __WIN32__
            SetLastError(NO_ERROR);
            apiretRc=GetCurrentDirectory(ulLength, &acTargetPath[strlen(acTargetPath)]);
            apiretRc=GetLastError();
#endif  /* __WIN32__ */
            if(apiretRc!=NO_ERROR)
                {
                UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_TARGETCURRENTDIR2, "", __LINE__);

                *puexcpError << "Invalid path or filename for Target ISO image, please correct!\n";
                *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosQueryCurrentDir()";
                throw(*puexcpError);
                }
            }
        iTargetPathRootLen=strlen(acTargetPath);
        if(acTargetPath[iTargetPathRootLen-1]!='\\')
            {
            strcat(acTargetPath, "\\");
            iTargetPathRootLen++;
            }
        }
                                        /* Check for trailing backslash, if nothing follows 
                                           the ISO image filename is missing */
    if(acTargetFile[0]=='\0')
        throw(UEXCP(APPLICATION_PREFIX, ERR_TARGETFILENAME, "Only path specified, please specify Target ISO image filename too"));
                                        /* Open the imagefile, if it does exist, we will
                                           use it to compare CD-ROM contents with, if not
                                           we'll write CD-ROM contents into a newly
                                           created file */
    strcpy(acTargetPathCopy, acTargetPath);
    strcat(acTargetPathCopy, acTargetFile);
#ifdef  __OS2__
    apiretRc=DosOpen(acTargetPathCopy,
        &hfileTargetFile,
        &ulAction,
        0,
        FILE_NORMAL,
        OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
        OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_SEQUENTIAL | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY,
        0);
#endif  /* __OS2__ */
#ifdef  __WIN32__
    SetLastError(NO_ERROR);
    hfileTargetFile=CreateFile(acTargetPathCopy,
        GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);
    apiretRc=GetLastError();
#endif  /* __WIN32__ */
    if(apiretRc==NO_ERROR)
        {
                                        /* We're comparing the CD-ROM with an ISO image.
                                           Check if that image is a RSJ track image, in
                                           which case we have to ignore the first 40 bytes */
#ifdef  __OS2__
        apiretRc=DosRead(hfileTargetFile, pbTargetData, RSJ_HEADERSIZE, &ulActual);
#endif  /* __OS2__ */
#ifdef  __WIN32__
        SetLastError(NO_ERROR);
        apiretRc=ReadFile(hfileTargetFile,
            pbTargetData,
            RSJ_HEADERSIZE,
            &ulActual,
            NULL);
        apiretRc=GetLastError();
#endif  /* __WIN32__ */
        if(apiretRc==NO_ERROR)
            {
                                        /* Check if we have a RSJ track image */
            while(TRUE)
                {
                if(ulActual<RSJ_HEADERSIZE)
                    break;
                                        /* No signature "RSJ CDWFS TRACK FILE" */
                if(memcmp(((RSJ_HEADER *)pbTargetData)->acSignature, RSJ_SIGNATURE, sizeof(RSJ_SIGNATURE)-1))
                    break;
                                        /* No test did fail, so we'll probably have
                                           an RSJ track image file here */
                iStatusFlag|=ISOCOMP_STATUS_TARGET_RSJ;
                break;
                }
            }
        else
            {
            UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_TARGETRSJDOSREAD, "", __LINE__);

            *puexcpError << "Invalid contents for Target ISO image specified, please correct!\n";
            *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosRead()";
            throw(*puexcpError);
            }
                                        /* Get file size in bytes, to compare with Source */
#ifdef  __OS2__
        DosSetFilePtr(hfileTargetFile, 0, FILE_END, &ulActual);
#endif  /* __OS2__ */
#ifdef  __WIN32__
        ulActual=SetFilePointer(hfileTargetFile, 0, NULL, FILE_END);
#endif  /* __WIN32__ */
        iTargetSectorsUsed=ulActual;
        if(iStatusFlag & ISOCOMP_STATUS_TARGET_RSJ)
            {
            iTargetSectorsUsed=(ulActual-RSJ_HEADERSIZE)/CDROM_SECTORSIZE_ISO;
#ifdef  __OS2__
            DosSetFilePtr(hfileTargetFile, RSJ_HEADERSIZE, FILE_BEGIN, &ulActual);
#endif  /* __OS2__ */
#ifdef  __WIN32__
            ulActual=SetFilePointer(hfileTargetFile, RSJ_HEADERSIZE, NULL, FILE_BEGIN);
#endif  /* __WIN32__ */
            }
        else
            {
                                        /* If the extension is .RAW or .BIN assume we
                                           read from a RAW track image file */
            pcExtension=strrchr(acTargetPathCopy, '.');
            if(pcExtension!=0)
                {
                if(!(strnicmp(pcExtension, RAW_EXTENSION1, sizeof(RAW_EXTENSION1))) ||                
                    !(strnicmp(pcExtension, RAW_EXTENSION2, sizeof(RAW_EXTENSION2))))
                    {
                    iTargetSectorsUsed/=CDROM_SECTORSIZE_RAW;
                    iStatusFlag|=ISOCOMP_STATUS_TARGET_RAW;
                    }
                else
                    {
                    pcExtension=0;
                    }
                }
                                        /* If we didn't find a extension, or the one
                                           we found didn't help, try to determine format
                                           from the file size */
            if(pcExtension==0)
                {
                if(((iTargetSectorsUsed%CDROM_SECTORSIZE_RAW)==0) &&
                    ((iTargetSectorsUsed%CDROM_SECTORSIZE_ISO)!=0))
                    {
                    iTargetSectorsUsed/=CDROM_SECTORSIZE_RAW;
                    iStatusFlag|=ISOCOMP_STATUS_TARGET_RAW;
                    }
                else
                    {
                    iTargetSectorsUsed/=CDROM_SECTORSIZE_ISO;
                    }
                }
#ifdef  __OS2__
            DosSetFilePtr(hfileTargetFile, 0, FILE_BEGIN, &ulActual);
#endif  /* __OS2__ */
#ifdef  __WIN32__
            ulActual=SetFilePointer(hfileTargetFile, 0, NULL, FILE_BEGIN);
#endif  /* __WIN32__ */
            }
        }
    else
        {
        char       *pcExtension;
        RSJ_HEADER  rsjHeader;

                                        /* Let's try to get to the filename extension */
        pcExtension=strrchr(acTargetPathCopy, '.');
        if(pcExtension!=0)
            {
                                        /* If the extension is .RSJ or .TRK assume we
                                           want to create an RSJ track image file */
            if(!(strnicmp(pcExtension, RSJ_EXTENSION1, sizeof(RSJ_EXTENSION1))) ||                
                !(strnicmp(pcExtension, RSJ_EXTENSION2, sizeof(RSJ_EXTENSION2))))
                {
                iStatusFlag|=ISOCOMP_STATUS_TARGET_RSJ;
                }
            }
                                        /* We're writing an ISO image or RSJ track
                                           image file from the CD-ROM */
#ifdef  __OS2__
        apiretRc=DosOpen(acTargetPathCopy,
            &hfileTargetFile,
            &ulAction,
            iSourceSectorsUsed*CDROM_SECTORSIZE_ISO+
                (iStatusFlag & ISOCOMP_STATUS_TARGET_RSJ ? RSJ_HEADERSIZE :0),
            FILE_NORMAL,
            OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_FAIL_IF_EXISTS,
            OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_SEQUENTIAL | OPEN_SHARE_DENYWRITE | OPEN_ACCESS_WRITEONLY,
            0);
#endif  /* __OS2__ */
#ifdef  __WIN32__
        SetLastError(NO_ERROR);
        hfileTargetFile=CreateFile(acTargetPathCopy,
            GENERIC_WRITE,
            FILE_SHARE_READ,
            NULL,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        apiretRc=GetLastError();
#endif  /* __WIN32__ */
        if(apiretRc!=NO_ERROR)
            {
            UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_TARGETDOSOPEN, "", __LINE__);

            *puexcpError << "Invalid path or filename for Target CD-ROM or image, please correct!\n";
            *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosOpen()";
            throw(*puexcpError);
            }
        iStatusFlag|=ISOCOMP_STATUS_TARGET_WRITE;
        if(iStatusFlag & ISOCOMP_STATUS_TARGET_RSJ)
            {
            memset(&rsjHeader, 0, sizeof(rsjHeader));
            memcpy(rsjHeader.acSignature, RSJ_SIGNATURE, sizeof(rsjHeader.acSignature));
            rsjHeader.ulIcon=RSJ_ICON;
            rsjHeader.ulFunction=RSJ_FUNCTION;
            rsjHeader.ulSectors=iSourceSectorsUsed;
            rsjHeader.ulHeaderSize=RSJ_HEADERSIZE;
#ifdef  __OS2__
            apiretRc=DosWrite(hfileTargetFile, &rsjHeader, sizeof(rsjHeader), &ulActual);
#endif  /* __OS2__ */
#ifdef  __WIN32__
            SetLastError(NO_ERROR);
            WriteFile(hfileTargetFile, &rsjHeader, sizeof(rsjHeader), &ulActual, NULL);
            apiretRc=GetLastError();
#endif  /* __WIN32__ */
            if(apiretRc!=NO_ERROR)
                {
                UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_TARGETRSJDOSWRITE, "", __LINE__);
    
                *puexcpError << "Can't write Target RSJ track image header!\n";
                *puexcpError << APPLICATION_MESSAGE_PREFIX << "Error SYS" << apiretRc << " returned by DosWrite()";
                throw(*puexcpError);
                }
            }
        }
}

void        ISOCOMP::readImage(void)
{
    APIRET          apiretRc;
    int             iSectorCurrent=0;
    ULONG           ulRetryCount=0;
    ULONG           ulFilePosSourceFile=0;
    ULONG           ulPercentageCompleted=0;

                                        /* Loop until all blocks of the files have been
                                           compared */
    while (TRUE)
        {
        cout << "Reading sector " << iSectorCurrent << " of " << iSourceSectorsUsed;
                                        /* If file is larger than buffer, start displaying a
                                           percentage base progress indicator, else just
                                           display the filename */
        if((iSourceSectorsUsed*CDROM_SECTORSIZE_ISO)>iCompareBufferSize)
            {
                                        /* Calculate in kB (division by 1024) to avoid 
                                           overflows */
            ulPercentageCompleted=(((ullSourceTotalRead>>10)*100)/(iSourceSectorsUsed*2));
            cout << " (" << ulPercentageCompleted << "% done)";
            }
        if(ulRetryCount>0)
            {
            cout << " Retry " << ulRetryCount;
            if(iStatusFlag & ISOCOMP_STATUS_LOG)
                {
                fstreamLogFile << "Reading sector " << iSectorCurrent << " of " << iSourceSectorsUsed;
                fstreamLogFile << " (" << ulPercentageCompleted << "% done)" " Retry " << ulRetryCount << endl;
                }
            }
        else
            cout << "        ";
        cout << "  \r";
        cout.flush();
        ulBytesReadSource=ulBytesReadTarget=0;
                                        /* Consistency check by checking the file pointer
                                           position */
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
            UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_SOURCEDOSFILEPTR, "", __LINE__);

            *puexcpError << "Error SYS" << apiretRc << " returned by Source DosSetFilePtr()";
            if(iStatusFlag & ISOCOMP_STATUS_LOG)
                {
                fstreamLogFile << MSG_TARGETDOSFILEPTR ": " "Error SYS" << apiretRc << " returned by Source DosSetFilePtr()" " at Line " << (int)__LINE__ << endl;
                }
            iCountProblem++;
            throw(*puexcpError);
            }
        apiretRcSource=readSourceFile();
                                        /* Check for errors */
        if(apiretRcSource!=NO_ERROR)
            {
            ulRetryCount++;
            if(ulRetryCount>=RETRY_COUNT)
                {
                UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_SOURCEDOSREAD, "", __LINE__);
    
                *puexcpError << "Error SYS" << apiretRc << " returned by Source DosRead()";
                if(iStatusFlag & ISOCOMP_STATUS_LOG)
                    {
                    fstreamLogFile << MSG_SOURCEDOSREAD ": " "Error SYS" << apiretRc << " returned by Source DosRead()" " at Line " << (int)__LINE__ << endl;
                    }
                iCountProblem++;
                throw(*puexcpError);
                }
                                        /* Reposition the head by reading the first
                                           and last sector. Al */
            readSourceFile(0);
            readSourceFile(iSourceSectorsUsed-1);
            readSourceFile(iSectorCurrent);
            readTargetFile(iSectorCurrent);
            continue;
            }
                                        /* Reset retry counter */
        ulRetryCount=0;
                                        /* We've successfully compared the current
                                           buffer contents, advance position in
                                           the file */
        ullSourceTotalMS+=puProfilingSource->elapsedMilliSec();
        ullSourceTotalRead+=ulBytesReadSource;
        iSectorCurrent+=(ulBytesReadSource/CDROM_SECTORSIZE_ISO);
                                        /* Calculate CRC for current buffer (and add to
                                           CRC of previous buffer) */
        ucrc32Source.update(pbSourceData, ulBytesReadSource);
                                        /* Update MD5 digest for current buffer (added to
                                           state from previous buffer) */
        umd5Source.update(pbSourceData, ulBytesReadSource);
                                        /* If nothing more could be read in the source
                                           file, we're done */
        if((ulBytesReadSource==0) || 
            ((iSectorCurrent*CDROM_SECTORSIZE_ISO+ulBytesReadSource)==(iSourceSectorsUsed*CDROM_SECTORSIZE_ISO)))
            {
            cout << "\r" << "                                                                               ";
            cout << "\r";
            cout.flush();
            break;
            }
        }
}

void        ISOCOMP::compareImage(void)
{
    APIRET          apiretRc;
    int             iSectorCurrent=0;
    ULONG           ulRetryCount=0;
    ULONG           ulFilePosSourceFile=0;
    ULONG           ulFilePosTargetFile=0;
    ULONG           ulPercentageCompleted=0;

                                        /* Loop until all blocks of the files have been
                                           compared */
    while (TRUE)
        {
#ifdef  DEBUG
        WRITESTDIO("\r\n");
        FLUSHSTDIO();
#endif  /* DEBUG */
        if(iStatusFlag & ISOCOMP_STATUS_TARGET_WRITE)
            cout << "Written sector " << iSectorCurrent << " of " << iSourceSectorsUsed;
        else
            cout << "Comparing sector " << iSectorCurrent << " of " << iSourceSectorsUsed;
                                        /* If file is larger than buffer, start displaying a
                                           percentage base progress indicator, else just
                                           display the filename */
        if((iSourceSectorsUsed*CDROM_SECTORSIZE_ISO)>iCompareBufferSize)
            {
                                        /* Calculate in kB (division by 1024) to avoid 
                                           overflows */
            ulPercentageCompleted=(((ullSourceTotalRead>>10)*100)/(iSourceSectorsUsed*2));
            cout << " (" << ulPercentageCompleted << "% done)";
            }
        if(ulRetryCount>0)
            {
            cout << " Retry " << ulRetryCount;
            if(iStatusFlag & ISOCOMP_STATUS_LOG)
                {
                if(iStatusFlag & ISOCOMP_STATUS_TARGET_WRITE)
                    fstreamLogFile << "Written sector " << iSectorCurrent << " of " << iSourceSectorsUsed;
                else
                    fstreamLogFile << "Comparing sector " << iSectorCurrent << " of " << iSourceSectorsUsed;
                fstreamLogFile << " (" << ulPercentageCompleted << "% done)" " Retry " << ulRetryCount << endl;
                }
            }
        else
            cout << "        ";
        cout << "  \r";
        cout.flush();

        ulBytesReadSource=ulBytesReadTarget=0;
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
            UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_SOURCEDOSFILEPTR, "", __LINE__);

            *puexcpError << "Error SYS" << apiretRc << " returned by Source DosSetFilePtr()";
            if(iStatusFlag & ISOCOMP_STATUS_LOG)
                {
                fstreamLogFile << MSG_TARGETDOSFILEPTR ": " "Error SYS" << apiretRc << " returned by Source DosSetFilePtr()" " at Line " << (int)__LINE__ << endl;
                }
            iCountProblem++;
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
            UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_TARGETDOSFILEPTR, "", __LINE__);

            *puexcpError << "Error SYS" << apiretRc << " returned by Target DosSetFilePtr()";
            if(iStatusFlag & ISOCOMP_STATUS_LOG)
                {
                fstreamLogFile << MSG_TARGETDOSFILEPTR ": " "Error SYS" << apiretRc << " returned by Target DosSetFilePtr()" " at Line " << (int)__LINE__ << endl;
                }
            iCountProblem++;
            throw(*puexcpError);
            }
                                        /* Adjust when reading a RAW instead of an ISO image */
        if(iStatusFlag & ISOCOMP_STATUS_SOURCE_RAW)
            ulFilePosSourceFile=(ulFilePosSourceFile/CDROM_SECTORSIZE_RAW)*CDROM_SECTORSIZE_ISO;
        if(iStatusFlag & ISOCOMP_STATUS_TARGET_RAW)
            ulFilePosTargetFile=(ulFilePosTargetFile/CDROM_SECTORSIZE_RAW)*CDROM_SECTORSIZE_ISO;
                                        /* If we have a RSJ track image file, we already
                                           are positioned at the first byte after the
                                           RSJ header, so we need to subtract it */
        if((ulFilePosSourceFile-(iStatusFlag & ISOCOMP_STATUS_SOURCE_RSJ ? RSJ_HEADERSIZE : 0))!=
            (ulFilePosTargetFile-(iStatusFlag & ISOCOMP_STATUS_TARGET_RSJ ? RSJ_HEADERSIZE : 0)))
            {
            UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_TARGETFILESIZE, "", __LINE__);

            *puexcpError << "Unexpected file positioning error (" << ulFilePosSourceFile << ":" << ulFilePosTargetFile <<")";
            if(iStatusFlag & ISOCOMP_STATUS_LOG)
                {
                fstreamLogFile << MSG_TARGETFILESIZE ": " "Unexpected file positioning error (" << ulFilePosSourceFile << ":" << ulFilePosTargetFile <<") at Line " << (int)__LINE__ << endl;
                }
            iCountProblem++;
            throw(*puexcpError);
            }
                                        /* Read iCompareBufferSize sized blocks of data from both
                                           the Source and Target file and compare them. If
                                           iCompareBufferSize bytes couldn't be read anymore, we
                                           have read the last block and can exit the loop.
                                           For writing, we can't multithread anyway, as
                                           the Target can't be written before the Source
                                           has been read */
        if(!(iStatusFlag & ISOCOMP_STATUS_TARGET_WRITE) &&
            (iStatusFlag & ISOCOMP_STATUS_MP))
            {
#ifdef  DEBUG
            WRITESTDIO("\r\nA1+2-3+ ");
            FLUSHSTDIO();
#endif  /* DEBUG */
                                        /* If we're running multithreaded, release sempahore
                                           to let thread read Target file while we read the
                                           Source file and wait until the thread has finished */
#ifdef  __WIN32__
            SetLastError(NO_ERROR);
#endif  /* __WIN32__ */
                                        /* Under WIN32, requesting a semaphore returns 0 which
                                           means that the object 0 waited for has been released,
                                           which is the same as NO_ERROR (Note! Correct only
                                           when waiting for 1 Mutex as we do here) */
            apiretRc=RELEASEMUTEXSEM(hmtxThread1);
            apiretRcSource=readSourceFile();
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
            WRITESTDIO("A1+2-3+ ");
            FLUSHSTDIO();
#endif  /* DEBUG */
            }
        else
            {
                                        /* If we're running singlethreaded, read Source and then
                                           Target file */
            apiretRcSource=readSourceFile();
            if(iStatusFlag & ISOCOMP_STATUS_TARGET_WRITE)
                apiretRcTarget=writeTargetFile();
            else
                apiretRcTarget=readTargetFile();
            }
                                        /* Check for errors */
        if(apiretRcSource!=NO_ERROR)
            {
            ulRetryCount++;
            if(ulRetryCount>=RETRY_COUNT)
                {
                UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_SOURCEDOSREAD, "", __LINE__);
    
                *puexcpError << "Error SYS" << apiretRc << " returned by Source DosRead()";
                if(iStatusFlag & ISOCOMP_STATUS_LOG)
                    {
                    fstreamLogFile << MSG_SOURCEDOSREAD ": " "Error SYS" << apiretRc << " returned by Source DosRead()" " at Line " << (int)__LINE__ << endl;
                    }
                iCountProblem++;
                throw(*puexcpError);
                }
                                        /* Reposition the head by reading the first
                                           and last sector. */
            readSourceFile(0);
            readSourceFile(iSourceSectorsUsed-1);
            readSourceFile(iSectorCurrent);
//            readTargetFile(iSectorCurrent);
            continue;
            }
        if(apiretRcTarget!=NO_ERROR)
            {
            ulRetryCount++;
            if(iStatusFlag & ISOCOMP_STATUS_TARGET_WRITE)
                {
                if(ulRetryCount>=RETRY_COUNT)
                    {
                    UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_TARGETDOSREAD, "", __LINE__);
        
                    *puexcpError << "Error SYS" << apiretRc << " returned by Target DosWrite()";
                    if(iStatusFlag & ISOCOMP_STATUS_LOG)
                        {
                        fstreamLogFile << MSG_TARGETDOSREAD ": " "Error SYS" << apiretRc << " returned by Target DosWrite()" " at Line " << (int)__LINE__ << endl;                        }
                    iCountProblem++;
                    throw(*puexcpError);
                    }
                }
            else
                {
                if(ulRetryCount>=RETRY_COUNT)
                    {
                    UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_TARGETDOSREAD, "", __LINE__);
        
                    *puexcpError << "Error SYS" << apiretRc << " returned by Target DosRead()";
                    if(iStatusFlag & ISOCOMP_STATUS_LOG)
                        {
                        fstreamLogFile << MSG_TARGETDOSREAD ": " "Error SYS" << apiretRc << " returned by Target DosRead()" " at Line " << (int)__LINE__ << endl;
                        }
                    iCountProblem++;
                    throw(*puexcpError);
                    }
                }
                                        /* Reposition the head by reading the first
                                           and last sector. */
//            readSourceFile(iSectorCurrent);
            readTargetFile(0);
            readTargetFile(iSourceSectorsUsed-1);
            readTargetFile(iSectorCurrent);
            continue;
            }
                                        /* No problem reading the Source and Target
                                           file, now see if it matches */
        if(iStatusFlag & ISOCOMP_STATUS_TARGET_WRITE)
            ulBytesReadTarget=ulBytesWrittenTarget;
                                        /* Check for what we have read */
        if(ulBytesReadSource!=ulBytesReadTarget)
            {
            UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_TARGETFILESIZE, "", __LINE__);

            *puexcpError << "Unexpected file size error (" << ulBytesReadSource << ":" << ulBytesReadTarget <<")";
            if(iStatusFlag & ISOCOMP_STATUS_LOG)
                {
                fstreamLogFile << MSG_TARGETFILESIZE ": " "Unexpected file size error (" << ulBytesReadSource << ":" << ulBytesReadTarget <<") at Line " << (int)__LINE__ << endl;
                }
            iCountProblem++;
            throw(*puexcpError);
            }
                                        /* Check for successful comparison, if the comparison
                                           failed, reposition the head and retry */
        if(!(iStatusFlag & ISOCOMP_STATUS_TARGET_WRITE) &&
            memcmp(pbSourceData, pbTargetData, ulBytesReadSource))
            {
            if(ulRetryCount>=RETRY_COUNT)
                {
                UEXCP  *puexcpError=new UEXCP(APPLICATION_PREFIX, ERR_COMPARISONFAILED, "", __LINE__);
    
                *puexcpError << "Contents of Source and taget differ";
                if(iStatusFlag & ISOCOMP_STATUS_LOG)
                    {
                    fstreamLogFile << MSG_COMPARISONFAILED ": Contents of Source and Target differ at Line " << (int)__LINE__ << endl;
                    }
                iCountProblem++;
                throw(*puexcpError);
                }
                                        /* Reposition the head by reading the first
                                           and last sector */
            readSourceFile(0);
            readSourceFile(iSourceSectorsUsed-1);
            readSourceFile(iSectorCurrent);
            readTargetFile(0);
            readTargetFile(iSourceSectorsUsed-1);
            readTargetFile(iSectorCurrent);
            ulRetryCount++;
            continue;
            }
                                        /* Reset retry counter */
        ulRetryCount=0;
                                        /* If nothing more could be read in the Source
                                           file, we're done */
        if((ulBytesReadSource==0) && 
            ((iSectorCurrent*CDROM_SECTORSIZE_ISO+ulBytesReadSource)==(iSourceSectorsUsed*CDROM_SECTORSIZE_ISO)))
            {
            cout << "\r" << "                                                                               ";
            cout << "\r";
            cout.flush();
            break;
            }
                                        /* We've successfully compared the current
                                           buffer contents, advance position in
                                           the files */
        ullSourceTotalMS+=puProfilingSource->elapsedMilliSec();
        ullTargetTotalMS+=puProfilingTarget->elapsedMilliSec();
        ullSourceTotalRead+=ulBytesReadSource;
        ullTargetTotalRead+=ulBytesReadTarget;
        iSectorCurrent+=(ulBytesReadSource/CDROM_SECTORSIZE_ISO);
                                        /* Calculate CRC for current buffer (and add to
                                           CRC of previous buffer) */
        ucrc32Source.update(pbSourceData, ulBytesReadSource);
                                        /* Update MD5 digest for current buffer (added to
                                           state from previous buffer) */
        umd5Source.update(pbSourceData, ulBytesReadSource);
        }
}

APIRET      ISOCOMP::readSourceFile(int iSector)
{
    APIRET          apiretRc;
    int             iSectorSize;
    int             iMediaBufferSize;

                                        /* If we read a file in RAW format, adjust properly */
    if(iStatusFlag & ISOCOMP_STATUS_SOURCE_RAW)
        {
        iSectorSize=CDROM_SECTORSIZE_RAW;
        iMediaBufferSize=(iCompareBufferSize/CDROM_SECTORSIZE_ISO)*CDROM_SECTORSIZE_RAW;
        }
    else
        {
        iSectorSize=CDROM_SECTORSIZE_ISO;
        iMediaBufferSize=iCompareBufferSize;
        }
                                        /* Read into the Source buffer pbSourceData
                                           iCompareBufferSize bytes, starting at
                                           sector iSector (= offset iSector*CDROM_SECTORSIZE_ISO
                                           in image) */
    if(iSector==-1)
        {
                                        /* If no sector is specified, read the buffer
                                           from the current file position (including advancing
                                           the file position) and profile that */
        puProfilingSource->start();
#ifdef  __OS2__
        apiretRc=DosRead(hfileSourceFile, pbSourceData, iMediaBufferSize, &ulBytesReadSource);
#endif  /* __OS2__ */
#ifdef  __WIN32__
        SetLastError(NO_ERROR);
        apiretRc=ReadFile(hfileSourceFile, pbSourceData, iMediaBufferSize, &ulBytesReadSource, NULL);
        apiretRc=GetLastError();
#endif  /* __WIN32__ */
        puProfilingSource->stop();
                                        /* If we read in RAW format, adjust it properly so as
                                           if we had read in ISO format */
        if(iStatusFlag & ISOCOMP_STATUS_SOURCE_RAW)
            ulBytesReadSource=convertRAW2ISO(pbSourceData, iCompareBufferSectors, ulBytesReadSource);
        }
    else
        {
        ULONG       ulActual;

                                        /* If a sector is specified, read just that sector,
                                           and ensure that afterwards we repositiion again
                                           to the beginning of that sector */
        if(iStatusFlag & ISOCOMP_STATUS_SOURCE_RSJ)
#ifdef  __OS2__
            apiretRc=DosSetFilePtr(hfileSourceFile, RSJ_HEADERSIZE+iSector*iSectorSize, FILE_BEGIN, &ulActual);
        else
            apiretRc=DosSetFilePtr(hfileSourceFile, iSector*iSectorSize, FILE_BEGIN, &ulActual);
        apiretRc=DosRead(hfileSourceFile, pbSourceData, iSectorSize, &ulBytesReadSource);
#endif  /* __OS2__ */
#ifdef  __WIN32__
            {
            SetLastError(NO_ERROR);
            ulActual=SetFilePointer(hfileSourceFile, RSJ_HEADERSIZE+iSector*iSectorSize, NULL, FILE_BEGIN);
            apiretRc=GetLastError();
            }
        else
            {
            SetLastError(NO_ERROR);
            ulActual=SetFilePointer(hfileSourceFile, iSector*iSectorSize, NULL, FILE_BEGIN);
            apiretRc=GetLastError();
            }
        SetLastError(NO_ERROR);
        apiretRc=ReadFile(hfileSourceFile, pbSourceData, iSectorSize, &ulBytesReadSource, NULL);
        apiretRc=GetLastError();
#endif  /* __WIN32__ */
                                        /* Now set the file position to that sector again */
        if(iStatusFlag & ISOCOMP_STATUS_SOURCE_RSJ)
#ifdef  __OS2__
            apiretRc=DosSetFilePtr(hfileSourceFile, RSJ_HEADERSIZE+iSector*iSectorSize, FILE_BEGIN, &ulActual);
        else
            apiretRc=DosSetFilePtr(hfileSourceFile, iSector*iSectorSize, FILE_BEGIN, &ulActual);
#endif  /* __OS2__ */
#ifdef  __WIN32__
            {
            SetLastError(NO_ERROR);
            ulActual=SetFilePointer(hfileSourceFile, RSJ_HEADERSIZE+iSector*iSectorSize, NULL, FILE_BEGIN);
            apiretRc=GetLastError();
            }
        else
            {
            SetLastError(NO_ERROR);
            ulActual=SetFilePointer(hfileSourceFile, iSector*iSectorSize, NULL, FILE_BEGIN);
            apiretRc=GetLastError();
            }
#endif  /* __WIN32__ */
                                        /* If we read in RAW format, adjust it properly so as
                                           if we had read in ISO format */
        if(iStatusFlag & ISOCOMP_STATUS_SOURCE_RAW)
            ulBytesReadSource=convertRAW2ISO(pbSourceData, 1, ulBytesReadSource);
        }
    return(apiretRc);
}

APIRET      ISOCOMP::readTargetFile(int iSector)
{
    APIRET          apiretRc;
    int             iSectorSize;
    int             iMediaBufferSize;

                                        /* If we read a file in RAW format, adjust properly */
    if(iStatusFlag & ISOCOMP_STATUS_TARGET_RAW)
        {
        iSectorSize=CDROM_SECTORSIZE_RAW;
        iMediaBufferSize=(iCompareBufferSize/CDROM_SECTORSIZE_ISO)*CDROM_SECTORSIZE_RAW;
        }
    else
        {
        iSectorSize=CDROM_SECTORSIZE_ISO;
        iMediaBufferSize=iCompareBufferSize;
        }
                                        /* Read the buffer from the current file position 
                                           and profile that */
    if(iSector==-1)
        {
                                        /* If no sector is specified, read the buffer
                                           from the current file position (including advancing
                                           the file position) and profile that */
        puProfilingTarget->start();
#ifdef  __OS2__
        apiretRc=DosRead(hfileTargetFile, pbTargetData, iMediaBufferSize, &ulBytesReadTarget);
#endif  /* __OS2__ */
#ifdef  __WIN32__
        SetLastError(NO_ERROR);
        apiretRc=ReadFile(hfileTargetFile, pbTargetData, iMediaBufferSize, &ulBytesReadTarget, NULL);
        apiretRc=GetLastError();
#endif  /* __WIN32__ */
        puProfilingTarget->stop();
                                        /* If we read in RAW format, adjust it properly so as
                                           if we had read in ISO format */
        if(iStatusFlag & ISOCOMP_STATUS_TARGET_RAW)
            ulBytesReadTarget=convertRAW2ISO(pbTargetData, iCompareBufferSectors, ulBytesReadTarget);
        }
    else
        {
        ULONG       ulActual;

                                        /* If a sector is specified, read just that sector,
                                           and ensure that afterwards we repostition again
                                           to the beginning of that sector */
        if(iStatusFlag & ISOCOMP_STATUS_TARGET_RSJ)
#ifdef  __OS2__
            apiretRc=DosSetFilePtr(hfileTargetFile, RSJ_HEADERSIZE+iSector*iSectorSize, FILE_BEGIN, &ulActual);
        else
            apiretRc=DosSetFilePtr(hfileTargetFile, iSector*iSectorSize, FILE_BEGIN, &ulActual);
        apiretRc=DosRead(hfileTargetFile, pbTargetData, iSectorSize, &ulBytesReadTarget);
#endif  /* __OS2__ */
#ifdef  __WIN32__
            {
            SetLastError(NO_ERROR);
            ulActual=SetFilePointer(hfileTargetFile, RSJ_HEADERSIZE+iSector*iSectorSize, NULL, FILE_BEGIN);
            apiretRc=GetLastError();
            }
        else
            {
            SetLastError(NO_ERROR);
            ulActual=SetFilePointer(hfileTargetFile, iSector*iSectorSize, NULL, FILE_BEGIN);
            apiretRc=GetLastError();
            }
        SetLastError(NO_ERROR);
        apiretRc=ReadFile(hfileTargetFile, pbTargetData, iSectorSize, &ulBytesReadTarget, NULL);
        apiretRc=GetLastError();
#endif  /* __WIN32__ */
                                        /* Now set the file position to that sector again */
        if(iStatusFlag & ISOCOMP_STATUS_TARGET_RSJ)
#ifdef  __OS2__
            apiretRc=DosSetFilePtr(hfileTargetFile, RSJ_HEADERSIZE+iSector*iSectorSize, FILE_BEGIN, &ulActual);
        else
            apiretRc=DosSetFilePtr(hfileTargetFile, iSector*iSectorSize, FILE_BEGIN, &ulActual);
#endif  /* __OS2__ */
#ifdef  __WIN32__
            {
            SetLastError(NO_ERROR);
            ulActual=SetFilePointer(hfileTargetFile, RSJ_HEADERSIZE+iSector*iSectorSize, NULL, FILE_BEGIN);
            apiretRc=GetLastError();
            }
        else
            {
            SetLastError(NO_ERROR);
            ulActual=SetFilePointer(hfileTargetFile, iSector*iSectorSize, NULL, FILE_BEGIN);
            apiretRc=GetLastError();
            }
#endif  /* __WIN32__ */
                                        /* If we read in RAW format, adjust it properly so as
                                           if we had read in ISO format */
        if(iStatusFlag & ISOCOMP_STATUS_TARGET_RAW)
            ulBytesReadTarget=convertRAW2ISO(pbTargetData, 1, ulBytesReadTarget);
        }
    return(apiretRc);
}

APIRET      ISOCOMP::writeTargetFile(void)
{
    APIRET          apiretRc;

                                        /* Write at the current file position from the 
                                           buffer */
    puProfilingTarget->start();
#ifdef  __OS2__
    apiretRc=DosWrite(hfileTargetFile, pbSourceData, ulBytesReadSource, &ulBytesWrittenTarget);
#endif  /* __OS2__ */
#ifdef  __WIN32__
    SetLastError(NO_ERROR);
    WriteFile(hfileTargetFile, pbSourceData, ulBytesReadSource, &ulBytesWrittenTarget, NULL);
    apiretRc=GetLastError();
#endif  /* __WIN32__ */
    puProfilingTarget->stop();
    return(apiretRc);
}

APIRET      ISOCOMP::getKey(int iGetKey, int iDoBeep)
{
    APIRET          apiretRc=NO_ERROR;
    int             iKey;

    if(iGetKey==TRUE)
        {
        cout << "            ==> Press Enter to continue ";
        cout.flush();
        }
    if((!(iStatusFlag & ISOCOMP_STATUS_NOBEEP)) && (iDoBeep==TRUE))
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

ULONG       ISOCOMP::convertRAW2ISO(BYTE *pbData, int iSectors, ULONG ulBytesReadRAW)
{
    int             iSector;

                                        /* Convert a buffer in RAW format to ISO format by
                                           removing the excessive data */
    for(iSector=0; iSector<iCompareBufferSectors; iSector++)
        {
        memcpy(&pbData[iSector*CDROM_SECTORSIZE_ISO+0],
            &pbData[iSector*CDROM_SECTORSIZE_RAW+16],
            CDROM_SECTORSIZE_ISO); 
        }        
    return((ulBytesReadRAW/CDROM_SECTORSIZE_RAW)*CDROM_SECTORSIZE_ISO);
}

void        ISOCOMP::processThread(void)
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
        } while((apiretRc!=NO_ERROR) && !(iStatusFlag & ISOCOMP_STATUS_SHUTDOWN));
                                        /* Keep thread running as long as ISOCOMP is
                                           running */
    while(!(iStatusFlag & ISOCOMP_STATUS_SHUTDOWN))
        {
#ifdef  __WIN32__
        SetLastError(NO_ERROR);
#endif  /* __WIN32__ */
                                        /* Wait until the semaphore is posted or 1
                                           timeout. In the first case we can read the
                                           Target file, in the second we just continue
                                           the loop */
        do  {
            apiretRc=REQUESTMUTEXSEM(hmtxThread1, MUTEX_TIMEOUT);
            } while((apiretRc!=NO_ERROR) && !(iStatusFlag & ISOCOMP_STATUS_SHUTDOWN));
        if(apiretRc==NO_ERROR)
            {
#ifdef  DEBUG
            WRITESTDIO("B1+2+3- ");
            FLUSHSTDIO();
#endif  /* DEBUG */
                                        /* If semaphore was posted, the Source file is
                                           just going to be read and we are going to read
                                           the Target file. Once we finish, release the
                                           semaphore so that the Source and Target buffers
                                           can be compared */
            if(iStatusFlag & ISOCOMP_STATUS_TARGET_WRITE)
                apiretRcTarget=writeTargetFile();
            else
                apiretRcTarget=readTargetFile();
            apiretRc=RELEASEMUTEXSEM(hmtxThread2);
            do  {
                apiretRc=REQUESTMUTEXSEM(hmtxThread3, MUTEX_TIMEOUT);
#ifdef  __WIN32__
                if((apiretRc!=NO_ERROR) && (apiretRc!=WAIT_TIMEOUT))
                     apiretRc=GetLastError();
#endif  /* __WIN32__ */
                } while((apiretRc!=NO_ERROR) && !(iStatusFlag & ISOCOMP_STATUS_SHUTDOWN));
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
                } while((apiretRc!=NO_ERROR) && !(iStatusFlag & ISOCOMP_STATUS_SHUTDOWN));
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

int     main(int argc, char *argv[])
{
    int             iReturnCode=0;
    ISOCOMP        *pisocompInstance=0;

    try
        {
        pisocompInstance=new ISOCOMP(argc, argv);
        pisocompInstance->initialize();
        try
            {
            pisocompInstance->process();
            }
        catch(UEXCP& ruexcpError)
            {
            if(pisocompInstance->getProblemCount()==0)
                pisocompInstance->usage();
            ruexcpError.print();
            BEEP(800, 200);
            iReturnCode=ruexcpError.getError();
            pisocompInstance->setProblemCount();
            }
                                        /* If no fatal exception happened, set return code
                                           depending on criticalness of recoverable errors */
        }
    catch(UEXCP& ruexcpError)
        {
        if(pisocompInstance!=0)
            {
            pisocompInstance->setProblemCount();
            pisocompInstance->usage();
            }
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
    if(pisocompInstance!=0)
        delete pisocompInstance;
#ifdef  __DEBUG_ALLOC__
    _dump_allocated(32);                /* Display memory leaks */
#endif  /* __DEBUG_ALLOC__ */
    return(iReturnCode);
}

void        _Optlink isocompThread(ISOCOMP *pISOCOMP)
{
    pISOCOMP->processThread();
    _endthread();
}

