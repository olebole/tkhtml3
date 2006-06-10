

#ifndef __RESTRACK_H__
#define __RESTRACK_H__

#include <tcl.h>

char * Rt_Alloc(const char * ,int);
char * Rt_Realloc(const char *, char *, int);
void Rt_Free(const char *, char *);

Tcl_ObjCmdProc Rt_AllocCommand;
Tcl_ObjCmdProc HtmlHeapDebug;

#endif

