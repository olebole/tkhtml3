static char const rcsid[] = "@(#) $Id: htmlexts.c,v 1.1 2001/06/17 22:40:05 peter Exp $";
/*
** The extra routines for the HTML widget for Tcl/Tk
**
** Copyright (C) 1997-2000 Peter MacDonald and BrowseX Systems Inc.
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
**   peter@browsex.com
**   http://browsex.com
*/

#include <tk.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "htmlexts.h"
#ifdef USE_TK_STUBS
# include <tkIntXlibDecls.h>
#endif
#include <X11/Xatom.h>
#include <assert.h>

#define TOKEN_LIST	1
#define TOKEN_MARKUP	2
#define TOKEN_DOM	4
#define StrEqual(a,b) (a[0]==b[0] && (!strcmp(a,b)))
#define StrIEqual(a,b) (tolower(a[0])==tolower(b[0]) && (!strcasecmp(a,b)))

int (*HtmlPostscriptPtr)(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
);

#ifndef _TCLHTML_

extern HtmlTokenMap *HtmlGetMarkupMap(int n);
static int HtmlRadioCount(HtmlWidget *htmlPtr, HtmlElement *radio);

void BgImageChangeProc(
  ClientData clientData,    /* Pointer to an HtmlImage structure */
  int x,                    /* Left edge of region that changed */
  int y,                    /* Top edge of region that changed */
  int w,                    /* Width of region that changes.  Maybe 0 */
  int h,                    /* Height of region that changed.  Maybe 0 */
  int newWidth,             /* New width of the image */
  int newHeight             /* New height of the image */
){
  HtmlWidget *htmlPtr; 
  htmlPtr = (HtmlWidget*)clientData;
  HtmlRedrawEverything(htmlPtr);
}

/* For animated images, update the image list */
int HtmlImageUpdateCmd(
  HtmlWidget *htmlPtr, Tcl_Interp *interp, int argc, char **argv) {
  int id;
  char *z;
  HtmlElement *p;
  HtmlImage *pImage;
  HtmlElement* pElem;

  if( Tcl_GetInt(interp, argv[2], &id) != TCL_OK){
    return TCL_ERROR;
  }
  p = HtmlTokenByIndex(htmlPtr, id, 0);
  if (!p) return TCL_ERROR;
  if (p->base.type != Html_IMG) return TCL_ERROR;
  HtmlAddImages(htmlPtr, p, p->image.pImage, argv[3],0);
  return TCL_OK;
}

/* For animated images, add the image to list */
int HtmlImageAddCmd(
  HtmlWidget *htmlPtr, Tcl_Interp *interp, int argc, char **argv) {
  int id;
  char *z;
  HtmlElement *p;
  HtmlImage *pImage;
  HtmlElement* pElem;

  if( Tcl_GetInt(interp, argv[2], &id) != TCL_OK){
    return TCL_ERROR;
  }
  p = HtmlTokenByIndex(htmlPtr, id, 0);
  if (!p) return TCL_ERROR;
  if (p->base.type != Html_IMG) return TCL_ERROR;
  HtmlAddImages(htmlPtr, p, p->image.pImage, argv[3],1);
  return TCL_OK;
}


/* For animated images, set the currently active Image */
int HtmlImageSetCmd(
  HtmlWidget *htmlPtr, Tcl_Interp *interp, int argc, char **argv){
  int id, idx;
  char *z;
  HtmlElement *p;
  HtmlImage *pImage;
  HtmlElement* pElem;

  if( Tcl_GetInt(interp, argv[2], &id) != TCL_OK 
   || Tcl_GetInt(interp, argv[3], &idx) != TCL_OK
  ){
    return TCL_ERROR;
  }
  if (idx<0) return TCL_ERROR;
  p = HtmlTokenByIndex(htmlPtr, id, 0);
  if (!p) return TCL_ERROR;
  if (p->base.type != Html_IMG) return TCL_ERROR;
  pImage=p->image.pImage;
  pImage->cur=idx;
  for(pElem = pImage->pList; pElem; pElem = pElem->image.pNext)
    pElem->image.redrawNeeded = 1;
  htmlPtr->flags |= REDRAW_IMAGES;
  HtmlScheduleRedraw(htmlPtr);
  return TCL_OK;
}

/* Return 1 if item given by id is on visible screen */
int HtmlOnScreen(
  HtmlWidget *htmlPtr, Tcl_Interp *interp, int argc, char **argv){
  int id, x, y, w, h;
  char *z;
  HtmlElement *p;
  HtmlImage *pImage;
  HtmlElement* pElem;
  Tk_Window clipwin = htmlPtr->clipwin;

  if( Tcl_GetInt(interp, argv[2], &id) != TCL_OK ){
    return TCL_ERROR;
  }
  if( !Tk_IsMapped(htmlPtr->tkwin) ){
    Tcl_AppendResult(interp, "0", 0);
    return TCL_OK;
  }
  w = Tk_Width(clipwin);
  h = Tk_Height(clipwin);
  x = htmlPtr->xOffset;
  y = htmlPtr->yOffset;

  p = HtmlTokenByIndex(htmlPtr, id, 0);
  if (!p) return TCL_ERROR;
  Tcl_AppendResult(interp, "1", 0);
  return TCL_OK;
}

int HtmlAttrOverCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  int x, y, n;
  char z[50];

  if( Tcl_GetInt(interp, argv[2], &x) != TCL_OK 
   || Tcl_GetInt(interp, argv[3], &y) != TCL_OK
  ){
    return TCL_ERROR;
  }
  HtmlGetAttrOver(htmlPtr, x + htmlPtr->xOffset, y + htmlPtr->yOffset, argv[4]);
  return TCL_OK;
}

int HtmlOverCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  int x, y, n;
  char z[50];

  if( Tcl_GetInt(interp, argv[2], &x) != TCL_OK 
   || Tcl_GetInt(interp, argv[3], &y) != TCL_OK
  ){
    return TCL_ERROR;
  }
  HtmlGetOver(htmlPtr, x + htmlPtr->xOffset, y + htmlPtr->yOffset, argc>4);
  return TCL_OK;
}

/*
** WIDGET image X Y
**
** Returns the image src name  that is beneath the position X,Y.
** Returns {} if there is no image beneath X,Y.
*/
int HtmlImageAtCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  int x, y, n;
  char z[50];

  if( Tcl_GetInt(interp, argv[2], &x) != TCL_OK 
   || Tcl_GetInt(interp, argv[3], &y) != TCL_OK
  ){
    return TCL_ERROR;
  }
  n = HtmlGetImageAt(htmlPtr, x + htmlPtr->xOffset, y + htmlPtr->yOffset);
  sprintf(z,"%d",n);
  Tcl_SetResult(interp, z, TCL_VOLATILE);
  return TCL_OK;
}

/* Set a background image for page, or table element. */
int HtmlSetImageBg(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  char *imgname,            
  HtmlElement *p
){
  Tk_Image bgimg, *nimg;
  int i;
  if (!imgname) bgimg=0;
  else
    bgimg = Tk_GetImage(htmlPtr->interp, htmlPtr->clipwin,
                                imgname, BgImageChangeProc, htmlPtr);
  if (!p)
    nimg=&htmlPtr->bgimage;
  else {
    switch (p->base.type) {
      case Html_TABLE:	
	nimg=&p->table.bgimage;
	break;
      case Html_TR:	
	nimg=&p->ref.bgimage;
	break;
      case Html_TH:	
      case Html_TD:	
	nimg=&p->cell.bgimage;
	break;
      default:
        Tcl_AppendResult(interp,"bg index not TABLE,TD,TR, or TH:", 0);
        return TCL_ERROR;
    }
  }
  if (*nimg) {
     Tk_FreeImage(*nimg);
  }
  *nimg=bgimg;
  HtmlRedrawEverything(htmlPtr);
  return TCL_OK;
}

/* Set a background image for page, or table element. */
int HtmlImageBgCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement *p;
  int i;
  if (argc==3)
    return HtmlSetImageBg(htmlPtr, interp, argv[2], 0);
  if( HtmlGetIndex(htmlPtr, argv[3], &p, &i)!=0 || !p){
    Tcl_AppendResult(interp,"malformed index: \"", argv[3], "\"", 0);
    return TCL_ERROR;
  }
  return HtmlSetImageBg(htmlPtr, interp, argv[2], p);
}

/* Return HTML with all image link names substitued with indexed. */
int HtmlImagesListCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement *p; char *z, buf[BUFSIZ];
  int ishtml=1, icnt=0;
  if (!strcmp(argv[2],"list")) ishtml=0;
  else if (strcmp(argv[2],"html")) {
    Tcl_AppendResult(interp,"invalid args", 0);
    return TCL_ERROR;
  }
  p=htmlPtr->pFirst;
  while(p) {
    if (ishtml) {
      switch( p->base.type ){
	case Html_IMG:
	  sprintf(buf,"<img src=%d.img>",icnt++);
	  Tcl_AppendResult(interp,buf,0);
	  break;
	default:
          HtmlToken2Txt(interp,p);
      }
    } else {
      if (p->base.type==Html_IMG){
	z= HtmlMarkupArg(p, "src", 0);
	if (z) z=HtmlResolveUri(htmlPtr, z);
        if (z) {
          Tcl_AppendResult(interp,z," ",0);
          HtmlFree(z);
	}
      }
    }
    p=p->pNext;
  }
  return TCL_OK;
}

int HtmlPostscriptCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
#ifdef USE_TCL_STUBS
  if (!HtmlPostscriptPtr) {
    Tcl_AppendResult(interp,"postscript command unimplemented", 0);
    return TCL_ERROR;
  }
  return HtmlPostscriptPtr(htmlPtr,interp,argc,argv);
#else
  return HtmlPostscript(htmlPtr,interp,argc,argv);
#endif
}

/*
** WIDGET coords INDEX	
*/
int HtmlCoordsCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement *p;
  int i, pct=0;

  HtmlLock(htmlPtr);
  if (argc<=2) {
    char wh[40];
    if( !HtmlUnlock(htmlPtr) ){
      sprintf(wh,"%d %d",htmlPtr->maxX,htmlPtr->maxY);
      Tcl_AppendResult(interp,wh,0);
    }
    return TCL_OK;
  }
  if( HtmlGetIndex(htmlPtr, argv[2], &p, &i)!=0 ){
    if( !HtmlUnlock(htmlPtr) ){
      Tcl_AppendResult(interp,"malformed index: \"", argv[2], "\"", 0);
    }
    return TCL_ERROR;
  }
  if (argc>3 && !strcmp(argv[3],"percent")) pct=1;
  if( !HtmlUnlock(htmlPtr) && p ){
    HtmlGetCoords(interp, htmlPtr, p, i, pct);
  }
  return TCL_OK;
}

int
HtmlFetchSelection(
    ClientData clientData,              /* Information about html widget. */
    int offset,                         /* Offset within selection of first
                                         * character to be returned. */
    char *buffer,                       /* Location in which to place
                                         * selection. */
    int maxBytes)                       /* Maximum number of bytes to place
                                         * at buffer, not including terminating
                                         * NULL character. */
{
  HtmlWidget *htmlPtr=(HtmlWidget*)clientData;
  int count;

  if (!htmlPtr->exportSelection) return 0;
  if ((!htmlPtr->selEnd.p) || (!htmlPtr->selEnd.p)) return 0;
  count=HtmlAscii2Buf(htmlPtr->interp, &htmlPtr->selBegin, &htmlPtr->selEnd,
    buffer, maxBytes, offset); 
  buffer[count]=0;
  return count;
}

#if 0
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
#endif

#define DUMPIMGDRAW      printf("BGIM: mx/my=%d/%d",  mx, my); \
      printf("sx/sy=%d,%d:   ",sx,sy); \
      printf("sw/sh=%d,%d ",  sw,sh); \
      printf("left/top=%d,%d ",  left,top); \ \
      printf("w/h=%d,%d ",  w,h);

int HtmlBGDraw(HtmlWidget *htmlPtr, int left, int top, int w, int h, 
  Pixmap pixmap, Tk_Image image) {
  int iw, ih, mx, my, dl, dt, sh, sw, sx, sy;
  dl=htmlPtr->dirtyLeft, dt=htmlPtr->dirtyTop;
  Tk_SizeOfImage(image,&iw,&ih);
  if (iw<4 && ih<4) return 0;  /* CPU Burners we ignore. */
  /* if (iw<=4 || ih<=4) return 0;  Buggy gifs? */
  sx=(left+dl)%iw;	/* X offset within image to start from */
  sw=(iw-sx);		/* Width of section of image to draw. */
  for (mx=0; mx<w; mx+=sw, sw=iw, sx=0) {
    sy=(top+dt)%ih;	/* Y offset within image to start from */
    sh=(ih-sy);		/* Height of section of image to draw. */
    for (my=0; my<h; my+=sh, sh=ih, sy=0) {
   /*printf("Tk_RedrawImage: %d %d %d %d %d %d\n", sx, sy, sw, sh, mx,my);*/
    Tk_RedrawImage(image, sx, sy, sw, sh, pixmap,mx,my);
    }
  }
  return 1;
}

int HtmlTblBGDraw(HtmlWidget *htmlPtr, int l, int t, int w, int h, 
  Pixmap pixmap, Tk_Image image) {
  int iw, ih, mx, my, dl, dt, dr, db, sh, sw, sx, sy, left=l, top=t,
    right, bottom, hd;
  left-=htmlPtr->xOffset;
  top-=htmlPtr->yOffset;
  dl=htmlPtr->dirtyLeft; dt=htmlPtr->dirtyTop;
  dr=htmlPtr->dirtyRight; db=htmlPtr->dirtyBottom;
  right=left+w-1;
  bottom=top+h-1;
  if (dr==0 && db==0) { dr=right; db=bottom; }
  if (left>dr || right<dl || top>db || bottom<dt) return 0;
  Tk_SizeOfImage(image,&iw,&ih);
  if (iw<4 && ih<4) return 0;  /* CPU Burners we ignore. */
  sx=(dl<left?0:(left-dl)%iw);	/* X offset within image to start from */
  sw=(iw-sx);		/* Width of section of image to draw. */
  for (mx=left-dl; w>0; mx+=sw, sw=iw, sx=0) {
    if (sw>w) sw=w;
    sy=(dt<top?0:(top-dt)%ih);	/* Y offset within image to start from */
    sh=(ih-sy);		/* Height of section of image to draw. */
    for (my=top-dt, hd=h; hd>0; my+=sh, sh=ih, sy=0) {
      if (sh>hd) sh=hd;
  /* printf("Tk_RedrawImage: %d %d %d %d %d %d\n", sx, sy, sw, sh, mx,my);  */
      Tk_RedrawImage(image, sx, sy, sw, sh, pixmap,mx,my);
      hd-=sh;
    }
    w-=sw;
  }
  return 1;
}

