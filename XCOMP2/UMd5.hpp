#ifndef _UMD5_HPP_
#define _UMD5_HPP_

/***********************************************************************\
 *                             UMD5 class                              *
 *              Copyright (C) by Stangl Roman, 2001, 2002              *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
 * UMD5.hpp     MD5 calculation (based on RFC 1321 specification)      *
 *              RSA Data Security, Inc. MD5 Message-Digest Algorithm   *
 *                                                                     *
 *********************************************************************** 
 * This implementation is derived from the sample implementation of    *
 * RFC 1321 under the following license:                               *
 *                                                                     *
 * Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All     *
 * rights reserved.                                                    *
 *                                                                     *
 * License to copy and use this software is granted provided that it   *
 * is identified as the "RSA Data Security, Inc. MD5 Message-Digest    *
 * Algorithm" in all material mentioning or referencing this software  *
 * or this function.                                                   *
 *                                                                     *
 * License is also granted to make and use derivative works provided   *
 * that such works are identified as "derived from the RSA Data        *
 * Security, Inc. MD5 Message-Digest Algorithm" in all material        *
 * mentioning or referencing the derived work.                         *
 *                                                                     *
 * RSA Data Security, Inc. makes no representations concerning either  *
 * the merchantability of this software or the suitability of this     *
 * software for any particular purpose. It is provided "as is"         *
 * without express or implied warranty of any kind.                    *
 *                                                                     *
 * These notices must be retained in any copies of any part of this    *
 * documentation and/or software.                                      *
 *                                                                     *
 *********************************************************************** 
 *                                                                     *
 * The following pairs of test messages and resulting digests can be   *
 * used to validate the implementation:                                *
 * ""                             ==> d41d8cd98f00b204e9800998ecf8427e *
 * "a"                            ==> 0cc175b9c0f1b6a831c399e269772661 *
 * "abc"                          ==> 900150983cd24fb0d6963f7d28e17f72 *
 * "message digest"               ==> f96b697d7cb7938d525a2f31aaf161d0 *
 * "abcdefghijklmnopqrstuvwxyz"   ==> c3fcd3d76192e4007dfb496cca67e13b *
 * "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789     *
 *                                ==> d174ab98d277d9f5a5611c2c9f419d9f *
 * "1234567890123456789012345678901234567890123456789012345678901234 \ *
 *  5678901234567890"             ==> 57edf4a22be3c955ac49da2e2107b67a *
 *                                                                     *
\***********************************************************************/

                                        /* MD5 digest size */
#define         UMD5_DIGEST_LENGTH      16
#define         UMD5_DIGEST_LENGTH_STRG (UMD5_DIGEST_LENGTH*2+1)

                                        /* MD5 context */
struct          MD5_CTX
{
    ULONG       aulState[4];            /* State (ABCD) */
                                        /* Number of Bits of message processed. Stored as modulo 
                                           2^64 (LSB first in aulCount[0], MSB in aulCount[1]) */
    ULONG       aulCount[2];            
                                        /* MD5 working area */
#define         UMD5_PADDING_LENGTH     64
                                        /* Input buffer */
    BYTE        abBuffer[UMD5_PADDING_LENGTH];          
};

                                        /* Constants for MD5Transform routine */
#define         S11                     7
#define         S12                     12
#define         S13                     17
#define         S14                     22
#define         S21                     5
#define         S22                     9
#define         S23                     14
#define         S24                     20
#define         S31                     4
#define         S32                     11
#define         S33                     16
#define         S34                     23
#define         S41                     6
#define         S42                     10
#define         S43                     15
#define         S44                     21

                                        /* F, G, H and I are basic MD5 functions. */
#define         F(x, y, z)              (((x) & (y)) | ((~x) & (z)))
#define         G(x, y, z)              (((x) & (z)) | ((y) & (~z)))
#define         H(x, y, z)              ((x) ^ (y) ^ (z))
#define         I(x, y, z)              ((y) ^ ((x) | (~z)))

                                        /* ROTATE_LEFT rotates x left n bits. */
