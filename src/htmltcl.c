static char const rcsid[] = "@(#) $Id: htmltcl.c,v 1.3 2002/03/06 18:10:59 peter Exp $";
/*
** The main routine for the HTML widget for Tcl/Tk
**
** Copyright (C) 1997-2000 D. Richard Hipp
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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "htmltcl.h"

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
} aSubcommand[nSubcommand] = {
  { "cget",      0,         3, 3, "CONFIG-OPTION",       HtmlCgetCmd },
  { "clear",     0,         2, 2, 0,                     HtmlClearCmd },
  { "parse",     0,         3, 7, "HTML-TEXT",           HtmlParseCmd },
  { "index",     0,         3, 3, "INDEX",               HtmlIndexCmd },
  { "insert",    0,         3, 3, "INDEX",               HtmlInsertCmd },
  { "layout",    0,         2, 2, 0,                     HtmlLayoutCmd },
  { "names",     0,         2, 2, 0,                     HtmlNamesCmd },
  { "get",       0,         2, 2, 0,                     HtmlGetCmd },
  { "text",      "ascii",   5, 5, "START END",           0 },
  { "text",      "delete",  5, 5, "START END",           0 },
  { "text",      "html",    5, 5, "START END",           0 },
  { "text",      "insert",  5, 5, "INDEX TEXT",          0 },
  { "token",     "append",  5, 5, "TAG ARGUMENTS",       0 },
  { "token",     "delete",  4, 5, "INDEX ?INDEX?",       0 },
  { "token",     "find",    4, 6, "TAG ?before|after|near INDEX?", 0},
  { "token",     "get",     4, 5, "INDEX ?INDEX?",       0 },
  { "token",     "handler", 4, 5, "TAG ?SCRIPT?",        HtmlTokenHandlerCmd },
  { "token",     "insert",  6, 6, "INDEX TAG ARGUMENTS", 0 },
  { "resolve",   0,         2, 0, "?URI ...?",           HtmlResolveCmd },
  { "configure", 0,         2, 0, "?ARGS...?",           HtmlConfigCmd },
#ifndef _TCLHTML_
  { "href",      0,         4, 4, "X Y",                 HtmlHrefCmd },
  { "selection", "clear",   3, 3, 0,                     HtmlSelectionClearCmd},
  { "selection", "set",     5, 5, "START END",           HtmlSelectionSetCmd },
  { "xview",     0,         2, 5, "OPTIONS...",          HtmlXviewCmd },
  { "yview",     0,         2, 5, "OPTIONS...",          HtmlYviewCmd },
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
      return (int)pCmd->xFunc;
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

/*
** This routine implements the command used by individual HTML widgets.
*/
int HtmlWidgetCommand(
  ClientData clientData,	/* The HTML widget data structure */
  Tcl_Interp *interp,		/* Current interpreter. */
  int argc,			/* Number of arguments. */
  char **argv			/* Argument strings. */
){
  HtmlWidget *htmlPtr = (HtmlWidget*) clientData;
  size_t length;
  int c;
  int i, fnd1=0;
  struct HtmlSubcommand *pCmd;

  if (argc < 2) {
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
        " option ?arg arg ...?\"", 0);
    return TCL_ERROR;
  }
  c = argv[1][0];
  length = strlen(argv[1]);
  for(i=0, pCmd=aSubcommand; i<nSubcommand; i++, pCmd++){
    if( pCmd->zCmd1==0 || c!=pCmd->zCmd1[0] 
    || strncmp(pCmd->zCmd1,argv[1],length)!=0 ){
      continue;
    }
    if( pCmd->zCmd2 ){
      int length2;
      int j;
      fnd1=1;
      if( argc<3 ){
        Tcl_AppendResult(interp, "wrong # args: should be \"",
          argv[0], " ", pCmd->zCmd1, " SUBCOMMAND ?OPTIONS...?", 0);
        return TCL_ERROR;
      }
      length2 = strlen(argv[2]);
      if (strncmp(pCmd->zCmd2,argv[2],length2)) continue;
    }
    if( argc<pCmd->minArgc || (argc>pCmd->maxArgc && pCmd->maxArgc>0) ){
      Tcl_AppendResult(interp,"wrong # args: should be \"", argv[0],
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
    if( pCmd->xFunc==0 ){
      Tcl_AppendResult(interp,"command not yet implemented", 0);
      return TCL_ERROR;
    }
    return (*pCmd->xFunc)(htmlPtr, interp, argc, argv);
  }
  if (fnd1) {
    Tcl_AppendResult(interp,"unknown subcommand \"", argv[2],
        "\" -- should be one of:", 0);
    for(i=0, pCmd=aSubcommand; i<nSubcommand; i++, pCmd++){
      if( pCmd->zCmd1==0 || c!=pCmd->zCmd1[0] 
      || strncmp(pCmd->zCmd1,argv[1],length)!=0 ){
        continue;
      }
      if( pCmd->zCmd2 ){
        Tcl_AppendResult(interp, " ", aSubcommand[i].zCmd2, 0);
        }
     }
     return TCL_ERROR;
  }
  Tcl_AppendResult(interp,"unknown command \"", argv[1], "\" -- should be "
    "one of:", 0);
  for(i=0; i<nSubcommand; i++){
    if( aSubcommand[i].zCmd1==0 || aSubcommand[i].zCmd1[0]=='_' ){ 
      continue;
    }
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
  htmlcmdhandle=Tcl_CreateCommand(interp,"html", HtmlCommand, 
       /* Tk_MainWindow(interp) */ 0, 0);
#ifdef DEBUG
  Tcl_LinkVar(interp, "HtmlTraceMask", (char*)&HtmlTraceMask, TCL_LINK_INT);
#endif
  Tcl_PkgProvide(interp, HTML_PKGNAME, HTML_PKGVERSION);
  return Htmlexts_Init(interp);
}

