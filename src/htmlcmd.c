static char const rcsid[] = "@(#) $Id: htmlcmd.c,v 1.21 2001/10/07 19:16:26 peter Exp $";
/*
** Routines to implement the HTML widget commands
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
#include <stdlib.h>
#include "htmlcmd.h"
#include <X11/Xatom.h>

/*
** WIDGET resolve ?URI ...?
**
** Call the TCL command specified by the -resolvercommand option
** to resolve the URL.
*/
int HtmlResolveCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  return HtmlCallResolver(htmlPtr, argv+2);
}

/*
** WIDGET cget CONFIG-OPTION
**
** Retrieve the value of a configuration option
*/
int HtmlCgetCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  int rc;
  Tk_ConfigSpec *cs=HtmlConfigSpec();
  if (htmlPtr->TclHtml)
    rc=TclConfigureWidget(interp, htmlPtr, cs,
		argc-2, argv+2,  (char *) htmlPtr, 0);
  else
    rc=Tk_ConfigureValue(interp, htmlPtr->tkwin, cs,
		(char *) htmlPtr, argv[2], 0);
  return rc;
}

/*
** WIDGET clear
**
** Erase all HTML from this widget and clear the screen.  This is
** typically done before loading a new document.
*/
int HtmlClearCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlClear(htmlPtr);
  htmlPtr->flags |= REDRAW_TEXT | VSCROLL | HSCROLL;
  HtmlScheduleRedraw(htmlPtr);
  return TCL_OK;
}

/*
** WIDGET configure ?OPTIONS?
**
** The standard Tk configure command.
*/
int HtmlConfigCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
#ifdef _TCLHTML_
  if (argc == 2) { /* ???? */
     return HtmlCgetCmd(htmlPtr, interp, argc, argv);
  } else if (argc == 3) {
     return HtmlCgetCmd(htmlPtr, interp, argc, argv);
  } else {
     return ConfigureHtmlWidget(interp, htmlPtr, argc-2, argv+2,
                                TK_CONFIG_ARGV_ONLY, 0);
  }
#else
  if (argc == 2) {
     return Tk_ConfigureInfo(interp, htmlPtr->tkwin, HtmlConfigSpec(),
        (char *) htmlPtr, (char *) NULL, 0);
  } else if (argc == 3) {
     return Tk_ConfigureInfo(interp, htmlPtr->tkwin, HtmlConfigSpec(),
        (char *) htmlPtr, argv[2], 0);
  } else {
     return ConfigureHtmlWidget(interp, htmlPtr, argc-2, argv+2,
                                TK_CONFIG_ARGV_ONLY, 0);
  }
#endif
}

/* Return pElem with attr "name" == value */
HtmlElement *HtmlAttrElem(  HtmlWidget *htmlPtr, char *name, char *value) {
    HtmlElement *p;
    char *z;
    for(p=htmlPtr->pFirst; p; p=p->pNext){
      if( p->base.type!=Html_A ) continue;
      z = HtmlMarkupArg(p,name,0);
      if(z && (!strcmp(z,value)))
        return p;
    }
    return 0;
}
/*
** WIDGET names
**
** Returns a list of names associated with <a name=...> tags.
*/
int HtmlNamesCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement *p;
  char *z;
  for(p=htmlPtr->pFirst; p; p=p->pNext){
    if( p->base.type!=Html_A ) continue;
    z = HtmlMarkupArg(p,"name",0);
    if( z ){
      Tcl_AppendElement(interp,z);
    }else{
      z = HtmlMarkupArg(p,"id",0);
      if( z ){
        Tcl_AppendElement(interp,z);
      }
    }
  }
  return TCL_OK;
}

