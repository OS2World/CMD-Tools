#ifndef _XCOMP_HPP_
#define _XCOMP_HPP_

/***********************************************************************\
 *                               XComp/2                               *
 *              Copyright (C) by Stangl Roman, 1999, 2002              *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
 * XComp.hpp    XComp/2 main header.                                   *
 *                                                                     *
\***********************************************************************/

                                        /* Requires compiler with 64-bit integer support */
typedef     long unsigned long          ULLONG, *PULLONG;

                                        /* Common definitions */
#define     EXTENDED_KEY                0xE0
#define     MUTEX_TIMEOUT               1000

                                        /* OS/2 specifics */
#ifdef  __OS2__
#define     INCL_DOSFILEMGR
#define     INCL_DOSERRORS
#define     INCL_DOSMISC
#define     INCL_DOSPROCESS
#define     INCL_DOSSEMAPHORES
#define     INCL_DOSMODULEMGR
#include    <os2.h>

#define     CLOSEFILE(handle)           DosClose(handle)
#define     WRITESTDIO(text)            { \
                                        ULONG   ulTemp; \
                                        DosWrite(1, text, sizeof(text)-1, &ulTemp); \
                                        }
#define     FLUSHSTDIO()                DosResetBuffer(1)
#define     REQUESTMUTEXSEM(handle, timeout) \
                                        DosRequestMutexSem(handle, timeout)
#define     RELEASEMUTEXSEM(handle)     DosReleaseMutexSem(handle)
#define     FINDCLOSE(handle)           DosFindClose(handle)
#define     BEEP(frequency, duration)   DosBeep(frequency, duration)
#define     SLEEP(duration)             DosSleep(duration)
#endif  /* __OS2__ */

                                        /* WIN32 specifics */
#ifdef  __WIN32__
#include    <windows.h>

typedef     unsigned long               APIRET;
typedef     HANDLE                      HDIR;
typedef     HANDLE                      HMTX;
#define     CCHMAXPATH                  MAX_PATH
#define     CLOSEFILE(handle)           CloseHandle(handle)
#define     WRITESTDIO(text)            { \
                                        ULONG   ulTemp; \
                                        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, sizeof(text)-1, &ulTemp, NULL); \
                                        }
#define     FLUSHSTDIO()                FlushFileBuffers(GetStdHandle(STD_OUTPUT_HANDLE))
#define     REQUESTMUTEXSEM(handle, timeout) \
                                        WaitForMultipleObjects(1, &handle, TRUE, timeout)
#define     RELEASEMUTEXSEM(handle)     ReleaseMutex(handle)
#define     FINDCLOSE(handle)           FindClose(handle)
#define     BEEP(frequency, duration)   Beep(frequency, duration)
#define     SLEEP(duration)             Sleep(duration)
#endif  /* __WIN32__ */

#include    <stream.h>
#include    <string.h>
#include    <memory.h>
#include    <time.h>
#include    <stdlib.h>
#include    <conio.h>

#include    "UExcp.hpp"
#include    "UProfiling.hpp"
#include    "UCrc32.hpp"
#include    "UMd5.hpp"

                                        /* String definitions */
#define     APPLICATION_PREFIX          "XCOMP"
#define     APPLICATION_MESSAGE_PREFIX  "          "
#ifdef  __OS2__
#define     COPYRIGHT_1                 "XCOMP/2 - The recursive file compare utility for OS/2, V3.10\n"
#endif  /* __OS2__ */
#ifdef  __WIN32__
#define     COPYRIGHT_1                 "XCOMP/2 - The recursive file compare utility for OS/2 (WIN32 port), V3.10\n"
#endif  /* __WIN32__ */
#define     COPYRIGHT_2                 "          (C) Roman Stangl 05, 2002 (Roman_Stangl@at.ibm.com)"
#define     COPYRIGHT_3                 "          http://www.geocities.com/SiliconValley/Pines/7885/\n"

                                        /* Message numbers */
