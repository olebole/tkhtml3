static char const rcsid[] = "@(#) $Id: htmltcl.c,v 1.6 2002/12/17 18:24:16 drh Exp $";
/*
** The main routine for the HTML widget for Tcl/Tk
**
** This source code is released into the public domain by the author,
** D. Richard Hipp, on 2002 December 17.  Instead of a license, here
** is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
*/
#include <tk.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include "htmltcl.h"

#define SafeCheck(interp,str) if (Tcl_IsSafe(interp)) { \
    Tcl_AppendResult(interp, str, " invalid in safe interp", 0); \
    return TCL_ERROR; \
}


/*
** Delete a single HtmlElement
*/
void HtmlDeleteElement(HtmlElement *p){
  int c, i;
  switch( p->base.type ){
    case Html_Block:
      if( p->block.z ){
        HtmlFree(p->block.z);
      }
      break;
    case Html_Text:
      if( p->text.zText!=(char*)((&p->text.zText)+1))
        HtmlFree(p->text.zText);
      break;
#ifndef _TCLHTML_
    case Html_TR:
      if (p->ref.bgimage) {
        Tk_FreeImage(p->ref.bgimage);
      }
      break;
    case Html_TH:
    case Html_TD:
      if (p->cell.bgimage) {
        Tk_FreeImage(p->cell.bgimage);
      }
      break;
    case Html_TABLE:
      if (p->table.bgimage) {
        Tk_FreeImage(p->table.bgimage);
      }
      break;
#endif
    default:
      break;
  }
  if ((c=p->base.count) && HtmlIsMarkup(p))
    if (p->markup.argv != (char**)p->markup.argv[c+1]) {
      for (i=0; i<c; i++)
        HtmlFree(p->markup.argv[i]);
      HtmlFree(p->markup.argv);
    }
  HtmlFree(p);
}

/*
** Erase all data from the HTML widget.  Bring it back to an
** empty screen.
**
** This happens (for example) when the "clear" method is invoked
** on the widget, or just before the widget is deleted.
*/
void HtmlClear(HtmlWidget *htmlPtr){
  int i;
  HtmlElement *p, *pNext;

  HtmlDeleteControls(htmlPtr);
  for(p=htmlPtr->pFirst; p; p=pNext){
    pNext = p->pNext;
    HtmlDeleteElement(p);
  }
  htmlPtr->pFirst = 0;
  htmlPtr->pLast = 0;
  htmlPtr->nToken = 0;
  htmlPtr->idind = 0;
  if( htmlPtr->zText ){
    HtmlFree(htmlPtr->zText);
  }
  htmlPtr->zText = 0;
  htmlPtr->nText = 0;
  htmlPtr->nAlloc = 0;
  htmlPtr->nComplete = 0;
  htmlPtr->iPlaintext = 0;
  htmlPtr->lastSized = 0;
  htmlPtr->nextPlaced = 0;
  htmlPtr->firstBlock = 0;
  htmlPtr->lastBlock = 0;
  htmlPtr->nInput = 0;
  htmlPtr->nForm = 0;
  htmlPtr->varId = 0;
  htmlPtr->paraAlignment = ALIGN_None;
  htmlPtr->rowAlignment = ALIGN_None;
  htmlPtr->anchorFlags = 0;
  htmlPtr->inDt = 0;
  htmlPtr->anchorStart = 0;
  htmlPtr->formStart = 0;
  htmlPtr->innerList = 0;
  htmlPtr->maxX = 0;
  htmlPtr->maxY = 0;
  htmlPtr->xOffset = 0;
  htmlPtr->yOffset = 0;
  htmlPtr->pInsBlock = 0;
  htmlPtr->ins.p = 0;
  htmlPtr->selBegin.p = 0;
  htmlPtr->selEnd.p = 0;
  htmlPtr->pSelStartBlock = 0;
  htmlPtr->pSelEndBlock = 0;
  htmlPtr->HasScript = 0;
  htmlPtr->HasFrames = 0;
  while( htmlPtr->styleStack ){
    HtmlStyleStack *p = htmlPtr->styleStack;
    htmlPtr->styleStack = p->pNext;
    HtmlFree(p);
  }
#ifndef _TCLHTML_
  ClearGcCache(htmlPtr);
#endif
  if( htmlPtr->zBaseHref ){
    HtmlFree(htmlPtr->zBaseHref);
    htmlPtr->zBaseHref = 0;
  }
#ifndef _TCLHTML_
  HtmlClearTk(htmlPtr);
#endif
  HtmlFreeTokenMap(htmlPtr);
}

/*
** This routine attempts to delete the widget structure.  But it won't
** do it if the widget structure is locked.  If the widget structure is
** locked, then when HtmlUnlock() is called and the lock count reaches
** zero, this routine will be called to finish the job.
*/
static void DestroyHtmlWidget(HtmlWidget *htmlPtr){
  int i;
  if( htmlPtr->locked>0 ) return;
  Tcl_DeleteCommand(htmlPtr->interp, htmlPtr->zCmdName);
#ifndef _TCLHTML_
  Tcl_DeleteCommand(htmlPtr->interp, htmlPtr->zClipwin);
#endif
  HtmlClear(htmlPtr);
  for(i=0; i<Html_TypeCount; i++){
    if( htmlPtr->zHandler[i] ){
      HtmlFree(htmlPtr->zHandler[i]);
      htmlPtr->zHandler[i] = 0;
    }
  }
  if( htmlPtr->insTimer ){
    Tcl_DeleteTimerHandler(htmlPtr->insTimer);
    htmlPtr->insTimer = 0;
  }
#ifndef _TCLHTML_
  DestroyHtmlWidgetTk(htmlPtr);
#endif
  HtmlFree(htmlPtr);
}

/*
** Remove a lock from the HTML widget.  If the widget has been
** deleted, then delete the widget structure.  Return 1 if the
** widget has been deleted.  Return 0 if it still exists.
**
** Normal Tk code (that is to say, code in the Tk core) uses
** Tcl_Preserve() and Tcl_Release() to accomplish what this
** function does.  But preserving and releasing are much more
** common in this code than in regular widgets, so this routine
** was invented to do the same thing easier and faster.
*/
int HtmlUnlock(HtmlWidget *htmlPtr){
  htmlPtr->locked--;
  if( htmlPtr->tkwin==0 && htmlPtr->locked<=0 ){
    Tcl_Interp *interp = htmlPtr->interp;
    Tcl_Preserve(interp);
    DestroyHtmlWidget(htmlPtr);
    Tcl_Release(interp);
    return 1;
  }
  return htmlPtr->tkwin==0;
}

/*
** Lock the HTML widget.  This prevents the widget structure from
** being deleted even if the widget itself is destroyed.  There must
** be a call to HtmlUnlock() to release the structure.
*/
void HtmlLock(HtmlWidget *htmlPtr){
  htmlPtr->locked++;
}

