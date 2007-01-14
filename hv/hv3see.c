
/*
 * hv3see.c --
 *
 *     This file contains C-code that contributes to the Javascript based
 *     scripting environment in the Hv3 web browser. It assumes the
 *     availability of SEE (Simple EcmaScript Interpreter) and the Boehm
 *     C/C++ garbage collecting memory allocator.
 *
 *     SEE:      http://www.adaptive-enterprises.com.au/~d/software/see/
 *     Boehm GC: http://www.hpl.hp.com/personal/Hans_Boehm/gc/
 *
 *     Although it may be still be used for other purposes, the design
 *     of this extension is heavily influenced by the requirements of Hv3.
 *     It is not a generic javascript interpreter interface for Tcl.
 *
 * TODO: Copyright.
 */

/*------------------------------------------------------------------------
 * DESIGN NOTES
 *
 * TODO: Organize all this so that others can follow.
 *     
 * Javascript values:
 *
 *     undefined
 *     null
 *     boolean    BOOL
 *     number     NUMBER
 *     string     STRING
 *     object     COMMAND
 *
 * Interpreter Interface:
 *
 *     set interp [::see::interp]
 *
 *     $interp global TCL-OBJECT
 *
 *     $interp eval JAVASCRIPT
 *         Evaluate the supplied javascript using SEE_Global_eval().
 *
 *     $interp function JAVASCRIPT
 *         Create a function object using SEE_Function_new() and return
 *         a javascript-value containing a reference to the function.
 *
 *     $interp native
 *         Create a native object using SEE_Function_new() and return
 *         a javascript-value containing a reference to the function.
 *
 *     $interp destroy
 *         Delete an interpreter. The command $interp is deleted by
 *         this command.
 *
 * Object Interface:
 *
 *         $obj Call         THIS ?ARG-VALUE...?
 *         $obj CanPut       PROPERTY
 *         $obj Construct    ?ARG-VALUE...?
 *         $obj DefaultValue
 *         $obj Delete       PROPERTY
 *         $obj Enumerator
 *         $obj Get          PROPERTY
 *         $obj HasProperty  PROPERTY
 *         $obj Put          PROPERTY ARG-VALUE
 *
 *     Argument PROPERTY is a simple property name (i.e. "className"). VALUE 
 *     is a typed javascript value. 
 *
 * Object resource management:
 *
 *     All based around the following interface:
 *
 *         $obj Finalize
 *
 *     1. The global object.
 *     2. Tcl-command based objects.
 *     3. Objects created by [$interp native] or [$interp function].
 *     4. Transients - objects created by javascript and passed to Tcl
 *        scripts.
 *
 *     1. There are no resource management issues on the global object.
 *        The script must ensure that the specified command exists for the
 *        lifetime of the interpreter. The [Finalize] method is never
 *        called by the extension on the global object (not even when the
 *        interpreter is destroyed).
 *
 *     2. Objects may be created by Tcl scripts by returning a value of the
 *        form {object COMMAND} from a call to the [Call], [Construct] or
 *        [Get] method of the "Object Interface" (see above). Assuming 
 *        that COMMAND is a Tcl command, the "COMMAND" values is
 *        transformed to a javascript value containing an object 
 *        reference as follows:
 *
 *            i. Search the SeeInterp.aTclObject table for a match. If
 *               found, use the SeeTclObject* as the (struct SEE_object *)
 *               value to pass to javascript.
 *
 *           ii. If not found, allocate and populate a new SeeTclObject
 *               structure. Insert it into the aTclObject[] table. It is
 *               allocated with SEE_NEW_FINALIZE(). The finalization
 *               callback:
 *               
 *               a) Removes the entry from athe aTclObject[] table, and
 *               b) Calls the [Finalize] method of the Tcl command.
 *
 *        If the Tcl script wants to delete the underlying Tcl object from
 *        within the [Finalize] method, it may safely do so.
 * 
 *     3. When an object is created by [native] or [function], an entry
 *        is added to the aNativeObject[] table and a Tcl command to
 *        access the object is created. Calling the [Finalize] method
 *        on the returned command deletes the aNativeObject[] entry
 *        and the Tcl command. The object itself is deleted by garbage
 *        collection when there are no references (note that the
 *        aNativeObject entry does count as a reference while it exists).
 *
 *            set native   [$interp native]
 *            set function [$interp function $function-body]
 *
 *            eval $native Put go [list [list object $function]]
 *            eval $function Finalize
 *
 *            # It is no longer safe to use $function. But the object
 *            # may still be referenced by obtaining a ref via the $native
 *            # object:
 *            set func [lindex [$native Get go] 1]
 *            eval $func Call ARGS
 *
 *            # Setting the property of another object to such a value
 *            # does a real copy of the underlying object reference. The
 *            # "go" property of the $native2 object is still good after
 *            # the following block, but the $func reference is not.
 *            set native2 [$interp native]
 *            $native2 Put go [list $native Get go]
 *            $native Finalize
 *
 *        The form of the $func reference is "$native Get go", so the
 *        executed command is eventually:
 *
 *            $native Get go Call ?ARGS...?
 *
 *        In some circumstances it may be conveniant to use this special
 *        syntax of the [Get] method directly. e.g.:
 *
 *            if {[$button HasProperty onclick]} {
 *                $button Get onclick Call [list [list object $eventobj]]
 *            }
 *
 *     4. References to these objects are always transient and should
 *        not be stored by a Tcl script. The only way to store a reference
 *        to such an object is to create a native object using 
 *        [$interp native] and store the javascript object as a property 
 *        of the native object. A reference retrieved via a [Get] on a
 *        native object may be used for the lifetime of that object.
 *
 *        When using this technique, the actual reference retrieved may
 *        be different from the one stored. But the object refered to
 *        is the same.
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
    #define GC_register_finalizer(a,b,c,d,e) 
#else
    #include <gc.h>
#endif

#include <string.h>
#include <assert.h>
#include <stdio.h>

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

typedef struct SeeInterp SeeInterp;
typedef struct SeeTclObject SeeTclObject;
typedef struct SeeJsObject SeeJsObject;

#define OBJECT_HASH_SIZE 257

struct SeeInterp {
    /* The two interpreters - SEE and Tcl. */
    struct SEE_interpreter interp;
    Tcl_Interp *pTclInterp;

    /* Hash table containing the objects created by the Tcl interpreter.
     * This maps from the Tcl command to the SeeTclObject structure.
     *
     * When this structure is allocated (in function tclSeeInterp()), a
     * seperate allocation is made for aTclObject using SEE_malloc_string().
     */
    SeeTclObject **aTclObject;

    /* Number of references to this structure. This is initially set to
     * one, and incremented each time an object is added to the aTclObject[]
     * table. It is decremented when an object when:
     *
     *     + A [$interp destroy] command is evaluated, or
     *     + An object is removed from the aTclObject[] table.
     *
     * When nRef reaches zero, this structure may be freed using GC_FREE()
     * (it is allocated with GC_MALLOC_UNCOLLECTABLE()). The structure may not
     * be freed before all of the SeeTclObject structures have been collected
     * as the finalizer for these structures refers to this one.
     */
    int nRef;

    /* Hash table containing the objects created by the Javascript side 
     * accessable via [$interp Get NAME] references. i.e.:
     *
     *     + Any object passed as arguments to a [Call], [Construct],
     *       or [Put] method currently executing.
     *     + Any object created by the [$interp function] or 
     *       [$interp native] commands that has not yet had [Finalize]
     *       called on it.
     * 
     * The hash table keys are integers: SeeJsObject.iKey. See function
     * hashInteger() below. Key values are allocated using variable
     * iNextKey.
     */
    int iNextKey;
    SeeJsObject *aJsObject[OBJECT_HASH_SIZE];

    /* Linked list of SeeJsObject structures that will be removed from
     * the aJsObject[] table next time removeTransientRefs() is called.
     */
    SeeJsObject *pTransient;

    /* Tcl name of the global object. The NULL-terminated string is
     * allocated using SEE_malloc_string() when the global object
     * is set (the [$interp global] command).
     */
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

