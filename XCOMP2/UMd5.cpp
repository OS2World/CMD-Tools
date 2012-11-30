/***********************************************************************\
 *                             UMD5 class                              *
 *              Copyright (C) by Stangl Roman, 2001, 2002              *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
 * UMD5.cpp     MD5 calculation (based on RFC 1321 specification)      *
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

#include    "UMD5.hpp"
#include    <memory.h>

            UMD5::UMD5(void)
{
    init();
}

            UMD5::~UMD5(void)
{
}

                                        /* MD5 initialization. Begins an MD5 operation, 
                                           writing a new context */
UMD5       &UMD5::init(void)
{
                                        /* Initialize MD5 context */
    memset(&md5Context, 0, sizeof(md5Context));
    md5Context.aulCount[0]=md5Context.aulCount[1]=0;
                                        /* Load magic initialization constants */
    md5Context.aulState[0]=0x67452301; 
    md5Context.aulState[1]=0xEFCDAB89;
    md5Context.aulState[2]=0x98BADCFE;
    md5Context.aulState[3]=0x10325476;
                                        /* Clear buffer for binary digest */
    memset(&abDigest, 0, sizeof(abDigest));
                                        /* Clear buffer for digest as an ASCII C-string */
    memset(&aucDigest, '\0', sizeof(aucDigest));
                                        /* Initialize padding */
    memset(&aucPadding, '\0', sizeof(aucPadding));
    aucPadding[0]=0x80;
    return(*this);
}

                                        /* MD5 block update operation. Continues an MD5 
                                           message-digest operation, processing another 
                                           message block, and updating the context */
UMD5       &UMD5::update(BYTE *pbInput, ULONG ulInputLen)
{
    ULONG           ulI,
                    ulIndex, 
                    ulPartLen;

                                        /* Compute number of bytes mod 64 of previous
                                           call to update() (though this may be the first
                                           one) */
    ulIndex=(ULONG)((md5Context.aulCount[0]>>3) & 0x3F);
                                        /* Update number of bits (1 byte is 8 bits thus
                                           n<<3 gives the size of n bytes in bits) of the
                                           message which we're going to transform now */
    if((md5Context.aulCount[0]+=((ULONG)ulInputLen<<3))<((ULONG)ulInputLen<<3))
        md5Context.aulCount[1]++;
                                        /* If 2**32 bytes are passed, as we count bits,
                                           only the bit count of 2**29 fits into
                                           aulCount[0], so we have to save the overflow
                                           in aulCount[1] */ 
    md5Context.aulCount[1]+=((ULONG)ulInputLen>>29);
    ulPartLen=UMD5_PADDING_LENGTH-ulIndex;
                                        /* Transform as many times as possible, that is
                                           if the message is larger than the length of
                                           MD5_CTX.abBuffer (64), then transform the message
                                           (64-byte) part by part */
    if(ulInputLen>=ulPartLen)
        {
        memcpy(&md5Context.abBuffer[ulIndex], pbInput, ulPartLen);
        transform(md5Context.aulState, (BYTE *)md5Context.abBuffer);

        for(ulI=ulPartLen; ulI+(UMD5_PADDING_LENGTH-1)<ulInputLen; ulI+=UMD5_PADDING_LENGTH)
            transform(md5Context.aulState, &pbInput[ulI]);
        ulIndex = 0;
        }
    else
        ulI=0;
                                        /* Fill buffer by copying remaining input that
                                           has not yet been transformed (because another
                                           call to update() may supply more input data) */
    memcpy(&md5Context.abBuffer[ulIndex], &pbInput[ulI], ulInputLen-ulI);
    return(*this);
}

                                        /* MD5 finalization. Ends an MD5 message-digest 
                                           operation, writing the the message digest and 
                                           zeroizing the context */
