/*
** Make a "wish" that includes the html widget.
**
** $Revision: 1.2 $
*/
#include "appinit.h"

int Et_AppInit(Tcl_Interp *interp){
  extern int Tkhtml_Init(Tcl_Interp*);
  Tkhtml_Init(interp);
  return TCL_OK;
}