int HtmlAdvanceLayout(
  HtmlWidget *htmlPtr    /* The HTML widget */
) {
  if( htmlPtr->LOendPtr ){
    if( htmlPtr->LOendPtr->pNext ){
      htmlPtr->formStart= htmlPtr->LOformStart;
      HtmlAddStyle(htmlPtr, htmlPtr->LOendPtr->pNext);
      HtmlSizer(htmlPtr);
    }
  }else if( htmlPtr->pFirst ){
    htmlPtr->paraAlignment = ALIGN_None;
    htmlPtr->rowAlignment = ALIGN_None;
    htmlPtr->anchorFlags = 0;
    htmlPtr->inDt = 0;
    htmlPtr->anchorStart = 0;
    htmlPtr->formStart = 0;
    htmlPtr->LOformStart= 0;
    htmlPtr->innerList = 0;
    htmlPtr->nInput = 0; 
    HtmlAddStyle(htmlPtr, htmlPtr->pFirst);
    HtmlSizer(htmlPtr);
  }
  htmlPtr->LOendPtr=htmlPtr->pLast;
  htmlPtr->LOformStart= htmlPtr->formStart;
  return TCL_OK;
}
/*
** WIDGET parse HTML
**
** Appends the given HTML text to the end of any HTML text that may have
** been inserted by prior calls to this command.  Then it runs the
** tokenizer, parser and layout engine as far as possible with the
** text that is available.  The display is updated appropriately.
*/
int HtmlParseCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  int i; char *ypos=0;
  HtmlIndex iStart, iEnd;
  HtmlElement *savePtr;
  iStart.p=0; iStart.i=0;
  htmlPtr->LOendPtr = htmlPtr->pLast;
  HtmlLock(htmlPtr);
  for (i=3; i<(argc-1); i+=2) {
    if ((!strcmp(argv[i],"-insert")) && htmlPtr->LOendPtr) {
      if (HtmlGetIndex(htmlPtr, argv[i+1], &iStart.p, &iStart.i)!=0 ){
        Tcl_AppendResult(interp,"malformed index: \"", argv[i+1], "\"", 0);
        return TCL_ERROR;
      }
      if (iStart.p) {
        savePtr=iStart.p->pNext;
        htmlPtr->pLast=iStart.p;
        iStart.p->pNext=0;
      }
    } else if ((!strcmp(argv[i],"-ypos")) && argv[i+1][0]) {
      htmlPtr->zGoto=(char*)strdup(argv[i+1]);
    }
  }
  HtmlTokenizerAppend(htmlPtr, argv[2]);
  if( HtmlIsDead(htmlPtr) ){
    return TCL_OK;
  }
  if( htmlPtr->LOendPtr ){
    htmlPtr->formStart= htmlPtr->LOformStart;
    if (iStart.p && savePtr) {
      HtmlAddStyle(htmlPtr, htmlPtr->LOendPtr);
      htmlPtr->pLast->pNext=savePtr;
      savePtr->base.pPrev=htmlPtr->pLast;
      htmlPtr->pLast=htmlPtr->LOendPtr;
      htmlPtr->flags |= (REDRAW_TEXT|RELAYOUT);
      HtmlScheduleRedraw(htmlPtr);
    } else if( htmlPtr->LOendPtr->pNext ){
      HtmlAddStyle(htmlPtr, htmlPtr->LOendPtr->pNext);
    }
  }else if( htmlPtr->pFirst ){
    htmlPtr->paraAlignment = ALIGN_None;
    htmlPtr->rowAlignment = ALIGN_None;
    htmlPtr->anchorFlags = 0;
    htmlPtr->inDt = 0;
    htmlPtr->anchorStart = 0;
    htmlPtr->formStart = 0;
    htmlPtr->innerList = 0;
    htmlPtr->nInput = 0; 
    HtmlAddStyle(htmlPtr, htmlPtr->pFirst);
  }
  if( !HtmlUnlock(htmlPtr) ){
    htmlPtr->flags |= EXTEND_LAYOUT;
    HtmlScheduleRedraw(htmlPtr);
  }
  if (htmlPtr->TclHtml)
    HtmlLayout(htmlPtr);
  return TCL_OK;
}

