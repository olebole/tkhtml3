/*
** Routines for processing URLs.
** $Revision: 1.4 $
**
** Copyright (C) 1997,1998 D. Richard Hipp
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Library General Public
** License as published by the Free Software Foundation; either
** version 2 of the License, or (at your option) any later version.
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Library General Public License for more details.
** 
** You should have received a copy of the GNU Library General Public
** License along with this library; if not, write to the
** Free Software Foundation, Inc., 59 Temple Place - Suite 330,
** Boston, MA  02111-1307, USA.
**
** Author contact information:
**   drh@acm.org
**   http://www.hwaci.com/drh/
*/
#include <tk.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "htmlurl.h"

/*
** The input azSeries[] is a sequence of URIs.  This command must
** resolve them all and put the result in the interp->result field
** of the interpreter associated with the HTML widget.  Return 
** TCL_OK on success and TCL_ERROR if there is a failure.
**
** This function can cause the HTML widget to be deleted or changed
** arbitrarily. 
*/
int HtmlCallResolver(
  HtmlWidget *htmlPtr,      /* The widget that is doing the resolving. */
  char **azSeries           /* A list of URIs.  NULL terminated */
){
  int rc = TCL_OK;          /* Return value of this function. */

  HtmlVerifyLock(htmlPtr);
  if( htmlPtr->zResolverCommand && htmlPtr->zResolverCommand[0] ){
    /*
    ** Append the current base URI then the azSeries arguments to the
    ** TCL command specified by the -resolvercommand optoin, then execute
    ** the result.
    **
    ** The -resolvercommand could do nasty things, such as delete
    ** the HTML widget out from under us.  Be prepared for the worst.
    */
    Tcl_DString cmd;
    Tcl_DStringInit(&cmd);
    Tcl_DStringAppend(&cmd, htmlPtr->zResolverCommand, -1);
    if( htmlPtr->zBase && htmlPtr->zBase[0] ){
      Tcl_DStringAppendElement(&cmd, htmlPtr->zBase);
    }
    while( azSeries[0] ){
      Tcl_DStringAppendElement(&cmd, azSeries[0]);
      azSeries++;
    }
    HtmlLock(htmlPtr);
    rc = Tcl_GlobalEval(htmlPtr->interp, Tcl_DStringValue(&cmd));
    Tcl_DStringFree(&cmd);
    if( HtmlUnlock(htmlPtr) ) return TCL_ERROR;
    if( rc!=TCL_OK ){
      Tcl_AddErrorInfo(htmlPtr->interp,
         "\n    (-resolvercommand executed by HTML widget)");
    }
  }else{
    /*
    ** No -resolvercommand has been specified.  Do the default
    ** resolver algorithm. 
    */
  }
  return rc;
}

/*
** Return the length of the protocol identifier string at the beginning
** of the given URL.  Return 0 if there is no protocol identifier string.
**
** Examples:
**
**        Input                          Output
**     ---------------------------       -------
**     http://www.mit.edu                5
**     ftp://sunsite.unc.edu/pub         4
**     mailto:drh@acm.org                7
**     ../tmp/file.html                  0
*/
static int ProtocolLength(char *z){
  int n;
  for(n=0; isalpha(z[n]); n++){ TestPoint(0); }
  if( n>0 && z[n]==':' ){
    n++;
    TestPoint(0);
  }else{
    n = 0;
    TestPoint(0);
  }
  return n;
}

/*
** Return the length of the host identifier string at the beginning
** of the given URL.  Return 0 if there is no host identifier string.
**
** Examples:
**
**        Input                          Output
**     ---------------------------       -------
**     //www.mit.edu:8080/user           18
**     //sunsite.unc.edu/pub             17
**     drh@acm.org                       0
**     ../tmp/file.html                  0
*/
static int HostLength(char *z){
  int n;
  if( z[0]!='/' || z[1]!='/' ){ TestPoint(0); return 0; }
  for(n=2; z[n] && z[n]!='/'; n++){ TestPoint(0); }
  return n;
}

/*
** Given a pathname of the given length, return the length of the
** directory component of that pathname.  The directory component does
** not include the final "/".  The initial "/" is never removed.
**
** Example:
**
**      input  = /home/drh/br/html.h    length=19
**      output = /home/drh/br           length=12
**
** Example 2:
**
**      input  = ../tmp/filex.html      length=16
**      output = ../tmp                 length=6
*/
static int DirectoryLength(char *z, int n){
  while( n>1 && z[n-1]!='/' ){
    n--;
    TestPoint(0);
  }
  if( n>1 && z[n-1]=='/' ){
    n--;
    TestPoint(0);
  }else{
    TestPoint(0);
  }
  return n;
}

/*
** Duplicate a string of length n.
*/
static char *StrNDup(char *z, int n){
  char *zResult = ckalloc( n + 1 );
  if( zResult ){
    memcpy(zResult, z, n);
    zResult[n] = 0;
    TestPoint(0);
  }else{
    UNTESTED;
  }
  return zResult;
}

/*
** Free a string.
*/
static void FreeString(char **pz){
  if( *pz ){
    ckfree(*pz);
    *pz = 0;
    TestPoint(0);
  }
}

/*
** Clear all base URL information from the widget.  This routine
** reclaims memory allocated to hold the base URL and should be called
** prior to widget deletion.
*/
void HtmlClearUrl(HtmlWidget *htmlPtr){
  FreeString(&htmlPtr->zProtocol);
  FreeString(&htmlPtr->zHost);
  FreeString(&htmlPtr->zDir);
  TestPoint(0);
}

