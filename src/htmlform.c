/*
** Routines used for processing HTML makeup for forms.
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
#include <stdarg.h>
#include "htmlform.h"

/*
** The following string is used in conjuction with the htmlPtr->varId
** value to construct a unique name for an array variable used by
** some input controls.
*/
#define VAR_NAME_BASE   "@Html"

/*
** The amount of spaced needed to hold a variable name
*/
#define VAR_SIZE   100

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
  char zName[VAR_SIZE];
  
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
  for(i=0; i<=htmlPtr->nForm; i++){
    sprintf(zName,"%s_%d_%d",VAR_NAME_BASE, htmlPtr->varId,i);
    Tcl_UnsetVar2(htmlPtr->interp, zName, 0, TCL_GLOBAL_ONLY);
    TestPoint(0);
  }
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
** Create the name of a state variable for an input.  The state variable
** is a global variable that holds the state of the input device.
** State variables are only used for inputs that are implemented using
** Tk widgets that have "-textvariable" option (or its equivalent.)
*/
static void MakeVarName(
  HtmlWidget *htmlPtr,   /* The widget with which the variable is associated */
  HtmlElement *pElem,    /* The <input> element using this variable */
  char *zBuf,            /* The name of the variable is written here */
  char *zInit,           /* An initial value for the variable */
  int force              /* True to force initialization.  If false, only
                         ** initialize if the variable doesn't exist */
){
  char *zName;
  int id;

  if( pElem->input.pForm==0 ){
    id = 0;
    TestPoint(0);
  }else{
    id = pElem->input.pForm->form.id;
    TestPoint(0);
  }
  zName = HtmlMarkupArg(pElem, "name", 0);
  if( zName && pElem->input.type==INPUT_TYPE_Radio ){
    sprintf(zBuf,"%s_%d_%d(%.*s)", VAR_NAME_BASE, htmlPtr->varId, id,
       VAR_SIZE - 30,  zName);
    TestPoint(0);
  }else{
    sprintf(zBuf,"%s_%d_%d(_%d)", VAR_NAME_BASE, htmlPtr->varId, id,
       pElem->input.cnt);
    TestPoint(0);
  }
  if( zInit ){
    if( force || Tcl_GetVar(htmlPtr->interp, zBuf, TCL_GLOBAL_ONLY)==0 ){
      Tcl_SetVar(htmlPtr->interp, zBuf, zInit, TCL_GLOBAL_ONLY);
      TestPoint(0);
    }else{
      TestPoint(0);
    }
  }else{
    TestPoint(0);
  }
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
  pElem->input.w = Tk_ReqWidth(pElem->input.tkwin);
  pElem->input.h = Tk_ReqHeight(pElem->input.tkwin);
  pElem->base.flags |= HTML_Visible;
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
** This is a convenience routine used to directly call the
** Tk command functions that implement Tk widgets.  The
** xFunc argument is a pointer to a Tk command function such
** as Tk_CheckbuttonCmd() or Tk_FrameCmd().  zName is the name
** of the widget to be created.  Additional arguments might follow.
*/
static void ExecCmd(
  HtmlWidget *htmlPtr,
  int (*xFunc)(ClientData, Tcl_Interp*, int, char**),
  char *zName,
  ...
){
  va_list ap;
  int argc;
  int i;
  char **argv;
  char *argvBuf[100];

  argc = 1;
  va_start(ap,zName);
  while( va_arg(ap,char*)!=0 ){
    argc++;
    TestPoint(0);
  }
  va_end(ap);
  if( argc>=sizeof(argvBuf)/sizeof(argvBuf[0]) ){
    argv = (char**)ckalloc( sizeof(char*) * argc+1 );
    TestPoint(0);
  }else{
    argv = argvBuf;
    TestPoint(0);
  }
  argv[0] = zName;
  va_start(ap,zName);
  for(i=1; i<argc; i++){
    argv[i] = va_arg(ap,char*);
    TestPoint(0);
  }
  argv[i] = 0;
  va_end(ap);
  (*xFunc)(htmlPtr->clipwin, htmlPtr->interp, argc, argv);
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
  char *zVal;            /* */
  char *zWidth;          /* */
  char *zHeight;         /* */
  char *zCallback;       /* Text of a callback */
  char *zWin;            /* Name of child widget that implements this input */
  int nWin;              /* Length of the child widget name */
  int incomplete = 0;    /* True if data is incomplete */
  char zValBuf[20];      /* Fake value for when value= omitted from radio */
  char zVar[VAR_SIZE];   /* Space to hold state variable name */
  char zWinBuf[1000];    /* Space to hold child widget name */
  char zCbBuf[1000];     /* Space to hold the callback */
 
  pElem->input.type = InputType(pElem);
  switch( pElem->input.type ){
    case INPUT_TYPE_Checkbox:
#if 0
      font = HtmlGetFont(htmlPtr, pElem->base.style.font);
      pElem->input.cnt = ++htmlPtr->nInput;
      zVal = HtmlMarkupArg(pElem, "checked", 0)!=0 ? "1" : "0";
      MakeVarName(htmlPtr, pElem, zVar, zVal, 1);
      zWin = MakeWindowName(htmlPtr, pElem, zWinBuf, sizeof(zWinBuf));
      ExecCmd(htmlPtr, Tk_CheckbuttonCmd,
         "checkbutton", zWin, "-variable", zVar, 
         "-font", Tk_NameOfFont(font), 
         "-padx", "0", "-pady", "0", "-text" , "", 0);
      SizeAndLink(htmlPtr, zWin, pElem);
      pElem->input.padLeft = pElem->input.w/2;
      FreeWindowName(zWin,zWinBuf);
      TestPoint(0);
#endif
      break;
    case INPUT_TYPE_Hidden:
      pElem->base.flags &= ~HTML_Visible;
      pElem->base.style.flags |= STY_Invisible;
      pElem->input.tkwin = 0;
      TestPoint(0);
      break;
    case INPUT_TYPE_Image:
      TestPoint(0);
      break;
    case INPUT_TYPE_Radio:
#if 0
      font = HtmlGetFont(htmlPtr, pElem->base.style.font);
      pElem->input.cnt = ++htmlPtr->nInput;
      zVal = HtmlMarkupArg(pElem, "value", 0);
      if( zVal==0 ){
        sprintf(zValBuf,"x%d",pElem->input.cnt);
        zVal = zValBuf;
        TestPoint(0);
      }else{
        TestPoint(0);
      }
      force = HtmlMarkupArg(pElem, "checked", 0)!=0;
      MakeVarName(htmlPtr, pElem, zVar, zVal, force);
      zWin = MakeWindowName(htmlPtr, pElem, zWinBuf, sizeof(zWinBuf));
      ExecCmd(htmlPtr, Tk_RadiobuttonCmd,
         "radiobutton", zWin, "-variable", zVar, "-value", zVal,
         "-font", Tk_NameOfFont(font), 
         "-padx", "0", "-pady", "0", "-text" , "", 0);
      SizeAndLink(htmlPtr, zWin, pElem);
      pElem->input.padLeft = pElem->input.w/2;
      FreeWindowName(zWin,zWinBuf);
#endif
      break;
    case INPUT_TYPE_Reset:
#if 0
      font = HtmlGetFont(htmlPtr, pElem->base.style.font);
      pElem->input.cnt = ++htmlPtr->nInput;
      zVal = HtmlMarkupArg(pElem, "value", "Reset");
      zWin = MakeWindowName(htmlPtr, pElem, zWinBuf, sizeof(zWinBuf));
      nWin = strlen(zWin);
      if( nWin + 50 > sizeof(zCbBuf) ){
        zCallback = ckalloc( nWin + 50 );
        TestPoint(0);
      }else{
        zCallback = zCbBuf;
        TestPoint(0);
      }
      if( zCallback ){
        sprintf(zCallback, "%s _re %d", Tk_PathName(htmlPtr->tkwin), 
          pElem->input.cnt);
        TestPoint(0);
      }else{
        zCallback = zCbBuf;
        zCbBuf[0] = 0;
        TestPoint(0);
      }
      ExecCmd(htmlPtr, Tk_ButtonCmd,
         "button", zWin, "-font", Tk_NameOfFont(font), "-text", zVal,
         "-command", zCallback,
         0);
      if( zCallback!=zCbBuf ){
        ckfree(zCallback);
        TestPoint(0);
      }else{
        TestPoint(0);
      }
      SizeAndLink(htmlPtr, zWin, pElem);
      FreeWindowName(zWin,zWinBuf);
#endif
      break;
    case INPUT_TYPE_Select:
#if 0
      if( pElem->input.tkwin==0 ){
        char *zSize = HtmlMarkupArg(pElem, "size", "1");
        char *zMultiple = HtmlMarkupArg(pElem, "multiple", 0)==0 ? "0" : "1";
        Tcl_DString str;
        pElem->input.cnt = ++htmlPtr->nInput;
        zWin = MakeWindowName(htmlPtr, pElem, zWinBuf, sizeof(zWinBuf));
        Tcl_DStringInit(&str);
        Tcl_DStringAppend(&str,"_Html_Select_Build",-1);
        Tcl_DStringAppendElement(&str, zWin);
        Tcl_DStringAppendElement(&str, zSize);
        Tcl_DStringAppendElement(&str, zMultiple);
        AddSelectOptions(&str,pElem);
        Tcl_GlobalEval(htmlPtr->interp, Tcl_DStringValue(&str));
        Tcl_DStringFree(&str);
        SizeAndLink(htmlPtr, zWin, pElem);
        FreeWindowName(zWin,zWinBuf);
        TestPoint(0);
      }else{
        TestPoint(0);
      }
#endif
      break;
    case INPUT_TYPE_Submit:
#if 0
      font = HtmlGetFont(htmlPtr, pElem->base.style.font);
      pElem->input.cnt = ++htmlPtr->nInput;
      zVal = HtmlMarkupArg(pElem, "value", "Submit");
      zWin = MakeWindowName(htmlPtr, pElem, zWinBuf, sizeof(zWinBuf));
      nWin = strlen(zWin);
      if( nWin + 50 > sizeof(zCbBuf) ){
        zCallback = ckalloc( nWin + 50 );
        TestPoint(0);
      }else{
        zCallback = zCbBuf;
        TestPoint(0);
      }
      if( zCallback ){
        sprintf(zCallback, "%s _su %d", Tk_PathName(htmlPtr->tkwin), 
          pElem->input.cnt);
        TestPoint(0);
      }else{
        zCallback = zCbBuf;
        zCbBuf[0] = 0;
        TestPoint(0);
      }
      ExecCmd(htmlPtr, Tk_ButtonCmd,
         "button", zWin, "-font", Tk_NameOfFont(font), "-text", zVal,
         "-command", zCallback,
         0);
      if( zCallback!=zCbBuf ){
        ckfree(zCallback);
        TestPoint(0);
      }else{
        TestPoint(0);
      }
      SizeAndLink(htmlPtr, zWin, pElem);
      FreeWindowName(zWin,zWinBuf);
#endif
      break;
    case INPUT_TYPE_Text:
    case INPUT_TYPE_Password:
    case INPUT_TYPE_File:
#if 0
      font = HtmlGetFont(htmlPtr, pElem->base.style.font);
      pElem->input.cnt = ++htmlPtr->nInput;
      zVal = HtmlMarkupArg(pElem, "value", "");
      MakeVarName(htmlPtr, pElem, zVar, 0, 0);
      zWin = MakeWindowName(htmlPtr, pElem, zWinBuf, sizeof(zWinBuf));
      zWidth = HtmlMarkupArg(pElem, "size", "20");
      ExecCmd(htmlPtr, Tk_EntryCmd,
         "entry", zWin, "-textvariable", zVar, 
         "-font", Tk_NameOfFont(font), 
         "-bd", "2", "-relief", "sunken", "-width", zWidth, 
         "-show", pElem->input.type==INPUT_TYPE_Password ? "*" : "",
         0);
      SizeAndLink(htmlPtr, zWin, pElem);
      FreeWindowName(zWin,zWinBuf);
      TestPoint(0);
#endif
      break;
    case INPUT_TYPE_TextArea:
#if 0
      if( pElem->input.tkwin==0 ){
        char *zCols = HtmlMarkupArg(pElem, "cols", "40");
        char *zRows = HtmlMarkupArg(pElem, "rows", "2");
        char *zWrap = HtmlMarkupArg(pElem, "wrap", "off");
        Tcl_DString str;
        pElem->input.cnt = ++htmlPtr->nInput;
        zWin = MakeWindowName(htmlPtr, pElem, zWinBuf, sizeof(zWinBuf));
        ExecCmd(htmlPtr, Tk_FrameCmd,
           "frame", zWin, "-bd", "0",
           0);
        Tcl_DStringInit(&str);
        Tcl_DStringAppend(&str,"_Html_Textarea_Build",-1);
        Tcl_DStringAppendElement(&str, zWin);
        Tcl_DStringAppendElement(&str, zCols);
        Tcl_DStringAppendElement(&str, zRows);
        if( stricmp(zWrap,"off")==0 ){
          zWrap = "none";
          TestPoint(0);
        }else{
          zWrap = "word";
          TestPoint(0);
        }
        Tcl_DStringAppendElement(&str, zWrap);
        Tcl_GlobalEval(htmlPtr->interp, Tcl_DStringValue(&str));
        Tcl_DStringFree(&str);
        SizeAndLink(htmlPtr, zWin, pElem);
        FreeWindowName(zWin,zWinBuf);
      }else{
        TestPoint(0);
      }
#endif
      break;
    case INPUT_TYPE_Applet:
#if 0
      if( htmlPtr->zAppletCommand && *htmlPtr->zAppletCommand ){
        Tcl_DString cmd;
        int i;
        Tk_Window tkwin;
        pElem->input.cnt = ++htmlPtr->nInput;
        MakeVarName(htmlPtr, pElem, zVar, 0, 0);
        zWin = MakeWindowName(htmlPtr, pElem, zWinBuf, sizeof(zWinBuf));
        zWidth = HtmlMarkupArg(pElem, "width", "200");
        zHeight = HtmlMarkupArg(pElem, "height", "200");
        ExecCmd(htmlPtr, Tk_FrameCmd,
           "frame", zWin, "-bd", "0",
           "-width", zWidth, "-height", zHeight,
           0);
        SizeAndLink(htmlPtr, zWin, pElem);
        Tcl_DStringInit(&cmd);
        Tcl_DStringAppend(&cmd,htmlPtr->zAppletCommand,-1);
        Tcl_DStringAppend(&cmd," ",1);
        Tcl_DStringAppendElement(&cmd,zWin);
        for(i=0; i<pElem->base.count; i++){
          Tcl_DStringAppendElement(&cmd,pElem->markup.argv[i]);
        }
        Tcl_Preserve(htmlPtr);
        Tcl_GlobalEval(htmlPtr->interp, Tcl_DStringValue(&cmd));
        Tcl_DStringFree(&cmd);
        tkwin = htmlPtr->tkwin;
        Tcl_Release(htmlPtr);
        /**** TBD: Deal with the case where tkwin==NULL.  As currently
        ***** implemented, it just core dumps... ****/
        FreeWindowName(zWin,zWinBuf);
        TestPoint(0);
      }else{
        pElem->input.w = 0;
        pElem->input.h = 0;
      }
#endif
      break;
    default:
      TestPoint(0);
      break;
  }
  return incomplete;
}

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

/*
** Append a name/value pair to the given DString
*/
static void AppendNameAndValue(Tcl_DString *str, char *zName, char *zValue){
  int len;
  char *z;

  if( zValue==0 ){ TestPoint(0); zValue = ""; }
  len = Tcl_DStringLength(str);
  z = Tcl_DStringValue(str);
  if( len>0 && z[len-1]!=' ' ){
    Tcl_DStringAppend(str,"&",1);
    TestPoint(0);
  }else{
    TestPoint(0);
  }
  Tcl_DStringAppend(str, zName, -1);
  Tcl_DStringAppend(str, "=", 1);
  EncodeText(str, zValue);
}

/*
** Add the field information associated with an <INPUT> to the given
** DString.  This version works only with the x-www-url encoding.
*/
static void AddFieldInfo(Tcl_DString *str, HtmlWidget *htmlPtr, HtmlElement *p){
  char *zName;
  char *zValue;
  char *zVarVal;
  char *zWin;
  Tcl_DString dstr;
  char zVar[VAR_SIZE];
  char zWinBuf[1000];    /* Space to hold child widget name */

  zName = HtmlMarkupArg( p, "name", 0);
  if( zName==0 ){ TestPoint(0); return; }
  switch( p->input.type ){
    case INPUT_TYPE_Text:
    case INPUT_TYPE_Password:
    case INPUT_TYPE_File:
      MakeVarName(htmlPtr, p, zVar, 0, 0);
      zValue = Tcl_GetVar(htmlPtr->interp, zVar, TCL_GLOBAL_ONLY);
      AppendNameAndValue(str, zName, zValue);
      TestPoint(0);
      break;
    case INPUT_TYPE_Checkbox:
      MakeVarName(htmlPtr, p, zVar, 0, 0);
      zVarVal = Tcl_GetVar(htmlPtr->interp, zVar, TCL_GLOBAL_ONLY);
      if( zVarVal==0 || *zVarVal=='0' ){
        TestPoint(0);
        break;
      }
      zValue = HtmlMarkupArg(p, "value", 0);
      if( zValue==0 ){ TestPoint(0); break; }
      AppendNameAndValue(str, zName, zValue);
      TestPoint(0);
      break;
    case INPUT_TYPE_Radio:
      MakeVarName(htmlPtr, p, zVar, 0, 0);
      zVarVal = Tcl_GetVar(htmlPtr->interp, zVar, TCL_GLOBAL_ONLY);
      zValue = HtmlMarkupArg(p, "value", 0);
      if( zVarVal==0 || zValue==0 ){ TestPoint(0); break; }
      if( strcmp(zVarVal,zValue)!=0 ){ TestPoint(0); break; }
      AppendNameAndValue(str, zName, zValue);
      TestPoint(0);
      break;
    case INPUT_TYPE_Select:
      TestPoint(0);
      break;
    case INPUT_TYPE_TextArea:
      zWin = MakeWindowName(htmlPtr, p, zWinBuf, sizeof(zWinBuf));
      Tcl_DStringInit(&dstr);
      Tcl_DStringAppend(&dstr, "_Html_Textarea_Get ", -1);
      Tcl_DStringAppend(&dstr, zWin, -1);
      Tcl_GlobalEval(htmlPtr->interp, Tcl_DStringValue(&dstr));
      Tcl_DStringFree(&dstr);
      FreeWindowName(zWin,zWinBuf);
      AppendNameAndValue(str, zName, htmlPtr->interp->result);
      Tcl_ResetResult(htmlPtr->interp);
      TestPoint(0);
      break;
    case INPUT_TYPE_Hidden:
      zValue = HtmlMarkupArg(p, "value", 0);
      if( zValue ){
        AppendNameAndValue(str, zName, zValue);
        TestPoint(0);
      }else{
        TestPoint(0);
      }
      break;
    default:
      TestPoint(0);
      break;
  }
}

/*
** This routine is called whenever a "Submit" or "image" button is
** pressed.  It job is to construct an HTTP query string and then call
** the -formcommand with this string.
*/
int HtmlSubmit(HtmlWidget *htmlPtr, int id){
  HtmlElement *p;            /* For scanning a list of elements */
  HtmlElement *pForm;        /* The <FORM> that contains the button pressed */
  char *zURL;                /* The action= field of the <FORM> */
  char *zMethod;             /* The method= field of the <FORM> */
  char *zName;               /* The name= field of the button */
  char *zValue;              /* The value= field of the button */
  int result;                /* Result of executing the callback */
  Tcl_DString str;           /* For building the callback command */

  /* No point in going on if there is no form callback... */
  if( htmlPtr->zFormCommand==0 || htmlPtr->zFormCommand[0]==0 ){
    TestPoint(0);
    return TCL_OK;
  }

  /* first find the button that was pressed */
  for(p=htmlPtr->pFirst; p; p=p->pNext){
    if( p->base.type==Html_INPUT ){
      if( p->input.cnt==id ){
        TestPoint(0);
        break;
      }else{
        TestPoint(0);
      }
    }else{
      TestPoint(0);
    }
  }
  if( p==0 ){ TestPoint(0); return TCL_OK; }
  pForm = p->input.pForm;
  if( pForm==0 ){ TestPoint(0); return TCL_OK; }

  /* Initialize the command */
  zURL = HtmlMarkupArg(pForm, "action", 0);
  if( zURL==0 ){ TestPoint(0); return TCL_OK; }
  zURL = HtmlCompleteUrl(htmlPtr, zURL);
  Tcl_DStringInit(&str);
  Tcl_DStringAppend(&str, htmlPtr->zFormCommand, -1);
  Tcl_DStringAppendElement(&str, zURL);
  ckfree(zURL);
  zMethod = HtmlMarkupArg(pForm, "method", "GET");
  Tcl_DStringAppendElement(&str, zMethod);
  Tcl_DStringAppend(&str, " ", 1);

  /* Handle the button that was pressed */
  switch( p->input.type ){
    case INPUT_TYPE_Submit:
      zName = HtmlMarkupArg(p, "name", 0);
      if( zName==0 ){ TestPoint(0); break; }
      zValue = HtmlMarkupArg(p, "value", "Submit");
      AppendNameAndValue(&str, zName, zValue);
      TestPoint(0);
      break;
    default:
      TestPoint(0);
      break;
  }

  /* Handle all other input fields */
  for(p=pForm; p && p->base.type!=Html_EndFORM; p=p->pNext){
    switch( p->base.type ){
      case Html_INPUT:
      case Html_SELECT:
      case Html_TEXTAREA:
        AddFieldInfo(&str, htmlPtr, p);
        TestPoint(0);
        break;
      default:
        TestPoint(0);
        break;
    }
  }

  /* Execute the command we've built. */
  result = Tcl_GlobalEval(htmlPtr->interp, Tcl_DStringValue(&str));
  Tcl_DStringFree(&str);
  return result;
}

/*
** Refill the initial contents of a <textarea> with information from
** the original HTML token stream.
*/
void HtmlResetTextarea(HtmlWidget *htmlPtr, HtmlElement *p){
  Tcl_DString str;
  int textSeen = 0;
  char *zWin;
  char zWinBuf[1000];

  HtmlAssert( p->base.type==Html_TEXTAREA );
  Tcl_DStringInit(&str);
  Tcl_DStringAppend(&str,"_Html_Textarea_Put ",-1);
  zWin = MakeWindowName(htmlPtr, p, zWinBuf, sizeof(zWinBuf));
  Tcl_DStringAppend(&str, zWin, -1);
  FreeWindowName(zWin,zWinBuf);
  Tcl_DStringStartSublist(&str);
  while( p && p->base.type!=Html_EndTEXTAREA ){
    switch( p->base.type ){
      case Html_Text:
        Tcl_DStringAppend(&str,p->text.zText, p->base.count);
        textSeen = 1;
        TestPoint(0);
        break;
      case Html_Space:
        if( !textSeen ){ TestPoint(0); break; }
        if( p->base.flags & HTML_NewLine ){
          Tcl_DStringAppend(&str,"\n",1);
          TestPoint(0);
        }else{
          int cnt = p->base.count;
          while( cnt ){
            int n = cnt;
            if( n>10 ) n = 10;
            Tcl_DStringAppend(&str,"           ",n);
            cnt -= n;
            TestPoint(0);
          }
        }
        break;
      default:
        TestPoint(0);
        break;
    }
    p = p->pNext;
  }
  Tcl_DStringEndSublist(&str);
  Tcl_GlobalEval(htmlPtr->interp, Tcl_DStringValue(&str));
  Tcl_DStringFree(&str);
}

/*
** This routine is called whenever a "Reset" button is pressed.  The
** value of all <INPUT>s within the same form are restored to their
** starting states.
*/
void HtmlReset(HtmlWidget *htmlPtr, int id){
  HtmlElement *p;            /* For scanning a list of elements */
  HtmlElement *pForm;        /* The <FORM> that contains the button pressed */
  char *zValue;              /* The value of a widget */
  int force;                 /* True to force a value on a widget */
  char zVar[VAR_SIZE];

  /* first find the form containing the button that was pressed */
  for(p=htmlPtr->pFirst; p; p=p->pNext){
    if( p->base.type==Html_INPUT ){
      if( p->input.cnt==id ){
        break;
        TestPoint(0);
      }else{
        TestPoint(0);
      }
    }else{
      TestPoint(0);
    }
  }
  if( p==0 ){ TestPoint(0); return; }
  pForm = p->input.pForm;
  if( pForm==0 ){ TestPoint(0); return; }
  sprintf(zVar,"%s_%d_%d", VAR_NAME_BASE, htmlPtr->varId, pForm->form.id);
  Tcl_UnsetVar2(htmlPtr->interp, zVar, 0, TCL_GLOBAL_ONLY);

  /* Now reset all fields in this form */
  for(p=pForm; p && p->base.type!=Html_EndFORM; p=p->pNext){
    switch( p->base.type ){
      case Html_INPUT:
      case Html_SELECT:
      case Html_TEXTAREA:
        switch( p->input.type ){
          case INPUT_TYPE_Text:
          case INPUT_TYPE_Password:
          case INPUT_TYPE_File:
            zValue = HtmlMarkupArg(p, "value", "");
            MakeVarName(htmlPtr, p, zVar, zValue, 1);
            TestPoint(0);
            break;
          case INPUT_TYPE_Checkbox:
            zValue = HtmlMarkupArg(p, "checked", 0)==0 ? "0" : "1";
            MakeVarName(htmlPtr, p, zVar, zValue, 1);
            TestPoint(0);
            break;
          case INPUT_TYPE_Radio:
            force = HtmlMarkupArg(p, "checked", 0)!=0;
            zValue = HtmlMarkupArg(p, "value", "");
            MakeVarName(htmlPtr, p, zVar, zValue, force);
            TestPoint(0);
            break;
          case INPUT_TYPE_Select:
            TestPoint(0);
            break;
          case INPUT_TYPE_TextArea:
            HtmlResetTextarea(htmlPtr, p);
            TestPoint(0);
            break;
          default:
            TestPoint(0);
            break;
        }
        break;
      default:
        TestPoint(0);
        break;
    }
  }
}
