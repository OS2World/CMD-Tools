#ifndef _UCRC32_HPP_
#define _UCRC32_HPP_

/***********************************************************************\
 *                            UCRC32 class                             *
 *              Copyright (C) by Stangl Roman, 1999, 2002              *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
 * UCrc32.hpp   CRC32 calculation.                                     *
 *                                                                     *
 *********************************************************************** 
 *                                                                     *
 * The following pairs of test messages and resulting digests can be   *
 * used to validate the implementation:                                *
 * "a"                            ==> E8B7BE43                         *
 * "abc"                          ==> 352441C2                         *
 * "message digest"               ==> 20159D7F                         *
 * "abcdefghijklmnopqrstuvwxyz"   ==> 4C2750BD                         *
 * "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789     *
 *                                ==> 1FC2E6D2                         *
 * "1234567890123456789012345678901234567890123456789012345678901234 \ *
 *  5678901234567890"             ==> 7CA94A72                         *
 *                                                                     *
\***********************************************************************/

#define         UCRC32_CRC_LENGTH       (sizeof(ULONG))
#define         UCRC32_CRC_LENGTH_STRG  (UCRC32_CRC_LENGTH*2+1)

class           UCRC32
{
public:
                UCRC32(void);
               ~UCRC32(void);
                                        /* CRC32 initialization. Begins a new CRC32
                                           calculation cycle */
    UCRC32     &init(void);
                                        /* Calculate CRC over a single buffer or in addition
                                           to the CRC calculated from the previous buffer */
    UCRC32     &update(BYTE *pbInput, ULONG ulInputLength);
                                        /* CRC32 finalization. Return CRC as ULONG or 
                                           ASCII C-string */
    BOOL        final(ULONG *pulCrc32);
    BOOL        final(UCHAR aucCrc[UCRC32_CRC_LENGTH_STRG], ULONG ulCrcSize);
                                        /* Convert a CRC32 C-string into an ULONG */
    static BOOL convert2ULong(UCHAR aucCrc[UCRC32_CRC_LENGTH_STRG], ULONG *pulCrc);
                                        /* Convert a CRC32 ULONG into a C-string into */
    static BOOL convert2String(ULONG ulCrc, UCHAR aucCrc[UCRC32_CRC_LENGTH_STRG]);
private:
                                        /* Used only during constructor */
    ULONG       reflectLookupTable(ULONG ulToken, ULONG ulTokenBits);
#define         UCRC32_TABLE_SIZE       256
                                        /* Lookup table array */
    ULONG       aulCrc32LookupTable[UCRC32_TABLE_SIZE];
                                        /* Cleared at init(), set during final() */
    ULONG       ulFinalDone;
                                        /* CRC32 value */
    ULONG       ulCRC;
                                        /* CRC32 value as a ASCII C-string */
    UCHAR       aucCRC[UCRC32_CRC_LENGTH_STRG];
};

#endif  /* _UCRC32_HPP_ */
