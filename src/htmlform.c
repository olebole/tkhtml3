/*
** Routines used for processing HTML makeup for forms.
** $Revision: 1.8 $
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
**
** When the TCL "exit" command is invoked, the order of operations
** here is very touchy.  
*/
void HtmlDeleteControls(HtmlWidget *htmlPtr){
  HtmlElement *p;        /* For looping over all controls */
  Tcl_Interp *interp;    /* The interpreter */
  
  interp = htmlPtr->interp;
  p = htmlPtr->firstInput;
  htmlPtr->firstInput = 0;
  htmlPtr->lastInput = 0;
  htmlPtr->nInput = 0;
  if( p==0 ) return;
  HtmlLock(htmlPtr);
  for(; p; p=p->input.pNext){
    if( p->input.pForm && p->input.pForm->form.id>0 
         && htmlPtr->zFormCommand && htmlPtr->zFormCommand[0]
         && !Tcl_InterpDeleted(interp) ){
      Tcl_DString cmd;
      int result;
      char zBuf[60];
      Tcl_DStringInit(&cmd);
      Tcl_DStringAppend(&cmd, htmlPtr->zFormCommand, -1);
      sprintf(zBuf," %d flush", p->input.pForm->form.id);
      Tcl_DStringAppend(&cmd, zBuf, -1);
      result = Tcl_GlobalEval(htmlPtr->interp, Tcl_DStringValue(&cmd));
      Tcl_DStringFree(&cmd);
      if( !Tcl_InterpDeleted(interp) ){
        if( result != TCL_OK ){
          Tcl_AddErrorInfo(htmlPtr->interp,
             "\n    (-formcommand flush callback executed by html widget)");
          Tcl_BackgroundError(htmlPtr->interp);
          TestPoint(0);
        }
        Tcl_ResetResult(htmlPtr->interp);
      }
      p->input.pForm->form.id = 0;
    }
    if( p->input.tkwin ){
      Tk_DestroyWindow(p->input.tkwin);
      p->input.tkwin = 0;
    }
  }
  HtmlUnlock(htmlPtr);
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
** Create the window name for a child widget.  Space to hold the name
** is obtained from ckalloc() and must be freed by the calling function.
*/
static char *MakeWindowName(
  HtmlWidget *htmlPtr,        /* The HTML widget */
  HtmlElement *pElem          /* The input that needs a child widget */
){
  int n;
  char *zBuf;

  n = strlen(Tk_PathName(htmlPtr->clipwin));
  zBuf = ckalloc( n + 20 );
  sprintf(zBuf,"%s.x%d",Tk_PathName(htmlPtr->clipwin), pElem->input.cnt);
  return zBuf;
}

/*
** A Input element is the input.  Mark this element as being
** empty.  It has no widget and doesn't appear on the screen.
**
** This is called for HIDDEN inputs or when the -formcommand
** callback doesn't create the widget.
*/
static void EmptyInput(HtmlElement *pElem){
  pElem->input.tkwin = 0;
  pElem->input.w = 0;
  pElem->input.h = 0;
  pElem->base.flags &= !HTML_Visible;
  pElem->base.style.flags |= STY_Invisible;
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
    EmptyInput(pElem);
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
  char *zWin;            /* Name of child widget that implements this input */
  int incomplete = 0;    /* True if data is incomplete */
  Tcl_DString cmd;       /* The complete -formcommand callback */
 
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
      int result;
      char zToken[50];

      if( pElem->input.pForm==0 || htmlPtr->zFormCommand==0 
           || htmlPtr->zFormCommand[0]==0 ){
        EmptyInput(pElem);
        break;
      }
      Tcl_DStringInit(&cmd);
      Tcl_DStringAppend(&cmd, htmlPtr->zFormCommand, -1);
      sprintf(zToken," %d input ",pElem->input.pForm->form.id);
      Tcl_DStringAppend(&cmd, zToken, -1);
      pElem->input.cnt = ++htmlPtr->nInput;
      zWin = MakeWindowName(htmlPtr, pElem);
      Tcl_DStringAppend(&cmd, zWin, -1);
      Tcl_DStringStartSublist(&cmd);
      HtmlAppendArglist(&cmd, pElem);
      Tcl_DStringEndSublist(&cmd);
      HtmlLock(htmlPtr);
      result = Tcl_GlobalEval(htmlPtr->interp, Tcl_DStringValue(&cmd));
      Tcl_DStringFree(&cmd);
      if( !HtmlUnlock(htmlPtr) ){
        SizeAndLink(htmlPtr, zWin, pElem);
      }
      ckfree(zWin);
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
