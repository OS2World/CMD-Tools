/***********************************************************************\
 *                              LX2Dump.c                              *
 *                 Copyright (C) by Stangl Roman, 1999                 *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
 * LX2Dump.c    Linear eXecutable Dump Facility/2 source code          *
 *                                                                     *
\***********************************************************************/

/********************************************************************************\
 *                                                                              *
 * Sections that begin/end with a special signature have their signature at the *
 * beginning/end of the section, other sections either have a size set in the   *
 * LX header - expressed by (...) - beside the offset of the sections beginning *
 * which is either relative to the LX header (<-) where e_lfanew must be added, *
 * of relative to the file beginning (<=) or just end where the next section    *
 * begins.                                                                      *
 *                                                                              *
 * Some sections have a fixed layout, some a layout with variable structure     *
 * sizes.                                                                       *
 *                                                                              *
 * +----------------------------------------+                                   *
 * ! MZ                                     !                                   *
 * !             DOS Header                 !                                   *
 * !                               e_lfanew !                                   *
 * +----------------------------------------+                                   *
 * ! LX                                     !                                   *
 * !              LX Header                 !                                   *
 * !                                        !  add  e_lfanew as offsets are     *
 * +----------------------------------------+  based on LX header beginning     *
 * Loader Section (Resident):                  ! (= means beginning of file)    *
 * +----------------------------------------+  !                                *
 * !                                        ! <- e32_objtab (e32_objcnt)        *
 * !             Object Table               !                                   *
 * !           (struct o32_obj)             !                                   *
 * !                                        !                                   *
 * +----------------------------------------+                                   *
 * !                                        ! <- e32_objmap (@E e32_obj.mapsize)*
 * !          Object Page Table             !                                   *
 * !           (struct o32_map)             !                                   *
 * !                                        !                                   *
 * +----------------------------------------+                                   *
 * !                                        ! <- e32_rsrctab (e32_rsrccnt)      *
 * !           [Resource Table]             !                                   *
 * !           (struct rsrc32)              !                                   *
 * +----------------------------------------+                                   *
 * !                                        ! <- e32_restab                     *
 * !         Resident Name Table            !                                   *
 * !     (struct res_nonres_table)          !                                   *
 * !         <=> bLen,ASCII,sOrd            !                                   *
 * !                                     00 !                                   *
 * +----------------------------------------+                                   *
 * !                                        ! <- e32_enttab                     *
 * !          Entry(point) Table            !    (count of entries cc is always *
 * !         (struct b32_bundle)            !    present, but may be 00)        *
 * !         (struct e32_entry)             !                                   *
 * !                                     00 !                                   *
 * +----------------------------------------+                                   *
 * !                                        ! <- e32_dirtab (e32_dircnt)        *
 * !    [Module Format Directives Table]    !                                   *
 * !                                        !                                   *
 * +----------------------------------------+                                   *
 * !                                        !                                   *
 * !       [Resident Directives Data]       !                                   *
 * !            [Verify Record]             !                                   *
 * !                                        !                                   *
 * +----------------------------------------+                                   *
 * !                                        ! <- e32_pagesum                    *
 * !           Per-Page Checksum            !                                   *
 * !         (ULONG ulChecksum[n])          !                                   *
 * !                                        !                                   *
 * +----------------------------------------+                                   *
 * Fixup Section (Optionally Resident):                     (e32_fixupsize)     *
 * +----------------------------------------+                                   *
 * !                                        ! <- e32_fpagetab                   *
 * !            Fixup Page Table            !                                   *
 * !        (ULONG ulOffsetForPage[n])      !                                   *
 * !                                        !                                   *
 * +----------------------------------------+                                   *
 * !                                        ! <- e32_frectab                    *
 * !           Fixup Record Table           !                                   *
 * !            (struct r32_rlc)            !                                   *
 * !                                        !                                   *
 * +----------------------------------------+                                   *
 * !                                        ! <- e32_impmod (e32_impmodcnt)     *
 * !        Import Module Name Table        !                                   *
 * !         (struct impname_table)         !                                   *
 * !           <=> bLen,ASCII               !                                   *
 * !                                        !                                   *
 * +----------------------------------------+                                   *
 * !                                        ! <- e32_impproc                    *
 * !      Import Procedure Name Table       !                                   *
 * !          (struct impname_table)        !                                   *
 * !           <=> bLen,ASCII               !                                   *
 * !                                        !                                   *
 * +----------------------------------------+                                   *
 * Non Resident Section:                                                        *
 * +----------------------------------------+                                   *
 * !                                        ! <= e32_datapage (e32_preload)?    *
 * !              Preload Pages             !                                   *
 * !                                        !                                   *
 * +----------------------------------------+                                   *
 * !                                        !                                   *
 * !            Demandload Pages            !                                   *
 * !                                        !                                   *
 * +----------------------------------------+                                   *
 * !                                        ! <= First Object Page Table        *
 * !       Physical Page Image Data         !    physical address               *
 * !                                        !                                   *
 * +----------------------------------------+                                   *
 * !                                        ! <= e32_nrestab (e32_cbnrestab)    *
 * !       Non Resident Name Table          !                                   *
 * !      (struct res_nonres_table)         !                                   *
 * !          <=> bLen,ASCII,sOrd           !                                   *
 * !                                     00 !                                   *
 * +----------------------------------------+                                   *
 * !                                        ! <  ?                              *
 * !     [Non Resident Directives Data]     !                                   *
 * !  ("Data" of Module Format Directives,  !                                   *
 * !    seems to be added for debugging)    !                                   *
 * +----------------------------------------+                                   *
 * ! NB04                                   ! <= e32_debuginfo (e32_debuglen)   *
 * !            [Debug Info]                !                                   *
 * !                                        !                                   *
 * +----------------------------------------+                                   *
\********************************************************************************/

#include    "LX2Dump.h"
                                        /* Buffer where module was read into */
char   *acBuffer=0;
char   *pBuffer=0;

int GetExportFromOrdinal(struct b32_bundle **ppB32Bundle, struct e32_entry **ppE32Entry, 
    struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr, int iOrdinalSearch)
{
    int                             iOrdinal;
    struct e32_entry               *pE32EntryPrevious=0;

    *ppB32Bundle=(struct b32_bundle *)&pBuffer[pE32Exe->e32_enttab+pExeHdr->e_lfanew];
    iOrdinal=1;
    while((*ppB32Bundle)->b32_cnt!=0)
        {
        int     iCount;

                                    /* The Export section is a b32_bundle, followed by
                                       1...n e32_entry structures (whose size depend
                                       on the type) */
        iCount=(*ppB32Bundle)->b32_cnt;
        pE32EntryPrevious=*ppE32Entry=(struct e32_entry *)((*ppB32Bundle)+1);
        for( ; iCount>0; iCount--, iOrdinal++)
            {
            if((*ppB32Bundle)->b32_type==EMPTY)
                {
                iOrdinal+=iCount;
                *ppE32Entry=(struct e32_entry *)(((unsigned char*)*ppB32Bundle)+sizeof(unsigned short));
                break;
                }
            if(iOrdinal==iOrdinalSearch)
                goto ExportFound;
            if((*ppB32Bundle)->b32_type==ENTRY16)
                {
                *ppE32Entry=(struct e32_entry *)(((unsigned char*)*ppE32Entry)+FIXENT16);
                }
            else if((*ppB32Bundle)->b32_type==GATE16)
                {
                *ppE32Entry=(struct e32_entry *)(((unsigned char*)*ppE32Entry)+GATEENT16);
                }
            else if((*ppB32Bundle)->b32_type==ENTRY32)
                {
                *ppE32Entry=(struct e32_entry *)(((unsigned char*)*ppE32Entry)+FIXENT32);
                }
            else if((*ppB32Bundle)->b32_type==ENTRYFWD)
                {
                *ppE32Entry=(struct e32_entry *)(((unsigned char*)*ppE32Entry)+FWDENT);
                }
            else
                {
                printf("Unknown Bundle type %xs\n", (*ppB32Bundle)->b32_type);
                }
            pE32EntryPrevious=*ppE32Entry;
            }
        *ppB32Bundle=(struct b32_bundle *)(*ppE32Entry);
        }
ExportFound:
    *ppE32Entry=pE32EntryPrevious;
    return(0);
}

