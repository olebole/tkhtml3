
/* 
 * hv3see.c --
 *
 *     This file contains C-code that contributes to the Javascript based
 *     scripting environment in the Hv3 web browser. It assumes the
 *     availability of SEE (Simple EcmaScript Interpreter) and the Boehm 
 *     C/C++ garbage collecting memory allocator.
 */

#include <tcl.h>
#include <see/see.h>

#ifdef NO_HAVE_GC
    /* If the symbol NO_HAVE_GC is defined, have SEE use regular malloc() 
     * instead of a garbage-collecting version. Of course, it leaks a
     * lot of memory when compiled this way.
     */
    #define GC_MALLOC_UNCOLLECTABLE(x) ckalloc(x)
    #define GC_FREE(x) ckfree((char *)x)
#else
    #include <gc.h>
#endif

#include <string.h>
#include <assert.h>
#include <stdio.h>

typedef struct SeeInterp SeeInterp;
typedef struct SeeTclObject SeeTclObject;
typedef struct SeeJsObject SeeJsObject;

#define OBJECT_HASH_SIZE 257

struct SeeInterp {
    struct SEE_interpreter interp;
    Tcl_Interp *pTclInterp;

    /* Hash table containing the objects created by the Tcl interpreter */
    SeeTclObject *aTclObject[OBJECT_HASH_SIZE];

    /* Hash table containing the objects created by the Javascript side */
    SeeJsObject *aJsObject[OBJECT_HASH_SIZE];

    /* Tcl name of the global object. */
    char *zGlobal;
};
static int iSeeInterp = 0;

/* Return a pointer to the V-Table for Tcl based javascript objects.
 */
static struct SEE_objectclass *getVtbl();

/* Each javascript object created by the Tcl-side is represented by
 * an instance of the following struct. 
 */
struct SeeTclObject {
    struct SEE_object object;
    SeeInterp *pTclSeeInterp;
    Tcl_Obj *pObj;

    /* Next entry (if any) in the SeeInterp.aObject hash table */
    SeeTclObject *pNext;
};

/* Each javascript object created on the javascript side and used by
 * Tcl is represented by an instance of the following struct.
 */
struct SeeJsObject {
    /* zTclName points to a NULL terminated string with the name of the Tcl
     * handle for this object. This is also the hash table key. pObject
     * points to the javascript object.
     */
    char *zTclName;
    struct SEE_object *pObject;

    /* Next entry (if any) in the SeeInterp.aObject hash table */
    SeeJsObject *pNext;
};

/*
 *---------------------------------------------------------------------------
 *
 * hashCommand --
 *     Return a hash value between 0 and (OBJECT_HASH_SIZE-1) for the
 *     nul-terminated string passed as an argument.
 *
 * Results: 
 *     Integer between 0 and (OBJECT_HASH_SIZE-1), inclusive.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int
hashCommand(zCommand)
    char *zCommand;
{
    unsigned int iSlot = 0;
    char *z;
    for (z = zCommand ; *z; z++) {
        iSlot = (iSlot << 3) + (int)(*z);
    }
    return (iSlot % OBJECT_HASH_SIZE);
}

static struct Tcl_Obj *
objectToCommand(pTclSeeInterp, pObject)
    SeeInterp *pTclSeeInterp;
    struct SEE_object *pObject;
{
    char zPtrCmd[64];
    int nPtrCmd;
    int iSlot;
    SeeJsObject *pJsObject;

    /* Check if the SEE object is actually a native Tcl object. If so,
     * return the corresponding Tcl command.
     */
    if (pObject->objectclass == getVtbl()) {
        return ((SeeTclObject *)pObject)->pObj;
    }

    sprintf(zPtrCmd, "::see::js%p", (void *)pObject);
    nPtrCmd = strlen(zPtrCmd);
    iSlot = hashCommand(zPtrCmd);
    for (
        pJsObject = pTclSeeInterp->aJsObject[iSlot];
        pJsObject && strcmp(pJsObject->zTclName, zPtrCmd);
        pJsObject = pJsObject->pNext
    );
    if (!pJsObject) {
        pJsObject = (SeeJsObject *)SEE_malloc(
            &pTclSeeInterp->interp, 
            1 + nPtrCmd + sizeof(SeeJsObject)
        );
        pJsObject->pObject = pObject;
        pJsObject->zTclName = (char *)&pJsObject[1];
        strcpy(pJsObject->zTclName, zPtrCmd);

        pJsObject->pNext = pTclSeeInterp->aJsObject[iSlot];
        pTclSeeInterp->aJsObject[iSlot] = pJsObject;
    }
   
    return Tcl_NewStringObj(zPtrCmd, nPtrCmd);
};