/* Entries in the SeeInterp.aJsObject[] hash table are instances of the
 * following structure.
 */
struct SeeJsObject {
    int iKey;
    struct SEE_object *pObject;

    /* Next entry (if any) in the SeeInterp.aJsObject hash table */
    SeeJsObject *pNext;
 
    /* Next entry in the SeeInterp.pTransient list */
    SeeJsObject *pNextTransient;
};

/*
 * Forward declarations for the event-target implementation.
 */
static Tcl_ObjCmdProc    eventTargetNew;
static Tcl_ObjCmdProc    eventTargetMethod;
static Tcl_CmdDeleteProc eventTargetDelete;
static struct SEE_object *eventTargetValue(SeeInterp *, Tcl_Obj *, Tcl_Obj *);


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

/*
 *---------------------------------------------------------------------------
 *
 * hashInteger --
 *     Return a hash value between 0 and (OBJECT_HASH_SIZE-1) for the
 *     integer passed as an argument.
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
hashInteger(iValue)
    int iValue;
{
    return (((unsigned int)iValue) % OBJECT_HASH_SIZE);
}

/*
 *---------------------------------------------------------------------------
 *
 * createObjectRef --
 *     Insert an entry in the SeeInterp.aJsObject[] table for pObject.
 *     return the integer key associated with the table entry.
 *
 * Results: 
 *     Integer.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int
createObjectRef(pTclSeeInterp, pObject, isTransient)
    SeeInterp *pTclSeeInterp;
    struct SEE_object *pObject;
    int isTransient;
{
    SeeJsObject *pJsObject;
    int iSlot;

    /* Create the new SeeJsObject structure. */
    pJsObject = SEE_NEW(&pTclSeeInterp->interp, SeeJsObject);
    pJsObject->iKey = pTclSeeInterp->iNextKey++;
    pJsObject->pObject = pObject;

    /* Insert it into the SeeInterp.aJsObject[] hash-table */
    iSlot = hashInteger(pJsObject->iKey);
    pJsObject->pNext = pTclSeeInterp->aJsObject[iSlot];
    pTclSeeInterp->aJsObject[iSlot] = pJsObject;

    if (isTransient) {
        pJsObject->pNextTransient = pTclSeeInterp->pTransient;
        pTclSeeInterp->pTransient = pJsObject;
    }

    return pJsObject->iKey;
}

/*
 *---------------------------------------------------------------------------
 *
 * removeObjectRef --
 *     Remove from the SeeInterp.aJsObject table the object with key iKey.
 *     If there is no such entry, a crash or assert() failure will result.
 *
 * Results: 
 *     None.
 *
 * Side effects:
 *     Removes an element from SeeInterp.aJsObject.
 *
 *---------------------------------------------------------------------------
 */