int DumpHeader(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr)
{
    printf("Magic[2]    : %c%c\n", pE32Exe->e32_magic[0], pE32Exe->e32_magic[1]);
    printf("Byte order  : %d (%s endian)\n", (int)pE32Exe->e32_border,
        (pE32Exe->e32_border==E32LEBO ? "Little" : "Big"));
    printf("Word order  : %d (%s endian)\n", (int)pE32Exe->e32_worder,
        (pE32Exe->e32_worder==E32LEWO ? "Little" : "Big"));
    printf("Exe level   : %d %s\n", (int)pE32Exe->e32_level,
        (pE32Exe->e32_level==E32LEVEL ? "" : "(Incompatible)"));
    printf("CPU         : %d (", (int)pE32Exe->e32_cpu);
    if(pE32Exe->e32_cpu==E32CPU286)
        printf("286+");
    else if(pE32Exe->e32_cpu==E32CPU386)
        printf("386+");
    else if(pE32Exe->e32_cpu==E32CPU486)
        printf("486+");
    else 
        printf("???+");
    printf(")\n");
    printf("OS          : %d %s\n", (int)pE32Exe->e32_os,
        (pE32Exe->e32_os==1 ? "" : "(non OS/2)"));
    printf("Version     : %08X\n", (int)pE32Exe->e32_ver);
    printf("Module Flags: %08X\n", (int)pE32Exe->e32_mflags);
    if(pE32Exe->e32_mflags & E32NOTMPSAFE)
        printf("                           %s\n", "SMP unsafe");
    if(pE32Exe->e32_mflags & E32LIBTERM)
        printf("                           %s\n", "Per Process Termination");
    else
        printf("                           %s\n", "Global Termination");
    if((pE32Exe->e32_mflags & E32MODMASK)==E32MODVDEV)
        printf("                           %s\n", "Virtual Device Driver");
    if((pE32Exe->e32_mflags & E32MODMASK)==E32MODPDEV)
        printf("                           %s\n", "Physical Device Driver");
    if((pE32Exe->e32_mflags & E32MODMASK)==E32MODPROTDLL)
        printf("                           %s\n", "Protected Memory Library");
    if(pE32Exe->e32_mflags & E32MODDLL)
        printf("                           %s\n", "DLL");
    if(pE32Exe->e32_mflags & E32MODEXE)
        printf("                           %s\n", "EXE");
    if(pE32Exe->e32_mflags & E32DEVICE)
        printf("                           %s\n", "Device Driver");
    if(pE32Exe->e32_mflags & E32NOLOAD)
        printf("                           %s\n", "Not loadable (incremental linking?)");
    if(pE32Exe->e32_mflags & E32PMAPI)
        printf("                           %s\n", "PM");
    if(pE32Exe->e32_mflags & E32PMW)
        printf("                           %s\n", "Windowcompat");
    if(pE32Exe->e32_mflags & E32NOPMW)
        printf("                           %s\n", "Nowindowcompat");
    if(pE32Exe->e32_mflags & E32NOEXTFIX)
        printf("                           %s\n", "No External Fixups");
    if(pE32Exe->e32_mflags & E32NOINTFIX)
        printf("                           %s\n", "No Internal Fixups");
    if(pE32Exe->e32_mflags & E32SYSDLL)
        printf("                           %s\n", "System DLL (Int. Fixup discarded)");
    if(pE32Exe->e32_mflags & E32LIBINIT)
        printf("                           %s\n", "Per Process Initialization");
    else
        printf("                           %s\n", "Global Initialization");
    if((pE32Exe->e32_mflags & 0xFFFC5CC3)!=0)
        printf("                           Unknown 0x%08X\n", (pE32Exe->e32_mflags & 0xFFFC5CC3));
    printf("Intial CS:EIP            : Object %d Offset %08X\n", 
        (int)pE32Exe->e32_startobj), (int)pE32Exe->e32_eip;
    printf("Inital SS:ESP            : Object %d Offset %08X\n",
        (int)pE32Exe->e32_stackobj, (int)pE32Exe->e32_esp);
    printf("Number of mem. pages     : %d (%08X)\n", 
        (int)pE32Exe->e32_mpages, (int)pE32Exe->e32_mpages);
    printf("Mem page size            : %d bytes\n", (int)pE32Exe->e32_pagesize);
    printf("Mem page shift           : %d (Object Page Table entry left shift)\n", (int)pE32Exe->e32_pageshift);
    printf("Loader size              : %d (%08X) (Resident Tables size)\n", 
        (int)pE32Exe->e32_ldrsize, (int)pE32Exe->e32_ldrsize);
    printf("Loader checksum          : %d\n", (int)pE32Exe->e32_ldrsum);

    printf("\n-> Fixup section, see below\n");
    printf("Fixup total size         : %08X (Fixup Page, Fixup Record, Import Module\n", (int)pE32Exe->e32_fixupsize);
    printf("                           %s\n", "          Name, Import Procedure Name Tables)");
    printf("Fixup checksum           : %08X\n", (int)pE32Exe->e32_fixupsum);
    printf("Fixup Page Table         : %08X\n", (int)pE32Exe->e32_fpagetab+
        (pE32Exe->e32_fpagetab ? pExeHdr->e_lfanew : 0));
    printf("Fixup Record Table       : %08X\n", (int)pE32Exe->e32_frectab+
        (pE32Exe->e32_frectab ? pExeHdr->e_lfanew : 0));

    printf("\n-> Object section, see below\n");
    printf("Object Table offset      : %08X\n", (int)pE32Exe->e32_objtab+
        (pE32Exe->e32_objtab ? pExeHdr->e_lfanew : 0));
    printf("Object Table count       : %d\n", (int)pE32Exe->e32_objcnt);
    printf("Object Page Table offset : %08X\n", (int)pE32Exe->e32_objmap+
        (pE32Exe->e32_objmap ? pExeHdr->e_lfanew : 0));
    printf("Obj. iterated. pages off.: %08X\n", (int)pE32Exe->e32_itermap);

    printf("\n-> Resource section\n");
    printf("Resource Table offset    : %08X\n", (int)pE32Exe->e32_rsrctab+ 
        (pE32Exe->e32_rsrctab ? pExeHdr->e_lfanew : 0));
    printf("Resource Table count     : %d\n", (int)pE32Exe->e32_rsrccnt);

    printf("\n-> Import section, see below\n");
    printf("Module directive tbl off.: %08X\n", (int)pE32Exe->e32_dirtab+
        (pE32Exe->e32_dirtab ? pExeHdr->e_lfanew : 0));
    printf("Module directive tbl cnt.: %d\n", (int)pE32Exe->e32_dircnt);
    printf("Import module name Table : %08X length %08X\n", (int)pE32Exe->e32_impmod+
        (pE32Exe->e32_impmod ? pExeHdr->e_lfanew : 0),
        (pE32Exe->e32_impproc-pE32Exe->e32_impmod));
    printf("Import count             : %d\n", (int)pE32Exe->e32_impmodcnt);
    printf("Import procname offset   : %08X\n", (int)pE32Exe->e32_impproc+
        (pE32Exe->e32_impproc ? pExeHdr->e_lfanew : 0));

    printf("\n-> Resident/Nonresident (Export Table) section, see below\n");
    printf("Resident Table offset    : %08X\n", (int)pE32Exe->e32_restab+
        (pE32Exe->e32_restab ? pExeHdr->e_lfanew : 0));
    printf("Entry Table offset       : %08X\n", (int)pE32Exe->e32_enttab+
        (pE32Exe->e32_enttab ? pExeHdr->e_lfanew : 0));
    printf("Non-resident offset      : %08X\n", (int)pE32Exe->e32_nrestab);
    printf("Non-resident size        : %d (%08X) bytes\n", 
        (int)pE32Exe->e32_cbnrestab, (int)pE32Exe->e32_cbnrestab);
    printf("Non-resident chksum      : %08X\n", (int)pE32Exe->e32_nressum);

    printf("\n-> Loader section\n");
    printf("Data page offset         : %08X\n", (int)pE32Exe->e32_datapage);
    printf("Preload pages            : %d\n", (int)pE32Exe->e32_preload);
    printf("Per-page chksum tbl. off.: %08X\n", (int)pE32Exe->e32_pagesum+
        (pE32Exe->e32_pagesum ? pExeHdr->e_lfanew : 0));
    printf("Autodata object          : %d (n/a for LX)\n", (int)pE32Exe->e32_autodata);
    printf("Debug offset             : %08X\n", (int)pE32Exe->e32_debuginfo);
    printf("Debug size               : %d (%08X)\n", 
        (int)pE32Exe->e32_debuglen, (int)pE32Exe->e32_debuglen);
    printf("Instance Preload pages   : %d\n", (int)pE32Exe->e32_instpreload);
    printf("Instance Demandload pages: %d\n", (int)pE32Exe->e32_instdemand);
    printf("Heap size                : %d\n", (int)pE32Exe->e32_heapsize);
    printf("Stack size               : %d\n", (int)pE32Exe->e32_stacksize);

    printf("\n");
    return(0);
}

