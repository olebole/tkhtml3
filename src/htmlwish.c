static char const rcsid[] = "@(#) $Id: htmlwish.c,v 1.4 2000/01/17 13:55:11 drh Exp $";
/*
** Make a "wish" that includes the html widget.
**
*/
#include "appinit.h"

int Et_AppInit(Tcl_Interp *interp){
  extern int Tkhtml_Init(Tcl_Interp*);
  Tkhtml_Init(interp);
  return TCL_OK;
}