static void
removeObjectRef(pTclSeeInterp, iKey)
    SeeInterp *pTclSeeInterp;
    int iKey;
{
    SeeJsObject *p;
    int iSlot = hashInteger(iKey);

    p = pTclSeeInterp->aJsObject[iSlot];
    assert(p);
    if (p->iKey == iKey) {
        pTclSeeInterp->aJsObject[iSlot] = p->pNext;
    } else {
        assert(p->pNext);
        while (p->pNext->iKey != iKey) {
            p = p->pNext;
            assert(p->pNext);
        }
        p->pNext = p->pNext->pNext;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * removeTransientRefs --
 *     Remove from the SeeInterp.aJsObject table all the objects that
 *     feature in the SeeInterp.pTransient list.
 *
 * Results: 
 *     None.
 *
 * Side effects:
 *     Removes elements from SeeInterp.aJsObject. Clears SeeInterp.pTransient.
 *
 *---------------------------------------------------------------------------
 */
static void
removeTransientRefs(pTclSeeInterp)
    SeeInterp *pTclSeeInterp;
{
    SeeJsObject *p;
    for (p = pTclSeeInterp->pTransient; p; p = p->pNextTransient) {
        removeObjectRef(pTclSeeInterp, p->iKey);
    }
    pTclSeeInterp->pTransient = 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * lookupObjectRef --
 *     Lookup the entry associated with parameter iKey in the 
 *     SeeInterp.aJsObject[] table. Return a pointer to the SEE object
 *     stored as part of the entry.
 *
 *     If there is no such entry in the SeeInterp.aJsObject[] table,
 *     return NULL.
 *
 * Results: 
 *     Pointer to SEE_object, or NULL.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static struct SEE_object *
lookupObjectRef(pTclSeeInterp, iKey)
    SeeInterp *pTclSeeInterp;
    int iKey;
{
    SeeJsObject *pJsObject;
    int iSlot = hashInteger(iKey);

    for (
        pJsObject = pTclSeeInterp->aJsObject[iSlot];
        pJsObject && pJsObject->iKey != iKey;
        pJsObject = pJsObject->pNext
    );

    if (!pJsObject) {
        Tcl_Interp *pTclInterp;
        char zBuf[64];
        sprintf(zBuf, "No such object: %d", iKey);
        Tcl_SetResult(pTclInterp, zBuf, TCL_VOLATILE);
        return 0;
    }

    return pJsObject->pObject;
}

/*
 *---------------------------------------------------------------------------
 *
 * primitiveValueToTcl --
 *
 *     Convert the SEE value *pValue to it's Tcl representation, assuming
 *     that *pValue holds a primitive value, not a javascript object. 
 *     Return a pointer to a new Tcl object (ref-count 0) containing
 *     the Tcl representation.
 *
 *     If *pValue does contain a javascript object, use SEE_ToPrimitive()
 *     to convert it to a primitive. The conversion is done on a copy
 *     to *pValue, so *pValue is never modified.
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
primitiveValueToTcl(pTclSeeInterp, pValue)
    SeeInterp *pTclSeeInterp;
    struct SEE_value *pValue;
{
    Tcl_Obj *aTclValues[2] = {0, 0};
    int nTclValues = 2;
    struct SEE_value copy;
    struct SEE_value *p = pValue;

    if (SEE_VALUE_GET_TYPE(pValue) == SEE_OBJECT) {
        SEE_ToPrimitive(&pTclSeeInterp->interp, pValue, 0, &copy);
        p = &copy;
    }

    switch (SEE_VALUE_GET_TYPE(p)) {

        case SEE_UNDEFINED:
            aTclValues[0] = Tcl_NewStringObj("undefined", -1);
            break;

        case SEE_NULL:
            aTclValues[0] = Tcl_NewStringObj("null", -1);
            break;

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

        case SEE_OBJECT: 
        default:
            assert(!"Bad value type");

    }

    assert(aTclValues[0]);
    if (!aTclValues[1]) {
        nTclValues = 1;
    }

    return Tcl_NewListObj(nTclValues, aTclValues);
}

/*
 *---------------------------------------------------------------------------
 *
 * argValueToTcl --
 *
 *     Convert the SEE value *pValue to it's Tcl form. If *pValue contains
 *     a primitive (non javascript-object) value, use primitiveValueToTcl()
 *     to convert it. 
 *
 *     If *pValue contains an object reference, then add an entry for the
 *     object to the SeeInterp.aJsObject[] array and return an object
 *     reference of the form:
 *
 *         {object {INTERP Get ID}}
 *
 *     The ID for the new object reference is added to the
 *     SeeInterp.pTransient list (so that it is removed by
 *     removeTransientRefs()).
 *
 * Results:
 *     Tcl object with ref-count set to 0.
 *
 * Side effects:
 *     May add an entry to SeeInterp.aJsObject[].
 *
 *---------------------------------------------------------------------------
 */
static Tcl_Obj *
argValueToTcl(pTclSeeInterp, pValue)
    SeeInterp *pTclSeeInterp;
    struct SEE_value *pValue;
{
    if (SEE_VALUE_GET_TYPE(pValue) == SEE_OBJECT) {
        int iKey;
        Tcl_Obj *aTclValues[2];
        struct SEE_object *pObject = pValue->u.object;

        aTclValues[0] = Tcl_NewStringObj("object", -1);

        if (pObject->objectclass == getVtbl()) {
          aTclValues[1] = ((SeeTclObject *)pObject)->pObj;
        } else {
          iKey = createObjectRef(pTclSeeInterp, pObject, 1);
          /* TODO: SeeInterp.aTransient[] */
          aTclValues[1] = Tcl_NewStringObj("Get", -1);
          Tcl_ListObjAppendElement(0, aTclValues[1], Tcl_NewIntObj(iKey));
        }
        return Tcl_NewListObj(2, aTclValues);
    } else {
        return primitiveValueToTcl(pTclSeeInterp, pValue);
    }
}

static void
finalizeSeeTclObject(p, closure)
    void *p;
    void *closure;
{
    SeeTclObject *pObject = (SeeTclObject *)p;
    SeeInterp *pTclSeeInterp = (SeeInterp *)closure;
    int iSlot = hashCommand(Tcl_GetString(pObject->pObj));

    Tcl_Obj *pFinalize;

    /* Remove the entry from the SeeInterp.aTclObject[] table */
    if (pTclSeeInterp->aTclObject[iSlot] == pObject) {
        pTclSeeInterp->aTclObject[iSlot] = pObject->pNext;
    } else {
        SeeTclObject *pTmp = pTclSeeInterp->aTclObject[iSlot];
        while (pTmp->pNext != pObject) {
            assert(pTmp && pTmp->pNext);
            pTmp = pTmp->pNext;
        }
        pTmp->pNext = pObject->pNext;
    }

    /* Execute the Tcl Finalize hook. Do nothing with the result thereof. */
    pFinalize = Tcl_DuplicateObj(pObject->pObj);
    Tcl_IncrRefCount(pFinalize);
    Tcl_ListObjAppendElement(0, pFinalize, Tcl_NewStringObj("Finalize", 8));
    Tcl_EvalObjEx(pTclSeeInterp->pTclInterp, pFinalize, TCL_GLOBAL_ONLY);

    /* Decrement the ref count on the Tcl object */
    Tcl_DecrRefCount(pObject->pObj);

    /* Decrement the ref count on the SeeInterp structure */
    pTclSeeInterp->nRef--;
    assert(pTclSeeInterp->nRef >= 0);
    if (pTclSeeInterp->nRef == 0) {
        GC_FREE(pTclSeeInterp);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * findOrCreateObject --
 *
 * Results:
 *     Pointer to SEE_object structure.
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
    Tcl_Interp *pTclInterp = pTclSeeInterp->pTclInterp;
    struct SEE_interpreter *pSeeInterp = &pTclSeeInterp->interp;
    char const *zCommand = Tcl_GetString(pTclCommand);
    int iSlot = hashCommand(zCommand);
    SeeTclObject *pObject;

    Tcl_Obj *pFirst;
    Tcl_Obj *pSecond;
    int iSecond;
    int iListLen;

    /* Check for the global object */
    if (pTclSeeInterp->zGlobal && !strcmp(zCommand, pTclSeeInterp->zGlobal)) {
        return pTclSeeInterp->interp.Global;
    }

    /* See if this is a javascript object reference. It is assumed to
     * be a javascript reference if:
     *
     *     (a) pTclCommand is a well-formatted list, and
     *     (b) pTclCommand has an even number of entries, and
     *     (c) the first element of pTclCommand is "Get".
     */
    if (
        TCL_OK == Tcl_ListObjLength(0, pTclCommand, &iListLen)  &&
        0 == (iListLen % 2) && iListLen >= 2                    &&
        TCL_OK == Tcl_ListObjIndex(0, pTclCommand, 0, &pFirst)  &&
        0 == strcmp(Tcl_GetString(pFirst), "Get")               &&
        TCL_OK == Tcl_ListObjIndex(0, pTclCommand, 1, &pSecond) &&
        TCL_OK == Tcl_GetIntFromObj(0, pSecond, &iSecond)
    ) {
        int ii;
        struct SEE_object *pNative = lookupObjectRef(pTclSeeInterp, iSecond);
        if (!pNative) return 0;

        for (ii = 3; ii < iListLen; ii += 2) {
            struct SEE_value val;
            Tcl_Obj *pProp;
            const char *zProp;

            Tcl_ListObjIndex(0, pTclCommand, ii, &pProp);
            zProp = Tcl_GetString(pProp);

            SEE_OBJECT_GETA(pSeeInterp, pNative, zProp, &val);
            if (SEE_VALUE_GET_TYPE(&val) != SEE_OBJECT) {
                Tcl_AppendResult(pTclInterp, "No such object: ", zProp, 0);
                return 0;
            } else {
                pNative = val.u.object;
            }
        }

        return pNative;
    }

    /* Search for an existing Tcl object */
    for (
        pObject = pTclSeeInterp->aTclObject[iSlot];
        pObject && strcmp(zCommand, Tcl_GetString(pObject->pObj));
        pObject = pObject->pNext
    );

    /* If pObject is still NULL, there is no existing object, create a new
     * SeeTclObject. The object is allocated with SEE_malloc_string() and
     * then the function finalizeSeeTclObject() added as a finalizer
     * using GC_register_finalizer().
     */
    if (!pObject) {
        int nByte = sizeof(SeeTclObject);
        pObject = (SeeTclObject *)SEE_malloc_string(pSeeInterp, nByte);
        GC_register_finalizer(pObject,finalizeSeeTclObject,pTclSeeInterp,0,0);

        pObject->object.objectclass = getVtbl();
        pObject->object.Prototype = pTclSeeInterp->interp.Object_prototype;
        pObject->pObj = pTclCommand;
        pObject->pTclSeeInterp = pTclSeeInterp;
        pObject->pNext = 0;
        Tcl_IncrRefCount(pObject->pObj);
    
        /* Insert the new object into the hash table */
        pObject->pNext = pTclSeeInterp->aTclObject[iSlot];
        pTclSeeInterp->aTclObject[iSlot] = pObject;

        /* Increase the reference count on the SeeInterp structure */
        pTclSeeInterp->nRef++;
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
            #define EVENT_VALUE -123
            struct ValueType {
                char const *zType;
                int eType;
                int nArg;
            } aType[] = {
                {"undefined", SEE_UNDEFINED, 0}, 
                {"null",      SEE_NULL, 0}, 
                {"number",    SEE_NUMBER, 1}, 
                {"string",    SEE_STRING, 1}, 
                {"boolean",   SEE_BOOLEAN, 1},
                {"object",    SEE_OBJECT, 1},
                {"event",     EVENT_VALUE, 2},
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

                case EVENT_VALUE: {
                    struct SEE_object *pObject = 
                        eventTargetValue(pInterp, apElem[0], apElem[1]);
                    if (pObject) {
                        SEE_SET_OBJECT(pValue, pObject);
                    } else {
                        SEE_SET_UNDEFINED(pValue);
                    }
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
    SeeInterp *pTclSeeInterp = (SeeInterp *)clientData;
    SEE_gcollect(&pTclSeeInterp->interp);
    pTclSeeInterp->nRef--;
    if (pTclSeeInterp->nRef == 0) {
        GC_FREE(pTclSeeInterp);
    }
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
        Tcl_Obj *pRes;

        if (pTclThis) {
            struct SEE_object *pThis = 0;
            pThis = findOrCreateObject(pTclSeeInterp, pTclThis);
            pFunction = SEE_Function_new(pSeeInterp, 0, 0, pInputCode);
            SEE_OBJECT_CALL(pSeeInterp, pFunction, pThis, 0, 0, &result);
        } else {
            SEE_Global_eval(pSeeInterp, pInputCode, &result);
        }

        pRes = primitiveValueToTcl(pTclSeeInterp, &result);
        Tcl_SetObjResult(pTclInterp, pRes);
    }

    SEE_INPUT_CLOSE(pInputCode);

    if (SEE_CAUGHT(try_ctxt)) {
        rc = handleJavascriptError(pTclSeeInterp, &try_ctxt);
    }

    return rc;
}

static void installHv3Global(SeeInterp *, struct SEE_object *);

/*
 *---------------------------------------------------------------------------
 *
 * objectCmd --
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int
objectCmd(pTclSeeInterp, p, objc, objv, apGet)
    SeeInterp *pTclSeeInterp;
    struct SEE_object *p;
    int objc;
    Tcl_Obj **objv;             /* Arguments  */
    Tcl_Obj **apGet;
{
    Tcl_Interp *pTclInterp = pTclSeeInterp->pTclInterp;
    struct SEE_interpreter *pSeeInterp = &pTclSeeInterp->interp;
    int rc = TCL_OK;

    int iChoice;

    enum OBJECT_enum {
        OBJECT_Call,
        OBJECT_CanPut,
        OBJECT_Construct,
        OBJECT_DefaultValue,
        OBJECT_Delete,
        OBJECT_Enumerator,
        OBJECT_Get,
        OBJECT_HasProperty,
        OBJECT_Put,
    };

    static const struct ObjectSubCommand {
        const char *zCommand;
        enum OBJECT_enum eSymbol;
        int nMinArgs;
        int nMaxArgs;
        char *zArgs;
    } aSubCommand[] = {
        {"Call",         OBJECT_Call,         1, -1, ""},
        {"CanPut",       OBJECT_CanPut,       1,  1, "PROPERTY"},
        {"Construct",    OBJECT_Construct,    0, -1, ""},
        {"DefaultValue", OBJECT_DefaultValue, 0,  0, ""},
        {"Delete",       OBJECT_Delete,       1,  1, "PROPERTY"},
        {"Enumerator",   OBJECT_Enumerator,   0,  0, ""},
        {"Get",          OBJECT_Get,          1, -1, "PROPERTY"},
        {"HasProperty",  OBJECT_HasProperty,  1,  1, "PROPERTY"},
        {"Put",          OBJECT_Put,          2,  2, "PROPERTY VALUE"},
        {0, 0, 0, 0}
    };

    int nMinArgs;
    int nMaxArgs;

    assert(objc >= 1);
    if (Tcl_GetIndexFromObjStruct(pTclInterp, objv[0], aSubCommand, 
            sizeof(struct ObjectSubCommand), "option", 0, &iChoice) 
    ){
        return TCL_ERROR;
    }

    nMinArgs = aSubCommand[iChoice].nMinArgs;
    nMaxArgs = aSubCommand[iChoice].nMaxArgs;
    if (objc < (nMinArgs + 1) || (nMaxArgs > 0 && objc > (nMaxArgs + 1))) {
        Tcl_WrongNumArgs(pTclInterp, 1, objv, aSubCommand[iChoice].zArgs);
        return TCL_ERROR;
    }

    switch (aSubCommand[iChoice].eSymbol) {

        /*
         * Call      THIS ?ARGS...?
         * Construct ?ARGS...?
         */
        case OBJECT_Call: 
        case OBJECT_Construct: {
            SEE_try_context_t try_ctxt;

            struct SEE_value **apArg  = 0;
            int nArg = objc - 1;

            struct SEE_object *pThis = 0;
            if (aSubCommand[iChoice].eSymbol == OBJECT_Call) {
                pThis = findOrCreateObject(pTclSeeInterp, objv[1]);
                nArg--;
            }

            if (nArg > 0) {
                int ii;
                int nByte = sizeof(struct SEE_Value *) * nArg;

                apArg = (struct SEE_value **)SEE_malloc(pSeeInterp, nByte);
                memset(apArg, 0, nByte);

                for (ii = 0; rc == TCL_OK && ii < nArg; ii++) {
                    Tcl_Obj *pObj = objv[ii + objc - nArg];
                    rc = objToValue(pTclSeeInterp, pObj, &apArg[ii]);
                }
            }
            if (rc != TCL_OK) break;

            SEE_TRY(pSeeInterp, try_ctxt) {
                Tcl_Obj *pRes;
                struct SEE_value res;
                if (pThis) {
                    SEE_OBJECT_CALL(pSeeInterp, p, pThis, nArg, apArg, &res);
                } else {
                    SEE_OBJECT_CONSTRUCT(pSeeInterp, p, 0, nArg, apArg, &res);
                }
                pRes = primitiveValueToTcl(pTclSeeInterp, &res);
                Tcl_SetObjResult(pTclInterp, pRes);
            }

            if (SEE_CAUGHT(try_ctxt)) {
                rc = handleJavascriptError(pTclSeeInterp, &try_ctxt);
            }
            
            break;
        }

        /*
         * CanPut      PROPERTY
         * HasProperty PROPERTY
         * Delete PROPERTY
         */
        case OBJECT_CanPut: {
            assert(0);
            break;
        }
        case OBJECT_HasProperty: {
            assert(0);
            break;
        }
        case OBJECT_Delete: {
            assert(0);
            break;
        }

        /*
         * DefaultValue
         */
        case OBJECT_DefaultValue: {
            struct SEE_value res;
            Tcl_Obj *pRes;
            SEE_OBJECT_DEFAULTVALUE(pSeeInterp, p, 0, &res);
            pRes = primitiveValueToTcl(pTclSeeInterp, &res);
            Tcl_SetObjResult(pTclInterp, pRes);
            break;
        }

        case OBJECT_Enumerator: {
            break;
        }

        /*
         * Put PROPERTY VALUE
         */
        case OBJECT_Put: {
            struct SEE_value val;
            const char *zProp = Tcl_GetString(objv[1]);
            rc = objToValue(pTclSeeInterp, objv[2], &val);
            if (rc == TCL_OK) {
                SEE_OBJECT_PUTA(pSeeInterp, p, zProp, &val, 0);
            }
            break;
        }

        /*
         * Get PROPERTY
         */
        case OBJECT_Get: {
            struct SEE_value val;
            const char *zProp = Tcl_GetString(objv[1]);
            SEE_OBJECT_GETA(pSeeInterp, p, zProp, &val);

            if (objc > 2) {
                if (SEE_VALUE_GET_TYPE(&val) != SEE_OBJECT) {
                    Tcl_AppendResult(pTclInterp, "Bad object reference", 0);
                    rc = TCL_ERROR;
                } else {
                    Tcl_Obj **a = &objv[2];
                    struct SEE_object *pSee = val.u.object;
                    rc = objectCmd(pTclSeeInterp, pSee, objc - 2, a, apGet);
                }
            } else {
                if (SEE_VALUE_GET_TYPE(&val) == SEE_OBJECT) {
                    Tcl_Obj *pRef = Tcl_NewListObj((objv-apGet) + 2, apGet);
                    Tcl_Obj *pRes = Tcl_NewStringObj("object", 6);
                    Tcl_ListObjAppendElement(0, pRes, pRef);
                    Tcl_SetObjResult(pTclInterp, pRes);
                } else {
                    Tcl_SetObjResult(pTclInterp, 
                        primitiveValueToTcl(pTclSeeInterp, &val)
                    );
                }
            }

            break;
        }
    }

    return rc;
}

/*
 *---------------------------------------------------------------------------
 *
 * interpCmd --
 *
 *       $interp eval JAVASCRIPT
 *       $interp destroy
 *       $interp function JAVASCRIPT
 *       $interp native
 *       $interp global COMMAND
 * 
 *       $interp Get ID ?OBJECT-CMD...?
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
interpCmd(clientData, pTclInterp, objc, objv)
    ClientData clientData;             /* The SeeInterp data structure */
    Tcl_Interp *pTclInterp;            /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    int iChoice;
    SeeInterp *pTclSeeInterp = (SeeInterp *)clientData;
    struct SEE_interpreter *pSeeInterp = &pTclSeeInterp->interp;

    enum INTERP_enum {
        INTERP_EVAL,
        INTERP_DESTROY,
        INTERP_FUNCTION,
        INTERP_NATIVE,
        INTERP_GLOBAL,
        INTERP_TOSTRING,
        INTERP_Get,
        INTERP_EVENTTARGET,
    };

    static const struct InterpSubCommand {
        const char *zCommand;
        enum INTERP_enum eSymbol;
        int nMinArgs;
        int nMaxArgs;
        char *zArgs;
    } aSubCommand[] = {
        {"destroy",     INTERP_DESTROY,     0, 0,     ""},
        {"eval",        INTERP_EVAL,        1, 1,     "JAVASCRIPT"},
        {"function",    INTERP_FUNCTION,    1, 1,     "JAVASCRIPT"},
        {"global",      INTERP_GLOBAL,      1, 1,     "TCL-COMMAND"},
        {"native",      INTERP_NATIVE,      0, 0,     ""},
        {"tostring",    INTERP_TOSTRING,    1, 1,     ""},
        {"Get",         INTERP_Get,         1, 10000, "ID"},
        {"eventtarget", INTERP_EVENTTARGET, 0, 0, ""},
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
            int rc = interpEvalThis(pTclSeeInterp, pThis, pProgram);
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

            if (pTclSeeInterp->zGlobal) {
                Tcl_ResetResult(pTclInterp);
                Tcl_AppendResult(pTclInterp, "Can call [global] only once.", 0);
                return TCL_ERROR;
            }
            pWindow = findOrCreateObject(pTclSeeInterp, objv[2]);
            installHv3Global(pTclSeeInterp, pWindow);

            zGlobal = Tcl_GetStringFromObj(objv[2], &nGlobal);
            pTclSeeInterp->zGlobal = SEE_malloc_string(pSeeInterp, nGlobal + 1);
            strcpy(pTclSeeInterp->zGlobal, zGlobal);
            break;
        }

        /*
         * seeInterp destroy
         *
         */
        case INTERP_DESTROY: {
            Tcl_DeleteCommand(pTclInterp, Tcl_GetString(objv[0]));
            break;
        }

        /*
         * $interp native
         * $interp function JAVASCRIPT
         */
        case INTERP_FUNCTION: 
        case INTERP_NATIVE: {
            struct SEE_object *pObject;
            Tcl_Obj *pRes;
            int iKey;

            if (aSubCommand[iChoice].eSymbol == INTERP_NATIVE) {
                pObject = SEE_Object_new(pSeeInterp);
            } else {
                struct SEE_input *pInputCode;
                pInputCode = SEE_input_utf8(pSeeInterp, Tcl_GetString(objv[2]));
                pObject = SEE_Function_new(pSeeInterp, 0, 0, pInputCode);
                SEE_INPUT_CLOSE(pInputCode);
            }

            /* Create and return a reference */
            iKey = createObjectRef(pTclSeeInterp, pObject, 0);
            pRes = Tcl_NewObj();
            Tcl_ListObjAppendElement(0, pRes, Tcl_NewStringObj("Get", 3));
            Tcl_ListObjAppendElement(0, pRes, Tcl_NewIntObj(iKey));
            Tcl_SetObjResult(pTclInterp, pRes);

            break;
        }

        /*
         * $interp tostring VALUE
         */
        case INTERP_TOSTRING: {
            struct SEE_value val;
            struct SEE_value res;
            objToValue(pTclSeeInterp, objv[2], &val);
            SEE_ToString(pSeeInterp, &val, &res);
            Tcl_SetObjResult(pTclInterp, Tcl_NewUnicodeObj(
                res.u.string->data, res.u.string->length
            ));
            break;
        }

        /*
         * $interp eventtarget
         */
        case INTERP_EVENTTARGET: {
            return eventTargetNew(clientData, pTclInterp, objc, objv);
        }

        /*
         * $interp Get ID ?Object-Command...?
         */
        case INTERP_Get: {
            int iKey;
            struct SEE_object *pObject;

            /* Locate the entry in SeeInterp.aJsObject for $ID. Return
             * TCL_ERROR if no such object can be found.
             */
            if (TCL_OK != Tcl_GetIntFromObj(pTclInterp, objv[2], &iKey)) {
                return TCL_ERROR;
            }
            pObject = lookupObjectRef(pTclSeeInterp, iKey);
            if (!pObject) {
                return TCL_ERROR;
            }

            if (objc == 4 && 0 == strcmp("Finalize", Tcl_GetString(objv[3]))) {
                removeObjectRef(pTclSeeInterp, iKey);
            } else if (objc > 3) {
                Tcl_Obj **a = (Tcl_Obj **)&objv[3];
                return objectCmd(pTclSeeInterp, pObject, objc - 3, a, &objv[1]);
            } else {
                Tcl_SetObjResult(pTclInterp, Tcl_NewListObj(objc-1, &objv[1]));
            }
            break;
        }
    }

    return TCL_OK;
}

static int 
tclSeeInterp(clientData, interp, objc, objv)
    ClientData clientData;             /* Unused */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    char zCmd[64];
    SeeInterp *pInterp;

    sprintf(zCmd, "::see::interp_%d", iSeeInterp++);

    pInterp = (SeeInterp *)GC_MALLOC_UNCOLLECTABLE(sizeof(SeeInterp));
    memset(pInterp, 0, sizeof(SeeInterp));
 
    /* Initialize a new SEE interpreter */
    SEE_interpreter_init_compat(&pInterp->interp, 
        SEE_COMPAT_JS15|SEE_COMPAT_SGMLCOM
    );

    /* Initialise the pTclInterp and nRef fields. */
    pInterp->pTclInterp = interp;
    pInterp->nRef = 1;

    /* Allocate and initialise the SeeInterp.aTclObject[] table. */
    pInterp->aTclObject = (SeeTclObject **)SEE_malloc_string(
        &pInterp->interp, sizeof(SeeTclObject *) * OBJECT_HASH_SIZE
    );
    memset(pInterp->aTclObject, 0, sizeof(SeeTclObject *) * OBJECT_HASH_SIZE);

    Tcl_CreateObjCommand(interp, zCmd, interpCmd, pInterp, delInterpCmd);
    Tcl_SetResult(interp, zCmd, TCL_VOLATILE);
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
            argValueToTcl(pTclSeeInterp, pVal)
    );

    rc = Tcl_EvalObjEx(pTclInterp, pScript, TCL_GLOBAL_ONLY);
    removeTransientRefs(pTclSeeInterp);
    if (rc != TCL_OK) {
        SEE_error_throw_sys(pInterp, 
            pInterp->TypeError, "%s", Tcl_GetStringResult(pTclInterp)
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
    int ret = 0;

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

    return 0;
}

static void 
SeeTcl_DefaultValue(pInterp, pObj, pHint, pRes)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_value *pHint;
    struct SEE_value *pRes;
{
    SeeTclObject *pObject = (SeeTclObject *)pObj;
    SeeInterp *pTclSeeInterp = pObject->pTclSeeInterp;
    Tcl_Interp *pTclInterp = pTclSeeInterp->pTclInterp;
    Tcl_Obj *pScript = Tcl_DuplicateObj(pObject->pObj);
    int rc;

    Tcl_ListObjAppendElement(
        pTclInterp, pScript, Tcl_NewStringObj("DefaultValue", 12)
    );
    rc = Tcl_EvalObjEx(pTclInterp, pScript, TCL_GLOBAL_ONLY);

    if (rc == TCL_OK) {
        objToValue(pTclSeeInterp, Tcl_GetObjResult(pTclInterp), pRes);
    } else {
        struct SEE_string *pString;
        pString = SEE_string_sprintf(
            &pTclSeeInterp->interp, "%s", Tcl_GetString(pObject->pObj)
        );
        SEE_SET_STRING(pRes, pString);
    }
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
        Tcl_ListObjAppendElement(0, pScript, 
            argValueToTcl(pTclSeeInterp,argv[ii])
        );
    }

    rc = Tcl_EvalObjEx(pTclInterp, pScript, TCL_GLOBAL_ONLY);
    removeTransientRefs(pTclSeeInterp);
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

struct SEE_scope;
struct SEE_scope {
  struct SEE_scope *next;
  struct SEE_object *obj;
};

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

#define JSTOKEN_OPEN_BRACKET    1
#define JSTOKEN_CLOSE_BRACKET   2
#define JSTOKEN_OPEN_BRACE      3
#define JSTOKEN_CLOSE_BRACE     4
#define JSTOKEN_SEMICOLON       5
#define JSTOKEN_NEWLINE         6
#define JSTOKEN_SPACE           7

#define JSTOKEN_WORD            8
#define JSTOKEN_PUNC            9

static int
jsToken(zCode, ePrevToken, peToken)
    const char *zCode;     /* String to read token from */
    int ePrevToken;                 /* Previous token type */
    int *peToken;                   /* OUT: Token type */
{
    int nToken = 1;
    int eToken = 0;

    assert(*zCode);

    unsigned char aIsPunct[128] = {
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,     /* 0x00 - 0x0F */
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,     /* 0x10 - 0x1F */
        1, 1, 1, 1, 1, 1, 1, 1,    1, 1, 1, 1, 1, 1, 1, 1,     /* 0x20 - 0x2F */
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 1, 1, 1, 1, 1, 1,     /* 0x30 - 0x3F */
        1, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,     /* 0x40 - 0x4F */
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 1, 0, 1, 1, 0,     /* 0x50 - 0x5F */
        1, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,     /* 0x60 - 0x6F */
        0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 1, 1, 1, 1, 0,     /* 0x70 - 0x7F */
    };
    
    switch (*zCode) {
        case '(':  eToken = JSTOKEN_OPEN_BRACKET; break;
        case ')':  eToken = JSTOKEN_CLOSE_BRACKET; break;
        case '{':  eToken = JSTOKEN_OPEN_BRACE; break;
        case '}':  eToken = JSTOKEN_CLOSE_BRACE; break;
        case ';':  eToken = JSTOKEN_SEMICOLON; break;
        case '\n': eToken = JSTOKEN_NEWLINE; break;
        case ' ':  eToken = JSTOKEN_SPACE; break;

        case '/': {
            /* C++ comment */
            if (zCode[1] == '/') {
              eToken = JSTOKEN_WORD;
              while (zCode[nToken] && zCode[nToken] != '\n') nToken++;
              if (zCode[nToken]) nToken++;
              break;
            }

            /* C comment */
            if (zCode[1] == '*') {
              eToken = JSTOKEN_WORD;
              while (zCode[nToken]) {
                if (zCode[nToken] == '/' && zCode[nToken - 1] == '*') {
                    nToken++;
                    break;
                }
                nToken++;
              }
              break;
            }

            /* Division sign */
            if (
                ePrevToken == JSTOKEN_WORD || 
                ePrevToken == JSTOKEN_CLOSE_BRACKET
            ) {
                eToken = JSTOKEN_PUNC;
                break;
            }

            /* Regex literal (fall through) */
        }

        case '"':
        case '\'': {
          int ii;
          for (ii = 1; zCode[ii] && zCode[ii] != zCode[0]; ii++) {
            if (zCode[ii] == '\\' && zCode[ii + 1]) ii++;
          }
          eToken = JSTOKEN_WORD;
          nToken = ii + (zCode[ii] ? 1 : 0);
          break;
        }

        default: {
            char c = *zCode;
            if (c >= 0 && aIsPunct[(int)c]) {
                eToken = JSTOKEN_PUNC;
            } else {
                int ii = 1;
                for ( ; zCode[ii] > 0 && 0 == aIsPunct[(int)zCode[ii]]; ii++);
                eToken = JSTOKEN_WORD;
                nToken = ii;
            }
            break;
        }
    }

    assert(eToken != 0);
    *peToken = eToken;
    return nToken;
}

/*
 *---------------------------------------------------------------------------
 *
 * tclSeeFormat --
 *
 *         ::see::format JAVASCRIPT-CODE
 *
 * Results:
 *     Standard Tcl result.
 *
 * Side effects:
 *     None
 *
 *---------------------------------------------------------------------------
 */
static int 
tclSeeFormat(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    char *zCode;
    char *zEnd;
    int nCode;

    int eToken = JSTOKEN_SEMICOLON;
    Tcl_Obj *pRet;

    static const int INDENT_SIZE = 2;
    const int MAX_INDENT = 40;
    static const char zWhite[] = "                                        ";

    #define IGNORE_NONE 0
    #define IGNORE_SPACE 1
    #define IGNORE_NEWLINE 2
    int eIgnore = IGNORE_NONE;
    int iIndent = 0;
    int iBracket = 0;

    assert(strlen(zWhite) == MAX_INDENT);

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "JAVASCRIPT-CODE");
        return TCL_ERROR;
    }
    pRet = Tcl_NewObj();
    Tcl_IncrRefCount(pRet);

    zCode = Tcl_GetStringFromObj(objv[1], &nCode);
    zEnd = &zCode[nCode];
    while (zCode < zEnd) {
        int nToken;
 
        /* Read a token from the input */
        char *zToken = zCode;
        nToken = jsToken(zCode, eToken, &eToken);
        zCode += nToken;

        // printf("TOKEN: %.*s\n", nToken, zToken);

        switch (eToken) {
            case JSTOKEN_OPEN_BRACKET:  iBracket++; break;
            case JSTOKEN_CLOSE_BRACKET: iBracket--; break;

            case JSTOKEN_OPEN_BRACE:
                if (iBracket == 0) {
                  iIndent += INDENT_SIZE;
                  Tcl_AppendToObj(pRet, "{\n", 2);
                  Tcl_AppendToObj(pRet, zWhite, MIN(MAX_INDENT, iIndent));
                  eIgnore = IGNORE_NEWLINE;
                  zToken = 0;
                }
                break;

            case JSTOKEN_CLOSE_BRACE:
                if (iBracket == 0) {
                  iIndent -= INDENT_SIZE;
                  if (eIgnore == IGNORE_NONE) {
                      Tcl_AppendToObj(pRet, "\n", 1);
                      Tcl_AppendToObj(pRet, zWhite, MIN(MAX_INDENT, iIndent));
                  } 
                  Tcl_AppendToObj(pRet, "}\n", 2);
                  Tcl_AppendToObj(pRet, zWhite, MIN(MAX_INDENT, iIndent));
                  eIgnore = IGNORE_NEWLINE;
                  zToken = 0;
                }
                break;

            case JSTOKEN_SEMICOLON: 
                if (iBracket == 0) {
                  Tcl_AppendToObj(pRet, ";\n", 2);
                  Tcl_AppendToObj(pRet, zWhite, MIN(MAX_INDENT, iIndent));
                  eIgnore = IGNORE_NEWLINE;
                  zToken = 0;
                }
                break;

            case JSTOKEN_NEWLINE: 
                if (eIgnore == IGNORE_NEWLINE) {
                    eIgnore = IGNORE_SPACE;
                    zToken = 0;
                }
                break;

            case JSTOKEN_SPACE: 
                if (eIgnore != IGNORE_NONE) {
                    zToken = 0;
                }
                break;

            case JSTOKEN_WORD:
            case JSTOKEN_PUNC:
                eIgnore = IGNORE_NONE;
                break;

            default:
                assert(!"Bad token type");
        }

        if (zToken) {
            Tcl_AppendToObj(pRet, zToken, nToken);
        }
    }

    Tcl_SetObjResult(interp, pRet);
    Tcl_DecrRefCount(pRet);
    return TCL_OK;
}

int 
Tclsee_Init(interp)
    Tcl_Interp *interp;
{
    /* Require stubs libraries version 8.4 or greater. */
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.4", 0) == 0) {
        return TCL_ERROR;
    }
#endif

    Tcl_PkgProvide(interp, "Tclsee", "0.1");
    Tcl_CreateObjCommand(interp, "::see::interp", tclSeeInterp, 0, 0);
    Tcl_CreateObjCommand(interp, "::see::format", tclSeeFormat, 0, 0);
    return TCL_OK;
}


/*----------------------------------------------------------------------- 
 * Event target container object notes:
 *
 *     set container [$interp eventtarget]
 *
 *     $container addEventListener    TYPE LISTENER USE-CAPTURE
 *     $container removeEventListener TYPE LISTENER USE-CAPTURE
 *     $container setLegacyListener   TYPE LISTENER
 *     $container runEvent            TYPE USE-CAPTURE THIS JS-ARG
 *     $container destroy
 *
 * where:
 * 
 *     TYPE is an event type - e.g. "click", "load" etc. Case insensitive.
 *     LISTENER is a javascript object reference (to a callable object)
 *     USE-CAPTURE is a boolean.
 *     JS-ARG is a typed javascript value to pass to the event handlers
 *
 * Implementing the EventTarget interface (see DOM Level 2) is difficult
 * using the current ::see::interp interface. The following code implements
 * a data structure to help with this while storing javascript function
 * references in garbage collected memory (so that they are not collected
 * prematurely).
 *
 * This data structure allows for storage of zero or more more pointers
 * to javascript objects. For each event "type" either zero or one 
 * legacy event-listener and any number of normal event-listeners may
 * be stored. An event type is any string.
 *
 * Calling [destroy] exactly once for each call to [eventtarget] is 
 * mandatory. Otherwise -> memory leak. Also, all event-target containers
 * should be destroyed before the interpreter that created them. TODO:
 * there should be an assert() to check this when NDEBUG is not defined.
 *
 * Implementation is in two C-functions below:
 *
 *     eventTargetNew()
 *     eventTargetMethod()
 */
typedef struct EventTarget EventTarget;
typedef struct ListenerContainer ListenerContainer;
typedef struct EventType EventType;

static int iNextEventTarget = 0;

struct EventTarget {
  SeeInterp *pTclSeeInterp;
  EventType *pTypeList;
};

struct EventType {
  char *zType;
  ListenerContainer *pListenerList;
  struct SEE_object *pLegacyListener;
  EventType *pNext;
};

struct ListenerContainer {
  int isCapture;                  /* True if a capturing event */
  struct SEE_object *pListener;   /* Listener function */
  ListenerContainer *pNext;       /* Next listener on this event type */
};

static struct SEE_object *
eventTargetValue(pTclSeeInterp, pEventTarget, pEvent)
    SeeInterp *pTclSeeInterp;
    Tcl_Obj *pEventTarget;
    Tcl_Obj *pEvent;
{
    Tcl_Interp *interp = pTclSeeInterp->pTclInterp;
    Tcl_CmdInfo info;
    int rc;
    EventTarget *p;
    EventType *pType;

    rc = Tcl_GetCommandInfo(interp, Tcl_GetString(pEventTarget), &info);
    if (rc != TCL_OK) return 0;
    p = (EventTarget *)info.clientData;

    for (
        pType = p->pTypeList;
        pType && strcasecmp(Tcl_GetString(pEvent), pType->zType);
        pType = pType->pNext
    );

    return (pType ? pType->pLegacyListener : 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * eventTargetDelete --
 *
 *     Called to delete an EventTarget object.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Frees the EventTarget object passed as an argument.
 *
 *---------------------------------------------------------------------------
 */
static void 
eventTargetDelete(clientData)
    ClientData clientData;          /* Pointer to the EventTarget structure */
{
    EventTarget *p = (EventTarget *)clientData;
    GC_FREE(p);
}

/*
 *---------------------------------------------------------------------------
 *
 * eventTargetMethod --
 *
 *     $eventtarget addEventListener    TYPE LISTENER USE-CAPTURE
 *
 *     $eventtarget removeEventListener TYPE LISTENER USE-CAPTURE
 *
 *     $eventtarget setLegacyListener   TYPE LISTENER
 *
 *     $eventtarget runEvent            TYPE USE-CAPTURE THIS JS-ARG
 *
 *     $eventtarget destroy
 *         Destroy the event-target object. This is eqivalent to
 *         evaluating [rename $eventtarget ""].
 *
 * Results:
 *     TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *     Whatever the method does (see above).
 *
 *---------------------------------------------------------------------------
 */
static int 
eventTargetMethod(clientData, interp, objc, objv)
    ClientData clientData;          /* Pointer to the EventTarget structure */
    Tcl_Interp *interp;             /* Current Tcl interpreter. */
    int objc;                       /* Number of arguments. */
    Tcl_Obj *CONST objv[];          /* Argument strings. */
{
    EventTarget *p = (EventTarget *)clientData;
    struct SEE_interpreter *pSeeInterp = &(p->pTclSeeInterp->interp);

    EventType *pType = 0;
    struct SEE_object *pListener = 0;
    int iChoice;

    enum EVENTTARGET_enum {
        ET_ADD,
        ET_REMOVE,
        ET_LEGACY,
        ET_INLINE,
        ET_RUNEVENT,
        ET_DESTROY
    };
    enum EVENTTARGET_enum eChoice;

    static const struct EventTargetSubCommand {
        const char *zCommand;
        enum EVENTTARGET_enum eSymbol;
        int nArgs;
        char *zArgs;
    } aSubCommand[] = {
        {"addEventListener",    ET_ADD,      3, "TYPE LISTENER USE-CAPTURE"},
        {"removeEventListener", ET_REMOVE,   3, "TYPE LISTENER USE-CAPTURE"},
        {"setLegacyListener",   ET_LEGACY,   2, "TYPE LISTENER"},
        {"setLegacyScript",     ET_INLINE,   2, "TYPE JAVASCRIPT"},
        {"runEvent",            ET_RUNEVENT, 4, "TYPE CAPTURE THIS JS-ARG"},
        {"destroy",             ET_DESTROY,  0, ""},
        {0, 0, 0, 0}
    };

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObjStruct(interp, objv[1], aSubCommand, 
            sizeof(struct EventTargetSubCommand), "option", 0, &iChoice) 
    ){
        return TCL_ERROR;
    }

    if (objc != aSubCommand[iChoice].nArgs + 2) {
        Tcl_WrongNumArgs(interp, 2, objv, aSubCommand[iChoice].zArgs);
        return TCL_ERROR;
    }

    /* If this is an ADD, LEGACY, RUNEVENT or REMOVE operation, search
     * for an EventType that matches objv[2]. If it is an ADD or LEGACY,
     * create the EventType if it does not already exist. 
     */
    eChoice = aSubCommand[iChoice].eSymbol;
    if (
        eChoice == ET_REMOVE || eChoice == ET_RUNEVENT || 
        eChoice == ET_ADD || eChoice == ET_LEGACY || eChoice == ET_INLINE
    ) {
        for (
            pType = p->pTypeList;
            pType && strcasecmp(Tcl_GetString(objv[2]), pType->zType);
            pType = pType->pNext
        );
    }
    if (
        (!pType) && 
        (eChoice == ET_ADD || eChoice == ET_LEGACY || eChoice == ET_INLINE)
    ) {
        int nType;
        char *zType = Tcl_GetStringFromObj(objv[2], &nType);

        nType++;
        pType = (EventType *)SEE_malloc(pSeeInterp, sizeof(EventType) + nType);
        memset(pType, 0, sizeof(EventType));

        pType->zType = (char *)&pType[1];
        strcpy(pType->zType, zType);
        pType->pNext = p->pTypeList;
        p->pTypeList = pType;
    }

    /* If this is an ADD, LEGACY, REMOVE operation, convert objv[3]
     * (the LISTENER) into a SEE_object pointer.
     */
    if (eChoice == ET_ADD || eChoice == ET_LEGACY || eChoice == ET_REMOVE) {
        pListener = findOrCreateObject(p->pTclSeeInterp, objv[3]);
    }
    if (eChoice == ET_INLINE) {
        struct SEE_input *pInputCode;
        pInputCode = SEE_input_utf8(pSeeInterp, Tcl_GetString(objv[3]));
        pListener = SEE_Function_new(pSeeInterp, 0, 0, pInputCode);
        SEE_INPUT_CLOSE(pInputCode);
    }

    Tcl_ResetResult(interp);

    switch (eChoice) {
        case ET_ADD: {
            ListenerContainer *pListenerContainer;
            int isCapture;
            if (TCL_OK != Tcl_GetBooleanFromObj(interp, objv[4], &isCapture)) {
                return TCL_ERROR;
            }
            isCapture = (isCapture ? 1 : 0);

            pListenerContainer = SEE_NEW(pSeeInterp, ListenerContainer);
            pListenerContainer->isCapture = isCapture;
            pListenerContainer->pListener = pListener;
            pListenerContainer->pNext = pType->pListenerList;
            pType->pListenerList = pListenerContainer;

            break;
        }

        case ET_REMOVE: {
            ListenerContainer **ppListenerContainer;
            int isCapture;
            if (TCL_OK != Tcl_GetBooleanFromObj(interp, objv[4], &isCapture)) {
                return TCL_ERROR;
            }
            isCapture = (isCapture ? 1 : 0);

            ppListenerContainer = &pType->pListenerList;
            while (*ppListenerContainer) {
                ListenerContainer *pL = *ppListenerContainer;
                if (pL->isCapture==isCapture && pL->pListener==pListener) {
                    *ppListenerContainer = pL->pNext;
                } else {
                    ppListenerContainer = &pL->pNext;
                }
            }

            break;
        }

        case ET_INLINE:
        case ET_LEGACY:
            pType->pLegacyListener = pListener;
            break;


        /*
         * $eventtarget runEvent TYPE IS-CAPTURE THIS EVENT
         */
        case ET_RUNEVENT: {
            ListenerContainer *pL;

            /* The result of this Tcl command */
            Tcl_Obj *pRes = 0;

            struct SEE_object *pThis;
            struct SEE_object *pLegacy = pType->pLegacyListener;

            /* The event object passed as an argument */
            struct SEE_object *pArg;
            struct SEE_value sArgValue;
            struct SEE_value *pArgValue;

            int isCapture;
            if (TCL_OK != Tcl_GetBooleanFromObj(interp, objv[3], &isCapture)) {
                return TCL_ERROR;
            }
            isCapture = (isCapture ? 1 : 0);

            pThis = findOrCreateObject(p->pTclSeeInterp, objv[4]);
            pArg = findOrCreateObject(p->pTclSeeInterp, objv[5]);
            SEE_SET_OBJECT(&sArgValue, pArg);
            pArgValue = &sArgValue;

            for (pL = pType->pListenerList; pL; pL = pL->pNext) {
                if (isCapture == pL->isCapture) {
                    struct SEE_value res;
                    struct SEE_object *p2 = pL->pListener;
                    SEE_OBJECT_CALL(pSeeInterp, p2, pThis, 1, &pArgValue, &res);
                }
            }

            if (!isCapture && pLegacy) {
                struct SEE_value res;
/* TODO: Pass the correct "this" object */
SEE_OBJECT_CALL(pSeeInterp, pLegacy, pThis, 1, &pArgValue, &res);
                // SEE_OBJECT_CALL(pSeeInterp, pLegacy, 0, 1, &pArgValue,&res);
                switch (SEE_VALUE_GET_TYPE(&res)) {
                    case SEE_BOOLEAN:
                        pRes = Tcl_NewBooleanObj(res.u.boolean);
                        break;

                    case SEE_NUMBER:
                        pRes = Tcl_NewBooleanObj((int)res.u.number);
                        break;

                    default:
                        break;
                }
            }

            /* Note: The SEE_OBJECT_CALL() above may end up executing
             * Tcl code in our main interpreter. Therefore it is important
             * to set the command result here, after SEE_OBJECT_CALL().
             *
             * This was causing a bug earlier.
             */
            if (!pRes) pRes = Tcl_NewBooleanObj(1);
            Tcl_SetObjResult(interp, pRes);
            break;
        }

        case ET_DESTROY:
            eventTargetDelete(clientData);
            break;

        default: assert(!"Can't happen");
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * eventTargetNew --
 *
 *         $see_interp eventtarget
 *
 *     Create a new event-target container.
 *
 * Results:
 *     Standard Tcl result.
 *
 * Side effects:
 *     None
 *
 *---------------------------------------------------------------------------
 */
static int 
eventTargetNew(clientData, interp, objc, objv)
    ClientData clientData;             /* Pointer to the SeeInterp structure */
    Tcl_Interp *interp;                /* Current Tcl interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    ClientData c;
    EventTarget *pNew;
    char zCmd[64];

    pNew = (EventTarget *)GC_MALLOC_UNCOLLECTABLE(sizeof(EventTarget));
    assert(pNew);
    memset(pNew, 0, sizeof(EventTarget));

    pNew->pTclSeeInterp = (SeeInterp *)clientData;

    sprintf(zCmd, "::see::et%d", iNextEventTarget++);
    c = (ClientData)pNew;
    Tcl_CreateObjCommand(interp, zCmd, eventTargetMethod, c, eventTargetDelete);
   
    Tcl_SetResult(interp, zCmd, TCL_VOLATILE);
    return TCL_OK;
}