/*
** This routine checks to see if an HTML widget has been
** destroyed.  It is always called after calling HtmlLock().
**
** If the widget has been destroyed, then the structure
** is unlocked and the function returns 1.  If the widget
** has not been destroyed, then the structure is not unlocked
** and the routine returns 0.
**
** This routine is intended for use in code like the following:
**
**     HtmlLock(htmlPtr);
**     // Do something that might destroy the widget
**     if( HtmlIsDead(htmlPtr) ) return;
**     // Do something that might destroy the widget
**     if( HtmlIsDead(htmlPtr) ) return;
**     // Do something that might destroy the widget
**     if( HtmlUnlock(htmlPtr) ) return;
*/
int HtmlIsDead(HtmlWidget *htmlPtr){
  if( htmlPtr->tkwin==0 ){
    HtmlUnlock(htmlPtr);
    return 1;
  }
  return 0;
}

HtmlElement *HtmlGetMap(HtmlWidget *htmlPtr, char *name) {
  HtmlElement *p=htmlPtr->pFirst;
  char *z, *zb;
  while (p) {
    if (p->base.type==Html_MAP) {
      z=HtmlMarkupArg(p,"name",0);
      zb=HtmlMarkupArg(p,"shape",0);
      if (zb && *zb!='r') return 0;
      if (z&&!strcmp(z,name))
        return p;
    }
    p=p->pNext;
  }
  return 0;
}

#define nSubcommand 70
/*
** The following array defines all possible widget command.  The main
** widget command function just parses up the command line, then vectors
** control to one of the command service routines defined in the 
** following array:
*/
static struct HtmlSubcommand {
  char *zCmd1;           /* First-level subcommand.  Required */
  char *zCmd2;           /* Second-level subcommand.  May be NULL */
  int minArgc;           /* Minimum number of arguments */
  int maxArgc;           /* Maximum number of arguments */
  char *zHelp;           /* Help string if wrong number of arguments */
  int (*xFunc)(HtmlWidget*,Tcl_Interp*,int,char**);  /* Cmd service routine */
  int (*xFuncObj)(HtmlWidget*,Tcl_Interp*,int,Tcl_Obj* CONST objv[]);  /* Obj cmd */
} aSubcommand[nSubcommand] = {
  { "cget",      0,         3, 3, "CONFIG-OPTION",       0, HtmlCgetObjCmd },
  { "clear",     0,         2, 2, 0,                     HtmlClearCmd },
  { "parse",     0,         3, 7, "HTML-TEXT",           0, HtmlParseCmd },
  { "index",     0,         3, 3, "INDEX",               HtmlIndexCmd },
  { "insert",    0,         3, 3, "INDEX",               HtmlInsertCmd },
  { "layout",    0,         2, 2, 0,                     HtmlLayoutCmd },
  { "names",     0,         2, 2, 0,                     HtmlNamesCmd },
  { "get",       0,         2, 2, 0,                     0, HtmlGetCmd },
  { "source",    0,         2, 2, 0,                     0, HtmlGetCmd },/*dup*/
  { "text",      "ascii",   3, 5, "START END",           HtmlTextAsciiCmd },
  { "text",	 "break",   4, 4, "INDEX",		HtmlTextInsertCmd},
  { "text",      "delete",  5, 5, "START END",           HtmlTextDeleteCmd },
  { "text",	 "find",    4, 7,"TEXT ?nocase? ?before|after INDEX?", HtmlTextFindCmd},
  { "text",      "html",    5, 5, "START END",           HtmlTextHtmlCmd },
  { "text",      "insert",  5, 5, "INDEX TEXT",          HtmlTextInsertCmd },
  { "text",	 "offset",  6, 6,"START NUM1 NUM2", HtmlTextOffsetCmd},
  { "text",	 "table",4, 6,"INDEX ?-images? ?-attrs?", HtmlTextTable},
  { "token",     "append",  5, 5, "TAG ARGUMENTS",       0 },
  { "token",     "delete",  4, 5, "INDEX ?INDEX?",       HtmlTokenDeleteCmd },
  { "token",     "define",  4, 5, "TAGNAME ATTRS",       HtmlTokenDefineCmd },
  { "token",     "find",    4, 6, "TAG ?before|after|near INDEX?", HtmlTokenFindCmd},
  { "token",     "get",     4, 5, "INDEX ?INDEX?",       HtmlTokenGetCmd },
  { "token",     "handler", 4, 5, "TAG ?SCRIPT?",        HtmlTokenHandlerCmd },
  { "token",     "insert",  6, 6, "INDEX TAG ARGUMENTS", HtmlTokenInsertCmd },
  { "token",	 "list",    3, 5, "START END",		 HtmlTokenListCmd},
  { "token",	 "markup",  5, 5, "START END",		 HtmlTokenMarkupCmd},
  { "token",	 "domtokens",5, 5, "START END",		 HtmlTokenDomCmd},
  { "token",	 "getend",  4, 4, "INDEX",		 HtmlTokenGetEnd},
  { "token",	 "attr",    4, 6, "INDEX ?NAME ?VALUE??",HtmlTokenAttr},
  { "token",	 "attrs",   4, 6, "attrlist",		 HtmlTokenAttrSearch},
  { "token",	 "unique",  4, 6, "tag",		 HtmlTokenUnique},
  { "token",	 "onEvents", 3, 5, "?-begin INDEX? ?-end INDEX? ?-range {INDEX INDEX}?",HtmlTokenOnEvents},
  { "dom",	 "value",   4, 5, "DomAddr ?newvalue?",  HtmlDomCmd},
  { "dom",	 "id",      4, 4, "DomAddr",		 HtmlDomCmd},
  { "dom",	 "ids",     4, 4, "DomAddr",		 HtmlDomCmd},
  { "dom",	 "addr",    4, 8, "index", 		 HtmlIdToDomCmd},
  { "dom",	 "tree",    4, 5, "index ?value?",	 HtmlDomTreeCmd},
  { "dom",	 "nameidx", 5, 5, "tag name",		 HtmlDomName2Index},
  { "dom",	 "radioidx", 6, 6, "form name idx",	 HtmlDomRadio2Index},
  { "dom",	 "formel",  5, 5, "form name",		 HtmlDomFormElIndex},
  { "resolve",   0,         2, 0, "?URI ...?",           HtmlResolveCmd },
  { "configure", 0,         2, 0, "?ARGS...?",           0, HtmlConfigCmd },
#ifndef _TCLHTML_
  { "href",      0,         4, 4, "X Y",                 0,HtmlHrefCmd },
  { "selection", "clear",   3, 3, 0,                     HtmlSelectionClearCmd},
  { "selection", "set",     5, 5, "START END",           HtmlSelectionSetCmd },
  { "xview",     0,         2, 5, "OPTIONS...",          HtmlXviewCmd },
  { "yview",     0,         2, 5, "OPTIONS...",          HtmlYviewCmd },
  { "bgimage",	 0,	    3, 4, 0,			 HtmlImageBgCmd },
  { "postscript",0,	    2,0,  0,			 HtmlPostscriptCmd },
  { "overattr",  0,	    5, 5, "X Y ATTR",		 HtmlAttrOverCmd},
  { "over",	 0,	    4, 5, "X Y ?-muponly?",	 HtmlOverCmd},
  { "imageat",	 0,	    4, 4, "X Y",		 HtmlImageAtCmd},
  { "imageset",	 0,	    4, 4, "ID INDEX",		 HtmlImageSetCmd},
  { "imageupdate",0,	    4, 4, "ID IMAGES",		 HtmlImageUpdateCmd},
  { "imageadd",	 0,	    4, 4, "ID IMAGES",		 HtmlImageAddCmd},
  { "onscreen",	 0,	    3, 3, "ID",			 HtmlOnScreen},
  { "forminfo",	 0,	    4, 4, "ID",			 HtmlFormInfo},
  { "coords",	 0,	    2, 4, "?INDEX ?percent??",	 HtmlCoordsCmd},
  { "images",	 0,	    3, 3, "?list|html?",	 HtmlImagesListCmd},
  { "refresh",	 0,	    2, 15,"", 			 HtmlRefreshCmd},
  { "htmlbp",	 0,	    2, 2, "",			 HtmlBP},
  { "sizewindow",0,	    3, 3, "",			 HtmlSizeWindow},
#endif
  { "debug",     "dump",    5, 5, "START END",           HtmlDebugDumpCmd },
  { "debug",     "testpt",  4, 4, "FILENAME",            HtmlDebugTestPtCmd },
  {0,0,0,0,0,0}
};