/*
** This routine searchs for an image beneath the coordinates x,y
** and returns src name to the image.  The text
** is held one of the markup.argv[] fields of the <a> markup.
*/
int HtmlGetImageAt(HtmlWidget *htmlPtr, int x, int y){
  HtmlBlock *pBlock;
  HtmlElement *pElem;
  int n;

  for(pBlock=htmlPtr->firstBlock; pBlock; pBlock=pBlock->pNext){
    if( pBlock->top > y || pBlock->bottom < y
     || pBlock->left > x || pBlock->right < x
    ){
      continue;
    }
    for (pElem = pBlock->base.pNext; pElem; pElem=pElem->pNext) {
      if (pBlock->pNext && pElem==pBlock->pNext->base.pNext) break;
      if (pElem->base.type==Html_IMG) {
	return HtmlTokenNumber(pElem);
      }
    }
  }
  return -1;
}

/* Find all attrs in attr list if over object. */
int HtmlGetAttrOver(HtmlWidget *htmlPtr, int x, int y, char *attr){
  HtmlBlock *pBlock;
  HtmlElement *pElem;
  int n=0, vargc, i, j; char *z, *az, **vargv;

  if (Tcl_SplitList(htmlPtr->interp, attr,&vargc,&vargv) || vargc<=0) {
    Tcl_AppendResult(htmlPtr->interp,"attrover error: ",attr,0);
    return TCL_ERROR;
  }

  for(pBlock=htmlPtr->firstBlock; pBlock; pBlock=pBlock->pNext){
    if( pBlock->top > y || pBlock->bottom < y
     || pBlock->left > x || pBlock->right < x
    ){
      continue;
    }
    for (pElem = pBlock->base.pNext; pElem; pElem=pElem->pNext) {
      if (pBlock->pNext && pElem==pBlock->pNext->base.pNext) break;
      if (HtmlIsMarkup(pElem)) {
	char nbuf[50];  int fnd=0;
        for(i=0; i<pElem->base.count; i+=2){
          az=pElem->markup.argv[i];
	  for (j=0; j<vargc; j++)
            if (az[0]==vargv[j][0] && (!strcmp(az,vargv[j]))) {fnd=1; break; }
	  if (j<vargc) break;
        }
	if (fnd) {
	   sprintf(nbuf,"%d ", HtmlTokenNumber(pElem));
	   Tcl_AppendResult(htmlPtr->interp,nbuf,0);
        }
      }
    }
  }
  HtmlFree(vargv);
  return TCL_OK;
}

int HtmlGetOver(HtmlWidget *htmlPtr, int x, int y, int justmarkup){
  HtmlBlock *pBlock;
  HtmlElement *pElem;
  int n=0, i, j; char *z, *az;

  for(pBlock=htmlPtr->firstBlock; pBlock; pBlock=pBlock->pNext){
    if( pBlock->top > y || pBlock->bottom < y
     || pBlock->left > x || pBlock->right < x
    ){
      continue;
    }
    for (pElem = pBlock->base.pNext; pElem; pElem=pElem->pNext) {
      char nbuf[50];
      if (pBlock->pNext && pElem==pBlock->pNext->base.pNext) break;
      if (HtmlIsMarkup(pElem) || (!justmarkup)) {
	sprintf(nbuf,"%d ", HtmlTokenNumber(pElem));
	Tcl_AppendResult(htmlPtr->interp,nbuf,0);
      }
    }
  }
  return TCL_OK;
}


/* Return the form colors, etc */
int OldHtmlFormColors(HtmlWidget *htmlPtr, int fid){
  HtmlElement *p;
  for(p=htmlPtr->pFirst; p; p=p->pNext){
    if (p->base.type==Html_FORM) {
      if(p->form.id==fid) {
        char buf[BUFSIZ];
        int bg=p->base.style.bgcolor;
        int fg=p->base.style.color;
        XColor *cbg=htmlPtr->apColor[bg];
        XColor *cfg=htmlPtr->apColor[fg];
        sprintf(buf,"%s %s", Tk_NameOfColor(cfg), Tk_NameOfColor(cbg));
        Tcl_AppendResult(htmlPtr->interp, buf,0);
        return TCL_OK;
      }
    }
  }
  return TCL_OK;
}

char *Clr2Name(char *str) {
  static char buf[50];
  if (str[0]=='#') {
    strcpy(buf,str);
    buf[17]=0;
  } else {
    int l=strlen(str), n=strspn(str,"abcdefABCDEF0123456789");
    if (n == l) {
      buf[0]='#';
      strncpy(buf+1,str,16);
      buf[17]=0;
    } else strcpy(buf,str);
  }
  return buf;
}

/* Return the form colors, etc */
int HtmlFormColors(HtmlWidget *htmlPtr, int fid, int n){
  HtmlElement *p, *pf=0;
  for(p=htmlPtr->pFirst; p; p=p->pNext){
    if (p->base.type==Html_INPUT) {
      if (!p->input.pForm) continue;
      if (p->input.pForm->form.id!=fid) continue;
      pf=p;
      if (--n) break; /* Not working properly */
    }
  }
  if (pf) {
    char buf[BUFSIZ], *c1, *c2;
    int bg=pf->base.style.bgcolor;
    int fg=pf->base.style.color;
    XColor *cbg=htmlPtr->apColor[bg];
    XColor *cfg=htmlPtr->apColor[fg];
#if 0
    sprintf(buf,"%s %s", Tk_NameOfColor(cfg), Tk_NameOfColor(cbg));
#else
     c1=Tk_NameOfColor(cfg);
    strcpy(buf,Clr2Name(c1));
    c2=Tk_NameOfColor(cbg);
    strcat(buf," ");
    strcat(buf,Clr2Name(c2));
#endif
    Tcl_AppendResult(htmlPtr->interp, buf,0);
    return TCL_OK;
  }
  return TCL_OK;
}

/* Get Form info. */
int HtmlFormInfo(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  if (argc>=4)
    return HtmlFormColors(htmlPtr, atoi(argv[2]), atoi(argv[3]));
  return TCL_OK;
}


#endif /*  _TCLHTML_ */

void HtmlToken2Txt(Tcl_Interp *interp, HtmlElement *p){
  static char zBuf[BUFSIZ];
  int j;
  char *zName;

  if( p==0 ) return;
  switch( p->base.type ){
  case Html_Text:
    Tcl_AppendResult(interp,p->text.zText,0);
    break;
  case Html_Space:
    if( p->base.flags & HTML_NewLine ){
      Tcl_AppendResult(interp,"\"\\n\"",0);
    }else{
      Tcl_AppendResult(interp,"\" \"",0);
    }
    break;
  case Html_Block:
    break;
  default:
    if( p->base.type >= HtmlGetMarkupMap(0)->type
    && p->base.type <= HtmlGetMarkupMap(HTML_MARKUP_COUNT-1)->type ){
      zName = HtmlGetMarkupMap(p->base.type - HtmlGetMarkupMap(0)->type)->zName;
    }else{
      zName = "Unknown";
    }
    Tcl_AppendResult(interp,"<",zName,0);
    /* ??? Doesn't work */
    for(j=1; j<p->base.count; j += 2){
      Tcl_AppendResult(interp, " ",p->markup.argv[j-1],"=",p->markup.argv[j]);
    }
    Tcl_AppendResult(interp,">",0);
    break;
  }
}

HtmlTokenMap *HtmlTypeToPmap(int typ) {
  HtmlTokenMap *pMap=HtmlMarkupMap+typ-Html_A; /* ??? Dangerous */
  if (typ<0 || typ>Html_TypeCount) return 0;
  return pMap;
}

int HtmlGetEndToken(int typ){
  HtmlTokenMap *pMap=HtmlTypeToPmap(typ);
  if (!pMap) return Html_Unknown;
  if (pMap && pMap[1].zName[0]=='/')
    return pMap[1].type;
  return Html_Unknown;
}

int HtmlNameToTypeAndEnd(char *zType, int *end){
  HtmlTokenMap *pMap=HtmlNameToPmap(zType);
  if (*end) *end=Html_Unknown;
  if (!pMap) return Html_Unknown;
  if (pMap[1].zName[0]=='/')
    *end=pMap[1].type;
  return pMap->type;
}

#define DOMMAXTOK 128
static char *TagAliases[] = {
  "anchor", "a",
  "link", "a",
  "row", "tr",
  "rows", "tr",
  "col", "td",
  "cols", "td",
  "column", "td",
  "columns", "td",
  "element", "input",
  "elements", "input",
  "options", "option",
  0,0
};

static int HtmlDomSubEl(char *tok, int *en) {
  int n, i, j;
  for (i=0; tok[i]; i++) {
    tok[i]=tolower(tok[i]);
  }
  if (!i) {
    return Html_Unknown;
  }
  for (j=0; TagAliases[j]; j+=2) {
    if (StrEqual(tok,TagAliases[j])) {
      strcpy(tok,TagAliases[j+1]);
      break;
    }
  }
  n=HtmlNameToTypeAndEnd(tok,en);
  if (n==Html_Unknown) {
    if (tok[--i] != 's') return Html_Unknown;
    tok[i]=0;
    n=HtmlNameToTypeAndEnd(tok,en);
  }
  return n;
}


static HtmlElement *HtmlFindEndPOther( HtmlWidget *htmlPtr, HtmlElement *sp,
  HtmlElement *op) {
  HtmlElement *p=sp;
  while (p) {
    if (p->base.type==sp->base.type) {
      if (p->ref.pOther==op) return p;
    }
    p=p->base.pNext;
  }
  return p;
}
 
/* Find End tag en, but ignore intervening begin/end tag pairs. */
static HtmlElement *HtmlFindEndNest(
  HtmlWidget *htmlPtr, 
  HtmlElement *sp,	/* Pointer to start from. */
  int en, 		/* End tag to search for */
  HtmlElement *lp	/* Last pointer to try. */
) {
  HtmlElement *p=sp->pNext;
  int lvl=0, n=sp->base.type;
  while (p) {
    if (p==lp) return 0;
    if (n==Html_LI) {
	if (p->base.type==Html_LI || p->base.type==Html_EndUL ||
	    p->base.type==Html_EndOL) {
          if (p->base.pPrev) return p->base.pPrev;
          return p;
	}
    } else if (p->base.type==n) {
      if (n==Html_OPTION) {
        if (p->base.pPrev) return p->base.pPrev;
        return p;
      }
      lvl++;
    } else if (p->base.type==en) {
      if (!lvl--) return p;
    }
    switch (p->base.type) {
      case Html_TABLE: p=p->table.pEnd; break; /* Optimization */
      case Html_FORM:  p=p->form.pEnd;  break;
      default: p=p->base.pNext;
    }
  }
  return 0;
}

/* Return element ptr to matching end tag, if any. For T? ignore nested tables. */
static HtmlElement *HtmlFindEndTag(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  HtmlElement *p,
  char *tok,int n, int en, 
  HtmlElement *tp) {
  HtmlElement *ep=p->base.pNext;
  int lvl=0;
  char buf[DOMMAXTOK+1];

  if (en==Html_Unknown) return p;
  switch (n) {
    case Html_TH:
    case Html_TR:
    case Html_TD: goto potherelem;
    case Html_FORM: goto nestelem;
    case Html_INPUT: {
      n=p->input.type;
    }
    default:
      while (ep) {
        if (ep->base.type==en) break;
        ep=ep->base.pNext;
      }
      return ep;
    }
  return 0;
/*      buf[0]='/';
      strcpy(buf+1,tok);
      if ((en=HtmlNameToType(tok))==Html_Unknown) return 0;
      while (ep && ep->base.type!=en) ep=ep->base.pNext;
      return ep;
    */

potherelem: /* Find matching end tag via pOther field. */
    while (ep) {
      if (ep->base.type==tp->base.type) return 0;
      else if (ep->base.type==en) {
        if (ep->ref.pOther==tp) return ep;
      }
      ep=ep->base.pNext;
    }
    return ep;

nestelem: /* Find matching end tag via nesting counter. */
      while (ep) {
        if (ep->base.type==n) lvl++;
        else if (ep->base.type==en) {
	  if (!lvl--) return p;
	}
        ep=ep->base.pNext;
      }
      return ep;
}

#define DOMFORMEQ(n) (n==Html_INPUT || n==Html_SELECT || n==Html_TEXTAREA)
#define DOMTAGEQ(n,t) (n==t || (DOMFORMEQ(n) && DOMFORMEQ(t)))
/* Get the index'th element of type n.
   When index is "", format integer value of max index into tok.
 */
