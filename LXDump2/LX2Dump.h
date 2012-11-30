#ifndef __LX2DUMP_H__
#define __LX2DUMP_H__

/***********************************************************************\
 *                              LX2Dump.c                              *
 *                 Copyright (C) by Stangl Roman, 1999                 *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
 * LX2Dump.h    Linear eXecutable Dump Facility/2 header file          *
 *                                                                     *
\***********************************************************************/

#pragma strings(readonly)

                                        /* BLDLEVEL information (in C source modules added via macro
                                           concatenation) for BuildLevel.cmd to generate BLDLEVEL information.
                                           Version and Signature should correspond (at least for a GA build) */
#define BLDLEVEL_VENDOR         "(C) Roman_Stangl@at.ibm.com"
#define BLDLEVEL_VERSION        "1.03 (02,2000)"
#define BLDLEVEL_SIGNATURE      0x00010003
#define BLDLEVEL_RELEASE        ((((BLDLEVEL_SIGNATURE & 0xFFFF0000)>>16)*100)+(BLDLEVEL_SIGNATURE & 0x000000FF))
#define BLDLEVEL_INFO           "LX/2 - Linear eXecutable Dump Facility/2"
#define BLDLEVEL_PRODUCT        "LX/2"

typedef unsigned long   DWORD;

#include    <os2.h>
#include    <newexe.h>
#include    <exe.h>
#define     SMP
#define     FOR_EXEHDR  1
#include    <exe386.h>

#include    <malloc.h>
#include    <memory.h>
#include    <stdio.h>
#include    <string.h>

#pragma pack(1)
                                        /* Structure for Resident and Non-Resident Name
                                           Table entries */
struct  res_nonres_table
{
    unsigned char           bLen;       /* Length of acName or 0 */
    char                    acName[1];  /* Name of length bLen */
    unsigned short          usOrd;      /* Ordinal at acSymbol[bLen] */
};

struct  nonres_exports
{
    unsigned char           bLen;
    char                    acExport[1];
    unsigned short          us_ord;
};

                                        /* Import Module/Procedure Name Table */
struct  impname_table
{
    unsigned char           bLen;
    char                    acName[1];
};

                                        /* Fixup Record */
struct r32_fixup
{
    unsigned char           nr_stype;   /* Source type */
    unsigned char           nr_flags;   /* Flag byte */
                                        /* Source offset/count */
    short                   r32_soff_cnt; 
    union targettype
    {
        unsigned short      int_obj;    /* Object index in Object Table */
        unsigned short      mod;        /* Module index in Import Name Table */
        unsigned short      ord;        /* Ordinal index in Module Entry Table */
    }                       r32_type;
    union targefixup
    {
                                        /* Internal fixup */
        struct intfixup
        {
            unsigned long   toff;       /* Target Offset (optional) */
        }                   intref;
                                        /* Import by ordinal */
        struct impbyord
        {
            unsigned long   ord;        /* Ordinal number */
            unsigned long   add;        /* Additive (optional) */

        }                   impord;
                                        /* Import by name */
        struct impbyname
        {
            unsigned long   proc;       /* Procedure name offset in Import Procedure Name Table */
            unsigned long   add;        /* Additive (optional) */
        }                   impname;
        struct  intentryrecord
        {
            unsigned long   add;        /* Additive (optional) */
        }                   intentry;
    }                       r32_target; 
    unsigned short          soff[1];    /* Source Offset(s) (optional) */
};

int GetExportFromOrdinal(struct b32_bundle **ppB32Bundle, struct e32_entry **ppE32Entry, 
    struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr, int iOrdinalSearch);
int DumpHeader(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr);
int DumpObjectTable(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr, ULONG bDumpPages);
int DumpResourceTable(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr);
int DumpImportModulesTable(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr);
int DumpImportProceduresTable(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr);
int DumpResidentNameTable(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr);
int DumpNonResidentNameTable(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr);
int DumpEntryTable(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr);
int DumpFixupPageTable(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr);
int DumpFixupRecordTable(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr);

#endif  /* __LX2DUMP_H__ */