/* Dynamically add command to HTML widget */
int HtmlCommandAdd(char *c1, char *c2, int m, int n, char *help, 
   int (*xFunc)(HtmlWidget*,Tcl_Interp*,int,char**)) {
 struct HtmlSubcommand *pCmd;
  int i;
  for(i=0, pCmd=aSubcommand; i<nSubcommand; i++, pCmd++) {
    if (!pCmd->zCmd1) {
      pCmd->zCmd1=(c1?strdup(c1):0); pCmd->zCmd2=(c2?strdup(c2):0);
      pCmd->minArgc=m; pCmd->maxArgc=n;
      pCmd->zHelp=(help?strdup(help):0); pCmd->xFunc=xFunc;
      pCmd->xFuncObj=0;
      return (int)pCmd->xFunc;
    }
  } 
}

/* Dynamically add Obj command to HTML widget */
int HtmlCommandAddObj(char *c1, char *c2, int m, int n, char *help, 
   int (*xFunc)(HtmlWidget*,Tcl_Interp*,int,Tcl_Obj* CONST objv[])) {
 struct HtmlSubcommand *pCmd;
  int i;
  for(i=0, pCmd=aSubcommand; i<nSubcommand; i++, pCmd++) {
    if (!pCmd->zCmd1) {
      pCmd->zCmd1=(c1?strdup(c1):0); pCmd->zCmd2=(c2?strdup(c2):0);
      pCmd->minArgc=m; pCmd->maxArgc=n;
      pCmd->zHelp=(help?strdup(help):0);
      pCmd->xFunc=0;
      pCmd->xFuncObj=xFunc;
      return (int)pCmd->xFuncObj;
    }
  } 
}

/* Dynamically remove command from to HTML widget */
int HtmlCommandDel(char *c1, char *c2) {
  int i;
  struct HtmlSubcommand *pCmd;
  for(i=0, pCmd=aSubcommand; i<nSubcommand; i++, pCmd++) {
    if (!pCmd->zCmd1) continue;
    if ((!strcmp(pCmd->zCmd1,c1))&&((!pCmd->zCmd2)||(!strcmp(pCmd->zCmd2,c2))))
      pCmd->zCmd1=pCmd->zCmd2=0;
  } 
  return 0;
}
int HtmlWidgetObjCommand(
  ClientData clientData,	/* The HTML widget data structure */
  Tcl_Interp *interp,		/* Current interpreter. */
  int objc,			/* Number of arguments. */
  Tcl_Obj *CONST objv[]			/* Argument strings. */
){
  HtmlWidget *htmlPtr = (HtmlWidget*) clientData;
  size_t length;
  int c;
  int i, fnd1=0;
  struct HtmlSubcommand *pCmd;
  char *cmd, *arg1=0, *arg2=0;

  cmd = Tcl_GetString(objv[0]);
  if (objc < 2) {
    Tcl_AppendResult(interp, "wrong # args: should be \"", cmd,
        " option ?arg arg ...?\"", 0);
    return TCL_ERROR;
  }
  arg1 = Tcl_GetStringFromObj(objv[1],&length);
  c = *arg1;
  for(i=0, pCmd=aSubcommand; i<nSubcommand; i++, pCmd++){
    if( pCmd->zCmd1==0 || c!=pCmd->zCmd1[0] 
    || strncmp(pCmd->zCmd1,arg1,length)!=0 ){
      continue;
    }
    if( pCmd->zCmd2 ){
      int length2;
      int j;
      fnd1=1;
      if( objc<3 ){
        Tcl_AppendResult(interp, "wrong # args: should be \"",
          cmd, " ", pCmd->zCmd1, " SUBCOMMAND ?OPTIONS...?", 0);
        return TCL_ERROR;
      }
      arg2 = Tcl_GetStringFromObj(objv[2],&length2);
      if (strncmp(pCmd->zCmd2,arg2,length2)) continue;
    }
    if( objc<pCmd->minArgc || (objc>pCmd->maxArgc && pCmd->maxArgc>0) ){
      Tcl_AppendResult(interp,"wrong # args: should be \"", cmd,
         " ", pCmd->zCmd1, 0);
      if( pCmd->zCmd2 ){
        Tcl_AppendResult(interp, " ", pCmd->zCmd2, 0);
      }
      if( pCmd->zHelp ){
        Tcl_AppendResult(interp, " ", pCmd->zHelp, 0);
      }
      Tcl_AppendResult(interp, "\"", 0);
      return TCL_ERROR;
    }
    if( pCmd->xFunc!=0 ){	/* Fake support for old non-obj method */
      int i, rc;
      char *sargv[20];
      char **argv;
      if (objc>=19) {
        argv=calloc(sizeof(char*),objc+2);
        for (i=0; i<objc; i++)
	  argv[i]=Tcl_GetString(objv[i]);
	argv[i]=0;
        rc=(*pCmd->xFunc)(htmlPtr, interp, objc, argv);
	free(argv);
	return rc;
      }
      for (i=0; i<objc; i++)
	sargv[i]=Tcl_GetString(objv[i]);
      sargv[i]=0;
      return (*pCmd->xFunc)(htmlPtr, interp, objc, sargv);
    }
    if( pCmd->xFuncObj==0 ){
      Tcl_AppendResult(interp,"command not yet implemented", 0);
      return TCL_ERROR;
    }
    return (*pCmd->xFuncObj)(htmlPtr, interp, objc, objv);
  }
  if (fnd1) {
    Tcl_AppendResult(interp,"unknown subcommand \"", arg2,
        "\" -- should be one of:", 0);
    for(i=0, pCmd=aSubcommand; i<nSubcommand; i++, pCmd++){
      if( pCmd->zCmd1==0 || c!=pCmd->zCmd1[0] 
      || strncmp(pCmd->zCmd1,arg1,length)!=0 ){
        continue;
      }
      if( pCmd->zCmd2 ){
        Tcl_AppendResult(interp, " ", aSubcommand[i].zCmd2, 0);
        }
     }
     return TCL_ERROR;
  }
  Tcl_AppendResult(interp,"unknown command \"", arg1, "\" -- should be "
    "one of:", 0);
  for(i=0; i<nSubcommand; i++){
    if( aSubcommand[i].zCmd1==0 || aSubcommand[i].zCmd1[0]=='_' ){ 
      continue;
    }
    if (i && (!strcmp(aSubcommand[i].zCmd1,aSubcommand[i-1].zCmd1)))
      continue;
    Tcl_AppendResult(interp, " ", aSubcommand[i].zCmd1, 0);
  }
  return TCL_ERROR;
}