BOOL        UMD5::final(void)
{
    BYTE            abBits[8];          /* 64-bit representation of MD5_CTX.aulCount[] */
    ULONG           ulIndex,
                    ulPadLen;
    UCHAR           ucChar;

                                        /* Save number of bits (as 64-bit word) */
    encode((BYTE *)abBits, md5Context.aulCount, 8);
                                        /* Pad out to 56 mod 64 (the remaining 8 bytes
                                           will be filled in by MD5_CTX.aulCount[]) */
    ulIndex=(ULONG)((md5Context.aulCount[0]>>3) & 0x3f);
    ulPadLen=(ulIndex<56) ? (56-ulIndex) : (120-ulIndex);
                                        /* Append padding to current part of message text 
                                           in MD5_CTX.abBuffer (and transform it to be
                                           included in message digest). If after the padding
                                           no space is left for the 64-bit representation of
                                           MD5_CTX.aulCount[], then another part will be
                                           added, as MD5 only operates on complete 64-byte 
                                           parts */ 
    update((BYTE *)aucPadding, ulPadLen);
                                        /* Append length (64-bit representation of
                                           MD5_CTX.aulCount[]), after the padding (and
                                           transform it to be included in message digest) */
    update(abBits, 8);
                                        /* Store state in digest */
    encode(abDigest, md5Context.aulState, UMD5_DIGEST_LENGTH);
                                        /* Store digest also as ASCII C-string */
    for(ulIndex=0; ulIndex<UMD5_DIGEST_LENGTH; ulIndex++)
        {
        ucChar=(abDigest[ulIndex] & 0xF0)>>4;
        aucDigest[ulIndex*2]=(ucChar>0x09 ? ucChar-0x0A+'A' : ucChar+'0');
        ucChar=(abDigest[ulIndex] & 0x0F);
        aucDigest[ulIndex*2+1]=(ucChar>0x09 ? ucChar-0x0A+'A' : ucChar+'0');
        }
                                        /* Zeroize sensitive information */
    memset(&md5Context, 0, sizeof(md5Context));
    return(TRUE);
}

BOOL        UMD5::final(ULONG ulDigestSize, BYTE abDigest[UMD5_DIGEST_LENGTH])
{
                                        /* Check length of result buffer */
    if(ulDigestSize!=UMD5_DIGEST_LENGTH)
        return(FALSE);
                                        /* MD5 finalization */
    final();
                                        /* Return result */
    memcpy(abDigest, this->abDigest, UMD5_DIGEST_LENGTH);
    return(TRUE);
}

BOOL        UMD5::final(UCHAR aucDigest[UMD5_DIGEST_LENGTH_STRG], ULONG ulDigestSize)
{
                                        /* Check length of result buffer */
    if(ulDigestSize<UMD5_DIGEST_LENGTH_STRG)
        return(FALSE);
                                        /* MD5 finalization */
    final();
                                        /* Return result */
    memcpy(aucDigest, this->aucDigest, UMD5_DIGEST_LENGTH_STRG);
    return(TRUE);
}

BOOL        UMD5::digest(ULONG ulDigestSize, BYTE abDigest[UMD5_DIGEST_LENGTH])
{
                                        /* Check length of result buffer */
    if((ulDigestSize!=UMD5_DIGEST_LENGTH) || (this->aucDigest[0]=='\0'))
        return(FALSE);
                                        /* Return result */
    memcpy(abDigest, this->abDigest, UMD5_DIGEST_LENGTH);
    return(TRUE);
}

BOOL        UMD5::digest(UCHAR aucDigest[UMD5_DIGEST_LENGTH_STRG], ULONG ulDigestSize)
{
                                        /* Check length of result buffer */
    if((ulDigestSize<UMD5_DIGEST_LENGTH_STRG) || (this->aucDigest[0]=='\0'))
        return(FALSE);
                                        /* Return result */
    memcpy(aucDigest, this->aucDigest, UMD5_DIGEST_LENGTH_STRG);
    return(TRUE);
}

                                        /* Convert a MD5 digest in string format into 
                                           halfbyte notation */
