/*
** Routines used for processing HTML makeup for forms.
** $Revision: 1.7 $
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
#include <stdarg.h>
#include "htmlform.h"

/*
** Unmap any input control that is currently mapped.
*/
void HtmlUnmapControls(HtmlWidget *htmlPtr){
  HtmlElement *p;

  for(p=htmlPtr->firstInput; p; p=p->input.pNext){
    if( p->input.tkwin!=0 && Tk_IsMapped(p->input.tkwin) ){
      Tk_UnmapWindow(p->input.tkwin);
      TestPoint(0);
    }else{
      TestPoint(0);
    }
  }
}

/*
** Map any control that should be visible according to the
** current scroll position.  At the same time, if any controls that
** should not be visible are mapped, unmap them.  After this routine
** finishes, all <INPUT> controls should be in their proper places
** regardless of where they might have been before.
*/
void HtmlMapControls(HtmlWidget *htmlPtr){
  HtmlElement *p;     /* For looping over all controls */
  int x, y, w, h;     /* Part of the virtual canvas that is visible */

  x = htmlPtr->xOffset;
  y = htmlPtr->yOffset;
  w = Tk_Width(htmlPtr->clipwin);
  h = Tk_Height(htmlPtr->clipwin);
  for(p=htmlPtr->firstInput; p; p=p->input.pNext){
    if( p->input.tkwin==0 ) continue;
    if( p->input.y < y+h 
     && p->input.y + p->input.h > y
     && p->input.x < x+w
     && p->input.x + p->input.w > x
    ){
      /* The control should be visible.  Make is so if it isn't already */
      Tk_MoveResizeWindow(p->input.tkwin, 
          p->input.x - x, p->input.y - y, 
          p->input.w, p->input.h);
      if( !Tk_IsMapped(p->input.tkwin) ){
        Tk_MapWindow(p->input.tkwin);
        TestPoint(0);
      }else{
        TestPoint(0);
      }
    }else{
      /* This control should not be visible.  Unmap it. */
      if( Tk_IsMapped(p->input.tkwin) ){
        Tk_UnmapWindow(p->input.tkwin);
        TestPoint(0);
      }else{
        TestPoint(0);
      }
    }
  }
}

/*
** Delete all input controls.  This happens when the HTML widget
** is cleared.
*/
void HtmlDeleteControls(HtmlWidget *htmlPtr){
  HtmlElement *p;     /* For looping over all controls */
  int i;
  
  for(p=htmlPtr->firstInput; p; p=p->input.pNext){
    if( p->input.tkwin ){
      Tk_DestroyWindow(p->input.tkwin);
      p->input.tkwin = 0;
      TestPoint(0);
    }else{
      TestPoint(0);
    }
  }
  htmlPtr->firstInput = 0;
  htmlPtr->lastInput = 0;
  htmlPtr->nInput = 0;
}

/*
** Return an appropriate type value for the given <INPUT> markup.
*/
static int InputType(HtmlElement *p){
  int type = INPUT_TYPE_Unknown;
  char *z;
  int i;
  static struct {
    char *zName;
    int type;
  } types[] = {
    { "checkbox",  INPUT_TYPE_Checkbox },
    { "file",      INPUT_TYPE_File     },
    { "hidden",    INPUT_TYPE_Hidden   },
    { "image",     INPUT_TYPE_Image    },
    { "password",  INPUT_TYPE_Password },
    { "radio",     INPUT_TYPE_Radio    },
    { "reset",     INPUT_TYPE_Reset    },
    { "submit",    INPUT_TYPE_Submit   },
    { "text",      INPUT_TYPE_Text     },
  };

  switch( p->base.type ){
    case Html_INPUT:
      z = HtmlMarkupArg(p, "type", 0);
      if( z==0 ){ TestPoint(0); break; }
      for(i=0; i<sizeof(types)/sizeof(types[0]); i++){
        if( stricmp(types[i].zName,z)==0 ){
          type = types[i].type;
          TestPoint(0);
          break;
        }
        TestPoint(0);
      }
      break;
    case Html_SELECT:
      type = INPUT_TYPE_Select;
      TestPoint(0);
      break;
    case Html_TEXTAREA:
      type = INPUT_TYPE_TextArea;
      TestPoint(0);
      break;
    case Html_APPLET:
      type = INPUT_TYPE_Applet;
      TestPoint(0);
      break;
    default:
      TestPoint(0);
      break;
  }
  return type;
}

/*
** Create the window name for a child widget.  Try to use the buffer
** supplied, but if it isn't big enough, get more memory from ckalloc().
**
** The calling function should invoke FreeWindowName() on the return
** value in order to deallocate any memory obtained from ckalloc().
*/
static char *MakeWindowName(
  HtmlWidget *htmlPtr,        /* The HTML widget */
  HtmlElement *pElem,         /* The input that needs a child widget */
  char *zBuf,                 /* Try to write the name here. */
  int nBuf                    /* Space available in zBuf[] */
){
  int n;

  n = strlen(Tk_PathName(htmlPtr->clipwin));
  if( n + 50 >= nBuf ){
    zBuf = ckalloc( n + 50 );
    TestPoint(0);
  }else{
    TestPoint(0);
  }
  sprintf(zBuf,"%s.x%d",Tk_PathName(htmlPtr->clipwin), pElem->input.cnt);
  return zBuf;
}