/*
 *---------------------------------------------------------------------------
 *
 * valueToObj --
 *
 * Results:
 *     Tcl object with ref-count set to 0.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static Tcl_Obj *
valueToObj(pInterp, pValue)
    SeeInterp *pInterp;
    struct SEE_value *pValue;
{
    Tcl_Obj *aTclValues[2] = {0, 0};

    switch (SEE_VALUE_GET_TYPE(pValue)) {

        case SEE_UNDEFINED:
            aTclValues[0] = Tcl_NewStringObj("undefined", -1);
            break;

        case SEE_NULL:
            aTclValues[0] = Tcl_NewStringObj("null", -1);
            break;

        case SEE_OBJECT: {
            aTclValues[0] = Tcl_NewStringObj("object", -1);
            aTclValues[1] = objectToCommand(pInterp, pValue->u.object);
            break;
        }

        case SEE_BOOLEAN:
            aTclValues[0] = Tcl_NewStringObj("boolean", -1);
            aTclValues[1] = Tcl_NewBooleanObj(pValue->u.boolean);
            break;

        case SEE_NUMBER:
            aTclValues[0] = Tcl_NewStringObj("number", -1);
            aTclValues[1] = Tcl_NewDoubleObj(pValue->u.number);
            break;

        case SEE_STRING:
            aTclValues[0] = Tcl_NewStringObj("string", -1);
            aTclValues[1] = Tcl_NewUnicodeObj(
                pValue->u.string->data, pValue->u.string->length
            );
            break;

        default:
            assert(!"Bad value type");

    }

    assert(aTclValues[0]);
    if (!aTclValues[1]) {
        aTclValues[1] = Tcl_NewStringObj("", -1);
    }

    return Tcl_NewListObj(2, aTclValues);
}

static void
finalizeSeeTclObject(pSeeInterp, p, closure)
    struct SEE_interp *pSeeInterp;
    void *p;
    void *closure;
{
    SeeTclObject *pObject = (SeeTclObject *)p;
    Tcl_DecrRefCount(pObject->pObj);
    pObject->pObj = 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * findOrCreateObject --
 *
 * Results:
 *     Pointer to SeeTclObject structure.
 *
 * Side effects:
 *     May create a new SeeTclObject structure and add it to the
 *     SeeInterp.aObject hash table.
 *
 *---------------------------------------------------------------------------
 */
static struct SEE_object *
findOrCreateObject(pTclSeeInterp, pTclCommand)
    SeeInterp *pTclSeeInterp;
    Tcl_Obj *pTclCommand;
{
    char const *zCommand = Tcl_GetString(pTclCommand);
    int iSlot = hashCommand(zCommand);
    SeeTclObject *pObject;
    SeeJsObject *pJsObject;

    /* Check for the global object */
    if (pTclSeeInterp->zGlobal && !strcmp(zCommand, pTclSeeInterp->zGlobal)) {
        return pTclSeeInterp->interp.Global;
    }

    /* Search for an existing Js object */
    for (
        pJsObject = pTclSeeInterp->aJsObject[iSlot];
        pJsObject;
        pJsObject = pJsObject->pNext
    ) {
        if (0 == strcmp(zCommand, pJsObject->zTclName)) {
            return (struct SEE_object *)(pJsObject->pObject);
        }
    }

    /* Search for an existing Tcl object */
    for (
        pObject = pTclSeeInterp->aTclObject[iSlot];
        pObject && strcmp(zCommand, Tcl_GetString(pObject->pObj));
        pObject = pObject->pNext
    );

    /* There is no existing object, create a new SeeTclObject. */
    if (!pObject) {
        pObject = SEE_NEW_FINALIZE(
            &pTclSeeInterp->interp, SeeTclObject,
            finalizeSeeTclObject, 0
        );
        pObject->object.objectclass = getVtbl();
        pObject->object.Prototype = pTclSeeInterp->interp.Object_prototype;
        pObject->pObj = pTclCommand;
        pObject->pTclSeeInterp = pTclSeeInterp;
        pObject->pNext = 0;
        Tcl_IncrRefCount(pObject->pObj);
    
        /* Insert the new object into the hash table */
        pObject->pNext = pTclSeeInterp->aTclObject[iSlot];
        pTclSeeInterp->aTclObject[iSlot] = pObject;
    }

    return (struct SEE_object *)pObject;
}

static int
objToValue(pInterp, pObj, pValue)
    SeeInterp *pInterp;
    Tcl_Obj *pObj;                  /* IN: Tcl js value */
    struct SEE_value *pValue;       /* OUT: Parsed value */
{
    int rc;
    int nElem = 0;
    Tcl_Obj **apElem = 0;

    Tcl_Interp *pTclInterp = pInterp->pTclInterp;