static HtmlElement *HtmlDOMGetIndex(
  HtmlWidget *htmlPtr, 
  HtmlElement *p, 
  Tcl_Interp *interp, 
  int n, 	/* Tag to search for */
  int en,	/* End tag */
  char *tok,
  char *a,
  int *ip,
  HtmlElement *tp,	/* Top pointer, tag were nested in. */
  HtmlElement *tlim,	/* Limit to stop at */
  int aflag) {
  int i=*ip, j, k, l, idx=0;
  char buf[DOMMAXTOK], *z;
  if (a[i+1]=='\"') i++;
  for (j=i+1, k=0; a[j]!=']' && a[j]!='\"' && a[j]!=')' && k<DOMMAXTOK; j++, k++) buf[k]=a[j];
  if (a[j]=='\"') j++;
  if ( (a[*ip]=='(' && a[j]==')') || (a[*ip]=='[' && a[j]==']')) {
    int isint=0;
    buf[k]=0;
    if (!k) {
      if ((idx=HtmlFormCount(htmlPtr,p,0))>0)
          goto fmtidx;
      idx=0;
    }
    for (l=0; l<k && isdigit(buf[l]); l++) ;
    if (l==k) { idx=atoi(buf); isint=1; }
    while (p) {
      if (p == tlim) {
	p=0; break;
      }
      if (DOMTAGEQ(n,p->base.type)) {
        if (isint) {
          if (aflag) {
	    z=HtmlMarkupArg(p, "href", 0);
            if ((aflag==2 && !z) || (aflag==1 && z)) {
	      p=p->base.pNext; continue;
	    }
	  }
          if (k>0) {
            if (idx) idx--;
            else break;
	  } else {
	    idx++;
	  }
        } else {
          z= HtmlMarkupArg(p, "name", 0);
          if (z && (!strcmp(z,buf))) {
            break;
          }
        }
        if (en != Html_Unknown)
          p=HtmlFindEndNest(htmlPtr,p,en,0);
          /*p=HtmlFindEndTag(htmlPtr,p,tok,n, en,tp); */
      }
      if (p) p=p->base.pNext;
    }
    if (!k) goto fmtidx;
    if (!p) {
      Tcl_AppendResult(interp, "DOM element not found: ", tok,"(",buf,")", 0);
      return 0;
    }
  } else {
    Tcl_AppendResult(interp, "invalid index: ", tok, 0);
    return 0;
  }
  *ip=j+1;
  return p;

fmtidx:
  sprintf(tok,"%d",idx);
  *ip=*ip+1;
  return 0;
}

/* Get elements ala DOM style. eg.
  $w dom id table
  $w dom id table(4)
  $w dom value table(4).row(0)
  $w dom value table(4).row(0).col(0).value 99
  $w dom value form(4).elements(0).value "Dogmeat"
  $w dom value form(4).textarea.value "Dogmeat"
  $w dom value form[4].elements[0].value  # Also accepts square brackets.
  $w dom value form["SALES"].elements["NAME"].value  # or use name attr as index.
*/
int HtmlDomIdLookup(
  HtmlWidget *htmlPtr,
  char *cname,
  const char *dname,
  HtmlElement **pp
) {
  Tcl_DString cmd;

  Tcl_Interp *interp=htmlPtr->interp;
  char tok[DOMMAXTOK], *a, *z;
  HtmlElement *p, *ep, *tp=0, *tlim=0;
  int n=0, ni=0, en, i, iswrite=0, aflag;
  int isvalue=!strcmp(cname,"value");
  a=(char*)dname;

  p=htmlPtr->pFirst;
  while (a[0]) {
    aflag=0;
    if ((a[0]=='(' || a[0]=='[') && (a[1]==')' || a[1]==']') && !a[2]) {
      if (isvalue || p->base.type!=Html_INPUT) goto idluperr;
      if (!(z=HtmlMarkupArg(p,"type",0))) goto idluperr;
      if (strcmp(z,"radio")) goto idluperr;
      sprintf(tok,"%d",HtmlRadioCount(htmlPtr,p));
      Tcl_AppendResult(interp, tok,0);
      return TCL_OK;
    }
    for (i=0; isalnum(a[i]) && i<DOMMAXTOK; i++) {
      tok[i]=a[i];
    }
    tok[i]=0;
    if (isvalue && (!a[i])) {
      z=HtmlMarkupArg(p,tok,0);
      Tcl_AppendResult(interp, z?z:"",0);
      return TCL_OK;
    }
    if ((n=HtmlDomSubEl(tok,&en)) == Html_Unknown) {
    /*  Tcl_AppendResult(interp, "Unknown DOM markup: ", a, 0);
      return TCL_ERROR; */
      return TCL_OK;
    } 
    if (StrEqual(tok,"anchor")) { aflag=1; n=Html_A; en=Html_EndA; }
    else if (StrEqual(tok,"link")) { aflag=2; n=Html_A; en=Html_EndA; }
    ni++;
    if (a[i]=='(' || a[i]=='[') {
      int savei=i;
      if (!((p=HtmlDOMGetIndex(htmlPtr,p,interp,n,ni==1?Html_Unknown:en,tok,a,&i, tp, tlim, aflag)))) {
        if (savei==(i-1)) {
	  Tcl_SetResult(interp,tok,TCL_VOLATILE);
	  if (pp) {
	    *pp=p;
	    Tcl_ResetResult(interp);
	  }
	  return TCL_OK;
	}
	Tcl_AppendResult(interp,"Invalid index",0);
	return TCL_ERROR;
      }
      if (ni) {
        switch (p->base.type) {
          case Html_TABLE: tlim=HtmlFindEndNest(htmlPtr,p,Html_EndTABLE,0); break;
          case Html_TD: tlim=HtmlFindEndNest(htmlPtr,p,Html_EndTD,0); break;
          case Html_TR: tlim=HtmlFindEndNest(htmlPtr,p,Html_EndTR,0); break;
	  case Html_FORM: tlim=HtmlFindEndNest(htmlPtr,p,Html_EndFORM,0); break;
	  case Html_UL: tlim=HtmlFindEndNest(htmlPtr,p,Html_EndUL,0); break;
	  case Html_OL: tlim=HtmlFindEndNest(htmlPtr,p,Html_EndOL,0); break;
	  case Html_DL: tlim=HtmlFindEndNest(htmlPtr,p,Html_EndDL,0); break;
        }
	tp=p;
      }
      a=a+i;
    }
    if (*a=='.') {
      a++;
    } else if (*a) {
       Tcl_AppendResult(htmlPtr->interp, "Unexpected char ",a," in tok ",dname,0);
       return TCL_ERROR;
    }
  }

  if (ni && isvalue) {
    Tcl_DStringInit(&cmd);
    HtmlAppendArglist(&cmd, p);
    Tcl_DStringResult(interp, &cmd);
    return TCL_OK;
  }

  if (!strcmp(cname,"ids")) {
    if (!tlim)
      tlim=HtmlFindEndTag(htmlPtr,p->pNext,tok,n, en, tp);
    if (p->base.type == Html_OPTION) {
      tlim=p->pNext;
      while (tlim) {
	int ty=tlim->base.type;
	if (ty==Html_EndOPTION || ty==Html_EndSELECT || ty==Html_EndFORM ||
           ty==Html_OPTION || ty==Html_INPUT) break;
	tlim=tlim->pNext;
      }
    }
    sprintf(tok,"%d %d",p?HtmlTokenNumber(p):-1,tlim?HtmlTokenNumber(tlim):-1);
  } else
    sprintf(tok,"%d",p?HtmlTokenNumber(p):-1);
  Tcl_AppendResult(interp, tok, 0);
  if (pp) {
    Tcl_ResetResult(interp);
    *pp=p;
  }
  return TCL_OK;

idluperr:
  Tcl_AppendResult(interp, "Error in dom id", cname, " ", dname, 0);
  return TCL_ERROR;
}

int HtmlDomCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  return HtmlDomIdLookup(htmlPtr, argv[2], argv[3], 0);
}


/* Count the number of tags of tp->typ before tp (inclusive). */
int HtmlCountTagsBefore(HtmlWidget *htmlPtr, int tag, HtmlElement *tp, int etag) {
  HtmlElement *p;
  int i=0;
  if (etag != Html_Unknown) {
    p=tp;
    while (p) {
      if (p->base.type == tag) i++;
      if (p->base.type==etag) return i;
      p=p->base.pPrev;
    }
    return i;
  }
  p=htmlPtr->pFirst;
  while (p) {
    if (tag == p->base.type) i++;
    if (tp==p) return i;
    p=p->base.pNext;
  }
  return 0;
}

HtmlElement *HtmlInObject(HtmlElement *p, int tag, int endtag) {
  int lvl=0;
  p=p->base.pNext;
  while (p) {
    if (p->base.type==tag)
      lvl++;
    else if (p->base.type==endtag) 
      if (!(lvl--))
        break;
    p=p->base.pNext;
  }
  return p;
}

/* If inside tag, format the subindex. */
int HtmlDOMFmtSubIndexGen(HtmlWidget *htmlPtr, HtmlElement *pStart, Tcl_DString *cmd,
  int tag, char *str, int pretag, HtmlElement *tp, int nostr) {
  char *z;
  if (!tp) return 0;
  if (pretag!=Html_Unknown) Tcl_DStringAppend(cmd, ".", -1);
  Tcl_DStringAppend(cmd, str, -1);
  Tcl_DStringAppend(cmd, "(", -1);
  if ((!nostr) && (z=HtmlMarkupArg(tp,"name",0))) {
    Tcl_DStringAppend(cmd, "\"", -1);
    Tcl_DStringAppend(cmd, z, -1);
    Tcl_DStringAppend(cmd, "\"", -1);
  } else {
    char buf[50];
    sprintf(buf,"%d",HtmlCountTagsBefore(htmlPtr, tag, pStart, pretag)-1);
    Tcl_DStringAppend(cmd, buf, -1);
  }
  Tcl_DStringAppend(cmd, ")", -1);
  return 1;
}

HtmlElement *HtmlFindBefore( HtmlElement *p, int tag) {
  while (p) {
    if (p->base.type == tag) return p;
    p=p->base.pPrev;
  }
  return 0;
}

/* If inside tag, format the subindex. */
int HtmlDOMFmtSubIndex(
  HtmlWidget *htmlPtr, 
  HtmlElement **pStart, 
  Tcl_DString *cmd,
  int tag, 		/* Tag we are formatting for */
  int endtag, 
  char *str, 
  int pretag, 
  HtmlElement *tp,
  int nostr
) {
  HtmlElement *ep, *p=*pStart;
/*  while (p && p->base.type != tag) p=p->pNext;
  *pStart=p; */
  if (!p) return 0;
  if ((ep=HtmlInObject(p,tag,endtag))) {
    if (!tp) {
      if (!ep) return 0;
      if (ep->base.type != Html_EndLI)
        tp=ep->ref.pOther;
      else
        tp=HtmlFindBefore(ep,Html_LI);
    }
    return HtmlDOMFmtSubIndexGen(htmlPtr, p, cmd, tag, str, pretag, tp, nostr);
  }
  return 0;
}

/* Given an ID, return the DOM style string address for the item.
   eg. tables[0].rows[1].columns[2] or hr[5].  
   You can specify -tag table to try a table spec first.
   */
int HtmlIdToDomCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  Tcl_DString cmd;
  char buf[100], *a=argv[3], *z;
  int try[10], ti=0, en, i=0, k, j, l, n, iswrite=0, atend, lvl=0;
  int sc=1, nostr=0; /* Short-circuit */
  HtmlElement *p, *tp=0, *fp=0;
  HtmlElement *pStart=0;
  Tcl_DStringInit(&cmd);
  try[i++]=Html_FORM;
  try[i++]=Html_TABLE;
  try[i++]=Html_UL;
  try[i++]=Html_Unknown;
  try[i++]=-1;

  if( HtmlGetIndex(htmlPtr, argv[3], &pStart, &i)!=0 ){
    Tcl_AppendResult(interp,"malformed index: \"", argv[3], "\"", 0);
    return TCL_ERROR;
  }
  if (!pStart) return TCL_OK;
  n=4;
  while ((n+1)<argc) {
    if (!strcmp(argv[n],"-tag")) {
      if ((k=HtmlNameToType(argv[n+1]))!= Html_Unknown) {
	for (j=i; j>0; j--) try[j]=try[j-1];
	try[0]=k;
      }
    } else if (!strcmp(argv[n],"-nostring")) {
      nostr=atoi(argv[n+1]);
    } else {
      Tcl_AppendResult(interp,"dom addr: unknown flag: \"", argv[n], "\"", 0);
      return TCL_ERROR;
    }
    n+=2;
  }
  while (try[ti]>=0) {
    tp=pStart;
    switch (try[ti++]) {
     case Html_UL:
       if (HtmlDOMFmtSubIndex(htmlPtr, &tp, &cmd, Html_UL, Html_EndUL, "ul", Html_Unknown,0, nostr)) {
          if (sc && tp->base.type==Html_UL) goto domfmtdone;
          if (HtmlDOMFmtSubIndex(htmlPtr, &tp, &cmd, Html_LI, Html_EndLI, "li", Html_UL,0, nostr)){
	  }
	  goto domfmtdone;
       } else if (HtmlDOMFmtSubIndex(htmlPtr, &tp, &cmd, Html_OL, Html_EndOL, "ol", Html_Unknown,0, nostr)) {
          if (sc && tp->base.type==Html_OL) goto domfmtdone;
          if (HtmlDOMFmtSubIndex(htmlPtr, &tp, &cmd, Html_LI, Html_EndLI, "li", Html_UL,0, nostr)){
	  }
	  goto domfmtdone;
       }
      break;
     case Html_FORM:
      if (HtmlDOMFmtSubIndex(htmlPtr, &tp, &cmd, Html_FORM, Html_EndFORM, "forms", Html_Unknown,0, nostr)){
        if (sc && tp->base.type==Html_FORM) goto domfmtdone;
        if (tp->base.type==Html_INPUT &&
          HtmlDOMFmtSubIndexGen(htmlPtr, tp, &cmd, Html_INPUT, "elements", Html_FORM,tp, nostr)){
          if (sc && tp->base.type==Html_INPUT) goto domfmtdone;
        } else if (HtmlDOMFmtSubIndex(htmlPtr, &tp, &cmd, Html_SELECT, Html_EndSELECT, "elements", Html_FORM,0, nostr)){
          if (sc && tp->base.type==Html_SELECT) goto domfmtdone;
        } else if (HtmlDOMFmtSubIndex(htmlPtr, &tp, &cmd, Html_TEXTAREA, Html_EndTEXTAREA, "elements", Html_FORM,0, nostr)){
          if (sc && tp->base.type==Html_TEXTAREA) goto domfmtdone;
        }
        goto domfmtdone;
      }
      break;
     case Html_TABLE:
      if (HtmlDOMFmtSubIndex(htmlPtr, &tp, &cmd, Html_TABLE, Html_EndTABLE, "tables", Html_Unknown, 0, nostr)){
        if (sc && tp->base.type==Html_TABLE) goto domfmtdone;
        if (HtmlDOMFmtSubIndex(htmlPtr, &tp, &cmd, Html_TR, Html_EndTR, "rows", Html_TABLE,0, nostr)) {
          if (sc && tp->base.type==Html_TR) goto domfmtdone;
          if (HtmlDOMFmtSubIndex(htmlPtr, &tp, &cmd, Html_TD, Html_EndTD, "columns", Html_TR,0, nostr)){
            if (sc && tp->base.type==Html_TD) goto domfmtdone;
          }
        } else if (HtmlDOMFmtSubIndex(htmlPtr, &tp, &cmd, Html_TH, Html_EndTH, "rows", Html_TABLE,0, nostr)) {
          if (sc && tp->base.type==Html_TH) goto domfmtdone;
          if (HtmlDOMFmtSubIndex(htmlPtr, &tp, &cmd, Html_TD, Html_EndTD, "columns", Html_TH,0, nostr)){
            if (sc && tp->base.type==Html_TD) goto domfmtdone;
          }
        }
	goto domfmtdone;
      }
      break;
     default:
      if (HtmlIsMarkup(pStart)) {
	int etyp, typ; char *setyp, *styp=HtmlGetTokenName(pStart);
	if (styp[0]!='/') {
	  typ=pStart->base.type;
	  if ((etyp=HtmlGetEndToken(typ))!=Html_Unknown)
            if (HtmlDOMFmtSubIndex(htmlPtr, &pStart, &cmd, typ, etyp, styp, Html_Unknown,pStart, nostr))
	      goto domfmtdone;
	}
      }
    }
  }
