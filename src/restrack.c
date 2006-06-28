/*
 * restrack.c --
 *
 *     This file contains wrappers for functions that dynamically allocate
 *     and deallocate resources (for example ckalloc() and ckfree()). The
 *     purpose of this is to provide a built-in system for debugging
 *     resource leaks and buffer-overruns.
 *
 *     Currently, the following resources are allocated using wrapper 
 *     functions in this file:
 *
 *         * Heap memory           - Rt_Alloc(), Rt_Realloc() and Rt_Free()
 *         * Tcl object references - Rt_IncrRefCount() and Rt_DecrRefCount()
 *
 * No tkhtml code outside of this file should call ckalloc() directly.
 *
 *-------------------------------------------------------------------------
 */
static const char rcsid[] = "$Id: restrack.c,v 1.7 2006/06/28 06:31:11 danielk1977 Exp $";

#ifdef HTML_RES_DEBUG
#define RES_DEBUG
#endif

#include "tcl.h"
#include "tk.h"

#include <stdio.h>

#ifdef RES_DEBUG 
  #include <execinfo.h> 
#endif

#include <string.h>
#include <assert.h>

#ifndef NDEBUG 

#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))

#define RES_ALLOC  0
#define RES_OBJREF 1
#define RES_GC     2
#define RES_PIXMAP 3
#define RES_XCOLOR 4

static const char *aResNames[] = {
    "memory allocation",                 /* RES_ALLOC */
    "tcl object reference",              /* RES_OBJREF */
    "GC",                                /* RES_GC */
    "pixmap",                            /* RES_PIXMAP */
    "xcolor",                            /* RES_XCOLOR */
    0
};
static int aResCounts[] = {0, 0, 0, 0, 0};

/*
 * If RES_DEBUG is defined and glibc is in use, then a little extra
 * accounting is enabled.
 *
 * The interface to the accounting system is:
 *
 *     ResAlloc()      - Note the allocation of a resource.
 *     ResFree()       - Note the deallocation of a resource.
 *     ResDump()       - Print a catalogue of all currently allocated
 *                       resources to stdout.
 *
 * Each resource is identified by two ClientData variables, one to identify
 * the type of resource and another to identify the unique resource
 * instance. Collectively, the two ClientData values make up a
 * "resource-id". The global hash table, aOutstanding, contains a mapping
 * between resource-id and a ResRecord structure instance for every
 * currently allocated resource:
 *
 *      (<res-type> <res-ptr>)    ->    ResRecord
 *
 * There can be more than one reference to a single resource, so reference
 * counted resources (Tcl_Obj* for example) can be used with this system.
 */

/*
 * Each ResRecord structure stores info for a currently allocated resource.
 * The information stored is the number of references to the resource, and
 * the backtraces of each of the call-chains that reserved a reference.
 * A "backtrace" is obtained from the non-standard backtrace() function
 * implemented in glibc. 
 *
 * There may be more backtraces than outstanding references. This is
 * because when a reference is returned via ResFree(), it is not possible
 * to tell which of the backtraces to remove. For example, if the sequence
 * of calls is:
 *
 *     Rt_IncrRefCount();
 *     Rt_IncrRefCount();
 *     Rt_DecrRefCount();
 *
 * the ResRecord structure has nRef==1, nStack==2 and aStack pointing to an
 * array of size 2.
 */
typedef struct ResRecord ResRecord;
struct ResRecord {
    int nRef;            /* Current number of outstanding references */
    int nStack;          /* Number of stored stack-dumps */
    int **aStack;        /* Array of stored stack-dumps */
};

/* 
 * Global hash table of currently outstanding resource references.
 */
#if defined(RES_DEBUG) && defined(__GLIBC__)
static Tcl_HashTable aOutstanding;
#endif