    rc = Tcl_ListObjGetElements(pTclInterp, pObj, &nElem, &apElem);
    if (rc == TCL_OK) {
        assert(nElem == 0 || 0 != strcmp("", Tcl_GetString(pObj)));

        if (nElem == 0) {
            SEE_SET_UNDEFINED(pValue);
        } else {
            int iChoice;
            struct ValueType {
                char const *zType;
                int eType;
                int nArg;
            } aType[] = {
                {"undefined", SEE_UNDEFINED, 0}, 
                {"null", SEE_NULL, 0}, 
                {"number", SEE_NUMBER, 1}, 
                {"string", SEE_STRING, 1}, 
                {"boolean", SEE_BOOLEAN, 1},
                {"object", SEE_OBJECT, 1},
                {0, 0, 0}
            };

            if (Tcl_GetIndexFromObjStruct(pTclInterp, apElem[0], aType,
                sizeof(struct ValueType), "type", 0, &iChoice) 
            ){
                Tcl_AppendResult(pTclInterp, 
                    " value was \"", Tcl_GetString(pObj), "\"", 0
                );
                return TCL_ERROR;
            }
            if (nElem != (aType[iChoice].nArg + 1)) {
                Tcl_AppendResult(pTclInterp, 
                    "Bad javascript value spec: \"", Tcl_GetString(pObj),
                    "\"", 0
                );
                return TCL_ERROR;
            }
            switch (aType[iChoice].eType) {
                case SEE_UNDEFINED:
                    SEE_SET_UNDEFINED(pValue);
                    break;
                case SEE_NULL:
                    SEE_SET_NULL(pValue);
                    break;
                case SEE_BOOLEAN: {
                    int val;
                    if (Tcl_GetBooleanFromObj(pTclInterp, apElem[1], &val)) {
                        return TCL_ERROR;
                    }
                    SEE_SET_BOOLEAN(pValue, val);
                    break;
                }
                case SEE_NUMBER: {
                    double val;
                    if (Tcl_GetDoubleFromObj(pTclInterp, apElem[1], &val)) {
                        return TCL_ERROR;
                    }
                    SEE_SET_NUMBER(pValue, val);
                    break;
                }
                case SEE_STRING: {
                    struct SEE_string *pString;
                    pString = SEE_string_sprintf(
                        &pInterp->interp, "%s", Tcl_GetString(apElem[1])
                    );
                    SEE_SET_STRING(pValue, pString);
                    break;
                }
                case SEE_OBJECT: {
                    struct SEE_object *pObject = 
                        findOrCreateObject(pInterp, apElem[1]);
                    SEE_SET_OBJECT(pValue, pObject);
                    break;
                }
            }
        }
    }
    return rc;
}

/*
 *---------------------------------------------------------------------------
 *
 * handleJavascriptError --
 *
 *     This function is designed to be called when a javascript error
 *     occurs (i.e. a SEE exception is thrown by either SEE_Global_eval()
 *     or SEE_OBJECT_CALL()).
 *
 *     Information is retrieved from the try-catch context passed as 
 *     argument pTry and loaded into the result of the Tcl-interpreter
 *     component of argument pTclSeeInterp. 
 *
 * Results:
 *     Always returns TCL_ERROR.
 *
 * Side effects:
 *     Sets the result of Tcl interpreter pTclSeeInterp->pTclInterp.
 *
 *---------------------------------------------------------------------------
 */
static int 
handleJavascriptError(pTclSeeInterp, pTry)
    SeeInterp *pTclSeeInterp;
    SEE_try_context_t *pTry;
{
    Tcl_Interp *pTclInterp = pTclSeeInterp->pTclInterp;
    struct SEE_interpreter *pSeeInterp = &pTclSeeInterp->interp;

    struct SEE_traceback *pTrace;

    struct SEE_value error;
    Tcl_Obj *pError = Tcl_NewStringObj("Javascript Error: ", -1);

    SEE_ToString(pSeeInterp, SEE_CAUGHT(*pTry), &error);
    if (SEE_VALUE_GET_TYPE(&error) == SEE_STRING) {
        struct SEE_string *pS = error.u.string;
        Tcl_AppendUnicodeToObj(pError, pS->data, pS->length);
    } else {
        Tcl_AppendToObj(pError, "unknown.", -1);
    }
    Tcl_AppendToObj(pError, "\n\n", -1);

    for (pTrace = pTry->traceback; pTrace; pTrace = pTrace->prev) {
        struct SEE_string *pLocation;

        pLocation = SEE_location_string(pSeeInterp, pTrace->call_location);
        Tcl_AppendUnicodeToObj(pError, pLocation->data, pLocation->length);
        Tcl_AppendToObj(pError, "  ", -1);

        switch (pTrace->call_type) {
            case SEE_CALLTYPE_CONSTRUCT: {
                char const *zClass =  pTrace->callee->objectclass->Class;
                if (!zClass) zClass = "?";
                Tcl_AppendToObj(pError, "new ", -1);
                Tcl_AppendToObj(pError, zClass, -1);
                break;
            }
            case SEE_CALLTYPE_CALL: {
                struct SEE_string *pName;
                Tcl_AppendToObj(pError, "call ", -1);
                pName = SEE_function_getname(pSeeInterp, pTrace->callee);
                if (pName) {
                    Tcl_AppendUnicodeToObj(pError, pName->data, pName->length);
                } else {
                    Tcl_AppendToObj(pError, "?", -1);
                }
                break;
            }
            default:
                assert(0);
        }

        Tcl_AppendToObj(pError, "\n", -1);
    }

    Tcl_SetObjResult(pTclInterp, pError);
    return TCL_ERROR;
}

static void 
delInterpCmd(clientData)
    ClientData clientData;             /* The SeeInterp data structure */
{
    SeeInterp *pInterp = (SeeInterp *)clientData;
    GC_FREE(pInterp);
}

