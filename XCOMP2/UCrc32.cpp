/***********************************************************************\
 *                            UCRC32 class                             *
 *              Copyright (C) by Stangl Roman, 2001, 2002              *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
 * UCrc32.cpp   CRC32 calculation.                                     *
 *                                                                     *
\***********************************************************************/

#pragma strings(readonly)

#ifdef  __WINDOWS__
#define     __WIN32__
#endif  /* __WINDOWS__ */

#ifdef  __OS2__
#define     INCL_DOSPROFILE
#include    <os2.h>
#endif  /* __OS2__ */

#ifdef  __WIN32__
#include    <windows.h>
#endif  /* __WIN32__ */  

#include    "UCrc32.hpp"

#include    <memory.h>

            UCRC32::UCRC32(void)
{
                                        /* Polynomial based on algorithm descriptions
                                           found on the Internet */
    ULONG           ulPolynomial=0x04C11DB7;
    int             iIndexLookup;
    int             iIndex;

    init();
                                        /* Initialize CRC table */
    for(iIndexLookup=0; iIndexLookup<UCRC32_TABLE_SIZE; iIndexLookup++)
        {
        aulCrc32LookupTable[iIndexLookup]=reflectLookupTable(iIndexLookup, 8)<<24;
        for(iIndex=0; iIndex<8; iIndex++)
            {
                aulCrc32LookupTable[iIndexLookup]=(aulCrc32LookupTable[iIndexLookup]<<1)^
                    (aulCrc32LookupTable[iIndexLookup] & 0x80000000 ? ulPolynomial : 0);
            }
        aulCrc32LookupTable[iIndexLookup]=
            reflectLookupTable(aulCrc32LookupTable[iIndexLookup], 32);
        }
}

            UCRC32::~UCRC32(void)
{
}

                                        /* CRC32 initialization. Begins a new CRC32
                                           calculation cycle */
UCRC32     &UCRC32::init(void)
{
                                        /* Initialize */
    ulCRC=0xFFFFFFFF;
    memset(aucCRC, '\0', sizeof(aucCRC));
    ulFinalDone=FALSE;
    return(*this);
}

                                        /* Calculate the CRC32 over a buffer of the
                                           specified size in bytes. When continuing
                                           with a new (filled) buffer, the calculation
                                           can continue with the CRC calculation of
                                           the previous buffer */
UCRC32     &UCRC32::update(BYTE *pbInput, ULONG ulInputLength)
{
                                        /* Start with what was calculated from the previous
                                           buffer */
    UCHAR          *pucData;
    int             iIndex;

                                        /* Just be sure */
    if(ulInputLength==0)
        return(*this);
                                        /* Perform the algorithm for every byte
                                           in the buffer using the lookup table */
    for(iIndex=0, pucData=(UCHAR *)pbInput; iIndex<ulInputLength; iIndex++, pucData++)
        {
        ulCRC=(ulCRC>>8)^aulCrc32LookupTable[(ulCRC & 0xFF)^*pucData];
        }
    return(*this);
}

                                        /* CRC32 finalization. Return CRC as ULONG or 
                                           ASCII C-string */
BOOL        UCRC32::final(ULONG *pulCrc32)
{
                                        /* If CRC32 calculation has not been ended XOR with 
                                           beginning value and end calculation */
    if(ulFinalDone==FALSE)
        ulCRC^=0xFFFFFFFF;
    ulFinalDone=TRUE;
    if(pulCrc32!=0)
        {
        *pulCrc32=ulCRC;
        return(TRUE);
        }
    else
        return(FALSE);
}

