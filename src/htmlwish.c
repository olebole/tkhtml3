static char const rcsid[] = "@(#) $Id: htmlwish.c,v 1.7 2002/09/22 16:55:46 peter Exp $";
/*
** Make a "wish" that includes the html widget.
**
*/
#include "appinit.h"

#ifndef _TCLHTML_
int Et_AppInit(Tcl_Interp *interp){
  extern int Tkhtml_Init(Tcl_Interp*);
  Tkhtml_Init(interp);
  return TCL_OK;
}
#endif