BOOL        UMD5::convert2ByteArray(UCHAR aucDigest[UMD5_DIGEST_LENGTH_STRG], BYTE *pbDigest)
{
    ULONG           ulIndex;
    UCHAR           ucChar;

    memset(pbDigest, 0, UMD5_DIGEST_LENGTH);
    for(ulIndex=0; ulIndex<UMD5_DIGEST_LENGTH_STRG; ulIndex++)
        {
        ucChar=aucDigest[ulIndex];
        ucChar=(ucChar>'9' ? ucChar-'A'+0x0A : ucChar-'0') & 0x0F;
        pbDigest[ulIndex>>1]|=(ulIndex % 2 ? (ucChar) : (ucChar<<4));
        }
    return(TRUE);
}

                                        /* Convert a MD5 digest in halfbyte notation into
                                           string format */
BOOL        UMD5::convert2String(BYTE *pbDigest, UCHAR aucDigest[UMD5_DIGEST_LENGTH_STRG])
{
    ULONG           ulIndex;
    UCHAR           ucChar;

    memset(aucDigest, '\0', UMD5_DIGEST_LENGTH_STRG);
                                        /* Store digest also as ASCII C-string */
    for(ulIndex=0; ulIndex<UMD5_DIGEST_LENGTH; ulIndex++)
        {
        ucChar=(pbDigest[ulIndex] & 0xF0)>>4;
        aucDigest[ulIndex*2]=(ucChar>0x09 ? ucChar-0x0A+'A' : ucChar+'0');
        ucChar=(pbDigest[ulIndex] & 0x0F);
        aucDigest[ulIndex*2+1]=(ucChar>0x09 ? ucChar-0x0A+'A' : ucChar+'0');
        }
    return(TRUE);
}

                                        /* MD5 basic transformation. Transforms state 
                                           based on block, that is the state is updated
                                           from the part of the message in abBlock[]
                                           (which in general is the input message (64 byte)
                                           part by part, but finally also the padding and
                                           counter of the number of bits transformed so far) */