int DumpObjectTable(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr, ULONG bDumpPages)
{
    struct o32_obj *pO32Obj=0;
    struct o32_map *pO32Map=0;
    int             iObjectTable;

    pO32Obj=(struct o32_obj *)&pBuffer[pE32Exe->e32_objtab+pExeHdr->e_lfanew];
    printf("\nDumping Object Table:\n");
    for(iObjectTable=1; iObjectTable<=pE32Exe->e32_objcnt; iObjectTable++)
        {
        int     iPageTable;

        printf("Obj. Rel.Base Codesize Flags    Tableidx Tablesize\n");    
        printf("%04X %08X %08X %08X %08X %08X\n", iObjectTable, 
            pO32Obj->o32_base, pO32Obj->o32_size, pO32Obj->o32_flags, 
            pO32Obj->o32_pagemap, pO32Obj->o32_mapsize);
        if(pO32Obj->o32_flags & OBJREAD)
            printf("                                                  %s\n", "Readable");
        if(pO32Obj->o32_flags & OBJWRITE)                       
            printf("                                                  %s\n", "Writeable");
        if(pO32Obj->o32_flags & OBJEXEC)                        
            printf("                                                  %s\n", "Executable");
        printf("                                                  %s%s\n", 
            (pO32Obj->o32_flags & OBJRSRC ? "" : "Non"), "Resource");
        printf("                                                  %s%s\n", 
            (pO32Obj->o32_flags & OBJDISCARD ? "" : "Non"), "Discardable");
        printf("                                                  %s%s\n", 
            (pO32Obj->o32_flags & OBJSHARED ? "" : "Non"), "Shared");
        printf("                                                  %s%s\n", 
            (pO32Obj->o32_flags & OBJPRELOAD ? "Preload" : "Loadoncall"), " Pages");
        printf("                                                  %s%s\n", 
            (pO32Obj->o32_flags & OBJINVALID ? "Invalid" : "Valid "), " Pages");
        printf("                                                  %s%s\n", 
            (pO32Obj->o32_flags & OBJPERM ? "Non" : ""), "Swappable");
        if(pO32Obj->o32_flags & OBJRESIDENT)                    
            printf("                                                  %s\n", "Resident (VDDs, PDDs only)");
        if(pO32Obj->o32_flags & OBJCONTIG)                      
            printf("                                                  %s\n", "Resident Contigout (VDDs, PDDs only)");
        if(pO32Obj->o32_flags & OBJDYNAMIC)                     
            printf("                                                  %s\n", "Resident lockable (VDDs, PDDs only)");
        if(pO32Obj->o32_flags & OBJALIAS16)                     
            printf("                                                  %s\n", "16:16 Alias");
        printf("                                                  %s%s\n", 
            (pO32Obj->o32_flags & OBJBIGDEF ? "32-bit Big" : "16-bit"), " Descriptor");
        printf("                                                  %s%s\n", 
            (pO32Obj->o32_flags & OBJCONFORM ? "" : "Non"), "Conforming");
        printf("                                                  %s%s\n", 
            (pO32Obj->o32_flags & OBJIOPL ? "" : "Non"), "IOPL");
        if((pO32Obj->o32_flags & 0xFFFF0800)!=0)
            printf("                                                  Unknown 0x%08X\n",
                pO32Obj->o32_flags & 0xFFFF0800);
        pO32Map=(struct o32_map *)&pBuffer[pE32Exe->e32_objmap+pExeHdr->e_lfanew];
        pO32Map=&pO32Map[pO32Obj->o32_pagemap-1];
        if(pO32Obj->o32_mapsize!=0)
            {
            printf("\tPage Tables:\n");
            printf("\tTableidx Offset   Physical Size Flags\n");
            }
        for(iPageTable=1; iPageTable<=pO32Obj->o32_mapsize; iPageTable++)
            {
            if(pO32Map->o32_pageflags!=ZEROED)
                printf("\t%08X %08X %08X %04X ", 
                    iPageTable, 
                    pO32Map->o32_pagedataoffset<<pE32Exe->e32_pageshift, 
                    pE32Exe->e32_datapage+(pO32Map->o32_pagedataoffset<<pE32Exe->e32_pageshift),
                    pO32Map->o32_pagesize);
            else
                printf("\t%08X n/a      n/a      %04X ", 
                    iPageTable, 
                    pO32Map->o32_pagedataoffset<<pE32Exe->e32_pageshift, 
                    pE32Exe->e32_datapage+(pO32Map->o32_pagedataoffset<<pE32Exe->e32_pageshift),
                    pO32Map->o32_pagesize);
            if(pO32Map->o32_pageflags==VALID)
                printf("Valid");           
            else if(pO32Map->o32_pageflags==ITERDATA)
                printf("Iterated (/Exepack:1)");           
            else if(pO32Map->o32_pageflags==INVALID)
                printf("Invalid (zero)");           
            else if(pO32Map->o32_pageflags==ZEROED)
                printf("Zeroed (zero)");           
            else if(pO32Map->o32_pageflags==RANGE)
                printf("Page-Range");           
            else if(pO32Map->o32_pageflags==ITERDATA2)
                printf("Iterated 2 (/Exepack:2)");           
            else
                printf("Unknown 0x%04\n", (int)pO32Map->o32_pageflags);
            printf("\n");
            if(bDumpPages==TRUE)
                {
                unsigned char      *pucOffsetPhysical;
                unsigned char       ucChar;
                int                 iPageSize;
                int                 iOffsetLogical;
                int                 iOffsetTemp;

                printf("\n    Offset:   +0          +4          +8          +C          ASCII Dump:\n");
                pucOffsetPhysical=(unsigned char *)&pBuffer[(pE32Exe->e32_datapage+(pO32Map->o32_pagedataoffset<<pE32Exe->e32_pageshift))];
                iPageSize=pO32Map->o32_pagesize;
                iOffsetLogical=0;
                while(iOffsetLogical<iPageSize)
                    {
                    printf("    %08X  ", iOffsetLogical);
                    for(iOffsetTemp=0; 
                        (iOffsetTemp<0x10) && (iOffsetLogical+iOffsetTemp<iPageSize);
                        iOffsetTemp++)
                        {
                        printf("%02X ", *(pucOffsetPhysical+iOffsetTemp));
                        }
                    for( ; iOffsetTemp<0x10; iOffsetTemp++)
                        printf("   ");
                    for(iOffsetTemp=0; 
                        (iOffsetTemp<0x10) && (iOffsetLogical+iOffsetTemp<iPageSize);
                        iOffsetTemp++)
                        {
                        ucChar=*(pucOffsetPhysical+iOffsetTemp);
                        if((ucChar<=0x06) || (ucChar>=0x32))
                            printf("%c", ucChar);
                        else
                            printf(" ");
                        }
                    printf("\n");
                    iOffsetLogical+=0x10;
                    pucOffsetPhysical+=0x10;
                    }
                printf("\n");
                printf("\n");
                }
            pO32Map++;
            }
        pO32Obj++;
        printf("\n");
        }
    return(0);
}