domfmtdone:
  Tcl_DStringResult(interp, &cmd);
  return TCL_OK;
}

/* Find all tags that contain an attr named in input list. Return TIDs.  */
int HtmlTokenAttrSearch(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement *p;
  char **vargv, *z, str[50]; int vargc, i, j, nocase, cnt=0;
  HtmlIndex be[2];
  if (TCL_OK!=HtmlBeginEnd(htmlPtr, be, argc-4, argv+4))
    return TCL_ERROR;
  if (Tcl_SplitList(interp, argv[3],&vargc,&vargv) || vargc<=0) {
    Tcl_AppendResult(interp,"token attrs error: ",argv[3],0);
    return TCL_ERROR;
  }
  for(p=be[0].p; p; p=p->pNext){
    if (HtmlIsMarkup(p)) {
      for (i=0; i<p->base.count; i+=2) {
        for (j=0; j<vargc; j++)
          if (StrEqual(vargv[j],p->markup.argv[i]))
            break;
        if (j<vargc) {
          if (cnt++) Tcl_AppendResult(interp," ",0);
          sprintf(str,"%d",p->base.id);
          Tcl_AppendResult(interp,str,0);
          break;
        }
      }
    }
    if (p==be[1].p) break;
  }
  HtmlFree(vargv);
  return TCL_OK;
}

/* Define the begin, and end indexes */
int HtmlBeginEnd(
  HtmlWidget *htmlPtr, HtmlIndex *be, int argc, char *argv[]) {
  char *cp, nbuf[50], *ep;
  int i, n;
  Tcl_Interp* interp=htmlPtr->interp;
  be[0].p=htmlPtr->pFirst;
  be[0].i=0;
  be[1].p=0;
  be[0].i=0;
  if (argc) {
    if( HtmlGetIndex(htmlPtr, argv[0], &be[0].p, &be[0].i)!=0 ){
      Tcl_AppendResult(interp,"malformed index: \"", argv[0], "\"", 0);
      return TCL_ERROR;
    }
  }
  if (argc>1) {
    if( HtmlGetIndex(htmlPtr, argv[1], &be[1].p, &be[1].i)!=0 ){
      Tcl_AppendResult(interp,"malformed index: \"", argv[1], "\"", 0);
      return TCL_ERROR;
    }
  }
  return TCL_OK;
}

/* Define the -begin, -end and -range options */
int HtmlBeginEndOpts(
  HtmlWidget *htmlPtr, HtmlIndex *be, int argc, char *argv[]) {
  char *cp, nbuf[50], *ep;
  int i, n;
  Tcl_Interp* interp=htmlPtr->interp;
  be[0].p=htmlPtr->pFirst;
  be[0].i=0;
  be[1].p=0;
  be[0].i=0;
  for (i=0; i<(argc-1); i+=2) {
    cp=argv[i];
    if (*cp++ != '-') return -1;
    if (StrEqual(cp,"begin")) {
      if( HtmlGetIndex(htmlPtr, argv[i+1], &be[0].p, &be[0].i)!=0 ){
        Tcl_AppendResult(interp,"malformed index: \"", argv[i+1], "\"", 0);
	return TCL_ERROR;
      }
    } else if (StrEqual(cp,"end")) {
      if( HtmlGetIndex(htmlPtr, argv[i+1], &be[1].p, &be[1].i)!=0 ){
        Tcl_AppendResult(interp,"malformed index: \"", argv[i+1], "\"", 0);
	return TCL_ERROR;
      }
    } else if (StrEqual(cp,"range")) {
      cp=argv[i+1];
      while (isspace(*cp)) cp++;
      ep=cp;
      while (*ep && !isspace(*ep)) ep++;
      while (isspace(*ep)) ep++;
      if (!*ep) {
        Tcl_AppendResult(interp,"malformed index: \"", argv[i+1], "\"", 0);
        return TCL_ERROR;
      }
      if( HtmlGetIndex(htmlPtr, cp, &be[0].p, &be[0].i)!=0 ||
          HtmlGetIndex(htmlPtr, ep, &be[1].p, &be[1].i)!=0 ){
        Tcl_AppendResult(interp,"malformed index: \"", argv[i+1], "\"", 0);
        return TCL_ERROR;
      }
    }
  }
  return TCL_OK;
}
/* Find all onEvent tags and return list of Event id Event id ... */
int HtmlTokenOnEvents(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement *p;
  char **vargv, *z, str[50], *cp; int vargc, i, nocase, cnt=0;
  HtmlIndex be[2];
  if (TCL_OK!=HtmlBeginEnd(htmlPtr, be, argc-3, argv+3))
    return TCL_ERROR;
  for(p=be[0].p; p; p=p->pNext){
    if (HtmlIsMarkup(p)) {
      for (i=0; i<p->base.count; i+=2) {
        cp=p->markup.argv[i];
        if (strlen(cp)>=3 && cp[0]=='o' && cp[1]=='n') {
            if (cnt++) Tcl_AppendResult(interp," ",0);
            sprintf(str,"%d",p->base.id);
            Tcl_AppendResult(interp,str," ",cp,0);
        }
      }
    }
    if (p==be[1].p) break;
  }
  return TCL_OK;
}

/* Translate a name attr index to a integer index. */
int HtmlDomName2Index(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement* p;
  int n=-1, i=-1, type;
  char *z, str[50];
  if ((type=HtmlNameToType(argv[3]))== Html_Unknown) return TCL_ERROR;
  for(p=htmlPtr->pFirst; p; p=p->pNext){
    if (p->base.type != type) {
      if (type != Html_INPUT &&
        p->base.type != Html_SELECT && p->base.type != Html_TEXTAREA)
        continue;
    }
    i++;
    z = HtmlMarkupArg(p, "name", 0); 
    if (!z) continue;
    if (!strcmp(z,argv[4])) { n=i; break; }
  }
  sprintf(str,"%d",n);
  Tcl_AppendResult(interp,str,0);
  return TCL_OK;
}

HtmlElement *HtmlGetIndexth(HtmlWidget *htmlPtr, int typ, int cnt) {
  HtmlElement* p;
  for(p=htmlPtr->pFirst; p; p=p->pNext)
    if (p->base.type==typ && !cnt--) return p;
  return 0;
}

/* Count the radios matching this one */
static int HtmlRadioCount(HtmlWidget *htmlPtr, HtmlElement *radio) {
  char *z, *rz;
  HtmlElement* p, *form; int cnt=0;
  assert(radio->base.type==Html_INPUT && radio->input.type==INPUT_TYPE_Radio);
  if (!(rz =HtmlMarkupArg(radio, "name", 0))) return 0;
  form=radio->input.pForm;
  for(p=form->form.pFirst; p && p->input.pForm==form; p=p->input.pNext){
    assert(p->base.type==Html_INPUT ||p->base.type==Html_SELECT|| p->base.type==Html_TEXTAREA);
    if (p==radio) cnt++;
    else if (p->input.type == INPUT_TYPE_Radio) {
      if ((z =HtmlMarkupArg(p, "name", 0)) && !strcmp(z,rz))
        cnt++;
    }
  }
  return cnt;
}

/* Translate a radio index to a form element index. */
static int HtmlDomRadio2Index(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement* p, *form;
  int n, type;
  char *z, str[50];
  n=atoi(argv[5]);
  form=HtmlGetIndexth(htmlPtr, Html_FORM, atoi(argv[3]));
  if (form)
   for(p=form->form.pFirst; p && p->input.pForm==form; p=p->input.pNext){
    assert(p->base.type==Html_INPUT ||p->base.type==Html_SELECT|| p->base.type==Html_TEXTAREA);
    if (p->input.type == INPUT_TYPE_Radio) {
      if (p->input.subid==n) {
	sprintf(str,"%d",p->input.id);
	Tcl_AppendResult(interp,str,0);
	return TCL_OK;
      }
    }
   }
  Tcl_AppendResult(interp,"radioidx failed:",argv[3]," ",argv[4]," ",argv[5],0);
  return TCL_ERROR;
}

/* Search through all of type tag and compile list of attr names */
int HtmlTokenUnique(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement* p;
  int i, j, k, type, nump=0;
  char *vars[201], *cp;
  HtmlIndex be[2];
  if (TCL_OK!=HtmlBeginEnd(htmlPtr, be, argc-4, argv+4))
    return TCL_ERROR;
  if ((type=HtmlNameToType(argv[3]))== Html_Unknown) return TCL_ERROR;
  for(p=be[0].p; p  && nump<200; p=p->pNext){
    if (p->base.type == type) {
      for(i=0; i<p->base.count; i+=2){
        for (j=0; j<nump; j++) {
          cp=p->markup.argv[i];
          if (*cp==vars[j][0] && (!strcmp(cp,vars[j]))) break;
        }
        if (j>=nump) vars[nump++]=p->markup.argv[i];
      }
    }
    if (p==be[1].p) break;
  }
  for (j=0; j<nump; j++)
    Tcl_AppendElement(htmlPtr->interp, vars[j]);
  return TCL_OK;
}

/* Return the element offset index for the named el in form */
int HtmlDomFormEl(HtmlWidget *htmlPtr, int form, char *el) {
  HtmlElement *p, *pstart;
  int i=0;
  char *z;
  p=pstart=htmlPtr->pFirst;
  if (!p) return -1;
  for(; p; p=p->pNext){
    if (p->base.type != Html_FORM) continue;
    if (!form--) break;
  }
  for(; p; p=p->pNext){
    switch (p->base.type) {
      case Html_INPUT:
	if (p->input.type==INPUT_TYPE_Radio) {
	}
      case Html_TEXTAREA:
      case Html_SELECT:
        z = HtmlMarkupArg(p, "name", 0);
        if (z && (!strcmp(z,el))) return i;
        i++;
	break;
      case Html_EndFORM: break;
    }
    if (pstart->form.pEnd==p) break;
  }
  return -1;
}

/* Return the element offset index for the named el in form */
int HtmlDomFormElIndex(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  int i=HtmlDomFormEl(htmlPtr,  atoi(argv[3]), argv[4]);
  char str[50];
  sprintf(str,"%d", i);
  Tcl_AppendResult(interp,str,0);
  return TCL_OK;
}


/* Return the HTML Doc as one big DOM tree list */
int HtmlDomTreeCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  Tcl_DString cmd;
  char *a=argv[3], *z;
  HtmlElement *p, *ep;
  int n[10], ni=0, en, i, k, j, l, iswrite=0, atend;
  if (!(p=htmlPtr->pFirst)) return TCL_OK;
  Tcl_DStringInit(&cmd);
  while (p) {
    
  switch (p->base.type) {
    case Html_TABLE:
    case Html_TH:
    case Html_TR:
    case Html_TD:
    case Html_A:
    case Html_FORM:
    case Html_INPUT: {
    }
    default: {
/*      buf[0]='/';
      strcpy(buf+1,tok);
      if ((en=HtmlNameToType(tok))==Html_Unknown) return 0;
      while (ep && ep->base.type!=en) ep=ep->base.pNext;
      return ep; */
      return 0;
    }
   }

  }
  Tcl_DStringAppend(&cmd, "", -1);
  Tcl_DStringStartSublist(&cmd);
  HtmlAppendArglist(&cmd, p);
  Tcl_DStringEndSublist(&cmd);
  return TCL_OK;
}

static int _HtmlTokenCmdSub(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv,           /* List of all arguments */
  int flag
){
  HtmlElement *pStart, *pEnd=0;
  int i;
  char *cb, *ce;
  if (argc<=3) cb="begin"; else cb=argv[3];
  if (argc<=4) ce=cb; else ce=argv[4];

  if( HtmlGetIndex(htmlPtr, cb, &pStart, &i)!=0 ){
    Tcl_AppendResult(interp,"malformed index: \"", cb, "\"", 0);
    return TCL_ERROR;
  }
  if(HtmlGetIndex(htmlPtr, ce, &pEnd, &i)!=0 ){
    Tcl_AppendResult(interp,"malformed index: \"", ce, "\"", 0);
    return TCL_ERROR;
  }
  if( pStart ){
    HtmlTclizeList(interp,pStart,pEnd ? pEnd->base.pNext : 0, flag);
  }
  return TCL_OK;
}