void        UMD5::transform(ULONG aulState[4], BYTE abBlock[UMD5_PADDING_LENGTH])
{
    ULONG           ulA=aulState[0], 
                    ulB=aulState[1], 
                    ulC=aulState[2],
                    ulD=aulState[3],
                    aulX[UMD5_DIGEST_LENGTH];

    decode(aulX, abBlock, UMD5_PADDING_LENGTH);
                                        /* Round 1 */
    FF(ulA, ulB, ulC, ulD, aulX[ 0], S11, 0xd76aa478); /* 1 */
    FF(ulD, ulA, ulB, ulC, aulX[ 1], S12, 0xe8c7b756); /* 2 */
    FF(ulC, ulD, ulA, ulB, aulX[ 2], S13, 0x242070db); /* 3 */
    FF(ulB, ulC, ulD, ulA, aulX[ 3], S14, 0xc1bdceee); /* 4 */
    FF(ulA, ulB, ulC, ulD, aulX[ 4], S11, 0xf57c0faf); /* 5 */
    FF(ulD, ulA, ulB, ulC, aulX[ 5], S12, 0x4787c62a); /* 6 */
    FF(ulC, ulD, ulA, ulB, aulX[ 6], S13, 0xa8304613); /* 7 */
    FF(ulB, ulC, ulD, ulA, aulX[ 7], S14, 0xfd469501); /* 8 */
    FF(ulA, ulB, ulC, ulD, aulX[ 8], S11, 0x698098d8); /* 9 */
    FF(ulD, ulA, ulB, ulC, aulX[ 9], S12, 0x8b44f7af); /* 10 */
    FF(ulC, ulD, ulA, ulB, aulX[10], S13, 0xffff5bb1); /* 11 */
    FF(ulB, ulC, ulD, ulA, aulX[11], S14, 0x895cd7be); /* 12 */
    FF(ulA, ulB, ulC, ulD, aulX[12], S11, 0x6b901122); /* 13 */
    FF(ulD, ulA, ulB, ulC, aulX[13], S12, 0xfd987193); /* 14 */
    FF(ulC, ulD, ulA, ulB, aulX[14], S13, 0xa679438e); /* 15 */
    FF(ulB, ulC, ulD, ulA, aulX[15], S14, 0x49b40821); /* 16 */

                                        /* Round 2 */
    GG(ulA, ulB, ulC, ulD, aulX[ 1], S21, 0xf61e2562); /* 17 */
    GG(ulD, ulA, ulB, ulC, aulX[ 6], S22, 0xc040b340); /* 18 */
    GG(ulC, ulD, ulA, ulB, aulX[11], S23, 0x265e5a51); /* 19 */
    GG(ulB, ulC, ulD, ulA, aulX[ 0], S24, 0xe9b6c7aa); /* 20 */
    GG(ulA, ulB, ulC, ulD, aulX[ 5], S21, 0xd62f105d); /* 21 */
    GG(ulD, ulA, ulB, ulC, aulX[10], S22,  0x2441453); /* 22 */
    GG(ulC, ulD, ulA, ulB, aulX[15], S23, 0xd8a1e681); /* 23 */
    GG(ulB, ulC, ulD, ulA, aulX[ 4], S24, 0xe7d3fbc8); /* 24 */
    GG(ulA, ulB, ulC, ulD, aulX[ 9], S21, 0x21e1cde6); /* 25 */
    GG(ulD, ulA, ulB, ulC, aulX[14], S22, 0xc33707d6); /* 26 */
    GG(ulC, ulD, ulA, ulB, aulX[ 3], S23, 0xf4d50d87); /* 27 */
    GG(ulB, ulC, ulD, ulA, aulX[ 8], S24, 0x455a14ed); /* 28 */
    GG(ulA, ulB, ulC, ulD, aulX[13], S21, 0xa9e3e905); /* 29 */
    GG(ulD, ulA, ulB, ulC, aulX[ 2], S22, 0xfcefa3f8); /* 30 */
    GG(ulC, ulD, ulA, ulB, aulX[ 7], S23, 0x676f02d9); /* 31 */
    GG(ulB, ulC, ulD, ulA, aulX[12], S24, 0x8d2a4c8a); /* 32 */

                                        /* Round 3 */
    HH(ulA, ulB, ulC, ulD, aulX[ 5], S31, 0xfffa3942); /* 33 */
    HH(ulD, ulA, ulB, ulC, aulX[ 8], S32, 0x8771f681); /* 34 */
    HH(ulC, ulD, ulA, ulB, aulX[11], S33, 0x6d9d6122); /* 35 */
    HH(ulB, ulC, ulD, ulA, aulX[14], S34, 0xfde5380c); /* 36 */
    HH(ulA, ulB, ulC, ulD, aulX[ 1], S31, 0xa4beea44); /* 37 */
    HH(ulD, ulA, ulB, ulC, aulX[ 4], S32, 0x4bdecfa9); /* 38 */
    HH(ulC, ulD, ulA, ulB, aulX[ 7], S33, 0xf6bb4b60); /* 39 */
    HH(ulB, ulC, ulD, ulA, aulX[10], S34, 0xbebfbc70); /* 40 */
    HH(ulA, ulB, ulC, ulD, aulX[13], S31, 0x289b7ec6); /* 41 */
    HH(ulD, ulA, ulB, ulC, aulX[ 0], S32, 0xeaa127fa); /* 42 */
    HH(ulC, ulD, ulA, ulB, aulX[ 3], S33, 0xd4ef3085); /* 43 */
    HH(ulB, ulC, ulD, ulA, aulX[ 6], S34,  0x4881d05); /* 44 */
    HH(ulA, ulB, ulC, ulD, aulX[ 9], S31, 0xd9d4d039); /* 45 */
    HH(ulD, ulA, ulB, ulC, aulX[12], S32, 0xe6db99e5); /* 46 */
    HH(ulC, ulD, ulA, ulB, aulX[15], S33, 0x1fa27cf8); /* 47 */
    HH(ulB, ulC, ulD, ulA, aulX[ 2], S34, 0xc4ac5665); /* 48 */

                                        /* Round 4 */
    II(ulA, ulB, ulC, ulD, aulX[ 0], S41, 0xf4292244); /* 49 */
    II(ulD, ulA, ulB, ulC, aulX[ 7], S42, 0x432aff97); /* 50 */
    II(ulC, ulD, ulA, ulB, aulX[14], S43, 0xab9423a7); /* 51 */
    II(ulB, ulC, ulD, ulA, aulX[ 5], S44, 0xfc93a039); /* 52 */
    II(ulA, ulB, ulC, ulD, aulX[12], S41, 0x655b59c3); /* 53 */
    II(ulD, ulA, ulB, ulC, aulX[ 3], S42, 0x8f0ccc92); /* 54 */
    II(ulC, ulD, ulA, ulB, aulX[10], S43, 0xffeff47d); /* 55 */
    II(ulB, ulC, ulD, ulA, aulX[ 1], S44, 0x85845dd1); /* 56 */
    II(ulA, ulB, ulC, ulD, aulX[ 8], S41, 0x6fa87e4f); /* 57 */
    II(ulD, ulA, ulB, ulC, aulX[15], S42, 0xfe2ce6e0); /* 58 */
    II(ulC, ulD, ulA, ulB, aulX[ 6], S43, 0xa3014314); /* 59 */
    II(ulB, ulC, ulD, ulA, aulX[13], S44, 0x4e0811a1); /* 60 */
    II(ulA, ulB, ulC, ulD, aulX[ 4], S41, 0xf7537e82); /* 61 */
    II(ulD, ulA, ulB, ulC, aulX[11], S42, 0xbd3af235); /* 62 */
    II(ulC, ulD, ulA, ulB, aulX[ 2], S43, 0x2ad7d2bb); /* 63 */
    II(ulB, ulC, ulD, ulA, aulX[ 9], S44, 0xeb86d391); /* 64 */

    aulState[0]+=ulA;
    aulState[1]+=ulB;
    aulState[2]+=ulC;
    aulState[3]+=ulD;
                                        /* Zeroize sensitive information */
    memset(aulX, 0, sizeof(aulX));
}

                                        /* Encodes input into output. Assumes 
                                           length is a multiple of 4 */