/*
** Construct a complete URL from the partial information given.  Fill in
** missing information based on the base URL of the current document.
**
** Space to hold the returned string is obtained from ckalloc().  The
** calling function must release this space when it is done with it.
*/
char *HtmlCompleteUrl(HtmlWidget *htmlPtr, char *zSrc){
  char *zProtocol;
  int nProtocol;
  char *zHost;
  int nHost;
  char *zDir;
  int nDir;
  char *zSep;
  char *zResult;

  nProtocol = ProtocolLength(zSrc);
  if( nProtocol ){
    int go = 0;
    zProtocol = zSrc;
    zSrc += nProtocol;
    if( nProtocol==5 ){
      if( strncmp(zProtocol,"http:",5)==0 || strncmp(zProtocol,"file:",5)==0 ){
        go = 1;
        TestPoint(0);
      }else{
        TestPoint(0);
      }
    }else if( nProtocol==4 && strncmp(zProtocol,"ftp:",4)==0 ){
      go = 1;
      TestPoint(0);
    }else{
      TestPoint(0);
    }
    if( !go ){
      /* If the protocol is anything other than "http:", "file:" or "ftp:",
      ** don't try to convert relative to absolute.  Doing so could mess
      ** up URLs like "mailto:drh@acm.org". */
      TestPoint(0);
      return StrNDup(zProtocol, strlen(zProtocol));
    }
  }else{
    zProtocol = htmlPtr->zProtocol;
    if( zProtocol==0 ){ zProtocol = ""; TestPoint(0); }
    nProtocol = strlen(zProtocol);
    TestPoint(0);
  }
  nHost = HostLength(zSrc);
  if( nHost ){
    zHost = zSrc;
    zSrc += nHost;
    TestPoint(0);
  }else{
    zHost = htmlPtr->zHost;
    if( zHost==0 ){
      if( nProtocol==0 || (nProtocol==5 && strncmp(zProtocol,"file:",5)==0) ){
        zHost = "";
        TestPoint(0);
      }else{
        zHost = "//localhost";
        TestPoint(0);
      }
    }else{
      TestPoint(0);
    }
    nHost = strlen(zHost);
  }
  if( zSrc[0]=='/' ){
    zDir = "";
    zSep = "";
    nDir = 0;
    TestPoint(0);
  }else{
    zDir = htmlPtr->zDir;
    if( zDir==0 ){
      zDir = "";
      zSep = "/";
      TestPoint(0);
    }else{
      zSep = "/";
      TestPoint(0);
    }
    nDir = strlen(zDir);
    while( nDir>0 && zDir[nDir-1]=='/' ){ nDir--; }
    while( zSrc[0]=='.' && zSrc[1]=='.' && (zSrc[2]=='/' || zSrc[2]==0) ){
      nDir = DirectoryLength(zDir,nDir);
      zSrc += 2;
      while( zSrc[0]=='/' ){ zSrc++; TestPoint(0); }
    }
    if( nDir>0 && zDir[nDir-1]=='/' ){
      zSep = "";
      TestPoint(0);
    }else{
      TestPoint(0);
    }
  }
  zResult = ckalloc( nProtocol + nHost + nDir + strlen(zSrc) + 2 );
  if( zResult ){
    sprintf(zResult,"%.*s%.*s%.*s%s%s",
       nProtocol, zProtocol, nHost, zHost, nDir, zDir, zSep, zSrc);
    TestPoint(0);
  }else{
    TestPoint(0);
  }
  return zResult;
}

/*
** This routine is called to change the base URL for the current document.
** The URL specified in the argument becomes the new base URL for this
** document.
*/
void HtmlChangeUrl(HtmlWidget *htmlPtr, char *zSrc){
  int n;
  char *zDir;
  int hasDir = 0;
  static char *aDirProto[] = { "http:", "ftp:", "file:" };
  int i;

  n = ProtocolLength(zSrc);
  if( n ){
    FreeString(&htmlPtr->zProtocol);
    htmlPtr->zProtocol = StrNDup(zSrc, n);
    zSrc += n;
    n = HostLength(zSrc);
    FreeString(&htmlPtr->zHost);
    htmlPtr->zHost = StrNDup(zSrc, n);
    zSrc += n;
    TestPoint(0);
  }else{
    TestPoint(0);
  }
  if( htmlPtr->zProtocol ){
    for(i=0; i<sizeof(aDirProto)/sizeof(aDirProto[0]); i++){
      if( strcmp(htmlPtr->zProtocol,aDirProto[i])==0 ){
        hasDir = 1;
        break;
      }
    }
  }
  if( !hasDir ){
    FreeString(&htmlPtr->zDir);
    htmlPtr->zDir = StrNDup( zSrc, strlen(zSrc) );
  }else{
    zDir = htmlPtr->zDir;
    if( zDir==0 ) zDir = "/";
    n = strlen(zDir);
    while( zSrc[0]=='.' && zSrc[1]=='.' && (zSrc[2]=='/' || zSrc[2]==0) ){
      n = DirectoryLength(zDir,n);
      zSrc += 2;
      while( zSrc[0]=='/' ){ zSrc++; TestPoint(0);}
    }
    if( zSrc[0]=='/' ){
      FreeString(&htmlPtr->zDir);
      htmlPtr->zDir = StrNDup( zSrc, strlen(zSrc) );
      TestPoint(0);
    }else if( zSrc[0]!=0 ){
      if( n>0 && zDir[n-1]=='/' ){ n--; }
      htmlPtr->zDir = ckalloc( n + strlen(zSrc) + 2 );
      if( htmlPtr->zDir ){
        sprintf(htmlPtr->zDir,"%.*s/%s",n, zDir, zSrc);
        TestPoint(0);
      }
      ckfree(zDir);
    }else{
      TestPoint(0);
    }
  }
}