/*
** WIDGET token attr INDEX NAME ?VALUE?
   If modifying an attribute, we reallocate the whole argv with alloc.
*/
int HtmlTokenAttr(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  char *name, *value, fake[99], **nv;
  HtmlElement *p;
  int i, j, l, tl, ol, c, nc;
  if( HtmlGetIndex(htmlPtr, argv[3], &p, &i)!=0 || p==0 || !HtmlIsMarkup(p)){
    Tcl_AppendResult(interp,"malformed index: \"", argv[3], "\"", 0);
    return TCL_ERROR;
  }
  if (argc<5) {
    Tcl_DString cmd;
    Tcl_DStringInit(&cmd);
    HtmlAppendArglist(&cmd, p);
    Tcl_DStringResult(interp, &cmd);
    return TCL_OK;
  }
  name=argv[4];
  if (argc<6) {
    if (!(value=HtmlMarkupArg(p,name,0))) {
/*      Tcl_AppendResult(interp,"attr not found: \"", name, "\"", 0);
      return TCL_ERROR; */
      return TCL_OK;
    }
    Tcl_AppendResult(interp,value?value:"",0);
    return TCL_OK;
  }
  value=argv[5];
  l=strlen(value);
  nc=c=p->base.count;
  for(j=0; j<c; j+=2)
    if (!strcasecmp(name,p->markup.argv[j])) break;
  if (j>=c || (ol=strlen(p->markup.argv[j]))<l) {
    if (j>=c) { nc+=2; p->base.count+=2; }
    if (c==0 || p->markup.argv == (char**)p->markup.argv[c+1]) {
      /* Migrate to dynamic allocations. */
      nv=(char**)HtmlAlloc(sizeof(char**)*(nc+2));
      for(i=0; i<c; i++) {
        char *val=p->markup.argv[i];
	if (i==(j+1)) val=value;
        nv[i]=(char*)HtmlAlloc(strlen(val)+1);
        if (!nv[i]) return TCL_ERROR;
	strcpy(nv[i],val);
      }
      nv[i]=0;
      nv[i+1]=(c==0?0:p->markup.argv[i+1]);
    } else if (nc>c) {
      nv=(char**)HtmlRealloc((char*)p->markup.argv,sizeof(char**)*(nc+2));
      i=c;
      nv[nc+1]=p->markup.argv[c+1];
      nv[nc]=0;
    } else {
      nv=p->markup.argv;
      HtmlFree(nv[j+1]);
      nv[j+1]=(char*)HtmlAlloc(strlen(value)+1);
    }
    if (nc>c) {
      nv[i+2]=0; /* Rallocate argv to be dynamic. */
      nv[i+3]=(c<=0?0:p->markup.argv[i+1]);
      nv[i]=(char*)HtmlAlloc(strlen(name)+1);;
      if (!nv[i]) return TCL_ERROR;
      strcpy(nv[i],name);
      ToLower(nv[i]);
      j=i;
      nv[i+1]=(char*)HtmlAlloc(strlen(value)+1);;
      if (!nv[i+1]) return TCL_ERROR;
      strcpy(nv[i+1],value);
    }
    p->markup.argv=nv;
  }
  /* sprintf(pElem->markup.argv[j+1],"%.*s",l,value); */
  strcpy(p->markup.argv[j+1],value);
  HtmlTranslateEscapes(p->markup.argv[j+1]);
  return TCL_OK;
}

/*
** WIDGET token list START END
*/
int HtmlTokenListCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  return _HtmlTokenCmdSub(htmlPtr,interp,argc,argv,TOKEN_LIST);
}

/*
** WIDGET token markup START END
*/
int HtmlTokenMarkupCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  return _HtmlTokenCmdSub(htmlPtr,interp,argc,argv,TOKEN_MARKUP|TOKEN_LIST);
}

/*
** WIDGET token dom START END
*/
int HtmlTokenDomCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  return _HtmlTokenCmdSub(htmlPtr,interp,argc,argv,TOKEN_DOM|TOKEN_LIST);
}

/*
** WIDGET text html START END
*/
int HtmlTextHtmlCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement *pStart, *pEnd;
  int i;
  char *cb, *ce;
  if (argc<=3) cb="begin"; else cb=argv[3];
  if (argc<=4) ce=cb; else ce=argv[4];

  if( HtmlGetIndex(htmlPtr, cb, &pStart, &i)!=0 ){
    Tcl_AppendResult(interp,"malformed index: \"", cb, "\"", 0);
    return TCL_ERROR;
  }
  if( HtmlGetIndex(htmlPtr, ce, &pEnd, &i)!=0 ){
    Tcl_AppendResult(interp,"malformed index: \"", ce, "\"", 0);
    return TCL_ERROR;
  }
  if( pStart ){
    HtmlTclizeHtml(interp,pStart,pEnd ? pEnd->base.pNext : 0);
  }
  return TCL_OK;
}

/*
** WIDGET text ascii START END
*/
int HtmlTextAsciiCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlIndex iStart, iEnd;
  int i;  
  char *cb, *ce;
  if (argc<=3) cb="begin"; else cb=argv[3];
  if (argc<=4) ce=cb; else ce=argv[4];

  if( HtmlGetIndex(htmlPtr, cb, &iStart.p, &iStart.i)!=0 ){
    Tcl_AppendResult(interp,"malformed index: \"", cb, "\"", 0);
    return TCL_ERROR;
  }
  if( HtmlGetIndex(htmlPtr, ce, &iEnd.p, &iEnd.i)!=0 ){
    Tcl_AppendResult(interp,"malformed index: \"", ce, "\"", 0);
    return TCL_ERROR;
  }
  if (iEnd.p && iStart.p) {
    if ((!iEnd.i) && (!strchr(ce,'.'))) {
       iEnd.p=iEnd.p->pNext;
    }
    HtmlTclizeAscii(interp,&iStart,&iEnd);
  }
  return TCL_OK;
}

/* Hard to describe, but used as follows: when you extract text, and do
   a regex on it, with -indices, you need to convert these offsets back
   into INDEXES. This returns those begin and end anchor. */
int HtmlTextOffsetCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlIndex iStart;
  int n1, n2;
  int h, i1, i2, i=0,j,k, n, m=0, fnd=0, sfnd=0;
  int si1, si2, mpos=0, ii[2];
  char zLine[256];
  HtmlElement *p1=0, *p2=0, *p;
  if (argc!=6) {
    Tcl_AppendResult(interp,argv[0], " text offset START NUM1 NUM2", 0);
    return TCL_ERROR;
  }
  if (HtmlGetIndex(htmlPtr, argv[3], &iStart.p, &iStart.i)!=0 || !iStart.p){
    Tcl_AppendResult(interp,"malformed index: \"", argv[3], "\"", 0);
    return TCL_ERROR;
  }
  n1=atoi(argv[4]);
  n2=atoi(argv[5]);
  if (n1<0 || n2<0) {
    Tcl_AppendResult(interp,"malformed offsets: ", argv[4], " or ", argv[5], 0);
    return TCL_ERROR;
  }
#if 1
  {
  p=iStart.p;
  if (p->base.type == Html_Text) {
    int tail;
    j=strlen(p->text.zText);
    tail=(j-iStart.i);
    if (tail>=n1) {
      p1=p;  i1=iStart.i+n1;
    }
    n1-=tail;
    if (tail>=n2) {
      p2=p;  i2=iStart.i+n2;
    }
    n2-=tail;
    p=p->pNext;
  }
  while (p && (!p2)) {
    j=0;
    switch (p->base.type) {
      case Html_Text:
        j=strlen(p->text.zText);
        break;
      case Html_Space:
        j=p->base.count;
        if( p->base.flags & HTML_NewLine ) j++;
        break;
    }
    if (j>0) {
      if (!p1) {
        if ((n1-j)<0) {
	  p1=p;
	  i1=n1;
	}
	n1-=j;
      }
      if (!p2) {
        if ((n2-j)<0) {
	  p2=p;
	  i2=n2;
	}
	n2-=j;
      }
    }
    p=p->pNext;
  }
  }
#else
  i1=-1; i2=-1;
  p=iStart.p;
  if (iStart.i>0 && p->base.type == Html_Text) {
    j=strlen(p->text.zText)-iStart.i;
    goto htoffset;
  } else if (n1==0) {
     p1=p; i1=iStart.i;
  }
  while (p) {
    j=0;
    switch (p->base.type) {
      case Html_Text:
        j=strlen(p->text.zText);
        break;
      case Html_Space:
        j=p->base.count;
        if( p->base.flags & HTML_NewLine ) j++;
        break;
    }
htoffset:
    if (j>0) {
      n1-=j;
      n2-=j;
      if (n1<=0 && !p1) { p1=p; i1= -n1-1; }
      /*if (n2<=0 && !p2) { p2=p; i2= -n2-1; break; } */
      if (n2<=0 && !p2) { p2=p; i2= j+n2; break; }
    }
    p=p->pNext;
  }
#endif
  if (p1 && p2) {
    n=HtmlTokenNumber(p1);
    m=HtmlTokenNumber(p2);
    if (i1<0) i1=0;
    if (i2<0) i2=0;
    if (n==m && i1>i2)
      sprintf(zLine,"%d.%d %d.%d",n,i2,m,i1);
    else
      sprintf(zLine,"%d.%d %d.%d",n,i1,m,i2);
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, zLine,0);
  }
  return TCL_OK;
}

int HtmlTextFindCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlIndex iStart, iEnd;
  int i, j=4, after=1, nocase=0;
  iStart.p=iEnd.p=0;  iStart.i=iEnd.i=0;
  if (!htmlPtr->pFirst) return TCL_OK;
  if (argc>4) {
    if (!strcasecmp(argv[j],"nocase")) { nocase=1; j++; }
  }
  if (argc>(j+1)) {
    if (HtmlGetIndex(htmlPtr, argv[j+1], &iStart.p, &iStart.i)!=0 ){
      Tcl_AppendResult(interp,"malformed index: \"", argv[3], "\"", 0);
      return TCL_ERROR;
    }
    if (!strcasecmp(argv[j],"before")) after=0;
    else if (strcasecmp(argv[j],"after")) {
      Tcl_AppendResult(interp,"before|after: \"", argv[j], "\"", 0);
      return TCL_ERROR;
    }
    if (after) { if (iStart.p) iStart.p=iStart.p->base.pNext; }
    else { iEnd.p=iStart.p; iStart.p=0; }
  }
  if (!iStart.p)
    iStart.p=htmlPtr->pFirst;
  if (iEnd.p && after)
    iEnd.p=iEnd.p->base.pNext;
/* ?????? Fix i1,i2 */
  HtmlTclizeFindText(interp,argv[3],&iStart,&iEnd,nocase,after);
  return TCL_OK;
}

/* Insert str into cp at n. */
char *StrInsert(char *cp, char *str, int n, int clen) {
  int l=strlen(str);
  if (clen<0) clen=strlen(cp);
  cp=HtmlRealloc(cp,clen+l+1);
  memmove(cp+n+l,cp+n,clen-n);
  strncpy(cp+n,str,l);
  return cp;
}

static void HtmlAddOffset(HtmlWidget *htmlPtr, HtmlElement *p, int n) {
  while (p) {
    p->base.offs+=n;
    p=p->pNext;
  }
}

void HtmlAddStrOffset(HtmlWidget *htmlPtr, HtmlElement *p, char *str,
  int offs) {
  int n=0, l=strlen(str);
  if ((!p) || !p->base.type==Html_Text) return;
  n=p->base.offs+offs;
  if (n<0 || htmlPtr->nText<=0) return;
  htmlPtr->zText=StrInsert(htmlPtr->zText,str,n,htmlPtr->nText);
  htmlPtr->nText +=l;
  HtmlAddOffset(htmlPtr,p->pNext,l);
}

static void HtmlDelStrOffset(HtmlWidget *htmlPtr, HtmlElement *p, int offs,
  int l) {
  int len, n=p->base.offs+offs;
  char *cp=htmlPtr->zText;
  assert(p->base.type==Html_Text);
  if (n<0 || htmlPtr->nText<=0) return;
  len=htmlPtr->nText-n;
  memmove(cp+n,cp+n+l,len);
  htmlPtr->nText -=l;
  HtmlAddOffset(htmlPtr,p->pNext,-l);
  cp=p->text.zText;
  len=strlen(cp);
  p->base.count-=l;
  memmove(cp+offs,cp+offs+l,len-offs-l+1);
}

/* Function that refreshes after internal adding/deleting elements. */
int  HtmlRefresh(HtmlWidget *htmlPtr, int idx) {
  htmlPtr->flags |= RELAYOUT;
  HtmlScheduleRedraw(htmlPtr);
}
int HtmlRefreshCmd( HtmlWidget *htmlPtr, Tcl_Interp *interp, int argc,
  char **argv){
  if (argc<3) htmlPtr->flags |= RELAYOUT;
  else  {
   int n=2;
   while (n<argc) {
    switch (argv[n][0]) {
    case 'i': htmlPtr->flags |= REDRAW_IMAGES; break;
    case 'r': htmlPtr->flags |= RESIZE_ELEMENTS; break;
    case 'f': htmlPtr->flags |= REDRAW_FOCUS; break;
    case 't': htmlPtr->flags |= REDRAW_TEXT; break;
    case 'b': htmlPtr->flags |= REDRAW_BORDER; break;
    case 'e': htmlPtr->flags |= EXTEND_LAYOUT; break;
    case 'c': htmlPtr->flags |= RESIZE_CLIPWIN; break;
    case 's': htmlPtr->flags |= STYLER_RUNNING; break;
    case 'a': htmlPtr->flags |= ANIMATE_IMAGES; break;
    case 'v': htmlPtr->flags |= VSCROLL; break;
    case 'h': htmlPtr->flags |= HSCROLL; break;
    case 'g': htmlPtr->flags |= GOT_FOCUS; break;
    case 'l': htmlPtr->flags |= RELAYOUT; break;
    default: 
      Tcl_AppendResult(interp, "Unknown refresh option: ", argv[n],0);
      return TCL_ERROR;
    }
    n++;
   }
  }
  HtmlRefresh(htmlPtr,0);
  HtmlScheduleRedraw(htmlPtr);
  return TCL_OK;
}

int HtmlSourceCmd( HtmlWidget *htmlPtr, Tcl_Interp *interp, int argc,
  char **argv){
  if (htmlPtr->zText) Tcl_AppendResult(interp, htmlPtr->zText, 0);
  return TCL_OK;
}