HtmlWidget *dbghtmlPtr=0;
int (*HtmlFetchSelectionPtr)(ClientData , int, char *, int ) = 0;
int (*htmlReformatCmdPtr)(Tcl_Interp *interp, char *str, char *dtype)=0;

/*
** This routine is used to register the "html" command with the
** Tcl interpreter.  This is the only routine in this file with
** external linkage.
*/
Tcl_Command htmlcmdhandle = 0;
DLL_EXPORT int Tclhtml_Init(Tcl_Interp *interp){
#ifdef USE_TCL_STUBS
  if( Tcl_InitStubs(interp,"8.3",0)==0 ){
    return TCL_ERROR;
  }
  if( Tk_InitStubs(interp,"8.3",0)==0 ){
    return TCL_ERROR;
  }
#endif
  htmlcmdhandle=Tcl_CreateObjCommand(interp,"html", HtmlObjCommand, 
       /* Tk_MainWindow(interp) */ 0, 0);
#ifdef DEBUG
  Tcl_LinkVar(interp, "HtmlTraceMask", (char*)&HtmlTraceMask, TCL_LINK_INT);
#endif
  Tcl_PkgProvide(interp, HTML_PKGNAME, HTML_PKGVERSION);
  return Htmlexts_Init(interp);
}

static const char ev[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int
getidx(char *buffer, int len, int *posn, int *atend) {
  char c;
  char *idx;
  if (*atend) return -1;
  do {
    if ((*posn)>=len) {
      *atend = 1;
      return -1;
    }
    c = buffer[(*posn)++];
    if (c&0x80 || c=='=') {
      *atend = 1;
      return -1;
    }
    idx = strchr(ev, c);
  } while (!idx);
  return idx - ev;
}

int
HtmlBase64decodeCmd(ClientData clientData, Tcl_Interp *interp,
	     int objc, Tcl_Obj *CONST objv[]) {
  Tcl_Obj *o;
  char *inbuffer;
  int ilen, olen, pos, tlen=1024, tpos=0, atend=0;
  char outbuffer[3], *tbuf;
  int c[4];

  tbuf=(char*)malloc(tlen);
  inbuffer = Tcl_GetStringFromObj(objv[3], &ilen);
  pos = 0;
  while (!atend) {
    if (inbuffer[pos]=='\n' ||inbuffer[pos]=='\r') { pos++; continue; }
    c[0] = getidx(inbuffer, ilen, &pos, &atend);
    c[1] = getidx(inbuffer, ilen, &pos, &atend);
    c[2] = getidx(inbuffer, ilen, &pos, &atend);
    c[3] = getidx(inbuffer, ilen, &pos, &atend);

    olen = 0;
    if (c[0]>=0 && c[1]>=0) {
      outbuffer[0] = ((c[0]<<2)&0xfc)|((c[1]>>4)&0x03);
      olen++;
      if (c[2]>=0) {
	outbuffer[1] = ((c[1]<<4)&0xf0)|((c[2]>>2)&0x0f);
	olen++;
	if (c[3]>=0) {
	  outbuffer[2] = ((c[2]<<6)&0xc0)|((c[3])&0x3f);
	  olen++;
	}
      }
    }

/*fprintf(stderr,"OB(%d): %x,%x,%x\n", olen,outbuffer[0],outbuffer[1],outbuffer[2]);*/
    if (olen>0) {
      if ((tpos+olen+1)>=tlen) {
        tbuf=realloc(tbuf,tlen+1024);
        tlen+=1024;
      }
      memcpy(tbuf+tpos,outbuffer,olen);
      tpos+=olen;
    }
  }
  o=Tcl_NewByteArrayObj(tbuf,tpos);
  Tcl_IncrRefCount (o);
  Tcl_SetObjResult (interp,o);
  Tcl_DecrRefCount (o);
  Tcl_SetObjResult(interp, o);
  free(tbuf);
  return TCL_OK;
}

int
HtmlBase64encodeCmd(ClientData clientData, Tcl_Interp *interp,
	     int objc, Tcl_Obj *CONST objv[]) {
  Tcl_Obj *result;
  char *ib;
  int i=0, ilen, olen, pos=0;
  char c[74];

  ib= Tcl_GetByteArrayFromObj (objv[3], &ilen);
  result = Tcl_NewStringObj("", 0);
  while (pos<ilen) {
#define P(n,s) ((pos+n)>ilen?'=':ev[s])
    c[i++]=ev[(ib[pos]>>2)&0x3f];
    c[i++]=P(1,((ib[pos]<<4)&0x30)|((ib[pos+1]>>4)&0x0f));
    c[i++]=P(2,((ib[pos+1]<<2)&0x3c)|((ib[pos+2]>>6)&0x03));
    c[i++]=P(3,ib[pos+2]&0x3f);
    if (i>=72) {
      c[i++]='\n';
      c[i]=0;
      Tcl_AppendToObj(result, c, i);
      i=0;
    }
    pos+=3;
  }
  if (i) {
/*    c[i++]='\n';*/
    c[i]=0;
    Tcl_AppendToObj(result, c, i);
    i=0;
  }
  Tcl_SetObjResult(interp, result);
  return TCL_OK;
}

#include <zlib.h>

static int
check_header(char *s, int len) {
  int i, method, flags, n;
  if (len<12) return -1;
  if (s[0] != 0x1f || s[1] != 0x8b) return -1;
  method=s[2];
  if (method != 8) return -1;
  flags=s[3];
  i=10;
  if (flags & 0x4) {
    n=s[i++]|(s[i++]<<8);
    if ((i+n)>=len) return -1;
    while (n-- && (signed char)s[i++] != EOF) ;
  }
  if (flags & 0x10) {
    while (i<len && s[i] && (signed char)s[i] != EOF) i++;
    i++;
  }
  if (flags & 0x8) {
    while (i<len && s[i] && (signed char)s[i] != EOF) i++;
    i++;
  }
  if (flags & 0x2)
     if ((i+=2)>=len) return -1;
  return i;
}

static int
HtmlGunzipCmd (ClientData clientData, Tcl_Interp * interp,
	    int objc, Tcl_Obj * CONST objv[])
{
    int r, ulen=1024, blen=ulen, fnlen; uLongf l=0; 
    char *fn, *from, *uncompr=0;
    gzFile zF=0;
    Tcl_Obj* o;
    if (objc<4) {  goto gunziperror; }
    from = Tcl_GetStringFromObj (objv[2], &fnlen);
    if (!strcmp(from,"file")) {
      SafeCheck(interp,"gunzip file");
      fn = Tcl_GetStringFromObj (objv[3], &fnlen);
      if (!(zF=gzopen(fn,"rb"))) goto gunziperror; 
      uncompr=(char*)malloc(ulen);
      for (;;) {
        r=gzread(zF,uncompr+l,ulen);
        if (r<0) goto gunziperror;
        l+=r;
        if (r==0) break;
        if ((l+ulen)>blen) {
          blen+=ulen;
          uncompr=(char*)realloc(uncompr,blen);
	}
      }
    } else if (!strcmp(from,"data")) {
      z_stream zf;
      int hlen;
      uncompr=(char*)malloc(ulen);
      from = Tcl_GetByteArrayFromObj (objv[3], &fnlen);
      hlen= check_header(from, fnlen);
      if (hlen<0 || hlen>fnlen) goto gunziperror;
      fnlen-=hlen; from+=hlen;
      zf.next_in=from;
      zf.avail_in=fnlen; zf.zalloc=Z_NULL; zf.zfree=Z_NULL; zf.opaque=Z_NULL;
      if (Z_OK != inflateInit2(&zf,-MAX_WBITS)) goto gunziperror;
      fn=uncompr;
      for (;;) {
	zf.next_out=fn;
	zf.avail_out=blen-zf.total_out;
	r=inflate(&zf,Z_NO_FLUSH);
        if (r == Z_STREAM_END) { l=zf.total_out; break; }
        if (r != Z_OK) {
	  inflateEnd(&zf);
	  goto gunziperror;
	}
	l=zf.total_out;
	if (zf.total_in == fnlen || zf.avail_in==0) break;
	blen+=1024;
	uncompr=(char*)realloc(uncompr,blen);
	fn=uncompr+l;
      }
      inflateEnd(&zf);
    } else goto gunziperror;
    o=Tcl_NewByteArrayObj(uncompr,l);
    Tcl_IncrRefCount (o);
    Tcl_SetObjResult (interp,o);
    Tcl_DecrRefCount (o);
    if (uncompr) free(uncompr);
    if (zF) gzclose(zF);
    return TCL_OK;
gunziperror:
    if (zF) gzclose(zF);
    if (uncompr) free(uncompr);
    Tcl_SetObjResult (interp, Tcl_NewStringObj ("gunzip error", -1));
    return TCL_ERROR;
}

static int
HtmlGzipCmd (ClientData clientData, Tcl_Interp * interp,
	    int objc, Tcl_Obj * CONST objv[])
{
    int i, n, r, l=0, ilen, fnlen;  unsigned char *ip;
    char *ib, *fn, *to, *compr=0;
    gzFile zF=0;
    Tcl_Obj* o;
    if (objc<4) {  goto gziperror; }
    to = Tcl_GetStringFromObj (objv[2], &fnlen);
    if (!strcmp(to,"file")) {
      SafeCheck(interp,"gzip file");
      if (objc<5) {  goto gziperror; }
      ib= Tcl_GetByteArrayFromObj (objv[4], &ilen);
      fn = Tcl_GetStringFromObj (objv[3], &fnlen);
      if (!(zF=gzopen(fn,"wb"))) goto gziperror; 
      for (;;) {
        r=gzwrite(zF,ib+l,ilen);
        if (r<0) goto gziperror;
        l+=r;
        if (r==0) break;
        ilen-=r;
        if (ilen<=0) break;
      }
    } else if (!strcmp(to,"data")) {
      int blen;
      z_stream zf;
      int crc=crc32(0l, Z_NULL, 0);
      ib = Tcl_GetByteArrayFromObj (objv[3], &fnlen);
      crc=crc32(0l, ib, fnlen);
      blen=(fnlen+fnlen/10+12);
      compr=(char*)calloc(blen+10,1);
      compr[0]= 0x1f; ((unsigned char*)compr)[1] = 0x8b;
      compr[2]=8;
      zf.next_in=ib;
      zf.avail_in=fnlen; zf.zalloc=Z_NULL; zf.zfree=Z_NULL; zf.opaque=Z_NULL;
      if (Z_OK != deflateInit2(&zf,Z_DEFAULT_COMPRESSION, Z_DEFLATED,
		-MAX_WBITS, 8, Z_DEFAULT_STRATEGY)) goto gziperror;
      fn=compr+10;
      for (;;) {
	zf.next_out=fn;
	zf.avail_out=blen-10-zf.total_out;
	r=deflate(&zf,Z_FINISH);
        if (r == Z_STREAM_END) { l=zf.total_out; break; }
        if (r != Z_OK) {
	  deflateEnd(&zf);
	  goto gziperror;
	}
	l=zf.total_out;
	if (zf.avail_out!=0) break;
	blen+=1024;
	compr=(char*)realloc(compr,blen+10);
	fn=compr+10+l;
      }
      ip=(unsigned char*)(compr+l+10);
      for (i=0; i<4; i++) {
        *ip++ =(crc&0xff);
	crc >>= 8;
      }
      n=zf.total_in;
      for (i=0; i<4; i++) {
        *ip++ =(n&0xff);
	n >>= 8;
      }
      l+=8;
      deflateEnd(&zf);
      o=Tcl_NewByteArrayObj(compr,l+10);
      Tcl_IncrRefCount (o);
      Tcl_SetObjResult (interp,o);
      Tcl_DecrRefCount (o);
      free(compr);
      return TCL_OK;
    } else goto gziperror;
    if (zF) gzclose(zF);
    return TCL_OK;
gziperror:
    if (zF) gzclose(zF);
    if (compr) free(compr);
    Tcl_SetObjResult (interp, Tcl_NewStringObj ("gzip error", -1));
    return TCL_ERROR;
}

static int
HtmlCrc32Cmd (ClientData clientData, Tcl_Interp * interp,
	    int objc, Tcl_Obj * CONST objv[])
{
    int len;
    char *ib, buf[30];
    if (objc<3) {
       Tcl_AppendResult(interp, "crc32 missing argument", 0);
       return TCL_ERROR;
    }
    ib = Tcl_GetByteArrayFromObj (objv[3], &len);
    sprintf(buf,"0x%lx",crc32(0l,ib,len));
    Tcl_AppendResult(interp, buf, 0);
    return TCL_OK;
}

int HtmlStdchanCmd(ClientData clientData, Tcl_Interp *interp,
	     int objc, Tcl_Obj *CONST objv[]) {
    Tcl_Channel chan;
    int mode, type;
    char *channelId = Tcl_GetString(objv[4]);
    char *stype=Tcl_GetString(objv[3]);

    chan = Tcl_GetChannel(interp, channelId, &mode);
    if (chan == (Tcl_Channel) NULL) {
        return TCL_ERROR;
    }
    if (!strcmp(stype,"stdin")) type=TCL_STDIN;
    else if (!strcmp(stype,"stdout")) type=TCL_STDOUT;
    else if (!strcmp(stype,"stderr")) type=TCL_STDERR;
    else  return TCL_ERROR;
    Tcl_SetStdChannel(chan, type);
    return TCL_OK;
}


/* Break up (fmt) text so that it is N chars or less between newlines. */
static int textfmtcmd(ClientData clientData, Tcl_Interp *interp,
	     int objc, Tcl_Obj *CONST objv[]) {
  Tcl_Obj *result;
  char *ib, *ob;
  char buf[1050];
  int ls=-1, i=0, n=0, ols, ilen, len;

  if (objc!=5) {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("textfmt needs two arguments "
					      "(text to format) len", -1));
    return TCL_ERROR;
  }
  ib = Tcl_GetStringFromObj(objv[3], &ilen);
  ob = Tcl_GetStringFromObj(objv[4], &ilen);
  len=atoi(ob);
  if (len<=0 || len>1024) {
   Tcl_SetObjResult(interp, Tcl_NewStringObj("length not 0..1024" , -1));
    return TCL_ERROR;
  }
  result = Tcl_NewStringObj("", 0);
  while (ib[n+i]) {
    buf[i]=ib[n+i];
    if (i>=1024 || buf[i]=='\n' || buf[i]=='\r') {
      if (i>=1024) buf[++i]='\n';
      buf[i+1]=0;
      Tcl_AppendToObj(result, buf, -1);
      n+=(i+1);
      i=0;
      continue;
    }
    if (isspace(buf[i])) ls=i;
    if (i>=len && ls>=0) {
/*      if (ls<0) {
        
        buf[i]='\n';
        buf[i+1]=0;
	n+=(i+1);
      } else */ {
        buf[i=ls]='\n';
        buf[ls+1]=0;;
	n+=(ls+1);
      }
      Tcl_AppendToObj(result, buf, -1);
      i=0;
      ls=-1;
    } else i++;
  }
  if (i) {
    buf[i]=0;
    Tcl_AppendToObj(result, buf, -1);
  }
  Tcl_SetObjResult(interp, result);
  return TCL_OK;
}