int DumpResourceTable(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr)
{
    struct rsrc32  *pR32Rsc;
    char           *apcResourceName[]={ "Mouse Pointer",
                                        "Bitmap",
                                        "Menu Template",
                                        "Dialog Template",
                                        "String Table",
                                        "Font Directory",
                                        "Font",
                                        "Accelerator Table",
                                        "Binary Data",
                                        "Error message Table",
                                        "Dialog include",
                                        "Key to VKey Table",
                                        "Key to UGL Table",
                                        "Glyph to Char Table",
                                        "Screen display info",
                                        "Function key area short",
                                        "Function key area long",
                                        "Help Table",
                                        "Help Subtable",
                                        "DBCS font Directory",
                                        "DBCS font Driver",
                                        "Unknown" };
    int             iResourceCount=pE32Exe->e32_rsrccnt;

    printf("\nDumping Resource Table:\n");
    if((pE32Exe->e32_rsrctab==0) || (pE32Exe->e32_rsrccnt==0))
        {
        printf("\tEmpty\n");
        return(1);
        }
    pR32Rsc=(struct rsrc32 *)&pBuffer[pE32Exe->e32_rsrctab+pExeHdr->e_lfanew];
    printf("Type Name        Size            Obj. Offset\n");
    for(iResourceCount=0; iResourceCount<pE32Exe->e32_rsrccnt; iResourceCount++)
        {
        printf("%04X %04X (%04d) %08X (%04d) %04X %08X (%s)\n",
            pR32Rsc->type,
            pR32Rsc->name,
            pR32Rsc->name,
            pR32Rsc->cb,
            pR32Rsc->cb,
            pR32Rsc->obj,
            pR32Rsc->offset,
            ((pR32Rsc->type-1)<(sizeof(apcResourceName)/sizeof(apcResourceName[0])) ?
                apcResourceName[pR32Rsc->type-1] : 
                apcResourceName[(sizeof(apcResourceName)/sizeof(apcResourceName[0]))]));
        pR32Rsc++;
        }
    return(0);
}

int DumpImportModulesTable(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr)
{
    struct impname_table            *pImpname_table;
    int                             iLen;
    int                             iModulesCount;

    printf("\nDumping Import modules:\n");
    if((pE32Exe->e32_impmod==0) || (pE32Exe->e32_impmodcnt==0))
        {
        printf("\tEmpty\n");
        return(1);
        }
                                        /* The Import Modules table are impname_table
                                           structures of e32_impmodcnt count */
    pImpname_table=(struct impname_table *)&pBuffer[pE32Exe->e32_impmod+pExeHdr->e_lfanew];
    for(iModulesCount=0; iModulesCount<pE32Exe->e32_impmodcnt; iModulesCount++)
        {
        printf("\t");
        for(iLen=0; iLen<pImpname_table->bLen; iLen++)
            printf("%c", pImpname_table->acName[iLen]);
        printf("\n");
        pImpname_table=(struct impname_table *)
            (((unsigned char *)pImpname_table)+pImpname_table->bLen+1);
        }
    return(0);
}

int DumpImportProceduresTable(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr)
{
    struct impname_table            *pImpname_table;
    int                             iLen;
    int                             iModulesCount;

    printf("\nDumping Import procedures:\n");
//    if((pE32Exe->e32_impproc==0) || ((pE32Exe->e32_impproc+pExeHdr->e_lfanew)>=pE32Exe->e32_datapage))
    if((pE32Exe->e32_impproc==0) || 
        (((BYTE *)pE32Exe)+pE32Exe->e32_impproc>=((BYTE *)pE32Exe)+(pE32Exe->e32_fpagetab+pE32Exe->e32_fixupsize)))
        {
        printf("\tEmpty\n");
        return(1);
        }
                                        /* The Import Procedures table are impname_table
                                           structures until impname_table.bLen is 0 */
    pImpname_table=(struct impname_table *)&pBuffer[pE32Exe->e32_impproc+pExeHdr->e_lfanew];
                                        /* For unknown reasons, the 1st byte can be 0
                                           even though the table is not emptry. As a
                                           workaround start with the next byte */
    if(pImpname_table->bLen==0)
        pImpname_table=(struct impname_table *)&pBuffer[pE32Exe->e32_impproc+pExeHdr->e_lfanew+1];
    while(pImpname_table->bLen!=0)
        {
        printf("\t");
        for(iLen=0; iLen<pImpname_table->bLen; iLen++)
            printf("%c", pImpname_table->acName[iLen]);
        printf("\n");
        pImpname_table=(struct impname_table *)
            (((unsigned char *)pImpname_table)+pImpname_table->bLen+1);
        }
    return(0);
}