static int
interpEvalThis(pTclSeeInterp, pTclThis, pCode)
    SeeInterp *pTclSeeInterp;
    Tcl_Obj *pTclThis;
    Tcl_Obj *pCode;
{
    struct SEE_interpreter *pSeeInterp = &(pTclSeeInterp->interp);
    Tcl_Interp *pTclInterp = pTclSeeInterp->pTclInterp;

    struct SEE_input *pInputCode;
    struct SEE_object *pFunction;


    int rc = TCL_OK;
    SEE_try_context_t try_ctxt;

    pInputCode = SEE_input_utf8(pSeeInterp, Tcl_GetString(pCode));

    SEE_TRY(pSeeInterp, try_ctxt) {
        struct SEE_value result;
        if (pTclThis) {
            struct SEE_object *pThis = 0;
            pThis = findOrCreateObject(pTclSeeInterp, pTclThis);
            pFunction = SEE_Function_new(pSeeInterp, 0, 0, pInputCode);
            SEE_OBJECT_CALL(pSeeInterp, pFunction, pThis, 0, 0, &result);
        } else {
            SEE_Global_eval(pSeeInterp, pInputCode, &result);
        }
        Tcl_SetObjResult(pTclInterp, valueToObj(pTclSeeInterp, &result));
    }

    SEE_INPUT_CLOSE(pInputCode);

    if (SEE_CAUGHT(try_ctxt)) {
        rc = handleJavascriptError(pTclSeeInterp, &try_ctxt);
    }

    return rc;
}

struct SEE_scope;
struct SEE_scope {
  struct SEE_scope *next;
  struct SEE_object *obj;
};

static void installHv3Global(SeeInterp *, struct SEE_object *);

static int 
interpCmd(clientData, pTclInterp, objc, objv)
    ClientData clientData;             /* The SeeInterp data structure */
    Tcl_Interp *pTclInterp;            /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    int iChoice;
    SeeInterp *pInterp = (SeeInterp *)clientData;
    struct SEE_interpreter *pSeeInterp = &pInterp->interp;

    enum INTERP_enum {
        INTERP_EVAL,
        INTERP_DESTROY,
        INTERP_GLOBAL
    };

    static const struct InterpSubCommand {
        const char *zCommand;
        enum INTERP_enum eSymbol;
        int nMinArgs;
        int nMaxArgs;
        char *zArgs;
    } aSubCommand[] = {
        {"eval",    INTERP_EVAL,    1, 2, "?THIS-OBJECT? JAVASCRIPT"},  
        {"destroy", INTERP_DESTROY, 0, 0, ""},
        {"global",  INTERP_GLOBAL,  1, 1, "TCL-COMMAND"},
        {0, 0, 0, 0}
    };

    if (objc<2) {
        Tcl_WrongNumArgs(pTclInterp, 1, objv, "SUBCOMMAND ...");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObjStruct(pTclInterp, objv[1], aSubCommand, 
            sizeof(struct InterpSubCommand), "option", 0, &iChoice) 
    ){
        return TCL_ERROR;
    }

    if (
        objc < (aSubCommand[iChoice].nMinArgs + 2) || 
        objc > (aSubCommand[iChoice].nMaxArgs + 2)
    ) {
        Tcl_WrongNumArgs(pTclInterp, 2, objv, aSubCommand[iChoice].zArgs);
        return TCL_ERROR;
    }

    switch (aSubCommand[iChoice].eSymbol) {

        /*
         * seeInterp eval ?THIS-OBJECT? PROGRAM-TEXT
         * 
         *     Evaluate a javascript script.
         */
        case INTERP_EVAL: {
            Tcl_Obj *pProgram = ((objc == 4) ? objv[3] : objv[2]);
            Tcl_Obj *pThis =    ((objc == 4) ? objv[2] : 0);
            int rc = interpEvalThis(pInterp, pThis, pProgram);
            if (rc != TCL_OK) return rc;
            break;
        }

        /*
         * seeInterp global TCL-COMMAND
         *
         */
        case INTERP_GLOBAL: {
            int nGlobal;
            char *zGlobal;
            struct SEE_object *pWindow;

            if (pInterp->zGlobal) {
                Tcl_ResetResult(pTclInterp);
                Tcl_AppendResult(pTclInterp, "Can call [global] only once.", 0);
                return TCL_ERROR;
            }
            pWindow = findOrCreateObject(pInterp, objv[2]);
            installHv3Global(pInterp, pWindow);

            zGlobal = Tcl_GetStringFromObj(objv[2], &nGlobal);
            pInterp->zGlobal = SEE_malloc_string(pSeeInterp, nGlobal + 1);
            strcpy(pInterp->zGlobal, zGlobal);
            break;
        }

        case INTERP_DESTROY: {
            Tcl_DeleteCommand(pTclInterp, Tcl_GetString(objv[0]));
            break;
        }
    }

    return TCL_OK;
}

static int 
tclSeeInterp(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    char zCmd[64];
    SeeInterp *pInterp;

    sprintf(zCmd, "::see::interp_%d", iSeeInterp++);

    pInterp = (SeeInterp *)GC_MALLOC_UNCOLLECTABLE(sizeof(SeeInterp));
    memset(pInterp, 0, sizeof(SeeInterp));

    /* SEE_interpreter_init(&pInterp->interp); */
    SEE_interpreter_init_compat(&pInterp->interp, 
        SEE_COMPAT_JS15|SEE_COMPAT_SGMLCOM
    );
    pInterp->pTclInterp = interp;
    Tcl_CreateObjCommand(interp, zCmd, interpCmd, pInterp, delInterpCmd);

    Tcl_SetResult(interp, zCmd, TCL_VOLATILE);
    return TCL_OK;
}