void        UMD5::encode(BYTE *pbOutput, ULONG *pulInput, unsigned int uiLen)
{
    unsigned int    uiI, 
                    uiJ;

    for(uiI=0, uiJ=0; uiJ<uiLen; uiI++, uiJ+=4)
        { 
                                        /* On little Endian machines (e.g. Intel)
                                           this could be written as
                                           *(ULONG *)&pbOutput[uiJ]=pulInput[uiI];
                                           too */
#ifdef  _M_I386
        *(ULONG *)&pbOutput[uiJ]=pulInput[uiI];
#else
        pbOutput[uiJ]=(BYTE)(pulInput[uiI] & 0xff);
        pbOutput[uiJ+1]=(BYTE)((pulInput[uiI]>>8) & 0xff);
        pbOutput[uiJ+2]=(BYTE)((pulInput[uiI]>>16) & 0xff);
        pbOutput[uiJ+3]=(BYTE)((pulInput[uiI]>>24) & 0xff);
#endif  /* _M_i386 */
        }
}

                                        /* Decodes input into output. Assumes 
                                           length is a multiple of 4 */
void        UMD5::decode(ULONG *pulOutput, BYTE *pbInput, unsigned int uiLen)
{
    unsigned int    uiI, 
                    uiJ;

    for(uiI=0, uiJ=0; uiJ<uiLen; uiI++, uiJ+=4)
        {
                                        /* On little Endian machines (e.g. Intel)
                                           this could be written as
                                           pulOutput[uiI]=*(ULONG *)&pbInput[uiJ];
                                           too */
#ifdef  _M_I386
        pulOutput[uiI]=*(ULONG *)&pbInput[uiJ];
#else
        pulOutput[uiI]=((ULONG)pbInput[uiJ]) | (((ULONG)pbInput[uiJ+1]) << 8) |
            (((ULONG)pbInput[uiJ+2]) << 16) | (((ULONG)pbInput[uiJ+3]) << 24);
#endif  /* _M_i386 */
        }
}

