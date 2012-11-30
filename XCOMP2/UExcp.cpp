/***********************************************************************\
 *                             UEXCP class                             *
 *              Copyright (C) by Stangl Roman, 1999, 2002              *
 * This Code may be freely distributed, provided the Copyright isn't   *
 * removed, under the conditions indicated in the documentation.       *
 *                                                                     *
 * UExcp.cpp    Generic exception handler.                             *
 *                                                                     *
\***********************************************************************/

#pragma strings(readonly)

#ifdef  __WINDOWS__
#define __WIN32__
#endif  /* __WINDOWS__ */

#include    <iostream.h>
#include    <iomanip.h>

#include    "UExcp.hpp"

            UEXCP::UEXCP(char *pcApplicationPrefix, int iError, char *pcMsg, int iLine)
{
                                        /* Clear streambuffer, as writing the stream
                                           does not write a terminating \0 */
    memset(acError, '\0', sizeof(acError));
    pstreambufError=new streambuf((char *)acError, sizeof(acError));
    piostreamError=new iostream(pstreambufError);
    *piostreamError << pcMsg;
    if(pcApplicationPrefix!=0)
        this->pcApplicationPrefix=pcApplicationPrefix;
    else
        this->pcApplicationPrefix="UEXCP";
    this->iError=iError+UEXCP_ERROR;
    this->iLine=iLine;
    delete pstreambufError;
}

            UEXCP::~UEXCP(void)
{
    delete piostreamError;
}

UEXCP      &UEXCP::operator<<(char *pcMsg)
{
    *piostreamError << pcMsg;
    return(*this);
}

UEXCP      &UEXCP::operator<<(int iMsg)
{
    *piostreamError << iMsg;
    return(*this);
}

UEXCP      &UEXCP::print(void)
{
    cout << pcApplicationPrefix; 
    if(iError>99)
        cout << iError << ": ";
    else if(iError>9)
        cout << "0" << iError << ": ";
    else
        cout << "00" << iError << ": ";
                                        /* Unfortunately I don't know how to let
                                           cout consume from the streambuffer directly */
    cout << acError;
    if(iLine!=0)
        cout << " at Line " << iLine;
    cout << "." << endl;
    return(*this);
}

char       *UEXCP::text(void)
{
    return(acError);
}

int         UEXCP::getError(void)
{
    return(iError);
}