int Tclsee_Init(interp)
    Tcl_Interp *interp;
{
    /* Require stubs libraries version 8.4 or greater. */
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.4", 0) == 0) {
        return TCL_ERROR;
    }
    if (Tk_InitStubs(interp, "8.4", 0) == 0) {
        return TCL_ERROR;
    }
#endif

    Tcl_PkgProvide(interp, "Tclsee", "1.0");
    Tcl_CreateObjCommand(interp, "::see::interp", tclSeeInterp, 0, 0);
    return TCL_OK;
}

static void 
SeeTcl_Get(pInterp, pObj, pProp, pRes)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_string *pProp;
    struct SEE_value *pRes;
{
    SeeTclObject *pObject = (SeeTclObject *)pObj;
    SeeInterp *pTclSeeInterp = pObject->pTclSeeInterp;
    Tcl_Interp *pTclInterp = pTclSeeInterp->pTclInterp;
    Tcl_Obj *pScript = Tcl_DuplicateObj(pObject->pObj);
    Tcl_Obj *pScriptRes;
    int rc;

    Tcl_IncrRefCount(pScript);
    Tcl_ListObjAppendElement(pTclInterp, pScript, Tcl_NewStringObj("Get", 3));
    Tcl_ListObjAppendElement(pTclInterp, pScript, 
            Tcl_NewUnicodeObj(pProp->data, pProp->length)
    );

    rc = Tcl_EvalObjEx(pTclInterp, pScript, TCL_GLOBAL_ONLY);
    Tcl_DecrRefCount(pScript);
    if (rc != TCL_OK) {
        SEE_error_throw_sys(pInterp, 
            pInterp->RangeError, "%s", Tcl_GetStringResult(pTclInterp)
        );
    }

    pScriptRes = Tcl_GetObjResult(pTclInterp);
    Tcl_IncrRefCount(pScriptRes);
    rc = objToValue(pTclSeeInterp, pScriptRes, pRes);
    Tcl_DecrRefCount(pScriptRes);
    if (rc != TCL_OK) {
        SEE_error_throw_sys(pInterp, 
            pInterp->RangeError, "%s", Tcl_GetStringResult(pTclInterp)
        );
    }
}