int HtmlLayoutCmd( HtmlWidget *htmlPtr,Tcl_Interp *interp,int argc,char **argv){
  HtmlLayout(htmlPtr);
  return TCL_OK;
}

#ifndef _TCLHTML_
/*
** WIDGET href X Y
**
** Returns the URL on the hyperlink that is beneath the position X,Y.
** Returns {} if there is no hyperlink beneath X,Y.
*/
int HtmlHrefCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  int x, y;
  char *z, *target=0;

  if( Tcl_GetInt(interp, argv[2], &x) != TCL_OK 
   || Tcl_GetInt(interp, argv[3], &y) != TCL_OK
  ){
    return TCL_ERROR;
  }
  z = HtmlGetHref(htmlPtr, x + htmlPtr->xOffset, y + htmlPtr->yOffset, &target);
  if( z ){
    HtmlLock(htmlPtr);
    z = HtmlResolveUri(htmlPtr, z);
    if(z && !HtmlUnlock(htmlPtr) ){
      Tcl_DString cmd;
      Tcl_DStringInit(&cmd);
      Tcl_DStringStartSublist(&cmd);
      Tcl_DStringAppendElement(&cmd, z);
      Tcl_DStringEndSublist(&cmd);
      if (target) {
        Tcl_DStringStartSublist(&cmd);
        Tcl_DStringAppendElement(&cmd, target);
        Tcl_DStringEndSublist(&cmd);
      }
      Tcl_DStringResult(interp, &cmd);
    }
    if (z) HtmlFree(z);
  }
  return TCL_OK;
}

/*
** WIDGET xview ?SCROLL-OPTIONS...?
**
** Implements horizontal scrolling in the usual Tk way.
*/
int HtmlXviewCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  if( argc==2 ){
    HtmlComputeHorizontalPosition(htmlPtr,interp->result);
  }else{
    int count;
    double fraction;
    int maxX = htmlPtr->maxX;
    int w = HtmlUsableWidth(htmlPtr);
    int offset = htmlPtr->xOffset;
    int type = Tk_GetScrollInfo(interp,argc,argv,&fraction,&count);
    switch( type ){
      case TK_SCROLL_ERROR:
        return TCL_ERROR;
      case TK_SCROLL_MOVETO:
        offset = fraction * maxX;
        break;
      case TK_SCROLL_PAGES:
        offset += (count * w * 9)/10;
        break;
      case TK_SCROLL_UNITS:
        offset += (count * w)/10;
        break;
    }
    if( offset + w > maxX ){
      offset = maxX - w;
    }else{
    }
    if( offset < 0 ){
      offset = 0;
    }else{
    }
    HtmlHorizontalScroll(htmlPtr, offset);
    htmlPtr->flags |= ANIMATE_IMAGES;
  }
  return TCL_OK;
}

/*
** WIDGET yview ?SCROLL-OPTIONS...?
**
** Implements vertical scrolling in the usual Tk way, but with one
** enhancement.  If the argument is a single word, the widget looks
** for a <a name=...> tag with that word as the "name" and scrolls
** to the position of that tag.
*/
int HtmlYviewCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  if( argc==2 ){
    HtmlComputeVerticalPosition(htmlPtr,interp->result);
  }else if( argc==3 ){
    HtmlElement *p=HtmlAttrElem(htmlPtr, "name", argv[2]);
    if (p)
      HtmlVerticalScroll(htmlPtr, p->anchor.y);
  }else{
    int count;
    double fraction;
    int maxY = htmlPtr->maxY;
    int h = HtmlUsableHeight(htmlPtr);
    int offset = htmlPtr->yOffset;
    int type = Tk_GetScrollInfo(interp,argc,argv,&fraction,&count);
    switch( type ){
      case TK_SCROLL_ERROR:
        return TCL_ERROR;
      case TK_SCROLL_MOVETO:
        offset = fraction * maxY;
        break;
      case TK_SCROLL_PAGES:
        offset += (count * h * 9)/10;
        break;
      case TK_SCROLL_UNITS:
        offset += (count * h)/10;
        break;
    }
    if( offset + h > maxY ){
      offset = maxY - h;
    }else{
    }
    if( offset < 0 ){
      offset = 0;
    }else{
    }
    HtmlVerticalScroll(htmlPtr, offset);
    htmlPtr->flags |= ANIMATE_IMAGES;
  }
  return TCL_OK;
}

