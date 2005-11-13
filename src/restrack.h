

#ifndef __RESTRACK_H__
#define __RESTRACK_H__

#include <tcl.h>

char * Rt_Alloc(int);
char * Rt_Realloc(char *, int);
void Rt_Free(char *);

Tcl_ObjCmdProc Rt_AllocCommand;

#endif