/* Atomic function to lock & copy contents of a file, then truncate input. */
static int lockcopycmd(ClientData clientData, Tcl_Interp *interp,
	     int objc, Tcl_Obj *CONST objv[]) {
  char *ib, *ob;
  int ilen, olen, indesc, outdesc, nread;
  char buf[1024];

  SafeCheck(interp,"lockcopy");
  ib = Tcl_GetStringFromObj(objv[1], &ilen);
  ob = Tcl_GetStringFromObj(objv[2], &olen);
  if ((indesc = open (ib, O_RDWR))) {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("open in failed", -1));
    return TCL_ERROR;
  }
#if !defined(__WIN32__) && !defined(sparc) && !(defined(hpux) || defined(__hpux)) && !defined(_AIX)
  if (flock(indesc, LOCK_EX)<0) {
    close(indesc);
    Tcl_SetObjResult(interp, Tcl_NewStringObj("lock failed", -1));
    return TCL_ERROR;
  }
#endif
  if ((outdesc=open(ob, O_WRONLY | O_CREAT | O_EXCL, 0666))<0) {
    close(indesc);
    Tcl_SetObjResult(interp, Tcl_NewStringObj("open out failed", -1));
    return TCL_ERROR;
  }
  while (1)
    {
      nread = read (indesc, buf, sizeof buf);
      if (nread != write (outdesc, buf, nread))
        {
          sprintf(buf,"copy failed: %s", strerror(errno));
          unlink (ob);
          Tcl_SetObjResult(interp, Tcl_NewStringObj(buf, -1));
          return TCL_ERROR;
        }
      if (nread < sizeof buf)
        break;
    }
  if (close(outdesc) < 0) {
    close(indesc);
    Tcl_SetObjResult(interp, Tcl_NewStringObj("close out failed", -1));
    return TCL_ERROR;
  }
