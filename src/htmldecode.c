/*
 *--------------------------------------------------------------------------
 * Copyright (c) 2005 Eolas Technologies Inc.
 * All rights reserved.
 *
 * This Open Source project was made possible through the financial support
 * of Eolas Technologies Inc.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <ORGANIZATION> nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
static char const rcsid[] = "@(#) $Id: htmldecode.c,v 1.1 2006/07/01 07:33:22 danielk1977 Exp $";


#include "html.h"

/*
 *---------------------------------------------------------------------------
 *
 * readUriEncodedByte --
 *
 *     This function is part of the implementation of the 
 *
 * Results:
 *     Returns a string containing the versions of the *.c files used
 *     to build the library
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
unsigned char readUriEncodedByte(unsigned char **pzIn){
    unsigned char *zIn = *pzIn;
    unsigned char c;

    do {
        c = *(zIn++); 
    } while (c == ' ' || c == '\n' || c == '\t');

    if (c == '%') {
        char c1 = *(zIn++);
        char c2 = *(zIn++);

        if (c1 >= '0' && c1 <= '9')      c = (c1 - '0');
        else if (c1 >= 'A' && c1 <= 'F') c = (c1 - 'A');
        else if (c1 >= 'a' && c1 <= 'f') c = (c1 - 'a');
        else return 0;
        c = c << 4;

        if (c2 >= '0' && c2 <= '9')      c += (c2 - '0');
        else if (c2 >= 'A' && c2 <= 'F') c += (c2 - 'A' + 10);
        else if (c2 >= 'a' && c2 <= 'f') c += (c2 - 'a' + 10);
        else return 0;
    }

    *pzIn = zIn;

    return c;
}

int read6bits(unsigned char **pzIn){
#if 0
    char const z64[] = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
#endif

    int map[256] = { 
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,  /* 0  */
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,  /* 16 */
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, 62, -1, -1, -1, 63,  /* 32 */
    52, 53, 54, 55, 56, 57, 58, 59,   60, 61, -1, -1, -1, -1, -1, -1,  /* 48 */
    -1,  0,  1,  2,  3,  4,  5,  6,    7,  8,  9, 10, 11, 12, 13, 14,  /* 64 */
    15, 16, 17, 18, 19, 20, 21, 22,   23, 24, 25, -1, -1, -1, -1, -1,  /* 80 */
    -1, 26, 27, 28, 29, 30, 31, 32,   33, 34, 35, 36, 37, 38, 39, 40,  /* 96 */
    41, 42, 43, 44, 45, 46, 47, 48,   49, 50, 51, -1, -1, -1, -1, -1   /* 112 */

    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,  /* 128 */
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1, -1, -1, -1, -1
    };
    unsigned char c;

    c = readUriEncodedByte(pzIn);
    return map[c];
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlDecode --
 *
 *         ::tkhtml::decode ?-base64? DATA
 *
 *     This command is designed to help scripts process "data:" URIs. It
 *     is completely separate from the html widget. 
 *
 * Results:
 *     Returns the decoded data.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlDecode(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    unsigned char *zOut;
    int jj;

    Tcl_Obj *pData;
    int nData;
    unsigned char *zData;
    int is64 = 0;

    if (objc != 3 && objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "?-base64? DATA");
        return TCL_ERROR;
    }
    pData = objv[objc - 1];
    is64 = (objc == 3);

    zData = (unsigned char *)Tcl_GetStringFromObj(pData, &nData);
    zOut = (unsigned char *)HtmlAlloc(0, nData);
    jj = 0;

    if (is64) {
        while (1) {
            int a = read6bits(&zData);
            int b = read6bits(&zData);
            int c = read6bits(&zData);
            int d = read6bits(&zData);
            int e = 0;
    
            if (a >= 0) e += a << 18;
            if (b >= 0) e += b << 12;
            if (c >= 0) e += c << 6;
            if (d >= 0) e += d;
    
            assert(jj < nData);
            if (b >= 0) zOut[jj++] = (e & 0x00FF0000) >> 16;
            assert(jj < nData);
            if (c >= 0) zOut[jj++] = (e & 0x0000FF00) >> 8;
            assert(jj < nData);
            if (d >= 0) zOut[jj++] = (e & 0x000000FF);
            if (d < 0) break;
        }
    } else {
        unsigned char c;
        while (0 != (c = readUriEncodedByte(&zData))) {
            zOut[jj++] = c;
        }
    }

    Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(zOut, jj));
    HtmlFree(0, zOut);
    return TCL_OK;
}




