/*
** Make a "wish" that includes the html widget.
**
** $Id: htmlwish.c,v 1.3 1999/12/21 12:45:38 drh Exp $
*/
#include "appinit.h"

int Et_AppInit(Tcl_Interp *interp){
  extern int Tkhtml_Init(Tcl_Interp*);
  Tkhtml_Init(interp);
  return TCL_OK;
}