/*
** WIDGET text delete START END
*/
int HtmlTextDeleteCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement *pStart, *pEnd, *pn;
  int ib, ie, idx=0;
  char *cb, *ce;
  if (argc<=3) cb="begin"; else cb=argv[3];
  if (argc<=4) ce=cb; else ce=argv[4];

  if( HtmlGetIndex(htmlPtr, cb, &pStart, &ib)!=0 ){
    Tcl_AppendResult(interp,"malformed index: \"", cb, "\"", 0);
    return TCL_ERROR;
  }
  if( HtmlGetIndex(htmlPtr, ce, &pEnd, &ie)!=0 ){
    Tcl_AppendResult(interp,"malformed index: \"", ce, "\"", 0);
    return TCL_ERROR;
  }
  if(!pStart ) return TCL_OK;
  idx=pStart->base.id;
  if (pEnd==pStart) {
    if (!ib && ((ie-1)>=strlen(pStart->text.zText)))
      HtmlRemoveElements(htmlPtr,pStart,pStart);
    else
      HtmlDelStrOffset(htmlPtr,pStart,ib,ie-ib+1);
  } else {
    pn=pStart->pNext;
    if (pStart->base.type==Html_Text && ib)
      HtmlDelStrOffset(htmlPtr,pStart,ib,ib+1);
    else
      HtmlRemoveElements(htmlPtr,pStart,pStart);
    pStart=pn;
    if (pEnd) {
      pn=pEnd->base.pPrev;
      if (pEnd->base.type==Html_Text && ((ie-1)>=strlen(pEnd->text.zText)))
        HtmlRemoveElements(htmlPtr,pEnd,pEnd);
      else
        HtmlDelStrOffset(htmlPtr,pEnd,0,ie);
      if (pStart==pEnd) pEnd=0;
      else pEnd=pn;
    }
    if (pEnd) {
      HtmlRemoveElements(htmlPtr,pStart,pEnd);
    }
  }
  HtmlRefresh(htmlPtr,idx);
  return TCL_OK;
}

/*
** WIDGET token insert INDEX TOKEN ARGS
*/
int HtmlTokenInsertCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement *pStart, *pEnd;
  int idx=0, i, tlen=strlen(argv[4]);  char *cp, *attr="";
  if (argc>5) {
    attr=argv[5];
    tlen+=strlen(attr);
  }
  if( HtmlGetIndex(htmlPtr, argv[3], &pStart, &i)!=0 ){
    Tcl_AppendResult(interp,"malformed index: \"", argv[3], "\"", 0);
    return TCL_ERROR;
  }
  if (pStart && pStart->base.type==Html_Text && i== pStart->base.count)
    pStart=pStart->pNext;
  HtmlInsertToken(htmlPtr, pStart, argv[4], attr,-1);
  cp=(char*)HtmlAlloc(tlen+6);
  if (argc>5) {
    sprintf(cp,"<%s %s>", argv[4], argv[5]);
  } else {
    sprintf(cp,"<%s>", argv[4]);
  }
  HtmlAddStrOffset(htmlPtr,pStart,cp, 0);
  HtmlFree(cp);
  if (pStart) idx=pStart->base.id;
  HtmlRefresh(htmlPtr,idx);
  htmlPtr->ins.p=pStart;
  htmlPtr->ins.i=0;
  return TCL_OK;
}

/*
** WIDGET token delete START END
*/
int HtmlTokenDeleteCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement *pStart, *pEnd;
  int i, idx=0;
  char *cb, *ce;
  if (argc<=3) cb="begin"; else cb=argv[3];
  if (argc<=4) ce=cb; else ce=argv[4];

  if( HtmlGetIndex(htmlPtr, cb, &pStart, &i)!=0 ){
    Tcl_AppendResult(interp,"malformed index: \"", cb, "\"", 0);
    return TCL_ERROR;
  }
  if (HtmlGetIndex(htmlPtr, ce, &pEnd, &i)!=0 ){
    Tcl_AppendResult(interp,"malformed index: \"", ce, "\"", 0);
    return TCL_ERROR;
  }
  if( pStart ){
    HtmlRemoveElements(htmlPtr,pStart,pEnd);
    idx=pStart->base.id;
  }
  HtmlRefresh(htmlPtr,idx);
  return TCL_OK;
}

/*    Given a start token, find the matching end token.  */
int HtmlTokenGetEnd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement *pStart=0;
  int i;

  if( HtmlGetIndex(htmlPtr, argv[3], &pStart, &i)!=0 ){
    Tcl_AppendResult(interp,"malformed index: \"", argv[3], "\"", 0);
    return TCL_ERROR;
  }
  if( pStart && HtmlIsMarkup(pStart)){
    HtmlElement *p;
    int en=HtmlGetEndToken(pStart->base.type);
    p=HtmlFindEndNest(htmlPtr, pStart, en, 0);
    if (p && p->base.id == 0) p=p->base.pNext;
    if (p) {
      char buf[20];
      sprintf(buf,"%d", HtmlTokenNumber(p));
      Tcl_AppendResult(interp,buf, 0);
    }
  }
  return TCL_OK;
}

int HtmlTokenGetCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlElement *pStart, *pEnd;
  int i;
  char *cb, *ce;
  if (argc<=3) cb="begin"; else cb=argv[3];

  if( HtmlGetIndex(htmlPtr, cb, &pStart, &i)!=0 ){
    Tcl_AppendResult(interp,"malformed index: \"", cb, "\"", 0);
    return TCL_ERROR;
  }
  if (argc<=4) pEnd=pStart; else {
    if (HtmlGetIndex(htmlPtr, argv[4], &pEnd, &i)!=0 ){
      Tcl_AppendResult(interp,"malformed index: \"", argv[4], "\"", 0);
      return TCL_ERROR;
    }
  }
  if (pEnd) pEnd=pEnd->base.pNext;
  if( pStart ){
    HtmlTclizeList(interp,pStart,pEnd, 0);
  }
  return TCL_OK;
}


int HtmlTokenFindCmd(
  HtmlWidget *htmlPtr,   /* The HTML widget */
  Tcl_Interp *interp,    /* The interpreter */
  int argc,              /* Number of arguments */
  char **argv            /* List of all arguments */
){
  HtmlIndex iStart, iEnd;
  int i, after=1, near=0;
  int type = HtmlNameToType(argv[3]);
  iStart.p=0; iEnd.p=0; iStart.i=0; iEnd.i=0;
  if (!htmlPtr->pFirst) return TCL_OK;
  if( type==Html_Unknown ){
    Tcl_AppendResult(interp,"unknown tag: \"", argv[3], "\"", 0);
    return TCL_ERROR;
  }
  if (argc>5) {
    if (HtmlGetIndex(htmlPtr, argv[5], &iStart.p, &iStart.i)!=0 ){
      Tcl_AppendResult(interp,"malformed index: \"", argv[5], "\"", 0);
      return TCL_ERROR;
    }
    if (!strcasecmp(argv[4],"before")) after=0;
    else if (!strcasecmp(argv[4],"near")) {
      near=HtmlTokenNumber(iStart.p); iStart.p=htmlPtr->pFirst;
    } else if (strcasecmp(argv[4],"after")) {
      Tcl_AppendResult(interp,"before|after|near: \"", argv[4], "\"", 0);
      return TCL_ERROR;
    }
    if (!near) {
      if (after) { if (iStart.p) iStart.p=iStart.p->base.pNext; }
      else { iEnd.p=iStart.p; iStart.p=0; }
    }
  }
  if (!iStart.p)
    iStart.p=htmlPtr->pFirst;
  if (!near)
    if (iEnd.p)
      iEnd.p=iEnd.p->base.pNext;
  HtmlTclizeFind(interp,type,iStart.p,iEnd.p,near);
  return TCL_OK;
}

/*
** Remove Elements from the list of elements
*/
void HtmlRemoveElements(HtmlWidget *p, HtmlElement* pElem, HtmlElement* pLast){
  HtmlElement *pPrev;
  pPrev=pElem->base.pPrev;
  if (p->pLast==pLast) p->pLast=pPrev;
  if (p->pFirst==pElem) p->pFirst=pLast->base.pNext;
  if (pPrev) {
    pPrev->base.pNext=pLast->base.pNext;
  }
  if (pLast) pLast->base.pPrev=pPrev;
  while (pElem) {
    pPrev=pElem->base.pNext;
    HtmlDeleteElement(pElem);
    p->nToken--;
    if (pElem==pLast) break;
    pElem=pPrev;
  }
}

/*
** Return all tokens between the two elements as a Tcl list.
*/
void HtmlTclizeList(Tcl_Interp *interp, HtmlElement *p, HtmlElement *pEnd,
  int flag){
  Tcl_DString str;
  int i, isatr;
  char *zName;
  char zLine[100];

  Tcl_DStringInit(&str);
  while( p && p!=pEnd ){
    isatr=0;
    switch( p->base.type ){
      case Html_Block:
        break;
      case Html_Text:
      case Html_COMMENT:
	if (flag&(TOKEN_MARKUP|TOKEN_DOM)) break;
        Tcl_DStringStartSublist(&str);
	if (flag&TOKEN_LIST) {
	  sprintf(zLine,"%d",p->base.id);
          Tcl_DStringAppendElement(&str,zLine);
	}
        Tcl_DStringAppendElement(&str,"Text");
        Tcl_DStringAppendElement(&str, p->text.zText);
        Tcl_DStringEndSublist(&str);
        break;
      case Html_Space:
	if (flag&(TOKEN_MARKUP|TOKEN_DOM)) break;
        if (flag&TOKEN_LIST) sprintf(zLine,"%d Space %d %d", p->base.id,
          p->base.count, (p->base.flags & HTML_NewLine)!=0);
        else sprintf(zLine,"Space %d %d",
          p->base.count, (p->base.flags & HTML_NewLine)!=0);
        Tcl_DStringAppendElement(&str,zLine);
        break;
      case Html_Unknown:
	if (flag&(TOKEN_MARKUP|TOKEN_DOM)) break;
        Tcl_DStringAppendElement(&str,"Unknown");
        break;
      case Html_EndVAR: case Html_VAR: case Html_EndU: case Html_U:
      case Html_EndTT: case Html_TT: case Html_EndSUP: case Html_SUP:
      case Html_EndSUB: case Html_SUB: case Html_EndSTRONG: case Html_STRONG:
      case Html_EndSTRIKE: case Html_STRIKE: case Html_EndSMALL:
      case Html_SMALL: case Html_EndSAMP: case Html_SAMP: case Html_EndS:
      case Html_S: case Html_EndP: case Html_EndMARQUEE: case Html_MARQUEE:
      case Html_EndLISTING: case Html_LISTING: case Html_EndKBD: case Html_KBD:
      case Html_EndI: case Html_I: case Html_EndFONT: case Html_FONT:
      case Html_EndEM: case Html_EM: case Html_EndDIV: case Html_DIV:
      case Html_EndDFN: case Html_DFN: case Html_EndCODE: case Html_CODE:
      case Html_EndCITE: case Html_CITE: case Html_EndCENTER: case Html_CENTER:
      case Html_BR: case Html_EndBLOCKQUOTE: case Html_BLOCKQUOTE:
      case Html_EndBIG: case Html_BIG: case Html_EndBASEFONT:
      case Html_BASEFONT: case Html_BASE: case Html_EndB: case Html_B:
        isatr=1;
      default:
	if (isatr && (flag&(TOKEN_DOM))) break;
        Tcl_DStringStartSublist(&str);
	if (flag&TOKEN_LIST) {
	  sprintf(zLine,"%d",p->base.id);
          Tcl_DStringAppendElement(&str,zLine);
	}
	if (!(flag&(TOKEN_MARKUP|TOKEN_DOM)))
          Tcl_DStringAppendElement(&str,"Markup");
        if( p->base.type >= HtmlGetMarkupMap(0)->type 
         && p->base.type <= HtmlGetMarkupMap(HTML_MARKUP_COUNT-1)->type ){
          zName = HtmlGetMarkupMap(p->base.type - HtmlGetMarkupMap(0)->type)->zName;
        }else{
          zName = "Unknown";
        }
        Tcl_DStringAppendElement(&str, zName);
        for(i=0; i<p->base.count; i++){
          Tcl_DStringAppendElement(&str, p->markup.argv[i]);
        }
        Tcl_DStringEndSublist(&str);
        break;
    }
    p = p->pNext;
  }
  Tcl_DStringResult(interp, &str);
}