/*
 *---------------------------------------------------------------------------
 *
 * ResAlloc --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
ResAlloc(v1, v2)
    ClientData v1;
    ClientData v2;
{
#if defined(RES_DEBUG) && defined(__GLIBC__)
    int key[2];
    int newentry;
    Tcl_HashEntry *pEntry;
    int *aFrame;
    ResRecord *pRec;

    static int init = 0;
    if (!init) {
        Tcl_InitHashTable(&aOutstanding, 2);
        init = 1;
    }

    key[0] = (int)v1;
    key[1] = (int)v2;

    pEntry = Tcl_CreateHashEntry(&aOutstanding, (const char *)key, &newentry);
    if (newentry) {
        pRec = (ResRecord *)ckalloc(sizeof(ResRecord));
        memset(pRec, 0, sizeof(ResRecord));
        Tcl_SetHashValue(pEntry, pRec);
    } else {
        pRec = Tcl_GetHashValue(pEntry);
    }

    aFrame = (int *)ckalloc(sizeof(int) * 30);
    backtrace((void *)aFrame, 29);
    aFrame[29] = 0;
    pRec->nRef++;
    pRec->nStack++;
    pRec->aStack = (int **)ckrealloc(
            (char *)pRec->aStack, 
            sizeof(int *) * pRec->nStack
    );
    pRec->aStack[pRec->nStack - 1] = aFrame;
#endif

    aResCounts[(int)v1]++;
}

/*
 *---------------------------------------------------------------------------
 *
 * ResFree --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
ResFree(v1, v2)
    ClientData v1;
    ClientData v2;
{
#if defined(RES_DEBUG) && defined(__GLIBC__)
    int key[2];
    Tcl_HashEntry *pEntry;
    ResRecord *pRec;

    key[0] = (int)v1;
    key[1] = (int)v2;

    pEntry = Tcl_FindHashEntry(&aOutstanding, (const char *)key);
    assert(pEntry);

    pRec = (ResRecord *)Tcl_GetHashValue(pEntry);
    pRec->nRef--;

    if (pRec->nRef == 0) {
        int i;
        ResRecord *pRec = (ResRecord *)Tcl_GetHashValue(pEntry);

        for (i = 0; i < pRec->nStack; i++) {
            ckfree((char *)pRec->aStack[i]);
        }
        ckfree((char *)pRec->aStack);
        ckfree((char *)pRec);

        Tcl_DeleteHashEntry(pEntry);
    }
#endif

    aResCounts[(int)v1]--;
}

/*
 *---------------------------------------------------------------------------
 *
 * ResDump --
 *
 *     Print the current contents of the global hash table aOutstanding to
 *     stdout. 
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
ResDump()
{
#if defined(RES_DEBUG) && defined(__GLIBC__)
    Tcl_HashEntry *pEntry;
    Tcl_HashSearch search;
    for (
        pEntry = Tcl_FirstHashEntry(&aOutstanding, &search);
        pEntry;
        pEntry = Tcl_NextHashEntry(&search)
    ) {
        int *aKey = (int *)Tcl_GetHashKey(&aOutstanding, pEntry);
        ResRecord *pRec = (ResRecord *)Tcl_GetHashValue(pEntry);
        int i;
        printf("RESOURCE %x %x ", aKey[0], aKey[1]);
        for (i = 0; i < pRec->nStack; i++) {
            int j;
            printf("{");
            for (j = 0; pRec->aStack[i][j]; j++) {
                printf("%x%s", pRec->aStack[i][j], pRec->aStack[i][j+1]?" ":"");
            }
            printf("} ");
        }
        printf("\n");
    }
#endif
}

/*
 *---------------------------------------------------------------------------
 * End of ResTrack code.
 *---------------------------------------------------------------------------
 */

/*
 * This hash table is used to maintain a summary of the currently 
 * outstanding calls to HtmlAlloc(). To be used to measure approximate 
 * heap usage.
 */
static Tcl_HashTable aMalloc;

static void 
initMallocHash() {
    static int init = 0;
    if (!init) {
        Tcl_InitHashTable(&aMalloc, TCL_STRING_KEYS);
        init = 1;
    }
}

static void
insertMallocHash(zTopic, nBytes) 
    const char *zTopic;
    int nBytes;
{
    int *aData;
    int isNewEntry;
    Tcl_HashEntry *pEntry;

    initMallocHash();

    pEntry = Tcl_CreateHashEntry(&aMalloc, zTopic, &isNewEntry);
    if (isNewEntry) {
        aData = (int *)ckalloc(sizeof(int) * 2);
        aData[0] = 1; 
        aData[1] = nBytes;
        Tcl_SetHashValue(pEntry, aData);
    } else {
        aData = Tcl_GetHashValue(pEntry);
        aData[0] += 1;
        aData[1] += nBytes;
    }
}