static void 
SeeTcl_Put(pInterp, pObj, pProp, pVal, flags)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_string *pProp;
    struct SEE_value *pVal;
    int flags;
{
    SeeTclObject *pObject = (SeeTclObject *)pObj;
    SeeInterp *pTclSeeInterp = pObject->pTclSeeInterp;
    Tcl_Interp *pTclInterp = pTclSeeInterp->pTclInterp;
    Tcl_Obj *pScript = Tcl_DuplicateObj(pObject->pObj);
    int rc;

    Tcl_ListObjAppendElement(pTclInterp, pScript, Tcl_NewStringObj("Put", 3));
    Tcl_ListObjAppendElement(pTclInterp, pScript, 
            Tcl_NewUnicodeObj(pProp->data, pProp->length)
    );
    Tcl_ListObjAppendElement(pTclInterp, pScript, 
            valueToObj(pTclSeeInterp, pVal)
    );

    rc = Tcl_EvalObjEx(pTclInterp, pScript, TCL_GLOBAL_ONLY);
    if (rc != TCL_OK) {
        SEE_error_throw_sys(pInterp, 
            pInterp->RangeError, "%s", Tcl_GetStringResult(pTclInterp)
        );
    }
}
static int 
SeeTcl_CanPut(pInterp, pObj, pProp)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_string *pProp;
{
    SeeTclObject *pObject = (SeeTclObject *)pObj;
    SeeInterp *pTclSeeInterp = pObject->pTclSeeInterp;
    Tcl_Interp *pTclInterp = pTclSeeInterp->pTclInterp;
    Tcl_Obj *pScript = Tcl_DuplicateObj(pObject->pObj);
    int rc;
    int ret;

    Tcl_ListObjAppendElement(pTclInterp, pScript, Tcl_NewStringObj("CanPut",6));
    Tcl_ListObjAppendElement(pTclInterp, pScript, 
            Tcl_NewUnicodeObj(pProp->data, pProp->length)
    );

    rc = Tcl_EvalObjEx(pTclInterp, pScript, TCL_GLOBAL_ONLY);
    if (
        rc != TCL_OK || TCL_OK != 
        Tcl_GetBooleanFromObj(pTclInterp, Tcl_GetObjResult(pTclInterp), &ret)
    ) {
        SEE_error_throw_sys(pInterp, 
            pInterp->RangeError, "%s", Tcl_GetStringResult(pTclInterp)
        );
    }
    return ret;
}
static int 
SeeTcl_HasProperty(pInterp, pObj, pProp)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_string *pProp;
{
    SeeTclObject *pObject = (SeeTclObject *)pObj;
    SeeInterp *pTclSeeInterp = pObject->pTclSeeInterp;
    Tcl_Interp *pTclInterp = pTclSeeInterp->pTclInterp;
    Tcl_Obj *pScript = Tcl_DuplicateObj(pObject->pObj);
    int rc;
    int ret;

    Tcl_ListObjAppendElement(pTclInterp, pScript, 
            Tcl_NewStringObj("HasProperty", 11)
    );
    Tcl_ListObjAppendElement(pTclInterp, pScript, 
            Tcl_NewUnicodeObj(pProp->data, pProp->length)
    );

    rc = Tcl_EvalObjEx(pTclInterp, pScript, TCL_GLOBAL_ONLY);
    if (
        rc != TCL_OK || TCL_OK != 
        Tcl_GetBooleanFromObj(pTclInterp, Tcl_GetObjResult(pTclInterp), &ret)
    ) {
        SEE_error_throw_sys(pInterp, 
            pInterp->RangeError, "%s", Tcl_GetStringResult(pTclInterp)
        );
    }
    return ret;
}
static int 
SeeTcl_Delete(pInterp, pObj, pProp)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_string *pProp;
{
    SeeTclObject *pObject = (SeeTclObject *)pObj;
    SeeInterp *pTclSeeInterp = pObject->pTclSeeInterp;
    Tcl_Interp *pTclInterp = pTclSeeInterp->pTclInterp;
    Tcl_Obj *pScript = Tcl_DuplicateObj(pObject->pObj);
    int rc;

    Tcl_ListObjAppendElement(pTclInterp, pScript, Tcl_NewStringObj("Delete",6));
    Tcl_ListObjAppendElement(pTclInterp, pScript, 
            Tcl_NewUnicodeObj(pProp->data, pProp->length)
    );

    rc = Tcl_EvalObjEx(pTclInterp, pScript, TCL_GLOBAL_ONLY);
    if (rc != TCL_OK) {
        SEE_error_throw_sys(pInterp, 
            pInterp->RangeError, "%s", Tcl_GetStringResult(pTclInterp)
        );
    }
}
static void 
SeeTcl_DefaultValue(pInterp, pObj, pThis, pHint, pRes)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_object *pThis;
    struct SEE_string *pHint;
    struct SEE_string *pRes;
{
    /* TODO: Don't understand this.... */
}
static struct SEE_enum * 
tclEnumerator(struct SEE_interpreter *, struct SEE_object *);
static struct SEE_enum *
SeeTcl_Enumerator(pInterp, pObj)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
{
    return tclEnumerator(pInterp, pObj);
}
static void 
tclCallOrConstruct(pMethod, pInterp, pObj, pThis, argc, argv, pRes)
    Tcl_Obj *pMethod;
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_object *pThis;
    int argc;
    struct SEE_value **argv;
    struct SEE_string *pRes;
{
    SeeTclObject *pObject = (SeeTclObject *)pObj;
    SeeInterp *pTclSeeInterp = pObject->pTclSeeInterp;
    Tcl_Interp *pTclInterp = pTclSeeInterp->pTclInterp;
    Tcl_Obj *pScript = Tcl_DuplicateObj(pObject->pObj);
    int rc;
    int ii;

    Tcl_ListObjAppendElement(0, pScript, pMethod);
    /* TODO: The "this" object */
    Tcl_ListObjAppendElement(0, pScript, Tcl_NewStringObj("THIS", 4));
    for (ii = 0; ii < argc; ii++) {
        Tcl_ListObjAppendElement(0,pScript,valueToObj(pTclSeeInterp,argv[ii]));
    }

    rc = Tcl_EvalObjEx(pTclInterp, pScript, TCL_GLOBAL_ONLY);
    if (rc != TCL_OK) {
        SEE_error_throw_sys(pInterp, 
            pInterp->RangeError, "%s", Tcl_GetStringResult(pTclInterp)
        );
    }

    rc = objToValue(pTclSeeInterp, Tcl_GetObjResult(pTclInterp), pRes);
    if (rc != TCL_OK) {
        SEE_error_throw_sys(pInterp, 
            pInterp->RangeError, "%s", Tcl_GetStringResult(pTclInterp)
        );
    }
}
static void 
SeeTcl_Construct(pInterp, pObj, pThis, argc, argv, pRes)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_object *pThis;
    int argc;
    struct SEE_value **argv;
    struct SEE_string *pRes;
{
    tclCallOrConstruct(
        Tcl_NewStringObj("Construct", 9),
        pInterp, pObj, pThis, argc, argv, pRes
    );
}
static void 
SeeTcl_Call(pInterp, pObj, pThis, argc, argv, pRes)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_object *pThis;
    int argc;
    struct SEE_value **argv;
    struct SEE_string *pRes;
{
    tclCallOrConstruct(
        Tcl_NewStringObj("Call", 4),
        pInterp, pObj, pThis, argc, argv, pRes
    );
}
static int 
SeeTcl_HasInstance(pInterp, pObj, pInstance)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_value *pInstance;
{
    printf("HasInstance!!!\n");
    return 0;
}
static void *
SeeTcl_GetSecDomain(pInterp, pObj)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
{
    printf("GetSecDomain!!!\n");
    return 0;
}