/*
** Return all tokens between the two elements as a Text.
*/
void HtmlTclizeAscii(Tcl_Interp *interp, HtmlIndex *s, HtmlIndex *e){
  int i,j, nsub=0;
  HtmlElement* p=s->p;
  Tcl_DString str;
  if (p && p->base.type==Html_Text) {
    nsub=s->i;
  }
  Tcl_DStringInit(&str);
  while( p) {
    switch( p->base.type ){
      case Html_Block:
        break;
      case Html_Text:
        j=strlen(p->text.zText);
	if (j<nsub) nsub=j;
        if (p==e->p) {
	  j= (e->i-nsub+1);
	}
        Tcl_DStringAppend(&str, p->text.zText+nsub,j-nsub);
	nsub=0;
        break;
      case Html_Space:
        for (j=0; j< p->base.count; j++) {
	  if (nsub-->0) continue;
          Tcl_DStringAppend(&str, " ", 1);
        }
        if ((p->base.flags & HTML_NewLine)!=0)
          Tcl_DStringAppend(&str, "\n",1);
	nsub=0;
        break;
      case Html_P:
      case Html_BR:
        Tcl_DStringAppend(&str, "\n",1);
	break;
      case Html_Unknown:
        break;
      default:
        break;
    }
    if (p==e->p) break;
    p = p->pNext;
  }
  Tcl_DStringResult(interp, &str);
}
/* Same as above, but puts ascii into buffer. */
int HtmlAscii2Buf(Tcl_Interp *interp, HtmlIndex *ip, HtmlIndex *ipEnd, 
  char *buffer, int len, int offset){
  int i,j,l, n=0, ob, oe;  char *cp;
  HtmlElement *pBegin=ip->p, *p=ip->p, *pEnd=ipEnd->p;
  ob=ip->i, oe=ipEnd->i;

  while (p) {
    switch( p->base.type ){
      case Html_Text:
        cp=p->text.zText;
	for (i=0,j=0; cp[i]; i++) {
	  if (ob>0) ob--;
	  else if (p==pEnd && i>=oe) return n;
	  else if (offset>0) offset--;
	  else if (n>=(len-1)) return n;
	  else {
	    buffer[n++]=cp[i];
	  }
	}
        break;
      case Html_P:
      case Html_BR:
	buffer[n++]='\n';
	break;
      case Html_Space:
        l=p->base.count; j=-1;
        if ((p->base.flags & HTML_NewLine)!=0) {
	   j=l++;
	}
	for (i=0; i<l; i++) {
	  if (ob>0) ob--;
	  else if (p==pEnd && i>=oe) return n;
	  else if (offset>0) offset--;
	  else if (n>=(len-1)) return n;
	  else {
	    buffer[n++]=(j==i?'\n':' ');
	  }
	}
        break;
    }
    if (p==pEnd) return n;
    p = p->pNext;
  }
  buffer[n]=0;
  return n;
}
/*
** Return all tokens between the two elements as a HTML.
*/
void HtmlTclizeHtml(Tcl_Interp *interp, HtmlElement *p, HtmlElement *pEnd){
  Tcl_DString str;
  int i,j;
  char *zName;
  char zLine[100];

  Tcl_DStringInit(&str);
  while( p && p!=pEnd ){
    switch( p->base.type ){
      case Html_Block:
        break;
      case Html_COMMENT:
        Tcl_DStringAppend(&str, "<!--",-1);
        Tcl_DStringAppend(&str, p->text.zText,-1);
        Tcl_DStringAppend(&str, "-->",-1);
        break;
      case Html_Text:
        Tcl_DStringAppend(&str, p->text.zText,-1);
        break;
      case Html_Space:
        for (j=0; j< p->base.count; j++) {
          Tcl_DStringAppend(&str, " ", 1);
        }
        if ((p->base.flags & HTML_NewLine)!=0)
          Tcl_DStringAppend(&str, "\n",1);
        break;
      case Html_Unknown:
        Tcl_DStringAppend(&str,"Unknown",-1);
        break;
      default:
        if( p->base.type >= HtmlGetMarkupMap(0)->type 
         && p->base.type <= HtmlGetMarkupMap(HTML_MARKUP_COUNT-1)->type ){
          zName = HtmlGetMarkupMap(p->base.type - HtmlGetMarkupMap(0)->type)->zName;
        }else{
          zName = "Unknown";
        }
        Tcl_DStringAppend(&str, "<",1);
        Tcl_DStringAppend(&str, zName,-1);
        for(i=0; i<(p->base.count-1); i++){
          Tcl_DStringAppend(&str, " ",1);
          Tcl_DStringAppend(&str, p->markup.argv[i++],-1);
          Tcl_DStringAppend(&str, "=",1);
          Tcl_DStringAppend(&str, p->markup.argv[i],-1);
        }
        Tcl_DStringAppend(&str, ">",1);
        break;
    }
    p = p->pNext;
  }
  Tcl_DStringResult(interp, &str);
}

void HtmlTclizeHtmlFmt(Tcl_Interp *interp, HtmlElement *p, HtmlElement *pEnd){
  Tcl_DString str;
  int i,j, indent=0;
  char *zName;
  char zLine[100];

  Tcl_DStringInit(&str);
  while( p && p!=pEnd ){
    switch( p->base.type ){
      case Html_Block:
        break;
      case Html_COMMENT:
        Tcl_DStringAppend(&str, "<!--",-1);
        Tcl_DStringAppend(&str, p->text.zText,-1);
        Tcl_DStringAppend(&str, "-->",-1);
        break;
      case Html_Text:
        Tcl_DStringAppend(&str, p->text.zText,-1);
        break;
      case Html_Space:
        for (j=0; j< p->base.count; j++) {
          Tcl_DStringAppend(&str, " ", 1);
        }
        if ((p->base.flags & HTML_NewLine)!=0)
          Tcl_DStringAppend(&str, "\n",1);
        break;
      case Html_Unknown:
        Tcl_DStringAppend(&str,"Unknown",-1);
        break;
      default:
        if( p->base.type >= HtmlGetMarkupMap(0)->type 
         && p->base.type <= HtmlGetMarkupMap(HTML_MARKUP_COUNT-1)->type ){
          zName = HtmlGetMarkupMap(p->base.type - HtmlGetMarkupMap(0)->type)->zName;
        }else{
          zName = "Unknown";
        }
        Tcl_DStringAppend(&str, "<",1);
        Tcl_DStringAppend(&str, zName,-1);
        for(i=0; i<(p->base.count-1); i++){
          Tcl_DStringAppend(&str, " ",1);
          Tcl_DStringAppend(&str, p->markup.argv[i++],-1);
          Tcl_DStringAppend(&str, "=",1);
          Tcl_DStringAppend(&str, p->markup.argv[i],-1);
        }
        Tcl_DStringAppend(&str, ">",1);
        break;
    }
    p = p->pNext;
  }
  Tcl_DStringResult(interp, &str);
}

/* Return num of chars at end of line that match chars at begin of pat */
int trailmatch(char *line, char *pat) {
  char *p, *e, *ep; int i, l=strlen(line);
  if (!l) return 0;
  for (i=0,l--; l>=i && line[l-i]==pat[i]; i++) ;
  return i;
}

/*
** Search all tokens between the two elements for pat.
*/
void HtmlTclizeFindText(Tcl_Interp *interp, char *pat, HtmlIndex *ip, HtmlIndex *iEnd, int nocase, int after){
  Tcl_DString str;
  int h, i1, i2, i=0,j,k, l=strlen(pat), n, m=0, fnd=0, sfnd=0;
  int si1, si2, mpos=0;
  char zLine[256];
  HtmlElement *p1=0, *p2=0, *sp1, *sp2, *p=ip->p, *pEnd=iEnd->p;
  if (nocase)
    for (k=0; k<l; k++) pat[k]=tolower(pat[k]);
  n=HtmlTokenNumber(p);
  i1=-1; i2=-1;
  while( p && p!=pEnd && !fnd ){
      switch (p->base.type) {
      case Html_Text:
        j=strlen(p->text.zText);
	for (k=0; k<j; k++) {
          char nchar=(nocase?tolower(p->text.zText[k]):p->text.zText[k]);
	  if (pat[mpos]==nchar) {
            mpos++;
	    if (!p1) { p1=p; i1=k; }
	  } else {
	    if (p1) {
	      mpos=0; p1=0; i1=-1; break;
	    }
	  }
	  if (mpos>=l) break;
	}
        break;
      case Html_Space:
        for (k=0; k< p->base.count; k++) {
	  if (pat[mpos]==' ') {
	    if (!p1) { p1=p; i1=k; }
	    mpos++;
	    if (mpos>=l) break;
	  } else {
	    mpos=0; p1=0; i1=-1; break;
	  }
        }
/*        if ((p->base.flags & HTML_NewLine)!=0) zLine[i=0]=0; */
        break;
    }
    if (mpos>=l) { fnd=1; i2=k; p2=p; }
    if (fnd && !after) {
      /* Primitive, but for reverse searches we search from the begining and
         take the last one matched. */
      sp1=p1; sp2=p2; si1=i1; si2=i2; sfnd=1;
      fnd=0;
    }
    if (fnd) break;
    p = p->pNext;
  }
  if ((!fnd) && sfnd && !after) {
    p1=sp1; p2=sp2; i1=si1; i2=si2; fnd=1;
  }
  if (fnd) {
    n=HtmlTokenNumber(p1);
    m=HtmlTokenNumber(p2);
    sprintf(zLine,"%d.%d %d.%d",n,i1,m,i2);
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, zLine,0);
  }
}
/*
** Search all tokens between the two elements for tag.
*/
void HtmlTclizeFind(Tcl_Interp *interp, int tag, HtmlElement *p, HtmlElement *pEnd, int near){
  Tcl_DString str;
  int i,j, n, nearest=0;
  char *zName;
  char zLine[100];

  Tcl_DStringInit(&str);
  while( p && p!=pEnd ){
    n=p->base.id;
    if (p->base.type==tag){
      if (near) {
	if (!nearest) nearest=n;
        else if (abs(near-n)<abs(near-nearest)) {
	       nearest=n; Tcl_DStringInit(&str); }
             else { p = p->pNext; n++; continue; }
      }
      switch (tag) {
      case Html_Block:
        break;
      case Html_Text:
        Tcl_DStringStartSublist(&str);
        sprintf(zLine,"%d",n);
        Tcl_DStringAppendElement(&str, zLine);
        Tcl_DStringAppendElement(&str, p->text.zText);
        Tcl_DStringEndSublist(&str);
        break;
      case Html_Space:
#if 0
        Tcl_DStringStartSublist(&str);
        for (j=0; j< p->base.count; j++) {
          Tcl_DStringAppend(&str, " ", 1);
        }
        if ((p->base.flags & HTML_NewLine)!=0)
          Tcl_DStringAppend(&str, "\n",1);
        Tcl_DStringEndSublist(&str);
#endif
        break;
      case Html_Unknown:
        Tcl_DStringAppend(&str,"Unknown",-1);
        break;
      default:
        Tcl_DStringStartSublist(&str);
        sprintf(zLine,"%d",n);
        Tcl_DStringAppendElement(&str, zLine);
        for(i=0; i<p->base.count; i++){
          Tcl_DStringAppendElement(&str, p->markup.argv[i]);
        }
        Tcl_DStringEndSublist(&str);

        break;
      }
    }
    p = p->pNext;
    n++;
  }
  Tcl_DStringResult(interp, &str);
}

extern int (*HtmlFetchSelectionPtr)(ClientData , int, char *, int );

static const char ev[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int atend;

static int
getidx(char *buffer, int len, int *posn) {
  char c;
  char *idx;
  if (atend) return -1;
  do {
    if ((*posn)>=len) {
      atend = 1;
      return -1;
    }
    c = buffer[(*posn)++];
    if (c<0 || c=='=') {
      atend = 1;
      return -1;
    }
    idx = strchr(ev, c);
  } while (!idx);
  return idx - ev;
}

int
base64decode(ClientData clientData, Tcl_Interp *interp,
	     int objc, Tcl_Obj *CONST objv[]) {
  Tcl_Obj *o;
  char *inbuffer;
  int ilen, olen, pos, tlen=1024, tpos=0;
  char outbuffer[3], *tbuf;
  int c[4];

  if (objc!=2) {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("needs one argument "
					      "(string to decode)", -1));
    return TCL_ERROR;
  }
  tbuf=(char*)malloc(tlen);
  inbuffer = Tcl_GetStringFromObj(objv[1], &ilen);
  pos = 0;
  atend = 0;
  while (!atend) {
    if (inbuffer[pos]=='\n' ||inbuffer[pos]=='\r') { pos++; continue; }
    c[0] = getidx(inbuffer, ilen, &pos);
    c[1] = getidx(inbuffer, ilen, &pos);
    c[2] = getidx(inbuffer, ilen, &pos);
    c[3] = getidx(inbuffer, ilen, &pos);

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
base64encode(ClientData clientData, Tcl_Interp *interp,
	     int objc, Tcl_Obj *CONST objv[]) {
  Tcl_Obj *result;
  char *ib;
  int i=0, ilen, olen, pos=0;
  char c[74];

  if (objc!=2) {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("needs one argument "
					      "(string to encode)", -1));
    return TCL_ERROR;
  }
  ib= Tcl_GetByteArrayFromObj (objv[1], &ilen);
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

#ifdef _TCLHTML_
static int
tclgzip (ClientData clientData, Tcl_Interp * interp,
            int objc, Tcl_Obj * CONST objv[]) {
}
static int
tclgunzip (ClientData clientData, Tcl_Interp * interp,
            int objc, Tcl_Obj * CONST objv[])
{
}
static int
tcldecompress (ClientData clientData, Tcl_Interp * interp,
            int objc, Tcl_Obj * CONST objv[]) {
}
#else

#include <zlib.h>

static int
tcldecompress (ClientData clientData, Tcl_Interp * interp,
	    int objc, Tcl_Obj * CONST objv[])
{
    int comprLen, uncomprLen=0, ulen=1024, blen=ulen;
    char *compr, *uncompr=(char*)malloc(ulen);
    int err;
    Tcl_Obj* o;
    z_stream d_stream; /* decompression stream */
    compr= Tcl_GetByteArrayFromObj (objv[1], &comprLen);

    d_stream.zalloc = (alloc_func)0;
    d_stream.zfree = (free_func)0;
    d_stream.opaque = (voidpf)0;

    d_stream.next_in  = compr;
    d_stream.avail_in = comprLen;
    d_stream.next_out = uncompr;

    err = inflateInit(&d_stream);
    if (err != Z_OK) goto inflaterror;

    d_stream.avail_out = blen;
    while (d_stream.total_in < comprLen) {
        err = inflate(&d_stream, Z_NO_FLUSH);
        if (err == Z_STREAM_END) break;
        if (err != Z_OK) goto inflaterror;
        if (d_stream.avail_out==0) {
          blen+=ulen;
          uncompr=(char*)realloc(uncompr,blen);
          d_stream.next_out = uncompr+(blen-ulen);
          d_stream.avail_out = blen;
	}
    }

    err = inflateEnd(&d_stream);
    if (err != Z_OK) goto inflaterror;
    o=Tcl_NewByteArrayObj(uncompr,uncomprLen);
    Tcl_IncrRefCount (o);
    Tcl_SetObjResult (interp,o);
    Tcl_DecrRefCount (o);
    free(uncompr);
    return TCL_OK;
inflaterror:
    free(uncompr);
    printf("ERR: %d\n",err);
    return TCL_ERROR;
}

static int
tclgunzip (ClientData clientData, Tcl_Interp * interp,
	    int objc, Tcl_Obj * CONST objv[])
{
    int r, l=0, ulen=1024, blen=ulen, fnlen;
    char *fn, *from, *uncompr=(char*)malloc(ulen);
    gzFile zF;
    Tcl_Obj* o;
    if (objc<3) {  goto gunziperror; }
    from = Tcl_GetStringFromObj (objv[1], &fnlen);
    if (!strcmp(from,"-file")) {
      fn = Tcl_GetStringFromObj (objv[2], &fnlen);
      if (!(zF=gzopen(fn,"rb"))) goto gunziperror; 
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
    } else goto gunziperror;
    o=Tcl_NewByteArrayObj(uncompr,l);
    Tcl_IncrRefCount (o);
    Tcl_SetObjResult (interp,o);
    Tcl_DecrRefCount (o);
    free(uncompr);
    return TCL_OK;
gunziperror:
    free(uncompr);
    Tcl_SetObjResult (interp, Tcl_NewStringObj ("gunzip error", -1));
    return TCL_ERROR;
}

static int
tclgzip (ClientData clientData, Tcl_Interp * interp,
	    int objc, Tcl_Obj * CONST objv[])
{
    int r, l=0, ilen, fnlen;
    char *ib, *fn, *to;
    gzFile zF;
    Tcl_Obj* o;
    if (objc<4) {  goto gziperror; }
    to = Tcl_GetStringFromObj (objv[1], &fnlen);
    ib= Tcl_GetByteArrayFromObj (objv[3], &ilen);
    if (!strcmp(to,"-file")) {
      fn = Tcl_GetStringFromObj (objv[2], &fnlen);
      if (!(zF=gzopen(fn,"wb"))) goto gziperror; 
      for (;;) {
        r=gzwrite(zF,ib+l,ilen);
        if (r<0) goto gziperror;
        l+=r;
        if (r==0) break;
        ilen-=r;
        if (ilen<=0) break;
      }
    } else goto gziperror;
    return TCL_OK;
gziperror:
    Tcl_SetObjResult (interp, Tcl_NewStringObj ("gzip error", -1));
    return TCL_ERROR;
}
#endif

static int
killpidcmd(ClientData clientData, Tcl_Interp *interp,
	     int objc, Tcl_Obj *CONST objv[]) {
#ifndef __WIN32__
  char *ib;
  int i=0, n, sig, ilen, olen, pos=0;
  char c[74];

  if (objc!=3) {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("needs two arguments "
					      "(pid signum)", -1));
    return TCL_ERROR;
  }
  ib = Tcl_GetStringFromObj(objv[1], &ilen);
  n = atoi(ib);
  ib = Tcl_GetStringFromObj(objv[2], &ilen);
  sig = atoi(ib);
  n=kill(n,sig);
  sprintf(c,"%d",n);
  Tcl_SetObjResult(interp, Tcl_NewStringObj(c,-1));
#endif
  return TCL_OK;
}

int stdchancmd(ClientData clientData, Tcl_Interp *interp,
	     int objc, Tcl_Obj *CONST objv[]) {
    Tcl_Channel chan;
    int mode, type;
    char *channelId = Tcl_GetString(objv[1]);
    char *stype=Tcl_GetString(objv[2]);

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

  if (objc!=3) {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("textfmt needs two arguments "
					      "(text to format) len", -1));
    return TCL_ERROR;
  }
  ib = Tcl_GetStringFromObj(objv[1], &ilen);
  ob = Tcl_GetStringFromObj(objv[2], &ilen);
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

  if (objc!=3) {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("lockcopy needs two arguments "
					      "src dest", -1));
    return TCL_ERROR;
  }
  ib = Tcl_GetStringFromObj(objv[1], &ilen);
  ob = Tcl_GetStringFromObj(objv[2], &olen);
  if ((indesc = open (ib, O_RDWR))) {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("open in failed", -1));
    return TCL_ERROR;
  }