int DumpResidentNameTable(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr)
{
    struct res_nonres_table        *pRes_nonres_table;
    int                             iLen;
    int                             iOrdinal;
    struct b32_bundle              *pB32Bundle=0;
    struct e32_entry               *pE32Entry=0;

    printf("\nDumping Resident Name Table:\n");
    if(pE32Exe->e32_restab==0)
        {
        printf("\tEmpty\n");
        return(1);
        }
                                        /* The Resident and Non-Resident name table are
                                           expected to be res_nonres_table structures */
    pRes_nonres_table=(struct res_nonres_table *)&pBuffer[pE32Exe->e32_restab+pExeHdr->e_lfanew];
    printf("\tOrdinal Object  Offset    Name\n");
    while(pRes_nonres_table->bLen!=0)
        {
        iOrdinal=(int)*((unsigned short *)&pRes_nonres_table->acName[pRes_nonres_table->bLen]);
        if(iOrdinal==0)
            {
            printf("\t%4d                      ", iOrdinal);
            for(iLen=0; iLen<pRes_nonres_table->bLen; iLen++)
                printf("%c", pRes_nonres_table->acName[iLen]);
            printf("\n");
            pRes_nonres_table=(struct res_nonres_table *)
                (((unsigned char *)&pRes_nonres_table->acName[pRes_nonres_table->bLen+sizeof(pRes_nonres_table->usOrd)]));
            continue;
            }
        pB32Bundle=0;
        pE32Entry=0;
        GetExportFromOrdinal(&pB32Bundle, &pE32Entry, pE32Exe, pExeHdr, iOrdinal);
        if((pB32Bundle)->b32_type==ENTRY16)
            {
            printf("\t%4d    %4d    %04X      ", iOrdinal, (int)pB32Bundle->b32_obj, 
                (int)pE32Entry->e32_variant.e32_offset.offset16);
            for(iLen=0; iLen<pRes_nonres_table->bLen; iLen++)
                printf("%c", pRes_nonres_table->acName[iLen]);
            printf("\n\t\t\t\t\tEntry16\n");
            if(pE32Entry->e32_flags & E32PARAMS)
                printf("%d words ", (int)((pE32Entry->e32_flags & E32PARAMS)>>3));
            if(pE32Entry->e32_flags & E32SHARED)
                printf("\t\t\t\t\tShared Data\n");
            if(pE32Entry->e32_flags & E32EXPORT)
                printf("\t\t\t\t\tExported\n");
            }
        else if((pB32Bundle)->b32_type==GATE16)
            {
            printf("\t%4d    %4d    %04X:%04X ", iOrdinal, (int)pB32Bundle->b32_obj, 
                (int)pE32Entry->e32_variant.e32_callgate.callgate, 
                (int)pE32Entry->e32_variant.e32_callgate.offset);
            for(iLen=0; iLen<pRes_nonres_table->bLen; iLen++)
                printf("%c", pRes_nonres_table->acName[iLen]);
            printf("\n\t\t\t\t\t286 Call Gate\n");
            }
        else if((pB32Bundle)->b32_type==ENTRY32)
            {
            printf("\t%4d    %4d    %08X  ", iOrdinal, (int)pB32Bundle->b32_obj, 
                (int)pE32Entry->e32_variant.e32_offset.offset32);
            for(iLen=0; iLen<pRes_nonres_table->bLen; iLen++)
                printf("%c", pRes_nonres_table->acName[iLen]);
            printf("\n\t\t\t\t\tEntry32\n");
            if(pE32Entry->e32_flags & E32PARAMS)
                printf("%d words ", (int)(pE32Entry->e32_flags & E32PARAMS));
            if(pE32Entry->e32_flags & E32SHARED)
                printf("\t\t\t\t\tShared Data\n");
            if(pE32Entry->e32_flags & E32EXPORT)
                printf("\t\t\t\t\tExported\n");
            }
        else if((pB32Bundle)->b32_type==ENTRYFWD)
            {
            struct impname_table   *pImpname_table;
            int                     iLen;
            int                     iModulesCount;

            printf("\t%4d    %4d    %04X:%04X ", iOrdinal, (int)pB32Bundle->b32_obj, 
                (int)pE32Entry->e32_variant.e32_fwd.modord, (int)pE32Entry->e32_variant.e32_fwd.value);
            for(iLen=0; iLen<pRes_nonres_table->bLen; iLen++)
                printf("%c", pRes_nonres_table->acName[iLen]);
            printf("\n\t\t\t\tForwarded to ");
            pImpname_table=(struct impname_table *)&pBuffer[pE32Exe->e32_impmod+pExeHdr->e_lfanew];
            for(iModulesCount=0; iModulesCount<pE32Exe->e32_impmodcnt; iModulesCount++)
                {
                if(iModulesCount+1==pE32Entry->e32_variant.e32_fwd.modord)
                    {
                    for(iLen=0; iLen<pImpname_table->bLen; iLen++)
                        printf("%c", pImpname_table->acName[iLen]);
                    break;
                    }
                pImpname_table=(struct impname_table *)
                    (((unsigned char *)pImpname_table)+pImpname_table->bLen+1);
                }
            printf(".%d\n", (int)pE32Entry->e32_variant.e32_fwd.value);
            }
        pRes_nonres_table=(struct res_nonres_table *)
            (((unsigned char *)&pRes_nonres_table->acName[pRes_nonres_table->bLen+sizeof(pRes_nonres_table->usOrd)]));
        }
    return(0);
}

int DumpNonResidentNameTable(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr)
{
    struct res_nonres_table        *pRes_nonres_table=0;
    int                             iLen;
    int                             iOrdinal;
    struct b32_bundle              *pB32Bundle=0;
    struct e32_entry               *pE32Entry=0;

    printf("\nDumping NonResident Name Table:\n");
    if(pE32Exe->e32_nrestab==0)
        {
        printf("\tEmpty\n");
        return(1);
        }
                                        /* The Resident and Non-Resident name table are
                                           expected to be res_nonres_table structures */
    pRes_nonres_table=(struct res_nonres_table *)&pBuffer[pE32Exe->e32_nrestab];
    printf("\tOrdinal Object  Offset    Name\n");
    while(pRes_nonres_table->bLen!=0)
        {
        iOrdinal=(int)*((unsigned short *)&pRes_nonres_table->acName[pRes_nonres_table->bLen]);
        if(iOrdinal==0)
            {
            printf("\t%4d                      ", iOrdinal);
            for(iLen=0; iLen<pRes_nonres_table->bLen; iLen++)
                printf("%c", pRes_nonres_table->acName[iLen]);
            printf("\n");
            pRes_nonres_table=(struct res_nonres_table *)
                (((unsigned char *)&pRes_nonres_table->acName[pRes_nonres_table->bLen+sizeof(pRes_nonres_table->usOrd)]));
            continue;
            }
        pB32Bundle=0;
        pE32Entry=0;
        GetExportFromOrdinal(&pB32Bundle, &pE32Entry, pE32Exe, pExeHdr, iOrdinal);
        if((pB32Bundle)->b32_type==ENTRY16)
            {
            printf("\t%4d    %4d    %08X  ", iOrdinal, (int)pB32Bundle->b32_obj, 
                (int)pE32Entry->e32_variant.e32_offset.offset16);
            for(iLen=0; iLen<pRes_nonres_table->bLen; iLen++)
                printf("%c", pRes_nonres_table->acName[iLen]);
            printf("\n\t\t\t\t\tEntry32\n");
            if(pE32Entry->e32_flags & E32PARAMS)
                printf("%d words ", (int)(pE32Entry->e32_flags & E32PARAMS));
            if(pE32Entry->e32_flags & E32SHARED)
                printf("\t\t\t\t\tShared Data\n");
            if(pE32Entry->e32_flags & E32EXPORT)
                printf("\t\t\t\t\tExported\n");
            }
        else if((pB32Bundle)->b32_type==GATE16)
            {
            printf("\t%4d    %4d    %04X:%04X ", iOrdinal, (int)pB32Bundle->b32_obj, 
                (int)pE32Entry->e32_variant.e32_callgate.callgate, 
                (int)pE32Entry->e32_variant.e32_callgate.offset);
            for(iLen=0; iLen<pRes_nonres_table->bLen; iLen++)
                printf("%c", pRes_nonres_table->acName[iLen]);
            printf("\n\t\t\t\t\tGate16\n");
            }
        else if((pB32Bundle)->b32_type==ENTRY32)
            {
            printf("\t%4d    %4d    %08X  ", iOrdinal, (int)pB32Bundle->b32_obj, 
                (int)pE32Entry->e32_variant.e32_offset.offset32);
            for(iLen=0; iLen<pRes_nonres_table->bLen; iLen++)
                printf("%c", pRes_nonres_table->acName[iLen]);
            printf("\n\t\t\t\t\tEntry32\n");
            if(pE32Entry->e32_flags & E32PARAMS)
                printf("%d words ", (int)(pE32Entry->e32_flags & E32PARAMS));
            if(pE32Entry->e32_flags & E32SHARED)
                printf("\t\t\t\t\tShared Data\n");
            if(pE32Entry->e32_flags & E32EXPORT)
                printf("\t\t\t\t\tExported\n");
            }
        else if((pB32Bundle)->b32_type==ENTRYFWD)
            {
            struct impname_table   *pImpname_table;
            int                     iLen;
            int                     iModulesCount;

            printf("\t%4d    %4d    %04X:%04X ", iOrdinal, (int)pB32Bundle->b32_obj, 
                (int)pE32Entry->e32_variant.e32_fwd.modord, (int)pE32Entry->e32_variant.e32_fwd.value);
            for(iLen=0; iLen<pRes_nonres_table->bLen; iLen++)
                printf("%c", pRes_nonres_table->acName[iLen]);
            printf("\n\t\t\t\t\tForwarded to ");
            pImpname_table=(struct impname_table *)&pBuffer[pE32Exe->e32_impmod+pExeHdr->e_lfanew];
            for(iModulesCount=0; iModulesCount<pE32Exe->e32_impmodcnt; iModulesCount++)
                {
                if(iModulesCount+1==pE32Entry->e32_variant.e32_fwd.modord)
                    {
                    for(iLen=0; iLen<pImpname_table->bLen; iLen++)
                        printf("%c", pImpname_table->acName[iLen]);
                    break;
                    }
                pImpname_table=(struct impname_table *)
                    (((unsigned char *)pImpname_table)+pImpname_table->bLen+1);
                }
            printf(".%d\n", (int)pE32Entry->e32_variant.e32_fwd.value);
            }
        else
            {
            printf("\t%4d                      ", iOrdinal);
            for(iLen=0; iLen<pRes_nonres_table->bLen; iLen++)
                printf("%c", pRes_nonres_table->acName[iLen]);
            printf("\n");
            }
        pRes_nonres_table=(struct res_nonres_table *)
            (((unsigned char *)&pRes_nonres_table->acName[pRes_nonres_table->bLen+sizeof(pRes_nonres_table->usOrd)]));
        }
    return(0);
}