/* The pSelStartBlock and pSelEndBlock values have been changed.
** This routine's job is to loop over all HtmlBlocks and either
** set or clear the HTML_Selected bits in the .base.flags field
** as appropriate.  For every HtmlBlock where the bit changes,
** mark that block for redrawing.
*/
static void UpdateSelection(HtmlWidget *htmlPtr){
  int selected = 0;
  HtmlIndex tempIndex;
  HtmlBlock *pTempBlock;
  int temp;
  HtmlBlock *p;

  for(p=htmlPtr->firstBlock; p; p=p->pNext){
    if( p==htmlPtr->pSelStartBlock ){
      selected = 1;
      HtmlRedrawBlock(htmlPtr, p);
    }else if( !selected && p==htmlPtr->pSelEndBlock ){
      selected = 1;
      tempIndex = htmlPtr->selBegin;
      htmlPtr->selBegin = htmlPtr->selEnd;
      htmlPtr->selEnd = tempIndex;
      pTempBlock = htmlPtr->pSelStartBlock;
      htmlPtr->pSelStartBlock = htmlPtr->pSelEndBlock;
      htmlPtr->pSelEndBlock = pTempBlock;
      temp = htmlPtr->selStartIndex;
      htmlPtr->selStartIndex = htmlPtr->selEndIndex;
      htmlPtr->selEndIndex = temp;
      HtmlRedrawBlock(htmlPtr, p);
    }else{
    }
    if( p->base.flags & HTML_Selected ){
      if( !selected ){
        p->base.flags &= ~HTML_Selected;
        HtmlRedrawBlock(htmlPtr,p);
      }else{
      }
    }else{
      if( selected ){
        p->base.flags |= HTML_Selected;
        HtmlRedrawBlock(htmlPtr,p);
      }else{
      }
    }
    if( p==htmlPtr->pSelEndBlock ){
      selected = 0;
      HtmlRedrawBlock(htmlPtr, p);
    }else{
    }
  }
}

/* Given the selection end-points in htmlPtr->selBegin
** and htmlPtr->selEnd, recompute pSelBeginBlock and
** pSelEndBlock, then call UpdateSelection to update the
** display.
**
** This routine should be called whenever the selection
** changes or whenever the set of HtmlBlock structures
** change.
*/
void HtmlUpdateSelection(HtmlWidget *htmlPtr, int forceUpdate){
  HtmlBlock *pBlock;
  int index;
  int needUpdate = forceUpdate;
  int temp;

  if( htmlPtr->selEnd.p==0 ){
    htmlPtr->selBegin.p = 0;
  }else{
  }
  HtmlIndexToBlockIndex(htmlPtr, htmlPtr->selBegin, &pBlock, &index);
  if( needUpdate || pBlock != htmlPtr->pSelStartBlock ){
    needUpdate = 1;
    HtmlRedrawBlock(htmlPtr, htmlPtr->pSelStartBlock);
    htmlPtr->pSelStartBlock = pBlock;
    htmlPtr->selStartIndex = index;
  }else if( index != htmlPtr->selStartIndex ){
    HtmlRedrawBlock(htmlPtr, pBlock);
    htmlPtr->selStartIndex = index;
  }else{
  }
  if( htmlPtr->selBegin.p==0 ){
    htmlPtr->selEnd.p = 0;
  }else{
  }
  HtmlIndexToBlockIndex(htmlPtr, htmlPtr->selEnd, &pBlock, &index);
  if( needUpdate || pBlock != htmlPtr->pSelEndBlock ){
    needUpdate = 1;
    HtmlRedrawBlock(htmlPtr, htmlPtr->pSelEndBlock);
    htmlPtr->pSelEndBlock = pBlock;
    htmlPtr->selEndIndex = index;
  }else if( index != htmlPtr->selEndIndex ){
    HtmlRedrawBlock(htmlPtr, pBlock);
    htmlPtr->selEndIndex = index;
  }else{
  }
  if( htmlPtr->pSelStartBlock 
  && htmlPtr->pSelStartBlock==htmlPtr->pSelEndBlock
  && htmlPtr->selStartIndex > htmlPtr->selEndIndex
  ){
    temp = htmlPtr->selStartIndex;
    htmlPtr->selStartIndex = htmlPtr->selEndIndex;
    htmlPtr->selEndIndex = temp;
  }else{
  }
  if( needUpdate ){
    htmlPtr->flags |= ANIMATE_IMAGES;
    UpdateSelection(htmlPtr);
  }else{
  }
}