#define     ERR_PIB                            0
#define     ERR_ARGUMENTS                      1
#define     ERR_SOURCEPATH                     2
#define     ERR_TARGETPATH                     3
#define     ERR_LOGFILEOPEN                    4
#define     ERR_CHKSUMFILEOPEN                 5 
#define     ERR_DOSQUERYSOURCEPATH             6 
#define     ERR_UNCSOURCEPATH                  7 
#define     ERR_CURRENTDISK                    8 
#define     ERR_SOURCEDRIVE                    9 
#define     ERR_CURRENTDIR                     10
#define     ERR_CURRENTDIR1                    11
#define     ERR_CURRENTDIR2                    12
#define     ERR_UNCTARGETPATH                  13
#define     ERR_UNCTARGETPATH1                 14
#define     ERR_TARGETDRIVE                    15
#define     ERR_CURRENTDIR4                    16
#define     ERR_TARGETPATHINVALID              17
#define     MSG_DOSFINDFIRSTSOURCE      "XCOMP118"
#define     ERR_DOSFINDFIRSTSOURCE             18
#define     MSG_DOSFINDNEXTSOURCE       "XCOMP119"
#define     ERR_DOSFINDNEXTSOURCE              19
#define     ERR_DOSFINDCLOSESOURCE             20
#define     MSG_DOSFINDFIRSTDIR         "XCOMP121"
#define     ERR_DOSFINDFIRSTDIR                21
#define     ERR_DOSFINDMEXTSOURCE              22
#define     MSG_DOSFINDNEXTDIR          "XCOMP123"
#define     ERR_DOSFINDNEXTDIR                 23
#define     ERR_DOSFINDCLOSETARGET             24
#define     MSG_DOSOPENSOURCE           "XCOMP125"
#define     MSG_DOSOPENTARGET           "XCOMP126"
#define     MSG_DOSFILEINFOSOURCE       "XCOMP127"
#define     MSG_DOSFILEINFOTARGET       "XCOMP128"
#define     MSG_FILESIZEDIFFERS         "XCOMP129"
#define     ERR_DOSFILEPTRSOURCE               30
#define     ERR_DOSFILEPTRTARGET               31
#define     MSG_INTERNALERROR           "XCOMP132"
#define     MSG_READSOURCE              "XCOMP133"
#define     MSG_READTARGET              "XCOMP134"
#define     MSG_RETRYCOUNT              "XCOMP135"
#define     MSG_COMPARESTATS            "XCOMP136"
#define     MSG_RETRYEXCEEDED           "XCOMP137"
#define     MSG_DIFFERENCE              "XCOMP138"
#define     MSG_DOSCLOSESOURCE          "XCOMP139"
#define     MSG_DOSCLOSETARGET          "XCOMP140"
#define     MSG_CHKSUMFILENAME          "XCOMP141"
#define     ERR_CHKSUMFILENAME                 41
#define     MSG_CHKSUMMISMATCH          "XCOMP142"
#define     ERR_CHKSUMMISMATCH                 42
#define     MSG_SKIPCHECKSUMFILE        "XCOMP143"
#define     MSG_CHKSUM2MUCH             "XCOMP144"
#define     ERR_CHKSUM2MUCH                    44

#define     MSG_THROUGHPUT              "XCOMP001"
#define     MSG_PROBLEMOPEN             "XCOMP002"
#define     MSG_PROBLEMCOMPARE          "XCOMP003"
#define     MSG_PROBLEMFINDFILES        "XCOMP005"
#define     MSG_PROBLEMFINDDIRS         "XCOMP005"
#define     MSG_PROBLEMCHKSUM           "XCOMP006"
#define     MSG_COMPARETOTAL            "XCOMP007"
#define     MSG_SIMULATION              "XCOMP008"


struct      CHKSUM_ENTRY;               /* Forward declaration */

struct      CHKSUM_ENTRYHDR
{
                                        /* Pointer to parent subdirectories array */
    CHKSUM_ENTRY
           *pParentDir;
                                        /* Index within parent array that points to
                                           our current array */
    ULONG   ulIndex;                    
};

struct      CHKSUM_ENTRYDIR
{
                                        /* Pointer to array of subdirectories within current
                                           (sub)directory */
    CHKSUM_ENTRY
           *pSubDir;
    VOID   *pReserved;                  /* Keep all CHKSUM_ENTRY* structures the same size */
};

struct      CHKSUM_ENTRYFILE
{
    ULONG   ulCrc32;                    /* CRC32 */
    BYTE   *pbMd5Digest;                /* MD5 */
};

