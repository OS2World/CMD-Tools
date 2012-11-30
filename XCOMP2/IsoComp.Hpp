#ifndef _ISOCOMP_HPP_
#define _ISOCOMP_HPP_

/***********************************************************************\
 *                              IsoComp/2                              *
 *              Copyright (C) by Stangl Roman, 2001, 2002              *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
 * IsoComp.cpp  IsoComp/2 main header.                                 *
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
#include    "UCRC32.hpp"
#include    "UMD5.hpp"

                                        /* String definitions */
#define     APPLICATION_PREFIX          "ISOCOMP"
#define     APPLICATION_MESSAGE_PREFIX  "            "
#ifdef  __OS2__
#define     COPYRIGHT_1                 "ISOCOMP/2 - The ISO image file compare utility for OS/2, V1.20\n"
#endif  /* __OS2__ */
#ifdef  __WIN32__
#define     COPYRIGHT_1                 "ISOCOMP/2 - The ISO image file compare utility for OS/2 (WIN32 port), V1.20\n"
#endif  /* __WIN32__ */
#define     COPYRIGHT_2                 "            (C) Roman Stangl 05, 2002 (Roman_Stangl@at.ibm.com)"
#define     COPYRIGHT_3                 "            http://www.geocities.com/SiliconValley/Pines/7885/\n"

                                        /* Message numbers (100 will get added by Exception handler) */
#define     ERR_PIB                               0
#define     ERR_ARGUMENTS                         1
#define     ERR_SOURCEPATH                        2
#define     ERR_TARGETPATH                        3
#define     ERR_LOGFILEOPEN                       4
#define     ERR_CHKSUMFILEOPEN                    5
#define     ERR_SOURCEDOSQUERYFSATTACH            6
#define     ERR_SOURCEFILESYSTEM                  7
#define     ERR_SOURCEDOSQUERYFSINFO              8
#define     ERR_SOURCESECTORSIZE                  9
#define     ERR_SOURCEDOSOPEN                    10
#define     ERR_TARGETDOSQUERYFSATTACH           11
#define     ERR_TARGETFILESYSTEM                 12
#define     ERR_TARGETDOSQUERYFSINFO             13
#define     ERR_TARGETSECTORSIZE                 14
#define     ERR_TARGETDOSOPEN                    15
#define     ERR_SOURCEDRIVE                      16
#define     ERR_SOURCEUNCPATH                    17
#define     ERR_SOURCECURRENTDISK                18
#define     ERR_SOURCECURRENTDIR                 19
#define     ERR_SOURCECURRENTDIR1                20
#define     ERR_SOURCECURRENTDIR2                21
#define     ERR_SOURCEFILENAME                   22
#define     ERR_SOURCERSJDOSREAD                 23
#define     ERR_TARGETDRIVE                      24
#define     ERR_TARGETUNCPATH                    25
#define     ERR_TARGETCURRENTDISK                26
#define     ERR_TARGETCURRENTDIR                 27
#define     ERR_TARGETCURRENTDIR1                28
#define     ERR_TARGETCURRENTDIR2                29
#define     ERR_TARGETFILENAME                   30
#define     ERR_TARGETRSJDOSREAD                 31
#define     ERR_TARGETRSJDOSWRITE                32

#define     ERR_FILESIZEDIFFERS                  33
#define     ERR_SOURCEDOSFILEPTR                 34
#define     ERR_TARGETDOSFILEPTR                 35
#define     ERR_TARGETFILEPOSITION               36 
#define     ERR_SOURCEDOSREAD                    37
#define     ERR_TARGETDOSREAD                    38
#define     ERR_TARGETFILESIZE                   39 
#define     ERR_COMPARISONFAILED                 40 
#define     ERR_CHKSUMMISMATCH                   41

#define     MSG_FILESIZEDIFFERS         "ISOCOMP133"
#define     MSG_SOURCEDOSFILEPTR        "ISOCOMP234"
#define     MSG_TARGETDOSFILEPTR        "ISOCOMP335"
#define     MSG_TARGETFILEPOSITION      "ISOCOMP436"
#define     MSG_SOURCEDOSREAD           "ISOCOMP537"
#define     MSG_TARGETDOSREAD           "ISOCOMP638"
#define     MSG_TARGETFILESIZE          "ISOCOMP739"
#define     MSG_COMPARISONFAILED        "ISOCOMP840"
#define     MSG_CHKSUMMISMATCH          "ISOCOMP941"

                                        /* Messages handled without Exception handler */