int DumpEntryTable(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr)
{
    struct b32_bundle              *pB32Bundle=0;
    struct e32_entry               *pE32Entry=0;
    struct e32_entry               *pE32EntryPrevious=0;
    int                             iLine;
    int                             iOrdinal;
 
    printf("\nDumping Entry Table:\n");
    if(pE32Exe->e32_enttab==0)
        {
        printf("\tEmpty\n");
        return(1);
        }
    printf("\tType:         Target:\n");
    pB32Bundle=(struct b32_bundle *)&pBuffer[pE32Exe->e32_enttab+pExeHdr->e_lfanew];
    iOrdinal=1;
    while(pB32Bundle->b32_cnt!=0)
        {
        int     iCount;

                                    /* The Export section is a b32_bundle, followed by
                                       1...n e32_entry structures (whose size depend
                                       on the type) */
        iCount=pB32Bundle->b32_cnt;
        pE32EntryPrevious=pE32Entry=(struct e32_entry *)(pB32Bundle+1);
        if(pB32Bundle->b32_type==EMPTY)
            {
            }
        else if(pB32Bundle->b32_type==ENTRY16)
            {
            printf("\t16-Bit Entry  ");
            printf("Obj. %2d ", (int)pB32Bundle->b32_obj);
            }
        else if(pB32Bundle->b32_type==GATE16)
            {
            printf("\t286 Call Gate ");
            printf("Obj. %2d ", (int)pB32Bundle->b32_obj);
            }
        else if(pB32Bundle->b32_type==ENTRY32)
            {
            printf("\t32-Bit Entry  ");
            printf("Obj. %2d ", (int)pB32Bundle->b32_obj);
            }
        else if(pB32Bundle->b32_type==ENTRYFWD)
            {
            printf("\tForwarder     ");
            printf("        ");
            }
        else
            {
            printf("\tUnknown\n");
            }
        for(iLine=0; iCount>0; iCount--, iLine++, iOrdinal++)
            {
            if(iLine!=0)
                printf("\t                      ");
            if(pB32Bundle->b32_type==EMPTY)
                {
                pE32Entry=(struct e32_entry *)(((unsigned char*)pB32Bundle)+sizeof(unsigned short));
                iOrdinal+=iCount;
                break;
                }
            else if(pB32Bundle->b32_type==ENTRY16)
                {
                printf("Ord. %4d Off. %04X      ", 
                    iOrdinal,
                    (int)pE32Entry->e32_variant.e32_offset.offset16);
                pE32Entry=(struct e32_entry *)(((unsigned char*)pE32Entry)+FIXENT16);
                }
            else if(pB32Bundle->b32_type==GATE16)
                {
                printf("Ord. %4d Off. %04X:%04X ", 
                    iOrdinal,
                    (int)pE32Entry->e32_variant.e32_callgate.callgate,
                    (int)pE32Entry->e32_variant.e32_callgate.offset);
                pE32Entry=(struct e32_entry *)(((unsigned char*)pE32Entry)+GATEENT16);
                }
            else if(pB32Bundle->b32_type==ENTRY32)
                {
                printf("Ord. %4d Off. %08X  ", 
                    iOrdinal,
                    (int)pE32Entry->e32_variant.e32_offset.offset32);
                pE32Entry=(struct e32_entry *)(((unsigned char*)pE32Entry)+FIXENT32);
                }
            else if(pB32Bundle->b32_type==ENTRYFWD)
                {
                printf("Ord. %4d Off. %04X:%08X ", 
                    iOrdinal,
                    (int)pE32Entry->e32_variant.e32_fwd.modord,
                    (int)pE32Entry->e32_variant.e32_fwd.value);
                pE32Entry=(struct e32_entry *)(((unsigned char*)pE32Entry)+FWDENT);
                }
            else
                {
                printf("\tUnknown\n");
                printf("Unknown Bundle type %x\n", (int)pB32Bundle->b32_type);
                }
            if(pB32Bundle->b32_type!=ENTRYFWD)
                if(pE32EntryPrevious->e32_flags)
                    {
                    if(pE32EntryPrevious->e32_flags & E32EXPORT)
                        printf("Exported ");
                    if(pE32EntryPrevious->e32_flags & E32SHARED)
                        printf("Shared ");
                    if(pE32EntryPrevious->e32_flags & E32PARAMS)
                        printf("%d word params ", (int)((pE32Entry->e32_flags & E32PARAMS)>>3));
                    }
            printf("\n");
            pE32EntryPrevious=pE32Entry;
            }
        pB32Bundle=(struct b32_bundle *)pE32Entry;
        }
ExportFound:
    return(0);
}

int DumpFixupPageTable(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr)
{
    unsigned long                  *pulOffset=0;
    unsigned long                   ulPhysicalOffset=0;
    int                             iPage=1;

    printf("\nDumping Fixup Page Table:\n");
    if(pE32Exe->e32_fpagetab==0)
        {
        printf("\tEmpty\n");
        return(1);
        }
    printf("\tPage Fixup Record Offset  File Offset\n"); 
                                        /* The Fixup Page Table ends with the Fixup Record
                                           Table e32_frectab */
    pulOffset=(unsigned long *)&pBuffer[pE32Exe->e32_fpagetab+pExeHdr->e_lfanew];
    while((pulOffset+1)<((unsigned long *)&pBuffer[(int)pE32Exe->e32_frectab+pExeHdr->e_lfanew]))
        {
        ulPhysicalOffset=pE32Exe->e32_frectab+pExeHdr->e_lfanew+*pulOffset;
        printf("\t%4X %08X             %08X\n", iPage, (int)*pulOffset, (int)ulPhysicalOffset);
        pulOffset++;
        iPage++;
        }
    return(0);
}