void
HtmlLostSelection(
    ClientData clientData)      /* Information about table widget. */
{
  HtmlWidget *htmlPtr = (HtmlWidget *) clientData;
  char *argv[3];
  argv[2]="";
  if (htmlPtr->exportSelection) {
    HtmlSelectionClearCmd(htmlPtr,0,3,argv);
  }
}

/*
** WIDGET selection set INDEX INDEX
*/
int HtmlSelectionSetCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv           /* List of all arguments */
){
  HtmlIndex selBegin, selEnd;
  int bi, ei;

  HtmlLock(htmlPtr);
  if( HtmlGetIndex(htmlPtr, argv[3], &selBegin.p, &selBegin.i) ){
    if( !HtmlUnlock(htmlPtr) ){
      Tcl_AppendResult(interp,"malformed index: \"", argv[3], "\"", 0);
    }
    return TCL_ERROR;
  }
  if( HtmlIsDead(htmlPtr) ) return TCL_OK;
  if( HtmlGetIndex(htmlPtr, argv[4], &selEnd.p, &selEnd.i) ){
    if( !HtmlUnlock(htmlPtr) ){
      Tcl_AppendResult(interp,"malformed index: \"", argv[4], "\"", 0);
    }
    return TCL_ERROR;
  }
  if( HtmlUnlock(htmlPtr) ) return TCL_OK;
  bi=HtmlTokenNumber(selBegin.p);   ei=HtmlTokenNumber(selEnd.p);
  if (!(selBegin.p && selEnd.p)) return TCL_OK;
  if (bi<ei || (bi==ei && selBegin.i<=selEnd.i)) {
    htmlPtr->selBegin = selBegin;
    htmlPtr->selEnd = selEnd;
  } else {
    htmlPtr->selBegin = selEnd;
    htmlPtr->selEnd = selBegin;
  }
  HtmlUpdateSelection(htmlPtr,0);
  if (htmlPtr->exportSelection) {
    Tk_OwnSelection(htmlPtr->tkwin, XA_PRIMARY, HtmlLostSelection,
                    (ClientData) htmlPtr);
  }

  return TCL_OK;
}

/*
** WIDGET selection clear
*/
int HtmlSelectionClearCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv           /* List of all arguments */
){
  htmlPtr->pSelStartBlock = 0;
  htmlPtr->pSelEndBlock = 0;
  htmlPtr->selBegin.p = 0;
  htmlPtr->selEnd.p = 0;
  UpdateSelection(htmlPtr);
  return TCL_OK;
}

#endif /* _TCLHTML_ */

#
/*
** Recompute the position of the insertion cursor based on the
** position in htmlPtr->ins.
*/
void HtmlUpdateInsert(HtmlWidget *htmlPtr){
#ifndef _TCLHTML_
  if (htmlPtr->TclHtml) return;
  HtmlIndexToBlockIndex(htmlPtr, htmlPtr->ins, 
                        &htmlPtr->pInsBlock, &htmlPtr->insIndex);
  HtmlRedrawBlock(htmlPtr, htmlPtr->pInsBlock);
  if( htmlPtr->insTimer==0 ){
    htmlPtr->insStatus = 0;
    HtmlFlashCursor(htmlPtr);
  }else{
  }
#endif /* _TCLHTML_ */
}