static void
freeMallocHash(zTopic, nBytes) 
    const char *zTopic;
    int nBytes;
{
    int *aData;
    Tcl_HashEntry *pEntry;

    initMallocHash();

    pEntry = Tcl_FindHashEntry(&aMalloc, zTopic);
    assert(pEntry);
    aData = Tcl_GetHashValue(pEntry);
    aData[0] -= 1;
    aData[1] -= nBytes;

    assert(aData[0] >= 0);
    assert(aData[1] >= 0);

    if (aData[0] == 0) {
        assert(aData[1] == 0);
        Tcl_DeleteHashEntry(pEntry);
    }
}

int 
HtmlHeapDebug(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp; 
    int objc;
    Tcl_Obj * const objv[];
{
    Tcl_Obj *pRet = Tcl_NewObj();
    Tcl_HashEntry *pEntry;
    Tcl_HashSearch search;
    for (
        pEntry = Tcl_FirstHashEntry(&aMalloc, &search);
        pEntry;
        pEntry = Tcl_NextHashEntry(&search)
    ) {
        const char *zTopic = (const char *)Tcl_GetHashKey(&aMalloc, pEntry);
        int *aData = (int *)Tcl_GetHashValue(pEntry);

        Tcl_Obj *pObj = Tcl_NewObj();
        Tcl_ListObjAppendElement(interp, pObj, Tcl_NewStringObj(zTopic, -1));
        Tcl_ListObjAppendElement(interp, pObj, Tcl_NewIntObj(aData[0]));
        Tcl_ListObjAppendElement(interp, pObj, Tcl_NewIntObj(aData[1]));
        Tcl_ListObjAppendElement(interp, pRet, pObj);
    }

    Tcl_SetObjResult(interp, pRet);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Rt_AllocCommand --
 *
 *         canvas3d_alloc
 *
 *     This Tcl command is only available if NDEBUG is not defined. It
 *     returns a list of two integers, the number of unmatched Rt_Alloc()
 *     and Rt_IncrRefCount calls respectively.
 *
 *     This function also invokes ResDump(). So if RES_DEBUG is defined at
 *     compile time and glibc is in use some data may be dumped to stdout.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
Rt_AllocCommand(clientData, interp, objc, objv)
  ClientData clientData;
  Tcl_Interp *interp; 
  int objc;
  Tcl_Obj * const objv[];
{
    int i;
    Tcl_Obj *pRet;

    pRet = Tcl_NewObj();
    for (i = 0; aResNames[i]; i++) {
        Tcl_Obj *pName = Tcl_NewStringObj(aResNames[i],-1);
        Tcl_ListObjAppendElement(interp, pRet, pName);
        Tcl_ListObjAppendElement(interp, pRet, Tcl_NewIntObj(aResCounts[i]));
    }
    Tcl_SetObjResult(interp, pRet);

    ResDump();
    return TCL_OK;
}
/*
 *---------------------------------------------------------------------------
 *
 * Rt_Alloc --
 *
 *     A wrapper around ckalloc() for use by code outside of this file.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
char *
Rt_Alloc(zTopic, n)
    const char *zTopic;
    int n;
{
    int nAlloc = n + 4 * sizeof(int);
    int *z = (int *)ckalloc(nAlloc);
    char *zRet = (char *)&z[2];
    z[0] = 0xFED00FED;
    z[1] = n;
    z[3 + n / sizeof(int)] = 0xBAD00BAD;

    ResAlloc(RES_ALLOC, z);
    insertMallocHash(zTopic ? zTopic : "malloc", n);

    memset(zRet, 0x55, n);
    return zRet;
}

/*
 *---------------------------------------------------------------------------
 *
 * Rt_Free --
 *
 *     A wrapper around ckfree() for use by code outside of this file.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
Rt_Free(zTopic, p)
    const char *zTopic;
    char *p;
{
    if (p) {
        int *z = (int *)p;
        int n = z[-1];
        assert(z[-2] == 0xFED00FED);
        assert(z[1 + n / sizeof(int)] == 0xBAD00BAD);
        memset(z, 0x55, n);
        ckfree((char *)&z[-2]);
        ResFree(RES_ALLOC, &z[-2]);
        freeMallocHash(zTopic ? zTopic : "malloc", n);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * Rt_Realloc --
 *
 *     A wrapper around ckrealloc() for use by code outside of this file.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
char * 
Rt_Realloc(zTopic, p, n)
    const char *zTopic;
    char *p;
    int n;
{
    char *pRet = Rt_Alloc(zTopic, n);
    if (p) {
        int current_sz = ((int *)p)[-1];
        memcpy(pRet, p, MIN(current_sz, n));
        Rt_Free(zTopic, (char *)p);
    }
    return pRet;
}

#if 0

/*
 *---------------------------------------------------------------------------
 *
 * Rt_IncrRefCount --
 *
 *     A wrapper around Tcl_IncrRefCount() for use by code outside of this
 *     file.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void Rt_IncrRefCount(pObj)
    Tcl_Obj *pObj;
{
    assert(pObj);
    Tcl_IncrRefCount(pObj);
    ResAlloc(RES_OBJREF, pObj);
}

/*
 *---------------------------------------------------------------------------
 *
 * Rt_DecrRefCount --
 *
 *     A wrapper around Tcl_DecrRefCount() for use by code outside of this
 *     file.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
Rt_DecrRefCount(pObj)
    Tcl_Obj *pObj;
{
    assert(pObj);
    Tcl_DecrRefCount(pObj);
    ResFree(RES_OBJREF, pObj);
}

/*
 *---------------------------------------------------------------------------
 *
 * Rt_GetGC --
 *
 *     A wrapper around Tk_GetGC() for use by code outside of this file.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
GC 
Rt_GetGC(tkwin, valueMask, gc_values)
    Tk_Window tkwin;
    unsigned long valueMask;
    XGCValues *gc_values;
{
    GC gc = Tk_GetGC(tkwin, valueMask, gc_values);
    ResAlloc(RES_GC, gc);
    return gc;
}

/*
 *---------------------------------------------------------------------------
 *
 * Rt_GetGC --
 *
 *     A wrapper around Tk_FreeGC() for use by code outside of this file.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
Rt_FreeGC(dpy, gc)
    Display *dpy;
    GC gc;
{
    ResFree(RES_GC, gc);
    Tk_FreeGC(dpy, gc);
}

/*
 *---------------------------------------------------------------------------
 *
 * Rt_GetPixmap --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
Pixmap 
Rt_GetPixmap(dpy, drawable, w, h, depth)
    Display *dpy;
    Drawable drawable;
    int w; 
    int h;
    int depth;
{
    Pixmap pixmap = Tk_GetPixmap(dpy, drawable, w, h, depth);
    ResAlloc(RES_PIXMAP, pixmap);
    return pixmap;
}

/*
 *---------------------------------------------------------------------------
 *
 * Rt_FreePixmap --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
Rt_FreePixmap(dpy, pixmap)
    Display *dpy;
    Pixmap pixmap;
{
    ResFree(RES_PIXMAP, pixmap);
    Tk_FreePixmap(dpy, pixmap);
}

/*
 *---------------------------------------------------------------------------
 *
 * Rt_FreeColor --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
Rt_FreeColor(color)
    XColor *color;
{
    ResFree(RES_XCOLOR, color);
    Tk_FreeColor(color);
}

/*
 *---------------------------------------------------------------------------
 *
 * Rt_AllocColorFromObj --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
XColor *
Rt_AllocColorFromObj(interp, win, pObj)
    Tcl_Interp *interp;
    Tk_Window win; 
    Tcl_Obj *pObj;
{
    XColor *color = Tk_AllocColorFromObj(interp, win, pObj);
    ResAlloc(RES_XCOLOR, color);
    return color;
}

/*
 *---------------------------------------------------------------------------
 *
 * Rt_GetColorByValue --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
XColor *
Rt_GetColorByValue(win, color)
    Tk_Window win;
    XColor *color;
{
    XColor *color2 = Tk_GetColorByValue(win, color);
    ResAlloc(RES_XCOLOR, color2);
    return color2;
}

#endif

#endif

