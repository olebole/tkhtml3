static char const rcsid[] = "@(#) $Id: htmlwish.c,v 1.6 2002/03/06 18:10:59 peter Exp $";
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