struct      CHKSUM_ENTRY
{
#define     CHKSUM_STATUS_ENTRYEMPTY    0x0000
#define     CHKSUM_STATUS_ENTRYHDR      0x0001
#define     CHKSUM_STATUS_ENTRYDIR      0x0002
#define     CHKSUM_STATUS_ENTRYFILE     0x0004
#define     CHKSUM_STATUS_FILEFOUND     0x8000
    USHORT  usStatus;                   /* CHKSUM_STATUS_* */
#define     CHKSUM_ENTRY_SIZEINITIAL    4
    USHORT  usSize;                     /* Array size (to the power of 2) */
    UCHAR  *pucName;                    /* Directory or File name */
                                        /* Directory of File data */
    union
    {
        CHKSUM_ENTRYHDR
            Hdr;
        CHKSUM_ENTRYDIR
            Dir;
        CHKSUM_ENTRYFILE
            File;
    };
};

#define     XCOMPDATASIZE               (1<<18)
#define     RETRY_COUNT                 3

                                        /* Verbose flags */
#define     XCOMP_STATUS_VERBOSE        0x00000001
#define     XCOMP_STATUS_VERBOSE2       0x00000002
#define     XCOMP_STATUS_LINENUMBER     0x00000004
#define     XCOMP_STATUS_SIMULATE       0x00000008
                                        /* Multithreaded flag */
#define     XCOMP_STATUS_MP             0x00000010
                                        /* Logfile flag */
#define     XCOMP_STATUS_LOG            0x00000020
                                        /* Checksum file active flag */
#define     XCOMP_STATUS_CHKSUM         0x00000040
                                        /* Checksum file will be written (instead of read) */
#define     XCOMP_STATUS_CHKSUM_LOG     0x00000080

                                        /* No pause flag (for miscompares and
                                           file not found errors) */
#define     XCOMP_STATUS_NOPAUSEERRCOMP 0x00000100
#define     XCOMP_STATUS_NOPAUSEERRFIND 0x00000200
                                        /* Pause on exit flag */
#define     XCOMP_STATUS_PAUSE_ONEXIT   0x00000400
                                        /* No recursion flag */
#define     XCOMP_STATUS_NORECURSION    0x00000800

                                        /* No beep flag */
#define     XCOMP_STATUS_NOBEEP         0x00001000
                                        /* Use small buffer flag */
#define     XCOMP_STATUS_SMALLBUFFER    0x00002000
                                        /* Checksum file read */
#define     XCOMP_STATUS_CHKSUM_READ    0x00004000

                                        /* Only Source files available (used for checksum calculation) */
#define     XCOMP_STATUS_SOURCE_ONLY    0x00100000

                                        /* CRC32 has been calculated successfully for last file */
#define     XCOMP_STATUS_CRC32_DONE     0x01000000
                                        /* MD5 digest has been calculated successfully for last file */
#define     XCOMP_STATUS_MD5_DONE       0x02000000
                                        /* MD5SUM compatibility mode (used when Checksum file
                                           has extenstion .MD5) */
#define     XCOMP_STATUS_MD5_MD5SUM     0x04000000

                                        /* XComp/2 shutdown status flag */
#define     XCOMP_STATUS_SHUTDOWN       0x10000000
                                        /* XComp/2 shutdown without cleanup */
#define     XCOMP_STATUS_ABORT          0x20000000

class       XCOMP
{
friend void    _Optlink xcompThread(XCOMP *pXCOMP);
public:
            XCOMP(int argc, char *argv[]);
           ~XCOMP(void);
    void    initialize(void);
    void    process(void);
    void    usage(void);
    int     getProblemFindFilesCount();
    int     getProblemFindDirsCount();
    int     getProblemOpenCount();
    int     getProblemCompareCount();
    int     getProblemChkSumCount();
private:
    void    verifySourcePath(void);
    void    verifyTargetPath(void);
    void    searchFiles(char *pcSourcePath, char *pcSourceFiles, char *pcTargetPath);
    int     readFiles(char *pcSourcePath, char *pcSourceFile);
    int     compareFiles(char *pcSourcePath, char *pcSourceFile, char *pcTargetPath);
    APIRET  readSourceFile(void);
    APIRET  readTargetFile(void);
    APIRET  getKey(int iGetKey=TRUE, int iDoBeep=TRUE);
    void    processThread(void);
    APIRET  readChkSumFile(void);
    APIRET  addChkSumEntry(UCHAR *pucCrc32, UCHAR *pucMD5, UCHAR *pucPath);
    CHKSUM_ENTRY
           *findChkSumEntry(CHKSUM_ENTRY *pChkSumEntry, UCHAR *pucPath);
    CHKSUM_ENTRY
           *deleteChkSumEntry(CHKSUM_ENTRY *pChkSumEntry);