int DumpFixupRecordTable(struct e32_exe *pE32Exe, struct exe_hdr *pExeHdr)
{
    BYTE                           *pStream;
    struct r32_fixup                R32_fixup;
    struct r32_fixup               *pR32_fixup=0;
    unsigned long                  *pulOffset=0;
    unsigned long                   ulPhysicalOffset=0;
    int                             iLastPage=-1;
    int                             iPage;
    int                             iCount;
    char                            acLibrary[]="12345678";
    char                           *apcRecordType[]={ "Byte Fixup  ",
                                                      "undefined   ",
                                                      "16-Bit Sel. ",
                                                      "16:16  Ptr. ",
                                                      "undefined   ",
                                                      "Offset(16)  ",
                                                      "16:32  Ptr. ",
                                                      "Offset(32)  ",
                                                      "Rel Off(32) " };

    printf("\nDumping Fixup Record Table:\n");
    if(pE32Exe->e32_frectab==0)
        {
        printf("\tEmpty\n");
        return(1);
        }
                                        /* The Fixup Record Table ends with the Import
                                           Modules Table e32_impmod */
    pStream=(BYTE *)&pBuffer[pE32Exe->e32_frectab+pExeHdr->e_lfanew];
    while(pStream<((BYTE *)&pBuffer[(int)pE32Exe->e32_impmod+pExeHdr->e_lfanew]))
        {
                                        /* The Fixup record is a variable size record that
                                           can best be interpreted as a stream of bytes.
                                           We thus use a common structure that gets filled
                                           from the stream step by step, while the stream
                                           pointer points to the next byte to read from stream */
        memset(&R32_fixup, 0, sizeof(R32_fixup));
        R32_fixup.nr_stype=*(unsigned char *)pStream;
        pStream++;
        R32_fixup.nr_flags=*(unsigned char *)pStream;
        pStream++;
                                        /* Normally r32_fixup.r32_soff_cnt is a short, unless
                                           the entry has the Source List Flat set (NRCHAIN) */
        if(R32_fixup.nr_stype & NRCHAIN)
            {
            R32_fixup.r32_soff_cnt=*(unsigned char *)pStream;
            pStream++;
            pR32_fixup=malloc(sizeof(struct r32_fixup)+R32_fixup.r32_soff_cnt*sizeof(unsigned short));
            memset(pR32_fixup, 0, sizeof(struct r32_fixup)+R32_fixup.r32_soff_cnt*sizeof(unsigned short));
            }
        else
            {
            R32_fixup.r32_soff_cnt=*(unsigned short *)pStream;
            pStream+=2;
            pR32_fixup=malloc(sizeof(struct r32_fixup));
            memset(pR32_fixup, 0, sizeof(*pR32_fixup));
            }
                                        /* The fixup may either be a 16-bit source offset or 
                                           an 8-bit count. */
        if(R32_fixup.nr_flags & NR16OBJMOD)
            {
            R32_fixup.r32_type.int_obj=*(unsigned short *)pStream;
            pStream+=2;
            }
        else
            {
            R32_fixup.r32_type.int_obj=*(unsigned char *)pStream;
            pStream++;
            }
        *pR32_fixup=R32_fixup;
                                        /* Find the page this fixup applies to */
        iPage=0;
        pulOffset=(unsigned long *)&pBuffer[pE32Exe->e32_fpagetab+pExeHdr->e_lfanew];
        while((pulOffset+1)<((unsigned long *)&pBuffer[(int)pE32Exe->e32_frectab+pExeHdr->e_lfanew]))
            {
            if(pStream<((BYTE *)&pBuffer[pE32Exe->e32_frectab+pExeHdr->e_lfanew])+*pulOffset)
                break;
            pulOffset++;
            iPage++;
            }
        if(iPage!=iLastPage)
            {
            printf("\nPage %2X Target Flags:      Src.Flags:\n", iPage);
            iLastPage=iPage;
            }
                                        /* Determine type by applying reference type mask */
        switch(pR32_fixup->nr_flags & NRRTYP)
        {
                                        /* Internal fixup */ 
        case NRRINT:              
            printf("\tInternal Fixup     %s", apcRecordType[pR32_fixup->nr_stype & NRSTYP]);      
                                        /* The internal fixup may point to either a 32-bit or
                                           16-bit target offset */ 
            if((pR32_fixup->nr_stype & NRSTYP)!=NRSSEG)
                {
                if(pR32_fixup->nr_flags & NR32BITOFF)
                    {
                    pR32_fixup->r32_target.intref.toff=*(unsigned long *)pStream;
                    pStream+=4;    
                    }
                else
                    {
                    pR32_fixup->r32_target.intref.toff=*(unsigned short *)pStream;
                    pStream+=2;    
                    }
                }
            if(!(pR32_fixup->nr_stype & NRCHAIN))
                printf("%04X Obj: %2d Off. %04X", 
                    (int)pR32_fixup->r32_soff_cnt,
                    (int)pR32_fixup->r32_type.int_obj, 
                    (int)pR32_fixup->r32_target.intref.toff);
            else
                {
                printf("     ");
                if((pR32_fixup->nr_stype & NRSTYP)!=NRSSEG)
                    printf("Obj: %2d Off. %04X", 
                        (int)pR32_fixup->r32_type.int_obj, 
                        (int)pR32_fixup->r32_target.intref.toff);
                }
            break;

                                        /* Import by ordinal */
        case NRRORD:                    
            {
            struct impname_table   *pImpname_table;
            int                     iModulesCount;
            int                     iProceduresCount;

            printf("\tImport by ordinal  %s", apcRecordType[pR32_fixup->nr_stype & NRSTYP]);      
                                        /* The import by ordinal fixup may point to either a 32-bit
                                           or 16-bit target offset or 8-bit import ordinal */ 
            if(pR32_fixup->nr_flags & NR8BITORD)
                {
                pR32_fixup->r32_target.impord.ord=*(unsigned char *)pStream;
                pStream++;    
                }
            else
                {
                if(pR32_fixup->nr_flags & NR32BITOFF)
                    {
                    pR32_fixup->r32_target.impord.ord=*(unsigned long *)pStream;
                    pStream+=4;    
                    }
                else
                    {
                    pR32_fixup->r32_target.impord.ord=*(unsigned short *)pStream;
                    pStream+=2;    
                    }
                }
                                        /* The import by ordinal fixup may contain
                                           additive fixup data */
            if(pR32_fixup->nr_flags & NRADD)
                {
                if(pR32_fixup->nr_flags & NR32BITADD)
                    {
                    pR32_fixup->r32_target.impord.add=*(unsigned long *)pStream;
                    pStream+=4;    
                    }
                else
                    {
                    pR32_fixup->r32_target.impord.add=*(unsigned short *)pStream;
                    pStream+=2;    
                    }
                printf("%04X (Add.) ", (int)pR32_fixup->r32_target.impord.add);
                }
                                        /* Find Import Module */
            memset(acLibrary, ' ', sizeof(acLibrary));
            acLibrary[sizeof(acLibrary)-1]='\0';
            pImpname_table=(struct impname_table *)&pBuffer[pE32Exe->e32_impmod+pExeHdr->e_lfanew];
            for(iModulesCount=0; iModulesCount<pE32Exe->e32_impmodcnt; iModulesCount++)
                {
                if(iModulesCount+1==pR32_fixup->r32_type.mod)
                    {
                    memcpy(acLibrary, pImpname_table->acName, pImpname_table->bLen);
                    break;
                    }
                pImpname_table=(struct impname_table *)
                    (((unsigned char *)pImpname_table)+pImpname_table->bLen+1);
                }
            printf("%s.%4d", 
                 acLibrary, (int)pR32_fixup->r32_target.impord.ord);
            if(!(pR32_fixup->nr_stype & NRCHAIN))
                {
                printf(" %04X (Off.)", (int)pR32_fixup->r32_soff_cnt);
                }
            }
            break;

                                        /* Import by name */
        case NRRNAM:                    
            {
            struct impname_table   *pImpname_table;
            int                     iLen;
            int                     iModulesCount;
            int                     iProceduresCount;

            printf("\tImport by name     %s", apcRecordType[pR32_fixup->nr_stype & NRSTYP]);      
                                        /* The import by name may either be a 32-bit
                                           or 16-bit offset */
            if(pR32_fixup->nr_flags & NR32BITOFF)
                {
                pR32_fixup->r32_target.impname.proc=*(unsigned long *)pStream;
                pStream+=4;    
                }
            else
                {
                pR32_fixup->r32_target.impname.proc=*(unsigned short *)pStream;
                pStream+=2;    
                }
                                        /* The import by name fixup may contain
                                           additive fixup data */
            if(pR32_fixup->nr_flags & NRADD)
                {
                if(pR32_fixup->nr_flags & NR32BITADD)
                    {
                    pR32_fixup->r32_target.impname.add=*(unsigned long *)pStream;
                    pStream+=4;    
                    }
                else
                    {
                    pR32_fixup->r32_target.impname.add=*(unsigned short *)pStream;
                    pStream+=2;    
                    }
                printf("%04X (Add.) ", (int)pR32_fixup->r32_target.impname.add);
                }
                                        /* Find Import Module */
            memset(acLibrary, ' ', sizeof(acLibrary));
            acLibrary[sizeof(acLibrary)-1]='\0';
            pImpname_table=(struct impname_table *)&pBuffer[pE32Exe->e32_impmod+pExeHdr->e_lfanew];
            for(iModulesCount=0; iModulesCount<pE32Exe->e32_impmodcnt; iModulesCount++)
                {
                if(iModulesCount+1==pR32_fixup->r32_type.mod)
                    {
                    memcpy(acLibrary, pImpname_table->acName, pImpname_table->bLen);
                    break;
                    }
                pImpname_table=(struct impname_table *)
                    (((unsigned char *)pImpname_table)+pImpname_table->bLen+1);
                }
            if(!(pR32_fixup->nr_stype & NRCHAIN))
                printf("%04X (Off.) ", (int)pR32_fixup->r32_soff_cnt); 
            else
                printf("    ");
            printf("%s.", acLibrary);
            pImpname_table=(struct impname_table *)&pBuffer[pE32Exe->e32_impproc+pExeHdr->e_lfanew+ 
                pR32_fixup->r32_target.impname.proc];
            if(pImpname_table->bLen!=0)
                for(iLen=0; iLen<pImpname_table->bLen; iLen++)
                    printf("%c", pImpname_table->acName[iLen]);
            }
            break;

                                        /* Internal entry table fixup */
        case NRRENT:                    
            printf("\tCall Gate          %s", apcRecordType[pR32_fixup->nr_stype & NRSTYP]);      
                                        /* The internal entry table fixup may contain
                                           additive fixup data */
            if(pR32_fixup->nr_flags & NRADD)
                {
                if(pR32_fixup->nr_flags & NR32BITADD)
                    {
                    pR32_fixup->r32_target.intentry.add=*(unsigned long *)pStream;
                    pStream+=4;    
                    }
                else
                    {
                    pR32_fixup->r32_target.intentry.add=*(unsigned short *)pStream;
                    pStream+=2;    
                    }
                printf("%04X (Add.) ", (int)pR32_fixup->r32_target.intentry.add);
                }
            if(!(pR32_fixup->nr_stype & NRCHAIN))
                printf("%04X", (int)pR32_fixup->r32_soff_cnt); 
            break;
        }
                                        /* The fixup may contain a list of source offsets,
                                           the r32_soff field contains the number of
                                           elements */
        if(pR32_fixup->nr_stype & NRCHAIN)
            {
            for(iCount=0; iCount<pR32_fixup->r32_soff_cnt; iCount++)
                {
                pR32_fixup->soff[iCount]=*(unsigned short *)pStream;
                pStream+=sizeof(unsigned short);
                switch(pR32_fixup->nr_flags & NRRTYP)
                {
                                        /* Internal fixup */ 
                case NRRINT:              
                    if(iCount>0)
                        printf("\t                                   ");
                    else
                        {
                        if((pR32_fixup->nr_stype & NRSTYP)!=NRSSEG)
                            printf("\n\t                                   ");
                        else
                            {
                            printf("Obj: %2d Off. %04X\n", 
                                (int)pR32_fixup->r32_type.int_obj, 
                                (int)pR32_fixup->soff[iCount]);
                            break;
                            }
                        }
                    printf("      %2d      %04X\n", 
                        (int)pR32_fixup->r32_type.int_obj, 
                        (int)pR32_fixup->soff[iCount]);
                    break;

                                        /* Import by ordinal */
                case NRRORD:                    
                    if(iCount>0)
                        printf("\t                                            ");      
                    if(iCount==0)
                        printf(" %04X (Off.)\n", (int)pR32_fixup->soff[iCount]);
                    else
                        printf(" %04X\n", (int)pR32_fixup->soff[iCount]);
                    break;

                                        /* Import by name */
                case NRRNAM:                    
                    if(iCount==0)
                        {
                        printf("\n\t                                   ");
                        printf(" %2d:%08X (Obj.:Offset)\n", 
                            (int)pR32_fixup->r32_type.int_obj, 
                            (int)pR32_fixup->soff[iCount]);
                        }
                    else
                        {
                        printf("\t                                   ");
                        printf(" %2d:%08X\n", 
                            (int)pR32_fixup->r32_type.int_obj, 
                            (int)pR32_fixup->soff[iCount]);
                        }
                    break;

                                        /* Internal entry table fixup */
                case NRRENT:                    
                    if(iCount>0)
                        printf("\t                               ");      
                    if(iCount==0)
                        printf("%04X (Off.)\n", (int)pR32_fixup->soff[iCount]);
                    else
                        printf("%04X\n", (int)pR32_fixup->soff[iCount]);
                    break;
                }
                }
            }
        else
            printf("\n");
                                        /* Clear */
        free(pR32_fixup);
        pR32_fixup=0;
        }

    return(0);
}

