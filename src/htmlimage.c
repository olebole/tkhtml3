/*
** Routines used for processing <IMG> markup
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
#include <string.h>
#include <stdlib.h>
#include "htmlimage.h"

/*
** Find the alignment for an image
*/
int HtmlGetImageAlignment(HtmlElement *p){
  char *z;
  int i;
  int result;

  static struct {
     char *zName;
     int iValue;
  } aligns[] = {
    { "bottom",     IMAGE_ALIGN_Bottom    },
    { "baseline",   IMAGE_ALIGN_Bottom    },
    { "middle",     IMAGE_ALIGN_Middle    },
    { "top",        IMAGE_ALIGN_Top       },
    { "absbottom",  IMAGE_ALIGN_AbsBottom },
    { "absmiddle",  IMAGE_ALIGN_AbsMiddle },
    { "texttop",    IMAGE_ALIGN_TextTop   },
    { "left",       IMAGE_ALIGN_Left      },
    { "right",      IMAGE_ALIGN_Right     },
  };

  z = HtmlMarkupArg(p, "align", 0);
  result = IMAGE_ALIGN_Bottom;
  if( z ){
    for(i=0; i<sizeof(aligns)/sizeof(aligns[0]); i++){
      if( stricmp(aligns[i].zName,z)==0 ){
        result = aligns[i].iValue;
        TestPoint(0);
        break;
      }else{
        TestPoint(0);
      }
    }
  }else{
    TestPoint(0);
  }
  return result;
}

/*
** This routine is called when an image changes.  If the size of the
** images changes, then we need to completely redo the layout.  If
** only the appearance changes, then this works like an expose event.
*/
static void ImageChangeProc(
  ClientData clientData,    /* Pointer to an HtmlImage structure */
  int x,                    /* Left edge of region that changed */
  int y,                    /* Top edge of region that changed */
  int w,                    /* Width of region that changes.  Maybe 0 */
  int h,                    /* Height of region that changed.  Maybe 0 */
  int newWidth,             /* New width of the image */
  int newHeight             /* New height of the image */
){
  HtmlImage *pImage;
  HtmlWidget *htmlPtr;
  HtmlElement *pElem;

  pImage = (HtmlImage*)clientData;
  htmlPtr = pImage->htmlPtr;
  if( pImage->w!=newWidth || pImage->h!=newHeight ){
    /* We have to completely redo the layout after adjusting the size
    ** of the images */
    for(pElem = pImage->pList; pElem; pElem = pElem->image.pNext){
      pElem->image.w = newWidth;
      pElem->image.h = newHeight;
      TestPoint(0);
    }
    htmlPtr->flags |= RELAYOUT;
    pImage->w = newWidth;
    pImage->h = newHeight;
    HtmlRedrawEverything(htmlPtr);
  }else{
    for(pElem = pImage->pList; pElem; pElem = pElem->image.pNext){
      pElem->image.redrawNeeded = 1;
    }
    htmlPtr->flags |= REDRAW_IMAGES;
    HtmlScheduleRedraw(htmlPtr);
  }
}

/*
** Given an <IMG> markup, find or create an appropriate HtmlImage
** structure and return a pointer to that structure.  NULL might
** be returned.
**
** This routine may invoke a callback procedure which could delete
** the HTML widget.  Use HtmlLock() if necessary to preserve the
** widget structure.
*/
HtmlImage *HtmlGetImage(HtmlWidget *htmlPtr, HtmlElement *p){
  char *zWidth;
  char *zHeight;
  char *zSrc;
  char *zImageName;
  char *azSeq[3];
  HtmlImage *pImage;
  int i;
  int result;
  Tcl_DString cmd;

  if( p->base.type!=Html_IMG ){ CANT_HAPPEN; return 0; }
  if( htmlPtr->zGetImage==0 || htmlPtr->zGetImage[0]==0 ){
    TestPoint(0);
    return 0;
  }
  azSeq[0] = HtmlMarkupArg(p, "src", "");
  azSeq[1] = 0;
  zSrc = "";
  HtmlLock(htmlPtr);
  if( azSeq[0]!=0 && HtmlCallResolver(htmlPtr, azSeq)==TCL_OK ){
    zSrc = htmlPtr->interp->result;
  }
  if( HtmlUnlock(htmlPtr) ) return 0;
  zWidth = HtmlMarkupArg(p, "width", "");
  zHeight = HtmlMarkupArg(p, "height", "");
  for(pImage=htmlPtr->imageList; pImage; pImage=pImage->pNext){
    if( strcmp(pImage->zUrl,zSrc)==0 ){
      ckfree(zSrc);
      TestPoint(0);
      return pImage;
    }
    TestPoint(0);
  }
  Tcl_DStringInit(&cmd);
  Tcl_DStringAppend(&cmd, htmlPtr->zGetImage, -1);
  Tcl_DStringAppendElement(&cmd,zSrc);
  Tcl_DStringAppendElement(&cmd,zWidth);
  Tcl_DStringAppendElement(&cmd,zHeight);
  Tcl_DStringStartSublist(&cmd);
  for(i=0; i+1<p->base.count; i+=2){
    char *z = p->markup.argv[i+1];
    Tcl_DStringAppendElement(&cmd,p->markup.argv[i]);
    Tcl_DStringAppendElement(&cmd,z);
    TestPoint(0);
  }
  Tcl_DStringEndSublist(&cmd);
  HtmlLock(htmlPtr);
  result = Tcl_GlobalEval(htmlPtr->interp, Tcl_DStringValue(&cmd));
  Tcl_DStringFree(&cmd);
  if( HtmlUnlock(htmlPtr) ) return 0;
  zImageName = htmlPtr->interp->result;
  pImage = (HtmlImage*)ckalloc( sizeof(HtmlImage) + strlen(zSrc) + 1 );
  memset(pImage,0,sizeof(HtmlImage));
  pImage->htmlPtr = htmlPtr;
  pImage->zUrl = (char*)&pImage[1];
  strcpy(pImage->zUrl,zSrc);
  Tcl_ResetResult(htmlPtr->interp);
  pImage->w = 0;
  pImage->h = 0;
  if( result==TCL_OK ){
    pImage->image = Tk_GetImage(htmlPtr->interp, htmlPtr->clipwin,
                                zImageName, ImageChangeProc, pImage);
    TestPoint(0);
  }else{
    Tcl_AddErrorInfo(htmlPtr->interp,
      "\n    (\"-imagecommand\" command executed by html widget)");
    Tcl_BackgroundError(htmlPtr->interp);
    pImage->image = 0;
    TestPoint(0);
  }
  if( pImage->image==0 ){
    ckfree((char*)pImage);
    TestPoint(0);
    return 0;
  }
  pImage->pNext = htmlPtr->imageList;
  htmlPtr->imageList = pImage;
  TestPoint(0);
  return pImage;
}