#if !defined(__WIN32__) && !defined(sparc) && !(defined(hpux) || defined(__hpux)) && !defined(_AIX)
  ftruncate (indesc, 0L);
  flock(indesc, LOCK_UN);
#endif
  close(indesc);

  return TCL_OK;
}

static int
xorstrcmd(ClientData clientData, Tcl_Interp *interp,
             int objc, Tcl_Obj *CONST objv[]) {
  Tcl_Obj *result;
  char *ib;
  unsigned char *ip;
  int i=0, n=0, p=-1, ilen, olen, random=0, ibegin=0, dorandom=0, doimagic=0;
  char c[BUFSIZ+1], rval, seed;

  c[0]=0x8e; c[1]=0x1d; c[2]=0x20; c[3]=0x13; c[4]=0;
  if (objc<4) {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("needs two arguments "
                                              "(password data)", -1));
    return TCL_ERROR;
  }
  result=Tcl_NewStringObj("",0);
  ip = Tcl_GetStringFromObj(objv[2], &ilen);
  ib = Tcl_GetStringFromObj(objv[3], &olen);
  for (i=4; i<objc; i++) {
    int alen;
    char *ap=Tcl_GetStringFromObj(objv[i], &alen);
    if (!strcmp(ap,"-randomize")) {
      dorandom=1;
    } else if (!strcmp(ap,"-imagic")) {
      doimagic=1;
    } else if (!strcmp(ap,"-omagic")) {
      if (c[0]!=ib[0] || c[1]!=ib[1] || c[2]!=ib[2] || 
        (c[3]!=0x13 && c[3] !=0x14)) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("Decryption failed",-1));
	return TCL_ERROR;
      }
      if (ib[3]==0x14) dorandom=1;
      ibegin=4;
    }
  }
  if (dorandom) {
    c[3]=0x14;
    random=1;
    seed=ip[0];
    for (i=1; i<ilen; i++) {
      seed=seed^ip[i];
    }
    p=(seed%ip[0]);
  }
  if (doimagic) {
    Tcl_AppendToObj(result, c, 4);
  }
  for (i=ibegin; i<olen; i++) {
    if ((n+1)>=BUFSIZ) {
      c[n]=0;
      Tcl_AppendToObj(result, c, n);
      n=0;
    }
    p++;
    if (p>=ilen) p=0;
    rval=ip[p];
    if (random) {
      if (p==(seed%ilen)) {
        seed=(ip[p]^seed);
      }
      rval=(ip[p]^seed);
    }
    c[n++]=(ib[i]^rval);
  }
  c[n]=0;
  if (n) Tcl_AppendToObj(result, c, n);
  Tcl_SetObjResult(interp, result);
  return TCL_OK;
}

static int
decryptsrcsub(ClientData clientData, Tcl_Interp *interp,
             int objc, Tcl_Obj *CONST objv[], int decrypt) {
  static char *srcpass="saj.k4S-pQk_d3HL";
  char pw[50], *ib;
  int i, rc=TCL_OK, olen;
  int vobjc=5; Tcl_Obj *vobjv[7];
  if (objc<4) {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("needs two arguments "
        "(password data)", -1));
    return TCL_ERROR;
  }
  vobjv[0]=objv[0];
  vobjv[1]=objv[1];
  vobjv[3]=objv[3];
  ib = Tcl_GetStringFromObj(objv[2], &olen);
  for (i=0; srcpass[i] && ib[i]; i++)  pw[i]=ib[i]^srcpass[i]; 
  pw[i]=0;
  vobjv[2]=Tcl_NewStringObj(pw,-1);
  vobjv[4]=Tcl_NewStringObj("-randomize",-1);
  if (TCL_OK==(rc=xorstrcmd(clientData, interp, vobjc, vobjv))) {
    if (decrypt) {
      char *cp=strdup(Tcl_GetStringResult(interp));
      rc=Tcl_GlobalEval(interp, cp);
      free(cp);
    }
  }
  Tcl_DecrRefCount(vobjv[2]);
  Tcl_DecrRefCount(vobjv[4]);
  return rc;
}
static int
srcencryptcmd(ClientData clientData, Tcl_Interp *interp,
             int objc, Tcl_Obj *CONST objv[]) {
  return decryptsrcsub(clientData, interp, objc, objv, 0);
}

