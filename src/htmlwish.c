static char const rcsid[] =
        "@(#) $Id: htmlwish.c,v 1.9 2005/03/23 01:36:54 danielk1977 Exp $";

/*
** Make a "wish" that includes the html widget.
**
** This source code is released into the public domain by the author,
** D. Richard Hipp, on 2002 December 17.  Instead of a license, here
** is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
*/
#include "appinit.h"

#ifndef _TCLHTML_
int
Et_AppInit(interp)
    Tcl_Interp *interp {
        extern int Tkhtml_Init(Tcl_Interp *);
         Tkhtml_Init(interp);
         return TCL_OK;
    }
#endif
