
#include <tcl.h>
#include <string.h>

#include <sys/time.h>

typedef struct InstCommand InstCommand;
typedef struct InstGlobal InstGlobal;
typedef struct InstFrame InstFrame;
typedef struct InstVector InstVector;
typedef struct InstData InstData;

struct InstCommand {
    Tcl_CmdInfo info;
    int isDeleted;

    Tcl_Obj *pFullName;

    int nCall;              /* Number of calls to this object command */
    Tcl_WideInt iTotal;     /* Total number of micro seconds */
    Tcl_WideInt iChildren;  /* Child number of micro seconds */

    InstGlobal *pGlobal;
    InstCommand *pNext;     /* Next in linked list starting at pGlobal */
};

struct InstFrame {
    InstCommand *pCommand;
};

struct InstVector {
    InstCommand *p1;
    InstCommand *p2;
};

struct InstData {
    int nCall;
    Tcl_WideInt iClicks;
};

struct InstGlobal {

    /* List of all InstCommand commands */
    InstCommand *pGlobal;

    /* Current caller frame. */
    InstFrame *pCaller;

    Tcl_HashTable aVector;
};

#define timevalToClicks(tv) ( \
    (Tcl_WideInt)tv.tv_usec + ((Tcl_WideInt)1000000 * (Tcl_WideInt)tv.tv_sec) \
)

static int 
execInst(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    InstCommand *p = (InstCommand *)clientData;
    InstGlobal *pGlobal = p->pGlobal;
    InstFrame frame;            /* This frame */
    InstFrame *pCaller;         /* Calling frame (if any) */

    int rc;

    InstVector vector;
    InstData *pData;
    Tcl_HashEntry *pEntry;
    int isNew = 0;

    Tcl_WideInt iClicks;
    struct timeval tv;

    pCaller = pGlobal->pCaller;
    pGlobal->pCaller = &frame;
    frame.pCommand = p;

    gettimeofday(&tv, 0);
    iClicks = timevalToClicks(tv);
    rc = p->info.objProc(p->info.objClientData, interp, objc, objv);
    gettimeofday(&tv, 0);
    iClicks = timevalToClicks(tv) - iClicks;

    /* Update the call vector */
    vector.p1 = (pCaller ? pCaller->pCommand : 0);
    vector.p2 = p;
    pEntry = Tcl_CreateHashEntry(&pGlobal->aVector, (char *)&vector, &isNew);
    if (isNew) {
        pData = (InstData *)ckalloc(sizeof(InstData));
        memset(pData, 0, sizeof(InstData));
        Tcl_SetHashValue(pEntry, pData);
    } else {
        pData = Tcl_GetHashValue(pEntry);
    }
    pData->nCall++;
    pData->iClicks += iClicks;

    /* Update the callers iChildren variable */
    if (pCaller) {
        pCaller->pCommand->iChildren += iClicks;
    }
    pGlobal->pCaller = pCaller;

    /* Update the total clicks and number of calls to this proc */
    p->iTotal += iClicks;
    p->nCall++;

    return rc;
}

static void
freeInstStruct(p)
    InstCommand *p;
{
    Tcl_DecrRefCount(p->pFullName);
    ckfree((void *)p);
}
static void 
freeInstCommand(clientData)
    ClientData clientData;
{
    InstCommand *p = (InstCommand *)clientData;
    if (p->info.deleteProc) {
        p->info.deleteProc(p->info.deleteData);
    }
    p->isDeleted = 1;
}

static int 
instCommand(clientData, interp, objc, objv)
    ClientData clientData;             /* Pointer to InstGlobal structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    InstGlobal *pGlobal = (InstGlobal *)clientData;
    Tcl_Command token;
    Tcl_CmdInfo new_info;
    InstCommand *pInst;

    token = Tcl_GetCommandFromObj(interp, objv[2]);
    if (!token) {
        Tcl_AppendResult(interp, "no such command: ", Tcl_GetString(objv[2]),0);
        return TCL_ERROR;
    }

    pInst = (InstCommand *)ckalloc(sizeof(InstCommand));
    memset(pInst, 0, sizeof(InstCommand));
    Tcl_GetCommandInfoFromToken(token, &pInst->info);
    pInst->pFullName = Tcl_NewObj();
    pInst->pGlobal = pGlobal;
    Tcl_IncrRefCount(pInst->pFullName);
    Tcl_GetCommandFullName(interp, token, pInst->pFullName);

    pInst->pNext = pGlobal->pGlobal;
    pGlobal->pGlobal = pInst;

    Tcl_GetCommandInfoFromToken(token, &new_info);
    new_info.objClientData = pInst;
    new_info.objProc = execInst;
    new_info.deleteProc = freeInstCommand;
    new_info.deleteData = pInst;
    Tcl_SetCommandInfoFromToken(token, &new_info);

    return TCL_OK;
}

static int 
instReport(clientData, interp, objc, objv)
    ClientData clientData;             /* Unused */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    InstGlobal *pGlobal = (InstGlobal *)clientData;
    InstCommand *p;
    Tcl_Obj *pRet;

    pRet = Tcl_NewObj();
    Tcl_IncrRefCount(pRet);

    for (p = pGlobal->pGlobal; p; p = p->pNext) {
        Tcl_WideInt iSelf;
        Tcl_Obj *pSub = Tcl_NewObj();
        Tcl_IncrRefCount(pSub);

        iSelf = p->iTotal - p->iChildren;

        Tcl_ListObjAppendElement(interp, pSub, p->pFullName);
        Tcl_ListObjAppendElement(interp, pSub, Tcl_NewIntObj(p->nCall));
        Tcl_ListObjAppendElement(interp, pSub, Tcl_NewIntObj(iSelf));
        Tcl_ListObjAppendElement(interp, pSub, Tcl_NewIntObj(p->iChildren));

        Tcl_ListObjAppendElement(interp, pRet, pSub);
        Tcl_DecrRefCount(pSub);
    }

    Tcl_SetObjResult(interp, pRet);
    Tcl_DecrRefCount(pRet);
    return TCL_OK;
}