#define         ROTATE_LEFT(x, n)       (((x) << (n)) | ((x) >> (32-(n))))

                                        /* FF, GG, HH, and II transformations for rounds 
                                           1, 2, 3, and 4. Rotation is separate from addition 
                                           to prevent recomputation. */
#define         FF(a, b, c, d, x, s, ac){ \
                                        (a) += F ((b), (c), (d)) + (x) + (ULONG)(ac); \
                                        (a) = ROTATE_LEFT ((a), (s)); \
                                        (a) += (b); \
                                        }
#define         GG(a, b, c, d, x, s, ac){ \
                                        (a) += G ((b), (c), (d)) + (x) + (ULONG)(ac); \
                                        (a) = ROTATE_LEFT ((a), (s)); \
                                        (a) += (b); \
                                        }
#define         HH(a, b, c, d, x, s, ac){ \
                                        (a) += H ((b), (c), (d)) + (x) + (ULONG)(ac); \
                                        (a) = ROTATE_LEFT ((a), (s)); \
                                        (a) += (b); \
                                        }
#define         II(a, b, c, d, x, s, ac){ \
                                        (a) += I ((b), (c), (d)) + (x) + (ULONG)(ac); \
                                        (a) = ROTATE_LEFT ((a), (s)); \
                                        (a) += (b); \
                                        }

class           UMD5
{
public:
                UMD5(void);
               ~UMD5(void);
                                        /* MD5 initialization. Begins an MD5 operation, 
                                           writing a new context */
    UMD5       &init(void);
                                        /* MD5 block update operation. Continues an MD5 
                                           message-digest operation, processing another 
                                           message block, and updating the context */
    UMD5       &update(BYTE *pbInput, ULONG ulInputLen);
                                        /* MD5 finalization. Ends an MD5 message-digest 
                                           operation, writing the the message digest and 
                                           zeroizing the context */
    BOOL        final(void);
    BOOL        final(ULONG ulDigestSize, BYTE abDigest[UMD5_DIGEST_LENGTH]);
    BOOL        final(UCHAR aucDigest[UMD5_DIGEST_LENGTH_STRG], ULONG ulDigestSize);
                                        /* Return last MD5 digest, final() needs to
                                           be called first */
    BOOL        digest(ULONG ulDigestSize, BYTE abDigest[UMD5_DIGEST_LENGTH]);
    BOOL        digest(UCHAR aucDigest[UMD5_DIGEST_LENGTH_STRG], ULONG ulDigestSize);
                                        /* Convert a MD5 C-string into halfbyte notation */
    static BOOL convert2ByteArray(UCHAR aucDigest[UMD5_DIGEST_LENGTH_STRG], BYTE *pbDigest);
                                        /* Convert a MD5 halfbyte notation into a C-string */
    static BOOL convert2String(BYTE *pbDigest, UCHAR aucDigest[UMD5_DIGEST_LENGTH_STRG]);
protected:
                                        /* MD5 basic transformation. Transforms state 
                                           based on block */
    void        transform(ULONG aulState[4], BYTE abBlock[UMD5_PADDING_LENGTH]);
                                        /* Encodes input into output. Assumes length 
                                           is a multiple of 4 */
    void        encode(BYTE *pubOutput, ULONG *pulInput, unsigned int uiLen);
                                        /* Decodes input into output. Assumes length
                                           is a multiple of 4 */
    void        decode(ULONG *pulOutput, BYTE *pbInput, unsigned int uiLen);
private:
                                        /* MD5 context */
    MD5_CTX     md5Context;
                                        /* MD5 digest filled by final() methods */
    BYTE        abDigest[UMD5_DIGEST_LENGTH];
    UCHAR       aucDigest[UMD5_DIGEST_LENGTH_STRG];
                                        /* Preformatted structure for padding */ 
    UCHAR       aucPadding[UMD5_PADDING_LENGTH];
};

#endif  /* _UMD5_HPP_ */