BOOL        UCRC32::final(UCHAR aucCRC[UCRC32_CRC_LENGTH_STRG], ULONG ulCrcSize)
{
    ULONG           ulIndex;
    BYTE           *pbCRC;
    UCHAR           ucChar;

                                        /* If CRC32 calculation has not been ended XOR with 
                                           beginning value and end calculation */
    final(0);
                                        /* Check length of result buffer */
    if(ulCrcSize<UCRC32_CRC_LENGTH_STRG)
        return(FALSE);
                                        /* Store digest also as ASCII C-string */
    pbCRC=(BYTE *)&ulCRC;
    for(ulIndex=0; ulIndex<UCRC32_CRC_LENGTH; ulIndex++)
        {
                                        /* On little Endian machines (e.g. Intel)
                                           we have to loop from LSB to MSB */
#ifdef  _M_I386
        ucChar=(pbCRC[UCRC32_CRC_LENGTH-1-ulIndex] & 0xF0)>>4;
#else
        ucChar=(pbCRC[ulIndex] & 0xF0)>>4;
#endif           
        this->aucCRC[ulIndex*2]=(ucChar>0x09 ? ucChar-0x0A+'A' : ucChar+'0');
#ifdef  _M_I386
        ucChar=(pbCRC[UCRC32_CRC_LENGTH-1-ulIndex] & 0x0F);
#else
        ucChar=(pbCRC[ulIndex] & 0x0F);
#endif           
        this->aucCRC[ulIndex*2+1]=(ucChar>0x09 ? ucChar-0x0A+'A' : ucChar+'0');
        }
                                        /* Return result */
    memcpy(aucCRC, this->aucCRC, UCRC32_CRC_LENGTH_STRG);
    return(TRUE);
}

                                        /* Convert a CRC32 checksum in string format
                                           into an ULONG */
BOOL        UCRC32::convert2ULong(UCHAR aucCrc[UCRC32_CRC_LENGTH_STRG], ULONG *pulCrc)
{
    ULONG           ulIndex;
    UCHAR          *pucCRC=(UCHAR *)pulCrc;
    UCHAR           ucChar;

    *pulCrc=0;
    for(ulIndex=0; ulIndex<UCRC32_CRC_LENGTH_STRG; ulIndex++)
        {
                                        /* On little Endian machines (e.g. Intel)
                                           we have to loop from LSB to MSB */
#ifdef  _M_I386
        ucChar=aucCrc[UCRC32_CRC_LENGTH_STRG-1-1-ulIndex];
#else
        ucChar=aucCrc[ulIndex];
#endif           
        ucChar=(ucChar>'9' ? ucChar-'A'+0x0A : ucChar-'0') & 0x0F;
#ifdef  _M_I386
        pucCRC[ulIndex>>1]|=(ulIndex % 2 ? (ucChar<<4) : (ucChar));
#else
        pucCRC[ulIndex>>1]|=(ulIndex % 2 ? (ucChar) : (ucChar<<4));
#endif           
        }
    return(TRUE);
}

                                        /* Convert a CRC32 checksum in ULONG format
                                           into a string  */
BOOL        UCRC32::convert2String(ULONG ulCrc, UCHAR aucCrc[UCRC32_CRC_LENGTH_STRG])
{
    ULONG           ulIndex;
    BYTE           *pbCrc;
    UCHAR           ucChar;

    memset(aucCrc, '\0', UCRC32_CRC_LENGTH_STRG);
                                        /* Store digest also as ASCII C-string */
    pbCrc=(BYTE *)&ulCrc;
    for(ulIndex=0; ulIndex<UCRC32_CRC_LENGTH; ulIndex++)
        {
                                        /* On little Endian machines (e.g. Intel)
                                           we have to loop from LSB to MSB */
#ifdef  _M_I386
        ucChar=(pbCrc[UCRC32_CRC_LENGTH-1-ulIndex] & 0xF0)>>4;
#else
        ucChar=(pbCrc[ulIndex] & 0xF0)>>4;
#endif           
        aucCrc[ulIndex*2]=(ucChar>0x09 ? ucChar-0x0A+'A' : ucChar+'0');
#ifdef  _M_I386
        ucChar=(pbCrc[UCRC32_CRC_LENGTH-1-ulIndex] & 0x0F);
#else
        ucChar=(pbCrc[ulIndex] & 0x0F);
#endif           
        aucCrc[ulIndex*2+1]=(ucChar>0x09 ? ucChar-0x0A+'A' : ucChar+'0');
        }
    return(TRUE);
}

                                        /* Used only during constructor to prefill
                                           lookup table */
ULONG       UCRC32::reflectLookupTable(ULONG ulToken, ULONG ulTokenBits)
{
    int             iIndex;
    ULONG           ulValue=0;

                                        /* Swap bits (MSB with LSB, ...) */
    for(iIndex=0; iIndex<ulTokenBits; iIndex++)
        {
        ulValue<<=1;
        if(ulToken & 0x00000001)
            ulValue|=0x00000001;
        ulToken>>=1;
        }
    return(ulValue);
}