static struct SEE_objectclass SeeTclObjectVtbl = {
    "Object",
    SeeTcl_Get,
    SeeTcl_Put,
    SeeTcl_CanPut,
    SeeTcl_HasProperty,
    SeeTcl_Delete,
    SeeTcl_DefaultValue,
    SeeTcl_Enumerator,
    SeeTcl_Construct,
    SeeTcl_Call,
    0, /* SeeTcl_HasInstance, */
    0  /* SeeTcl_GetSecDomain */
};
static struct SEE_objectclass *getVtbl() {
    return &SeeTclObjectVtbl;
}


/* Sub-class of SEE_enum (v-table SeeTclEnumVtbl, see below) for iterating
 * through the properties of a Tcl-based object.
 */
typedef struct SeeTclEnum SeeTclEnum;
struct SeeTclEnum {
  struct SEE_enum base;

  int iCurrent;
  int nString;
  struct SEE_string **aString;
};

static struct SEE_string *
SeeTclEnum_Next(pSeeInterp, pEnum, pFlags)
    struct SEE_interpreter *pSeeInterp;
    struct SEE_enum *pEnum;
    int *pFlags;                          /* OUT: true for "do not enumerate" */
{
    SeeTclEnum *pSeeTclEnum = (SeeTclEnum *)pEnum;
    if (pSeeTclEnum->iCurrent < pSeeTclEnum->nString) {
        if (pFlags) *pFlags = 0;
        return pSeeTclEnum->aString[pSeeTclEnum->iCurrent++];
    }
    return NULL;
}

static struct SEE_enumclass SeeTclEnumVtbl = {
  0,  /* Unused */
  SeeTclEnum_Next
};


static struct SEE_enum *
tclEnumerator(pInterp, pObj)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
{
    SeeTclObject *pObject = (SeeTclObject *)pObj;
    SeeInterp *pTclSeeInterp = pObject->pTclSeeInterp;

    Tcl_Interp *pTclInterp = pTclSeeInterp->pTclInterp;
    Tcl_Obj *pScript = Tcl_DuplicateObj(pObject->pObj);

    Tcl_Obj *pRet = 0;       /* Return value of script */
    Tcl_Obj **apRet = 0;     /* List elements of pRet */
    int nRet = 0;            /* size of apString */

    SeeTclEnum *pEnum;
    int ii;

    Tcl_ListObjAppendElement(0, pScript, Tcl_NewStringObj("Enumerator", 10));
    if (
        TCL_OK != Tcl_EvalObjEx(pTclInterp, pScript, TCL_GLOBAL_ONLY) ||
        0      == (pRet = Tcl_GetObjResult(pTclInterp)) ||
        TCL_OK != Tcl_ListObjGetElements(pTclInterp, pRet, &nRet, &apRet)
    ) {
        SEE_error_throw_sys(pInterp, 
            pInterp->RangeError, "%s", Tcl_GetStringResult(pTclInterp)
        );
    }

    pEnum = SEE_malloc(&pTclSeeInterp->interp,
        sizeof(SeeTclEnum) + sizeof(struct SEE_String *) * nRet
    );
    pEnum->base.enumclass = &SeeTclEnumVtbl;
    pEnum->iCurrent = 0;
    pEnum->nString = nRet;
    pEnum->aString = (struct SEE_string **)(&pEnum[1]);
    
    for (ii = 0; ii < nRet; ii++) {
        pEnum->aString[ii] = SEE_string_sprintf(
             &pTclSeeInterp->interp, "%s", Tcl_GetString(apRet[ii])
        );
    }

    return (struct SEE_enum *)pEnum;
}

typedef struct Hv3GlobalObject Hv3GlobalObject;
struct Hv3GlobalObject {
    struct SEE_object object;
    struct SEE_object *pWindow;
    struct SEE_object *pGlobal;
};

static struct SEE_object *
hv3GlobalPick(pInterp, pObj, pProp)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_string *pProp;
{
    Hv3GlobalObject *p = (Hv3GlobalObject *)pObj;
    if (SEE_OBJECT_HASPROPERTY(pInterp, p->pWindow, pProp)) {
        return p->pWindow;
    } 
    return p->pGlobal;
}

static void 
Hv3Global_Get(pInterp, pObj, pProp, pRes)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_string *pProp;
    struct SEE_value *pRes;
{
    SEE_OBJECT_GET(pInterp, hv3GlobalPick(pInterp, pObj, pProp), pProp, pRes);
}
static void 
Hv3Global_Put(pInterp, pObj, pProp, pVal, flags)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_string *pProp;
    struct SEE_value *pVal;
    int flags;
{
    struct SEE_object *p = hv3GlobalPick(pInterp, pObj, pProp);
    SEE_OBJECT_PUT(pInterp, p, pProp, pVal, flags);
}
static int 
Hv3Global_CanPut(pInterp, pObj, pProp)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_string *pProp;
{
    struct SEE_object *p = hv3GlobalPick(pInterp, pObj, pProp);
    return SEE_OBJECT_CANPUT(pInterp, p, pProp);
}
static int 
Hv3Global_HasProperty(pInterp, pObj, pProp)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_string *pProp;
{
    Hv3GlobalObject *p = (Hv3GlobalObject *)pObj;
    return (
        SEE_OBJECT_HASPROPERTY(pInterp, p->pWindow, pProp) ||
        SEE_OBJECT_HASPROPERTY(pInterp, p->pGlobal, pProp)
    );
}
static int 
Hv3Global_Delete(pInterp, pObj, pProp)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_string *pProp;
{
    Hv3GlobalObject *p = (Hv3GlobalObject *)pObj;
    return SEE_OBJECT_DELETE(pInterp, p->pGlobal, pProp);
}
static void 
Hv3Global_DefaultValue(pInterp, pObj, pHint, pRes)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_value *pHint;
    struct SEE_value *pRes;
{
    Hv3GlobalObject *p = (Hv3GlobalObject *)pObj;
    SEE_OBJECT_DEFAULTVALUE(pInterp, p->pGlobal, pHint, pRes);
}