/*
** Free the window name obtained from MakeWindowName(), if needed.
*/
static void FreeWindowName(char *zWin, char *zWinBuf){
  if( zWin!=zWinBuf ){
    ckfree(zWin);
    TestPoint(0);
  }else{
    TestPoint(0);
  }
}

/*
** zWin is the name of a child widget that is used to implement an
** input element.  Query Tk for information about this widget (such
** as its size) and put that information in the pElem structure
** that represents the input.
*/
static void SizeAndLink(HtmlWidget *htmlPtr, char *zWin, HtmlElement *pElem){
  pElem->input.tkwin = Tk_NameToWindow(htmlPtr->interp, zWin, htmlPtr->clipwin);
  if( pElem->input.tkwin==0 ){
    Tcl_ResetResult(htmlPtr->interp);
    pElem->input.w = 0;
    pElem->input.h = 0;
    pElem->base.flags &= !HTML_Visible;
    pElem->base.style.flags |= STY_Invisible;
  }else{
    pElem->input.w = Tk_ReqWidth(pElem->input.tkwin);
    pElem->input.h = Tk_ReqHeight(pElem->input.tkwin);
    pElem->base.flags |= HTML_Visible;
  }
  pElem->input.pNext = 0;
  if( htmlPtr->firstInput==0 ){
    htmlPtr->firstInput = pElem;
    TestPoint(0);
  }else{
    htmlPtr->lastInput->input.pNext = pElem;
    TestPoint(0);
  }
  htmlPtr->lastInput = pElem;
}

/*
** The "p" argument points to a <select>.  This routine scans all
** subsequent elements (up to the next </select>) looking for
** <option> tags.  For each option tag, it appends three elements
** to the "str" DString:
**
**     *        1 or 0 to indicated whether or not the element is
**              selected.
**
**     *        The value returned if this element is selected.
**
**     *        The text displayed for this element.
*/
static void AddSelectOptions(Tcl_DString *str, HtmlElement *p){
  /* TBD */
}

/*
** This routine implements the Sizer() function for <INPUT>,
** <SELECT> and <TEXTAREA> markup.
**
** A side effect of sizing these markups is that widgets are
** created to represent the corresponding input controls.
**
** The function normally returns 0.  But if it is dealing with
** a <SELECT> or <TEXTAREA> that is incomplete, 1 is returned.
** In that case, the sizer will be called again at some point in
** the future when more information is available.
*/
int HtmlControlSize(HtmlWidget *htmlPtr, HtmlElement *pElem){
  Tk_Font font;
  int force;             /* True to force variable initialization */
  char *zWin;            /* Name of child widget that implements this input */
  int nWin;              /* Length of the child widget name */
  int incomplete = 0;    /* True if data is incomplete */
  char zWinBuf[1000];    /* Space to hold child widget name */
 
  pElem->input.type = InputType(pElem);
  switch( pElem->input.type ){
    case INPUT_TYPE_Checkbox:
    case INPUT_TYPE_Hidden:
    case INPUT_TYPE_Image:
    case INPUT_TYPE_Radio:
    case INPUT_TYPE_Reset:
    case INPUT_TYPE_Submit:
    case INPUT_TYPE_Text:
    case INPUT_TYPE_Password:
    case INPUT_TYPE_File: {
      pElem->base.flags &= ~HTML_Visible;
      pElem->base.style.flags |= STY_Invisible;
      pElem->input.tkwin = 0;
      break;
    }
    case INPUT_TYPE_Select: {
      pElem->base.flags &= ~HTML_Visible;
      pElem->base.style.flags |= STY_Invisible;
      pElem->input.tkwin = 0;
      break;
    }
    case INPUT_TYPE_TextArea: {
      pElem->base.flags &= ~HTML_Visible;
      pElem->base.style.flags |= STY_Invisible;
      pElem->input.tkwin = 0;
      break;
    }
    case INPUT_TYPE_Applet: {
      pElem->base.flags &= ~HTML_Visible;
      pElem->base.style.flags |= STY_Invisible;
      pElem->input.tkwin = 0;
      break;
    }
    default: {
      CANT_HAPPEN;
      pElem->base.flags &= ~HTML_Visible;
      pElem->base.style.flags |= STY_Invisible;
      pElem->input.tkwin = 0;
      break;
    }
  }
  return incomplete;
}

#if 0
/*
** The following array determines which characters can be put directly
** in a query string and which must be escaped.
*/
static char needEscape[] = {
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1,
};
#define NeedToEscape(C) ((C)>0 && (C)<127 && needEscape[(int)(C)])

/*
** Append to the given DString, an encoded version of the given
** text.
*/
static void EncodeText(Tcl_DString *str, char *z){
  int i;
  while( *z ){
    for(i=0; z[i] && !NeedToEscape(z[i]); i++){ TestPoint(0); }
    if( i>0 ){ TestPoint(0); Tcl_DStringAppend(str, z, i); }
    z += i;
    while( *z && NeedToEscape(*z) ){
      if( *z==' ' ){
        Tcl_DStringAppend(str,"+",1);
        TestPoint(0);
      }else if( *z=='\n' ){
        Tcl_DStringAppend(str, "%0D%0A", 6);
        TestPoint(0);
      }else if( *z=='\r' ){
        /* Ignore it... */
        TestPoint(0);
      }else{
        char zBuf[5];
        sprintf(zBuf,"%%%02X",0xff & *z);
        Tcl_DStringAppend(str, zBuf, 3);
        TestPoint(0);
      }
      z++;
    }
  }
}
#endif