                                        /* Commandline arguments */
    int     argc;
    char  **argv;
                                        /* File definitions */
    char    acExecutableFile[CCHMAXPATH];
    char    acLogFile[CCHMAXPATH];
    char    acChkSumFile[CCHMAXPATH];
    int     iCompareBufferSize;
    char    acSourcePath[CCHMAXPATH];
    int     iSourcePathRootLen;
    char    acSourceFiles[CCHMAXPATH];
    char    acTargetPath[CCHMAXPATH];
    int     iTargetPathRootLen;
#ifdef  __OS2__
    HFILE   hfileSourceFile;
    HFILE   hfileTargetFile;
#endif  /* __OS2__ */
#ifdef  __WIN32__
    HANDLE  hfileSourceFile;
    HANDLE  hfileTargetFile;
#endif  /* __WIN32__ */
                                        /* I/O */
    fstream fstreamLogFile;
    fstream fstreamChkSumFile;
    ULONG   ulBytesReadSourceFile;
    ULONG   ulBytesReadTargetFile;
                                        /* Buffers */
    BYTE   *pbSourceData;
    BYTE   *pbTargetData;
                                        /* Problem counters */
    int     iCountProblemNone;
    int     iCountProblemFindFiles;     /* Find files in a directory access errors */
    int     iCountProblemFindDirs;      /* Find directories in a directory access errors */
    int     iCountProblemOpen;          /* File open errors */
    int     iCountProblemCompare;       /* File miscompare errors */
    int     iCountProblemChkSum;        /* File Checksum errors (Checksum mismatches) */
    int     iCountProblemChkSum2Much;   /* File Checksum errors (Checksum too much in file) */
                                        /* Throughput calculations */
    ULLONG  ullSourceTotalRead;
    ULLONG  ullTargetTotalRead;
    ULLONG  ullSourceTotalMS;
    ULLONG  ullTargetTotalMS;
    ULLONG  ullTotalMS;
    UProfiling
           *puProfilingSource;
    UProfiling
           *puProfilingTarget;
    UProfiling
           *puProfilingTotal;
                                        /* Return codes */
    APIRET  apiretRcSource;
    APIRET  apiretRcTarget;
                                        /* Semaphore */
    HMTX    hmtxThread1;
    HMTX    hmtxThread2;
    HMTX    hmtxThread3;
                                        /* Checksum calculation classes */
    UCRC32  ucrc32Source;
    UMD5    umd5Source;
                                        /* Checksums for CRC32 and MD5 calculated from Source,
                                           both in binary and C-string format */
    UCHAR   aucCrc32[UCRC32_CRC_LENGTH_STRG];
    ULONG   ulCrc32;
    UCHAR   aucMd5Digest[UMD5_DIGEST_LENGTH_STRG];
    BYTE    abMd5Digest[UMD5_DIGEST_LENGTH];
                                        /* Root for data read from Checksum file */
    CHKSUM_ENTRY
           *pChkSumEntryRoot;
                                        /* XCOMP_STATUS_* flags */
    int     iStatusFlag;
};

                                        /* Line written to and read from Checksum file when
                                           running in XComp/2's mode */
struct      CHKSUM
{
    UCHAR   aucTextCRC32[7];
    UCHAR   aucCrc32[UCRC32_CRC_LENGTH_STRG-1];
    UCHAR   aucTextMD5[6];
    UCHAR   aucMd5Digest[UMD5_DIGEST_LENGTH_STRG-1];
    UCHAR   aucTextPath[7];
    UCHAR   aucPath[CCHMAXPATH];
};

                                        /* Line written to and read from Checksum file when
                                           running in MD5SUM's mode */
struct      MD5SUM
{
    UCHAR   aucMd5Digest[UMD5_DIGEST_LENGTH_STRG-1];
    UCHAR   aucTextPath[2];
    UCHAR   aucPath[CCHMAXPATH];
};

extern "C"
{
    void    _Optlink xcompThread(XCOMP *pXCOMP);
}

#endif  /* _COMP_HPP_ */