static struct SEE_enum * 
Hv3Global_Enumerator(struct SEE_interpreter *, struct SEE_object *);

static struct SEE_objectclass Hv3GlobalObjectVtbl = {
    "Hv3GlobalObject",
    Hv3Global_Get,
    Hv3Global_Put,
    Hv3Global_CanPut,
    Hv3Global_HasProperty,
    Hv3Global_Delete,
    Hv3Global_DefaultValue,
    Hv3Global_Enumerator,
    0,
    0,
    0, /* SeeTcl_HasInstance, */
    0  /* SeeTcl_GetSecDomain */
};

typedef struct Hv3GlobalEnum Hv3GlobalEnum;
struct Hv3GlobalEnum {
    struct SEE_enum base;
    struct SEE_enum *pWindowEnum;
    struct SEE_enum *pGlobalEnum;
};

static struct SEE_string *
Hv3GlobalEnum_Next(pSeeInterp, pEnum, pFlags)
    struct SEE_interpreter *pSeeInterp;
    struct SEE_enum *pEnum;
    int *pFlags;                          /* OUT: true for "do not enumerate" */
{
    Hv3GlobalEnum *p = (Hv3GlobalEnum *)pEnum;
    struct SEE_string *pRet;

    pRet = SEE_ENUM_NEXT(pSeeInterp, p->pWindowEnum, pFlags);
    if (!pRet) {
        pRet = SEE_ENUM_NEXT(pSeeInterp, p->pGlobalEnum, pFlags);
    }

    return pRet;
}

static struct SEE_enumclass Hv3GlobalEnumVtbl = {
  0,  /* Unused */
  Hv3GlobalEnum_Next
};

static struct SEE_enum *
Hv3Global_Enumerator(pInterp, pObj)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
{
    Hv3GlobalObject *p = (Hv3GlobalObject *)pObj;
    Hv3GlobalEnum *pEnum;

    pEnum = SEE_NEW(pInterp, Hv3GlobalEnum);
    pEnum->base.enumclass = &Hv3GlobalEnumVtbl;
    pEnum->pWindowEnum = SEE_OBJECT_ENUMERATOR(pInterp, p->pWindow);
    pEnum->pGlobalEnum = SEE_OBJECT_ENUMERATOR(pInterp, p->pGlobal);
    return ((struct SEE_enum *)pEnum);
}

/*
 *---------------------------------------------------------------------------
 *
 * installHv3Global --
 *
 *     Install an object (argument pWindow) as the hv3-global object
 *     for interpreter pTclSeeInterp. This function should only be
 *     called once in the lifetime of pTclSeeInterp.
 *
 *     The supplied object, pWindow, is attached to the real global 
 *     object (SEE_interp.Global) so that the following SEE_objectclass
 *     interface functions are intercepted and handled as follows:
 *
 *         Get:
 *             If the HasProperty() method of pWindow returns true,
 *             call the Get method of pWindow. Otherwise, call Get on
 *             the real global object.
 *         Put:
 *             If the HasProperty() method of pWindow returns true,
 *             call the Put method of pWindow. Otherwise, call Put on
 *             the real global object.
 *         CanPut:
 *             If the HasProperty() method of pWindow returns true,
 *             call the CanPut method of pWindow. Otherwise, call CanPut 
 *             on the real global object.
 *         HasProperty():
 *             Return the logical OR of HasProperty called on pWindow and
 *             the real global object.
 *         Enumerator():
 *             Return a wrapper SEE_enum object that first iterates through
 *             the entries returned by a pWindow iterator, then through
 *             the entries returned by the real global object.
 *
 *     The Delete() and DefaultValue() methods are passed through to
 *     the real global object.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static void 
installHv3Global(pTclSeeInterp, pWindow)
    SeeInterp *pTclSeeInterp;
    struct SEE_object *pWindow;
{
    struct SEE_scope *pScope;
    struct SEE_interpreter *p = &pTclSeeInterp->interp;
    Hv3GlobalObject *pGlobal = SEE_NEW(p, Hv3GlobalObject);

    assert(p->Global->objectclass != &Hv3GlobalObjectVtbl);

    pGlobal->object.objectclass = &Hv3GlobalObjectVtbl;
    pGlobal->object.Prototype = p->Global->Prototype;
    pGlobal->pWindow = pWindow;
    pGlobal->pGlobal = p->Global;

    for (pScope = p->Global_scope; pScope; pScope = pScope->next) {
        if (pScope->obj == p->Global) {
            pScope->obj = (struct SEE_object *)pGlobal;
        }
    }
    p->Global = (struct SEE_object *)pGlobal;
}