int main(int argc, char *argv[])
{
#define BUFFER_SIZE         5000000

    FILE                   *pFile;
    struct exe_hdr         *pExeHdr;
    int                     iLenExeHdr;
    struct exe             *pExe;
    int                     iLenExe;
    struct e32_exe         *pE32Exe;
    int                     iLenExe32;
    struct o32_obj         *pO32Obj;
    struct o32_map         *pO32Map;
    struct b32_bundle      *pB32Bundle;
    struct e32_entry       *pE32Entry;
    struct nonres_table    *pnonres_Table; 
    struct nonres_exports  *pnonres_Exports;

    printf("\nLX/2 Dump V%d.%02d - OS/2 LX (Linear Executable) Dump Facility\n", 
        (int)BLDLEVEL_SIGNATURE>>16, (int)(BLDLEVEL_SIGNATURE & 0xFF));
    printf("Copyright (C) by Roman Stangl, 1999 (Roman_Stangl@at.ibm.com)\n");
    printf("                 http://www.geocitis.com/SiliconValley/Pines/7885/\n\n");
    acBuffer=malloc(BUFFER_SIZE);
    pBuffer=(unsigned char *)((int)(acBuffer+0x10000) & 0xFFFF0000);
    if((argc>1) && ((pFile=fopen(argv[1], "rb"))!=0))
        {
        printf("\nStarting analysis of %s\n\n", argv[1]);
        fread(pBuffer, sizeof(char), BUFFER_SIZE, pFile); 
        fclose(pFile);
        pExeHdr=(struct exe_hdr *)pBuffer;
        iLenExeHdr=sizeof(*pExeHdr);
        pE32Exe=(struct e32_exe *)&pBuffer[pExeHdr->e_lfanew];
        iLenExe32=sizeof(*pE32Exe);
        if((pE32Exe->e32_magic[0]!='L') && (pE32Exe->e32_magic[1]!='X'))
            {
            printf("File is not LX executable!\n\n");
            return(1);
            }
        DumpHeader(pE32Exe, pExeHdr);
        if((argc>2) && (strstr(strupr(argv[2]), "DUMP")))
            DumpObjectTable(pE32Exe, pExeHdr, TRUE);
        else
            DumpObjectTable(pE32Exe, pExeHdr, FALSE);
        DumpResourceTable(pE32Exe, pExeHdr);
        DumpImportModulesTable(pE32Exe, pExeHdr);
        DumpImportProceduresTable(pE32Exe, pExeHdr);
        DumpResidentNameTable(pE32Exe, pExeHdr);
        DumpNonResidentNameTable(pE32Exe, pExeHdr);
        DumpEntryTable(pE32Exe, pExeHdr);
        DumpFixupPageTable(pE32Exe, pExeHdr);
        DumpFixupRecordTable(pE32Exe, pExeHdr);
        }
    else if(argc<2)
        {
        printf("Incomplete commandline specified!\n");
        printf("Syntax: LX2DUMP filename.ext [-Dump]\n");
        }
    else if(pFile==0)
        {
        printf("File \"%s\" could not be opened!\n", argv[1]); 
        printf("Syntax: LX2DUMP filename.ext [-Dump]\n");
        }
    fclose(pFile);
    return(0);
}