static int
srcdecryptcmd(ClientData clientData, Tcl_Interp *interp,
             int objc, Tcl_Obj *CONST objv[]) {
  return decryptsrcsub(clientData, interp, objc, objv, 1);
}

extern int tkhtmlexiting;

static int
HtmlExitCmd(ClientData clientData, Tcl_Interp *interp,
             int objc, Tcl_Obj *CONST objv[]) {
  /* Sad, but TK is core dumping on exit.  So here is the backway out. */
  SafeCheck(interp,"exit");
  exit(0);
}

/*
  htmlReformatCmd:
      html reformat  $from  $to  $text
     plain, http, url or html
  Doesn't work for UTF chars > 0x7f
*/
#define ChrIdx(str,i) (cp=Tcl_UtfAtIndex(str,i))?*cp:0
/* Tcl_UtfToUniChar(cp, &uch); ch=uch;*/
#define CmpStr(str,var) (str[0] == var[0] && (!strcmp(str,var)))

static int HtmlReformatCmd(ClientData clientData, Tcl_Interp *interp,
    int objc, Tcl_Obj *CONST objv[]
) {    
  char buf[132], *cp; int n=0, i=0, l;
  Tcl_UniChar ch;
  char *stype=Tcl_GetString(objv[2]);
  char *dtype=Tcl_GetString(objv[3]);
  char *str=Tcl_GetString(objv[4]);
  l=Tcl_NumUtfChars(str,-1);
  if (CmpStr(dtype,"plain")) {
    if (!(CmpStr(stype,"http") || CmpStr(stype,"url"))) {
      Tcl_AppendResult(interp, "html reformat: source type must be \"http\" or \"url\": ", dtype, 0);
      return TCL_ERROR;
    }
    for (i=0; i<l; n++,i++) {
      if (n>=128) {
        buf[n]=0;
        Tcl_AppendResult(interp, buf, 0);
        buf[n=0]=0;
      }
      ch=ChrIdx(str,i);
      if (ch != '%' || (i+2)>=l)
        buf[n]=ch;
      else {
	char cbuf[3];
        cbuf[0]=ChrIdx(str,i+1);
        cbuf[1]=ChrIdx(str,i+2);
        if (isxdigit(cbuf[0]) && isxdigit(cbuf[1])) {
	  int ich;
          cbuf[3]=0;
	  sscanf(cbuf,"%2x",&ich);
	  sprintf(buf+n,"%c",ich);
	  i+=2;
	} else
          buf[n]=ch;
      }
    }
    buf[n]=0;
    Tcl_AppendResult(interp, buf, 0);
    return TCL_OK;
  }
  if (!CmpStr(stype,"plain")) {
    Tcl_AppendResult(interp, "html reformat: source type must be \"plain\": ",
	dtype, 0);
    return TCL_ERROR;
  }
  if (CmpStr(dtype,"url") || CmpStr(dtype,"http")) {
    for (i=0; i<l; n++,i++) {
      if (n>=128) {
        buf[n]=0;
        Tcl_AppendResult(interp, buf, 0);
        buf[n=0]=0;
      }
      ch=ChrIdx(str,i);
      if (isalnum(ch) || ch=='$' || ch=='-' || ch=='_' ||
        ch=='.' || (dtype[0]=='u' && ch=='/'))
           buf[n]=ch;
      else if (ch==' ') buf[n]='+';
      else if (ch=='\n') n--;
      else {
        buf[n++]='%'; sprintf(buf+n,"%02X",(unsigned char)ch); n++;
      }
    }
    buf[n]=0;
    Tcl_AppendResult(interp, buf, 0);
    return TCL_OK;
  }
  if (CmpStr(dtype,"html")) {
    for (i=0; i<l; n++,i++) {
      if (n>=120) {
        buf[n]=0;
        Tcl_AppendResult(interp, buf, 0);
        buf[n=0]=0;
      }
      ch=ChrIdx(str,i);
      if (ch == '&') {
        strcpy(buf+n, "&amp;");
	n+=5;
      } else if (ch == '<') {
        strcpy(buf+n, "&lt;");
	n+=4;
      } else if (ch == '>') {
        strcpy(buf+n, "&gt;");
	n+=4;
      } else
	buf[n++]=ch;
    }
    buf[n]=0;
    Tcl_AppendResult(interp, buf, 0);
    return TCL_OK;
  }
  Tcl_AppendResult(interp, "html reformat: unknown type: ", dtype, 0);
  return TCL_ERROR;
}

  /*    html urlsplit $url
  **
  ** Split a URL into a list of its parts.
  */
static int HtmlUrlsplitCmd(
    ClientData clientData,        /* The HTML widget data structure */
    Tcl_Interp *interp,           /* Current interpreter. */
    int objc,                     /* Number of arguments. */
    Tcl_Obj *CONST objv[]                 /* Argument strings. */
){
    char *url = Tcl_GetString(objv[2]);
    Tcl_DString str;
    char *z, zs, *zn;
    Tcl_DStringInit(&str);
    
    if (!(z = strchr(url, ':'))) {
      Tcl_DStringAppendElement(&str,"");
      z=url;
    } else {
      *z=0;
      Tcl_DStringAppendElement(&str,url);
      *z++ =':';
    }
    while (*z && *z == '/')
      z++;
    zn=z;
    while (*z && (isalnum(*z) || *z == '.' || *z == ':' || *z == '-'))
      z++;
    zs=0;
    if (z==zn) {
      Tcl_DStringAppendElement(&str,"");
    } else {
      zs=*z;
      *z=0;
      Tcl_DStringAppendElement(&str,zn);
      *z = zs;
    }
    zn=z;
    while (*z && *z != '?' && *z != '#')
      z++;
    zs=0;
    if (z==zn) {
      Tcl_DStringAppendElement(&str,"");
    } else {
      zs=*z;
      *z=0;
      Tcl_DStringAppendElement(&str,zn);
      *z++ = zs;
    }
    zs=0;
    zn=z;
    while (*z && *z != '#')
      z++;
    zs=0;
    if (z==zn) {
      Tcl_DStringAppendElement(&str,"");
    } else {
      zs=*z;
      *z=0;
      Tcl_DStringAppendElement(&str,zn);
      *z = zs;
    }

    Tcl_DStringResult(interp, &str); 
    return TCL_OK;
}

  /*    html urljoin  $scheme $authority $path $query $fragment
  **
  ** Merge together the parts of a URL into a single value URL.
  */