#if !defined(__WIN32__) && !defined(sparc)
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
#if !defined(__WIN32__) && !defined(sparc)
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
  if (objc<3) {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("needs two arguments "
                                              "(password data)", -1));
    return TCL_ERROR;
  }
  result=Tcl_NewStringObj("",0);
  ip = Tcl_GetStringFromObj(objv[1], &ilen);
  ib = Tcl_GetStringFromObj(objv[2], &olen);
  for (i=3; i<objc; i++) {
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
  int vobjc=4; Tcl_Obj *vobjv[6];
  if (objc<3) {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("needs two arguments "
        "(password data)", -1));
    return TCL_ERROR;
  }
  vobjv[0]=objv[0];
  vobjv[2]=objv[2];
  ib = Tcl_GetStringFromObj(objv[1], &olen);
  for (i=0; srcpass[i] && ib[i]; i++)  pw[i]=ib[i]^srcpass[i]; 
  pw[i]=0;
  vobjv[1]=Tcl_NewStringObj(pw,-1);
  vobjv[3]=Tcl_NewStringObj("-randomize",-1);
  if (TCL_OK==(rc=xorstrcmd(clientData, interp, vobjc, vobjv))) {
    if (decrypt) {
      char *cp=strdup(Tcl_GetStringResult(interp));
      rc=Tcl_GlobalEval(interp, cp);
      free(cp);
    }
  }
  Tcl_DecrRefCount(vobjv[1]);
  Tcl_DecrRefCount(vobjv[3]);
  return rc;
}
static int
decryptsrccmd(ClientData clientData, Tcl_Interp *interp,
             int objc, Tcl_Obj *CONST objv[]) {
  return decryptsrcsub(clientData, interp, objc, objv, 1);
}
static int
srcencryptcmd(ClientData clientData, Tcl_Interp *interp,
             int objc, Tcl_Obj *CONST objv[]) {
  return decryptsrcsub(clientData, interp, objc, objv, 0);
}


#ifndef __WIN32__
#include <netdb.h>

static void donslookup(char *name, char *buf) {
  struct hostent *he = gethostbyname(name);
  buf[0]=0;
  if (he) {
    int i; unsigned char* cp;
    cp=he->h_addr;
    sprintf(buf,"%d.%d.%d.%d", cp[0],cp[1],cp[2],cp[3]);
  }
}

static int nslookup(ClientData clientData, Tcl_Interp *interp,
             int objc, Tcl_Obj *CONST objv[]) {
  Tcl_Obj *result;
  char *ib;
  int i=00, ilen;
  char buf[300];

  if (objc!=2) {
    Tcl_SetObjResult(interp, Tcl_NewStringObj("needs one argument "
                                              "(name to lookup)", -1));
    return TCL_ERROR;
  }
  ib = Tcl_GetStringFromObj(objv[1], &ilen);
  donslookup(ib,buf);
  result = Tcl_NewStringObj("", 0);
  Tcl_AppendToObj(result, buf, strlen(buf));
  Tcl_SetObjResult(interp, result);
  return TCL_OK;
}
#endif

#if INTERFACE
#define DLL_EXPORT
#endif
#if defined(USE_TCL_STUBS) && defined(__WIN32__)
# undef DLL_EXPORT
# define DLL_EXPORT __declspec(dllexport)
#endif

extern int tkhtmlexiting;

int
HtmlExitImmed(ClientData clientData, Tcl_Interp *interp,
             int objc, Tcl_Obj *CONST objv[]) {
  /* Sad, but TK is core dumping on exit.  So here is the backway out. */
  exit(0);
}

int HtmlBP () {
  return TCL_OK;
}
static char *BegEnd = " ?-begin INDEX? ?-end INDEX? ?-range {INDEX INDEX}?";

/*DLL_EXPORT*/ int Htmlexts_Init(Tcl_Interp *interp) {
#ifdef USE_TCL_STUBS
  HtmlPostscriptPtr=0;
  if( Tcl_InitStubs(interp,"8.3",0)==0 ){
    return TCL_ERROR;
  }
#ifndef _TCLHTML_
  if( Tk_InitStubs(interp,"8.3",0)==0 ){
    return TCL_ERROR;
  }
#endif /* _TCLHTML_ */
#else
#ifndef _TCLHTML_
  HtmlPostscriptPtr=HtmlPostscript;
#endif /* _TCLHTML_ */
  Tcl_PkgProvide(interp, HTML_PKGNAME"pr", HTML_PKGVERSION);
#endif
  Tcl_CreateObjCommand(interp, "stdchan", stdchancmd,
		       (ClientData)0, NULL);
  Tcl_CreateObjCommand(interp, "textfmt", textfmtcmd,
		       (ClientData)0, NULL);
  Tcl_CreateObjCommand(interp, "lockcopy", lockcopycmd,
		       (ClientData)0, NULL);
  Tcl_CreateObjCommand(interp, "base64decode", base64decode,
		       (ClientData)0, NULL);
  Tcl_CreateObjCommand(interp, "base64encode", base64encode,
		       (ClientData)0, NULL);
  Tcl_CreateObjCommand(interp, "killpid", killpidcmd,
		       (ClientData)0, NULL);
  Tcl_CreateObjCommand(interp, "xorstr", xorstrcmd,
		       (ClientData)0, NULL);
  Tcl_CreateObjCommand(interp, "srcencrypt", srcencryptcmd,
		       (ClientData)0, NULL);
  Tcl_CreateObjCommand(interp, "decryptsrc", decryptsrccmd,
		       (ClientData)0, NULL);
  Tcl_CreateObjCommand(interp, "tkhtmlexit", HtmlExitImmed,
		       (ClientData)0, NULL);
  Tcl_CreateObjCommand (interp, "gunzip", tclgunzip,
			(ClientData) 0, NULL);
  Tcl_CreateObjCommand (interp, "gzip", tclgzip,
			(ClientData) 0, NULL);
  Tcl_CreateObjCommand (interp, "decompress", tcldecompress,
			(ClientData) 0, NULL);
#ifndef __WIN32__
  Tcl_CreateObjCommand(interp, "nslookup", nslookup,
                       (ClientData)0, NULL);
#endif

  HtmlCommandDel("text","ascii");
  HtmlCommandDel("text","delete");
  HtmlCommandDel("text","html");
  HtmlCommandDel("text","insert");
  HtmlCommandDel("token","delete");
  HtmlCommandDel("token","find");
  HtmlCommandDel("token","get");
  HtmlCommandDel("token","insert");
  HtmlCommandAdd("text", "ascii",3, 5, "START END",HtmlTextAsciiCmd);
  HtmlCommandAdd("text","delete",3, 5, "START END",HtmlTextDeleteCmd);
  HtmlCommandAdd("text","insert",5, 5, "INDEX TEXT",HtmlTextInsertCmd);
  HtmlCommandAdd("text","break",4, 4, "INDEX",HtmlTextInsertCmd);
  HtmlCommandAdd("text","html",3, 5, "START END", HtmlTextHtmlCmd );
  HtmlCommandAdd("text","find",4, 7,"TEXT ?nocase? ?before|after INDEX?", HtmlTextFindCmd);
  HtmlCommandAdd("text","offset",6, 6,"START NUM1 NUM2", HtmlTextOffsetCmd);
  HtmlCommandAdd("token","delete",4, 5, "INDEX ?INDEX?",HtmlTokenDeleteCmd);
  HtmlCommandAdd("token","insert",5, 6, "INDEX TAG ARGS",HtmlTokenInsertCmd);
  HtmlCommandAdd("token","find",4, 6, "TAG ?before|after|near INDEX?", HtmlTokenFindCmd);
  HtmlCommandAdd("token","get",3, 5, "INDEX ?INDEX?",HtmlTokenGetCmd);
  HtmlCommandAdd("token","list",3, 5, "START END",HtmlTokenListCmd);
  HtmlCommandAdd("token","markup",5, 5, "START END",HtmlTokenMarkupCmd);
  HtmlCommandAdd("token","domtokens",5, 5, "START END",HtmlTokenDomCmd);
  HtmlCommandAdd("token","getend",4, 4, "INDEX",HtmlTokenGetEnd);
  HtmlCommandAdd("token","attr",4, 6, "INDEX ?NAME ?VALUE??",HtmlTokenAttr);
  HtmlCommandAdd("token", "attrs", 4, 6, "attrlist",HtmlTokenAttrSearch);
  HtmlCommandAdd("token", "unique", 4, 6, "tag",HtmlTokenUnique);
  HtmlCommandAdd("token", "onEvents", 3, 5, BegEnd,HtmlTokenOnEvents);
  HtmlCommandAdd("dom", "value", 4, 5, "DomAddr ?newvalue?",HtmlDomCmd);
  HtmlCommandAdd("dom", "id", 4, 4, "DomAddr",HtmlDomCmd);
  HtmlCommandAdd("dom", "ids", 4, 4, "DomAddr",HtmlDomCmd);
  HtmlCommandAdd("dom", "addr", 4, 8, "index", HtmlIdToDomCmd);
  HtmlCommandAdd("dom", "tree", 4, 5, "index ?value?",HtmlDomTreeCmd);
  HtmlCommandAdd("dom", "nameidx", 5, 5, "tag name",HtmlDomName2Index);
  HtmlCommandAdd("dom", "radioidx", 6, 6, "form name idx",HtmlDomRadio2Index);
  HtmlCommandAdd("dom", "formel", 5, 5, "form name",HtmlDomFormElIndex);
#ifndef _TCLHTML_
  HtmlCommandAdd("bgimage", 0, 3, 4, 0,HtmlImageBgCmd );
  HtmlCommandAdd("postscript",0,2,0,0,HtmlPostscriptCmd );
  HtmlCommandAdd("overattr",0,5, 5, "X Y ATTR",HtmlAttrOverCmd);
  HtmlCommandAdd("over",0,4, 5, "X Y ?-muponly?",HtmlOverCmd);
  HtmlCommandAdd("imageat",0,4, 4, "X Y",HtmlImageAtCmd);
  HtmlCommandAdd("imageset",0,4, 4, "ID INDEX",HtmlImageSetCmd);
  HtmlCommandAdd("imageupdate",0,4, 4, "ID IMAGES",HtmlImageUpdateCmd);
  HtmlCommandAdd("imageadd",0,4, 4, "ID IMAGES",HtmlImageAddCmd);
  HtmlCommandAdd("onscreen",0,3, 3, "ID",HtmlOnScreen);
  HtmlCommandAdd("forminfo",0,4, 4, "ID",HtmlFormInfo);
  HtmlCommandAdd("coords",0,2,4,"?INDEX ?percent??",HtmlCoordsCmd);
  HtmlCommandAdd("images",0, 3, 3, "?list|html?", HtmlImagesListCmd);
  HtmlCommandAdd("refresh",0, 2, 15, "", HtmlRefreshCmd);
  HtmlCommandAdd("source",0, 2, 2, "", HtmlSourceCmd);
  HtmlCommandAdd("htmlbp",0, 2, 2, "", HtmlBP);
  HtmlCommandAdd("sizewindow",0, 3, 3, "", HtmlSizeWindow);
  HtmlFetchSelectionPtr=HtmlFetchSelection;
#endif /* _TCLHTML_ */
  Tcl_LinkVar(interp, "tkhtmlexiting", (char*)&tkhtmlexiting, TCL_LINK_INT);
  return TCL_OK;
}