/*
** WIDGET token handler TAG ?SCRIPT?
*/
int HtmlTokenHandlerCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  int type = HtmlNameToType(argv[3]);
  if( type==Html_Unknown ){
    Tcl_AppendResult(interp,"unknown tag: \"", argv[3], "\"", 0);
    return TCL_ERROR;
  }
  if( argc==4 ){
    if( htmlPtr->zHandler[type]!=0 ){
      interp->result = htmlPtr->zHandler[type];
    }
  }else{
    if( htmlPtr->zHandler[type]!=0 ){
      HtmlFree(htmlPtr->zHandler[type]);
    }
    htmlPtr->zHandler[type] = HtmlAlloc( strlen(argv[4]) + 1 );
    if( htmlPtr->zHandler[type] ){
      strcpy(htmlPtr->zHandler[type],argv[4]);
    }
  }
  return TCL_OK;
}

/*
** WIDGET index INDEX	
*/
int HtmlIndexCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement *p;
  int i;

  HtmlLock(htmlPtr);
  if( HtmlGetIndex(htmlPtr, argv[2], &p, &i)!=0 ){
    if( !HtmlUnlock(htmlPtr) ){
      Tcl_AppendResult(interp,"malformed index: \"", argv[2], "\"", 0);
    }
    return TCL_ERROR;
  }
  if( !HtmlUnlock(htmlPtr) && p ){
    sprintf(interp->result, "%d.%d", HtmlTokenNumber(p), i);
  }else{
  }
  return TCL_OK;
}

/*
** WIDGET get
*/
int HtmlGetCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  Tcl_AppendResult(interp,htmlPtr->zText, 0);
  return TCL_OK;
}

/*
** WIDGET insert INDEX
*/
int HtmlInsertCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv           /* List of all arguments */
){
  HtmlIndex ins;
  if( argv[2][0]==0 ){
    HtmlRedrawBlock(htmlPtr, htmlPtr->pInsBlock);
    htmlPtr->insStatus = 0;
    htmlPtr->pInsBlock = 0;
    htmlPtr->ins.p = 0;
  }else{
    HtmlLock(htmlPtr);
    if( HtmlGetIndex(htmlPtr, argv[2], &ins.p, &ins.i) ){
      if( !HtmlUnlock(htmlPtr) ){
        Tcl_AppendResult(interp,"malformed index: \"", argv[1], "\"", 0);
      }
      return TCL_ERROR;
    }
    if( HtmlUnlock(htmlPtr) ) return TCL_OK;
    HtmlRedrawBlock(htmlPtr, htmlPtr->pInsBlock);
    htmlPtr->ins = ins;
    HtmlUpdateInsert(htmlPtr);
  }
  return TCL_OK;
}

/*
** WIDGET debug dump START END
*/
int HtmlDebugDumpCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement *pStart, *pEnd;
  int i;

  if( HtmlGetIndex(htmlPtr, argv[3], &pStart, &i)!=0 ){
    Tcl_AppendResult(interp,"malformed index: \"", argv[3], "\"", 0);
    return TCL_ERROR;
  }
  if( HtmlGetIndex(htmlPtr, argv[4], &pEnd, &i)!=0 ){
    Tcl_AppendResult(interp,"malformed index: \"", argv[4], "\"", 0);
    return TCL_ERROR;
  }
  if( pStart ){
    HtmlPrintList(pStart,pEnd ? pEnd->base.pNext : 0);
  }
  return TCL_OK;
}

/*
** WIDGET debug testpt FILENAME
*/
int HtmlDebugTestPtCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlTestPointDump(argv[3]);
  return TCL_OK;
}