static int HtmlUrljoinCmd(
    ClientData clientData,        /* The HTML widget data structure */
    Tcl_Interp *interp,           /* Current interpreter. */
    int objc,                     /* Number of arguments. */
    Tcl_Obj *CONST objv[]                 /* Argument strings. */
){
    char *z, zLine[100];
    Tcl_DString str;
    char *scheme=Tcl_GetString(objv[2]);
    char *authority=Tcl_GetString(objv[3]);
    char *path=Tcl_GetString(objv[4]);
    char *query=Tcl_GetString(objv[5]);
    char *fragment=Tcl_GetString(objv[6]);
    Tcl_DStringInit(&str);
    if (*scheme) {
      Tcl_DStringAppend(&str, scheme, -1);
      Tcl_DStringAppend(&str, ":", 1);
    }
    if (*authority) {
      Tcl_DStringAppend(&str, "//", 2);
      Tcl_DStringAppend(&str, authority, -1);
    }
    if (*path) {
      if (*path != '/')
	Tcl_DStringAppend(&str, "/", 1);
      Tcl_DStringAppend(&str, path, -1);
    }
    if (*query) {
      if (*query != '?')
	Tcl_DStringAppend(&str, "?", 1);
      Tcl_DStringAppend(&str, query, -1);
    }
    if (*fragment) {
      if (*fragment != '#')
	Tcl_DStringAppend(&str, "#", 1);
      Tcl_DStringAppend(&str, fragment, -1);
    }
    Tcl_DStringResult(interp, &str); 
    return TCL_OK;
}

static struct HtmlCmd {
  char *zCmd1;           /* First-level subcommand.  Required */
  char *zCmd2;           /* Second-level subcommand.  May be NULL */
  int minArgc;           /* Minimum number of arguments */
  int maxArgc;           /* Maximum number of arguments */
  char *zHelp;           /* Help string if wrong number of arguments */
  int (*xFuncObj)(ClientData,Tcl_Interp*,int,Tcl_Obj* CONST objv[]);  /* Obj cmd */
} acommand[] = {
  { "base64", "decode",4, 4, "DATA",HtmlBase64decodeCmd},
  { "base64", "encode",4, 4, "DATA",HtmlBase64encodeCmd},
  { "crc32", 0, 3, 3, "DATA",HtmlCrc32Cmd},
  { "xor", "encrypt", 5, 5, "DATA PASSWORD",srcencryptcmd},
  { "xor", "decrypt", 5, 5, "DATA PASSWORD",srcdecryptcmd},
  { "xor", "xor", 5, 8, "DATA PASSWORD ?-imagic -omagic -randomize?",xorstrcmd},
  { "lockcopy", 0, 4, 4, "SRCFILE DSTFILE",lockcopycmd},
  { "exit", 0, 2, 2, "DATA",HtmlExitCmd},
  { "gzip", "file",5, 5, "FILE DATA",HtmlGzipCmd},
  { "gzip", "data",4, 4, "DATA",HtmlGzipCmd},
  { "gunzip", "file",4, 4, "FILE",HtmlGunzipCmd},
  { "gunzip", "data",4, 4, "DATA",HtmlGunzipCmd},
  { "reformat", 0, 5, 5, "FROM TO TEXT",HtmlReformatCmd},
  { "stdchan", "stdin", 4, 4, "CHANNEL",HtmlStdchanCmd},
  { "stdchan", "stdout", 4, 4, "CHANNEL",HtmlStdchanCmd},
  { "stdchan", "stderr", 4, 4, "CHANNEL",HtmlStdchanCmd},
  { "text", "format", 5, 5, "DATA LEN",textfmtcmd},
  { "urljoin", 0, 7, 7, "SCHEME AUTHORITY PATH QUERY FRAGMENT",HtmlUrljoinCmd},
  { "urlsplit", 0, 3, 3, "URL",HtmlUrlsplitCmd},
  { 0 }
};

int HtmlCommandObj(
  ClientData clientData,	/* The HTML widget data structure */
  Tcl_Interp *interp,		/* Current interpreter. */
  int objc,			/* Number of arguments. */
  Tcl_Obj *CONST objv[]			/* Argument strings. */
){
  size_t length;
  int c;
  int i, fnd1=0;
  struct HtmlCmd *pCmd;
  char *cmd, *arg1=0, *arg2=0;

  cmd = Tcl_GetString(objv[0]);
  if (objc < 2) {
    Tcl_AppendResult(interp, "wrong # args: should be \"", cmd,
        " option ?arg arg ...?\"", 0);
    return TCL_ERROR;
  }
  arg1 = Tcl_GetStringFromObj(objv[1],&length);
  c = *arg1;
  for(i=0, pCmd=acommand; pCmd->zCmd1; i++, pCmd++){
    if( pCmd->zCmd1==0 || c!=pCmd->zCmd1[0] 
    || strncmp(pCmd->zCmd1,arg1,length)!=0 ){
      continue;
    }
    if( pCmd->zCmd2 ){
      int length2;
      int j;
      fnd1=1;
      if( objc<3 ){
        Tcl_AppendResult(interp, "wrong # args: should be \"",
          cmd, " ", pCmd->zCmd1, " SUBCOMMAND ?OPTIONS...?", 0);
        return TCL_ERROR;
      }
      arg2 = Tcl_GetStringFromObj(objv[2],&length2);
      if (strncmp(pCmd->zCmd2,arg2,length2)) continue;
    }
    if( objc<pCmd->minArgc || (objc>pCmd->maxArgc && pCmd->maxArgc>0) ){
      Tcl_AppendResult(interp,"wrong # args: should be \"", cmd,
         " ", pCmd->zCmd1, 0);
      if( pCmd->zCmd2 ){
        Tcl_AppendResult(interp, " ", pCmd->zCmd2, 0);
      }
      if( pCmd->zHelp ){
        Tcl_AppendResult(interp, " ", pCmd->zHelp, 0);
      }
      Tcl_AppendResult(interp, "\"", 0);
      return TCL_ERROR;
    }
    if( pCmd->xFuncObj==0 ){
      Tcl_AppendResult(interp,"command not yet implemented", 0);
      return TCL_ERROR;
    }
    return (*pCmd->xFuncObj)(clientData, interp, objc, objv);
  }
  if (fnd1) {
    Tcl_AppendResult(interp,"unknown subcommand \"", arg2,
        "\" -- should be one of:", 0);
    for(i=0, pCmd=acommand; i<nSubcommand; i++, pCmd++){
      if( pCmd->zCmd1==0 || c!=pCmd->zCmd1[0] 
      || strncmp(pCmd->zCmd1,arg1,length)!=0 ){
        continue;
      }
      if( pCmd->zCmd2 ){
        Tcl_AppendResult(interp, " ", acommand[i].zCmd2, 0);
        }
     }
     return TCL_ERROR;
  }
  Tcl_AppendResult(interp,"unknown command \"", arg1, "\" -- should be "
    "one of:", 0);
  for(i=0; acommand[i].zCmd1; i++){
    if (i && (!strcmp(acommand[i].zCmd1,acommand[i-1].zCmd1)))
      continue;
    Tcl_AppendResult(interp, " ", acommand[i].zCmd1, 0);
  }
  return TCL_ERROR;
}