#define     MSG_SUCCESS                 "ISOCOMP001"
#define     MSG_THROUGHPUT              "ISOCOMP002"
#define     MSG_CRC                     "ISOCOMP003"
#define     MSG_MD5                     "ISOCOMP004"

#define     ISOCOMPDATASIZE             (1<<17)
#define     RETRY_COUNT                 3

                                        /* Multithreaded flag */
#define     ISOCOMP_STATUS_MP           0x00000001
                                        /* Logfile active flag */
#define     ISOCOMP_STATUS_LOG          0x00000002
                                        /* No beep flag */
#define     ISOCOMP_STATUS_NOBEEP       0x00000004
                                        /* Use small buffer flag */
#define     ISOCOMP_STATUS_SMALLBUFFER  0x00000008
                                        /* Checksum file active flag */
#define     ISOCOMP_STATUS_CHKSUM       0x00000010
                                        /* Checksum file will be written (instead of read) */
#define     ISOCOMP_STATUS_CHKSUM_LOG   0x00000020

                                        /* Pause on exit flag */
#define     ISOCOMP_STATUS_PAUSE_ONEXIT 0x00000100

                                        /* Only Source file available */
#define     ISOCOMP_STATUS_SOURCE_ONLY  0x00010000
                                        /* Source is ISO image file (instead of CD-ROM) flag */
#define     ISOCOMP_STATUS_SOURCE_FILE  0x00020000
                                        /* Source is RSJ track image file flag */
#define     ISOCOMP_STATUS_SOURCE_RSJ   0x00040000
                                        /* Source is in RAW (2352 instead of 2048 
                                           bytes/sector) format */
#define     ISOCOMP_STATUS_SOURCE_RAW   0x00080000
                                        /* Target is CD-ROM (instead of image file) flag */
#define     ISOCOMP_STATUS_TARGET_CDROM 0x00100000
                                        /* Write ISO image file flag */
#define     ISOCOMP_STATUS_TARGET_WRITE 0x00200000
                                        /* Target is RSJ track image file flag */
#define     ISOCOMP_STATUS_TARGET_RSJ   0x00400000
                                        /* TARGET is in RAW (2352 instead of 2048 
                                           bytes/sector) format */
#define     ISOCOMP_STATUS_TARGET_RAW   0x00800000

                                        /* CRC32 has been calculated successfully */
#define     ISOCOMP_STATUS_CRC32_DONE   0x01000000
                                        /* MD5 digest has been calculated successfully */
#define     ISOCOMP_STATUS_MD5_DONE     0x02000000

                                        /* IsoComp/2 shutdown status flag */
#define     ISOCOMP_STATUS_SHUTDOWN     0x10000000
                                        /* IsoComp/2 shutdown without cleanup */
#define     ISOCOMP_STATUS_ABORT        0x20000000

class       ISOCOMP
{
friend void    _Optlink isocompThread(ISOCOMP *pISOCOMP);
public:
            ISOCOMP(int argc, char *argv[]);
           ~ISOCOMP(void);
    void    initialize(void);
    void    process(void);
    void    usage(void);
    int     setProblemCount();
    int     getProblemCount();
private:
    void    verifySourceDrive(void);
    void    verifySourceFile(void);
    void    verifyTargetDrive(void);
    void    verifyTargetFile(void);
    void    readImage(void);
    void    compareImage(void);
    APIRET  readSourceFile(int iSector=-1);
    APIRET  readTargetFile(int iSector=-1);
    APIRET  writeTargetFile(void);
    APIRET  getKey(int iGetKey=TRUE, int iDoBeep=TRUE);
    ULONG   convertRAW2ISO(BYTE *pbData, int iSectors, ULONG ulBytesReadRAW);
    void    processThread(void);

