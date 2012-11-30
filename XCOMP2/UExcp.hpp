#ifndef _UEXCP_HPP_
#define _UEXCP_HPP_

/***********************************************************************\
 *                             UEXCP class                             *
 *              Copyright (C) by Stangl Roman, 1999, 2002              *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
 * UExcp.hpp    Generic exception handler.                             *
 *                                                                     *
\***********************************************************************/

                                        /* Add 100 to error codes, which can only be resolved by
                                           raising an exception, so that 1 ... 99 is available for
                                           recoverable errors */
#define         UEXCP_ERROR             100

class           UEXCP
{
public:
                UEXCP(char *pcAppliationPrefix, int iError, char *pcMsg, int iLine=0);
               ~UEXCP(void);
    UEXCP      &operator<<(char *pcMsg);
    UEXCP      &operator<<(int iMsg);
    UEXCP      &print(void);
    char       *text(void);
    int         getError(void);
private:
    char        acError[256];
    streambuf  *pstreambufError;
    iostream   *piostreamError;
    char       *pcApplicationPrefix;
    int         iError;
    int         iLine;
};

#endif  /* _UEXCP_HPP_ */