static int 
instVectors(clientData, interp, objc, objv)
    ClientData clientData;             /* Unused */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    InstGlobal *pGlobal = (InstGlobal *)clientData;
    Tcl_Obj *pRet;

    Tcl_HashSearch sSearch;
    Tcl_HashEntry *pEntry;

    pRet = Tcl_NewObj();
    Tcl_IncrRefCount(pRet);

    for (
        pEntry = Tcl_FirstHashEntry(&pGlobal->aVector, &sSearch); 
        pEntry;
        pEntry = Tcl_NextHashEntry(&sSearch)
    ) {
        InstData *pData;
        InstVector *pVector;

        pData = (InstData *)Tcl_GetHashValue(pEntry);
        pVector = (InstVector *)Tcl_GetHashKey(&pGlobal->aVector, pEntry);

        if (!pVector->p1) {
            Tcl_Obj *pTop = Tcl_NewStringObj("<toplevel>", -1);
            Tcl_ListObjAppendElement(interp, pRet, pTop);
        } else {
            Tcl_ListObjAppendElement(interp, pRet, pVector->p1->pFullName);
        }
        Tcl_ListObjAppendElement(interp,pRet,pVector->p2->pFullName);
        Tcl_ListObjAppendElement(interp,pRet,Tcl_NewIntObj(pData->nCall));
        Tcl_ListObjAppendElement(interp,pRet,Tcl_NewWideIntObj(pData->iClicks));
    }

    Tcl_SetObjResult(interp, pRet);
    Tcl_DecrRefCount(pRet);

    return TCL_OK;
}

static int 
instZero(clientData, interp, objc, objv)
    ClientData clientData;             /* InstGlobal structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    InstGlobal *pGlobal = (InstGlobal *)clientData;
    InstCommand *p;
    InstCommand **pp;
    Tcl_Obj *pRet;

    Tcl_HashSearch sSearch;
    Tcl_HashEntry *pEntry;

    pRet = Tcl_NewObj();
    Tcl_IncrRefCount(pRet);

    pp = &pGlobal->pGlobal;
    for (p = *pp; p; p = *pp) {
        p->nCall = 0;
        p->iTotal = 0;
        p->iChildren = 0;
        if (p->isDeleted) {
            *pp = p->pNext;
            freeInstStruct(p);
        }else{
            pp = &p->pNext;
        }
    }

    for (
        pEntry = Tcl_FirstHashEntry(&pGlobal->aVector, &sSearch); 
        pEntry;
        pEntry = Tcl_NextHashEntry(&sSearch)
    ) {
        InstData *pData = Tcl_GetHashValue(pEntry);
        ckfree((void *)pData);
    }
    Tcl_DeleteHashTable(&pGlobal->aVector);
    Tcl_InitHashTable(&pGlobal->aVector, sizeof(InstVector)/sizeof(int));

    Tcl_SetObjResult(interp, pRet);
    Tcl_DecrRefCount(pRet);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * instrument_objcmd --
 *
 *     Implementation of [instrument_objcmd]. Syntax is:
 *
 *         instrument command COMMAND
 *         instrument report
 *
 * Results:
 *     TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *     See above.
 *
 *---------------------------------------------------------------------------
 */
static int 
instrument_objcmd(clientData, interp, objc, objv)
    ClientData clientData;             /* Unused */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    int iChoice;
    struct SubCmd {
        const char *zName;
        Tcl_ObjCmdProc *xFunc;
    } aSub[] = {
        { "command", instCommand }, 
        { "report",  instReport }, 
        { "vectors", instVectors }, 
        { "zero",    instZero }, 
        { 0, 0 }
    };

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "SUB-COMMAND");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObjStruct(interp, 
            objv[1], aSub, sizeof(aSub[0]), "sub-command", 0, &iChoice)
    ){
        return TCL_ERROR;
    }

    return aSub[iChoice].xFunc(clientData, interp, objc, objv);
}

static void 
instDelCommand(clientData)
    ClientData clientData;
{
    InstGlobal *p = (InstGlobal *)clientData;
    /* TODO */
}

void
HtmlInstrumentInit(interp)
    Tcl_Interp *interp;
{
    InstGlobal *p = (InstGlobal *)ckalloc(sizeof(InstGlobal));
    memset(p, 0, sizeof(InstGlobal));
    Tcl_InitHashTable(&p->aVector, sizeof(InstVector)/sizeof(int));
    Tcl_CreateObjCommand(interp, 
        "::tkhtml::instrument", instrument_objcmd, (ClientData)p, instDelCommand
    );
}