                                        /* Commandline arguments */
    int     argc;
    char  **argv;
                                        /* File definitions */
    char    acExecutableFile[CCHMAXPATH];
    char    acLogFile[CCHMAXPATH];
    char    acChkSumFile[CCHMAXPATH];
                                        /* Size of pbSource/TargetData buffers in bytes, normalized
                                           to ISO sector sizes (2048 Bytes/Sector) */
    int     iCompareBufferSize;         
                                        /* Size of pbSource/TargetData buffers in ISO sectors */
    int     iCompareBufferSectors;      
    char    acSourcePath[CCHMAXPATH];
    char    acSourceFile[CCHMAXPATH];
    char    acTargetPath[CCHMAXPATH];
    char    acTargetFile[CCHMAXPATH];
    int     iSourcePathRootLen;
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
    char    acSourceFilesystem[9];      /* Source CD-ROM media filesystem (only CDFS supported) */
    char    acTargetFilesystem[9];      /* Target CD-ROM media filesystem (only CDFS supported) */
    int     iSourceSectorsUsed;         /* Number of sectors on CD-ROM source media */
    int     iTargetSectorsUsed;         /* Number of sectors on CD-ROM target media */
                                        /* A CD-ROM sector in ISO format (mode1/2048) consists of:
                                           <2048 bytes data> */
#define     CDROM_SECTORSIZE_ISO        2048
#define     CDROM_ISO_OFFSET_DATA       0
                                        /* A CD-ROM sector in ISO format (mode1/2352) consists of:
                                           <12 bytes sync><4 bytes header><2048 bytes data><288 bytes ECC> */
#define     CDROM_SECTORSIZE_RAW        2352
#define     CDROM_RAW_OFFSET_SYNC       0
#define     CDROM_RAW_OFFSET_HEADER     12
#define     CDROM_RAW_OFFSET_DATA       16
#define     CDROM_RAW_OFFSET_ECC        2064
                                        /* CD-ROM media sector size (only 2048 Bytes/Sector is supported, 
                                           except when reading from a file in RAW format, but that
                                           also gets rearranged to look like ISO sectors) */
    int     iSourceSectorSize;          
                                        /* File data normalized to ISO format (that is, if a file
                                           in RAW data is read, data is rearranged to look like
                                           as if ISO data was read) */
    ULONG   ulFileposSource;            
    ULONG   ulFileposTarget;
    ULONG   ulBytesReadSource;
    ULONG   ulBytesReadTarget;
    ULONG   ulBytesWrittenTarget;
    ULONG   ulSectorsRead;
                                        /* Buffers (allocated for RAW sectors, but after reading
                                           buffers get rearranged to contain ISO sectors) */
    BYTE   *pbSourceData;
    BYTE   *pbTargetData;
                                        /* Problem counters */
    int     iCountProblem;
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
                                        /* Checksums for CRC32 and MD5 calculated from Source */
    UCRC32  ucrc32Source;
    UMD5    umd5Source;
    UCHAR   aucCrc32[UCRC32_CRC_LENGTH_STRG];
    UCHAR   aucMd5Digest[UMD5_DIGEST_LENGTH_STRG];
                                        /* ISOCOMP_STATUS_* flags */
    int     iStatusFlag;
};

                                        /* RAW (2352 Bytes/Sector) images are also assumed
                                           when specifying their "usual" extensions */
#define     RAW_EXTENSION1              ".RAW"
#define     RAW_EXTENSION2              ".BIN"

                                        /* As far as I could reverse engineer, this is
                                           how an RSJ track image looks like, that
                                           prefixes a normal ISO images */
#define     RSJ_EXTENSION1              ".TRK"
#define     RSJ_EXTENSION2              ".RSJ"

struct      RSJ_HEADER
{
#define     RSJ_SIGNATURE               "RSJ CDWFS TRACK FILE"
    char    acSignature[20];
    ULONG   ulReserved1;
#define     RSJ_ICON                    0x00000001
    ULONG   ulIcon;
#define     RSJ_FUNCTION                0x00000800
    ULONG   ulFunction;
    ULONG   ulSectors;
#define     RSJ_HEADERSIZE              0x00000028
    ULONG   ulHeaderSize;
};

                                        /* Line written to and read from checksum file */
struct      CHKSUM
{
    UCHAR   aucTextCRC32[7];
    UCHAR   aucCrc32[UCRC32_CRC_LENGTH_STRG-1];
    UCHAR   aucTextMD5[6];
    UCHAR   aucMd5Digest[UMD5_DIGEST_LENGTH_STRG-1];
    UCHAR   aucTextPath[7];
    UCHAR   uaPath[CCHMAXPATH];
};

extern "C"
{
    void    _Optlink isocompThread(ISOCOMP *pISOCOMP);
}


#endif  /* _ISOCOMP_HPP_ */

