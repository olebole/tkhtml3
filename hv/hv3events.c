
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
    if (rc == 0) return 0;
    p = (EventTarget *)info.objClientData;

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

static Tcl_Obj *
listenerToString(pSeeInterp, pListener)
    struct SEE_interpreter *pSeeInterp;
    struct SEE_object *pListener;
{
    struct SEE_value val;
    struct SEE_value res;

    SEE_OBJECT_DEFAULTVALUE(pSeeInterp, pListener, 0, &val);
    SEE_ToString(pSeeInterp, &val, &res);
    return stringToObj(res.u.string);
}

/*
 *---------------------------------------------------------------------------
 *
 * eventTargetDump --
 *
 *     This function is used to introspect the event-target object from
 *     the Tcl level. The return value is a list. Each element of
 *     the list takes the following form:
 *
 *       {EVENT-TYPE LISTENER-TYPE JAVASCRIPT}
 *
 *     where EVENT-TYPE is the event-type string passed to [addEventListener]
 *     or [setLegacyListener]. LISTENER-TYPE is one of "legacy", "capturing"
 *     or "non-capturing". JAVASCRIPT is the "tostring" version of the
 *     js object to call to process the event.
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
eventTargetDump(interp, p)
    Tcl_Interp *interp;
    EventTarget *p;
{
    EventType *pType;
    Tcl_Obj *apRow[3];
    Tcl_Obj *pRet;
    struct SEE_interpreter *pSeeInterp = &(p->pTclSeeInterp->interp);

    pRet = Tcl_NewObj();
    Tcl_IncrRefCount(pRet);

    for (pType = p->pTypeList; pType; pType = pType->pNext) {
        ListenerContainer *pL;

        Tcl_Obj *pEventType = Tcl_NewStringObj(pType->zType, -1);
        Tcl_IncrRefCount(pEventType);
        apRow[0] = pEventType;

        if (pType->pLegacyListener) {
            apRow[1] = Tcl_NewStringObj("legacy", -1);
            apRow[2] = listenerToString(pSeeInterp, pType->pLegacyListener);
            Tcl_ListObjAppendElement(interp, pRet, Tcl_NewListObj(3, apRow));
        }

        for (pL = pType->pListenerList; pL; pL = pL->pNext) {
            const char *zType = (pL->isCapture?"capturing":"non-capturing");
            apRow[1] = Tcl_NewStringObj(zType, -1);
            apRow[2] = listenerToString(pSeeInterp, pL->pListener);
            Tcl_ListObjAppendElement(interp, pRet, Tcl_NewListObj(3, apRow));
        }

        Tcl_DecrRefCount(pEventType);
    }
    

    Tcl_SetObjResult(interp, pRet);
    Tcl_DecrRefCount(pRet);
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
 *     $eventtarget removeLegacyListener TYPE
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
        ET_DESTROY,
        ET_DUMP,
        ET_REMOVE,
        ET_REMLEGACY,
        ET_RUNEVENT,
        ET_LEGACY,
        ET_INLINE
    };
    enum EVENTTARGET_enum eChoice;

    static const struct EventTargetSubCommand {
        const char *zCommand;
        enum EVENTTARGET_enum eSymbol;
        int nArgs;
        char *zArgs;
    } aSubCommand[] = {
        {"addEventListener",     ET_ADD,       3, "TYPE LISTENER USE-CAPTURE"},
        {"destroy",              ET_DESTROY,   0, ""},
        {"dump",                 ET_DUMP,      0, ""},
        {"removeEventListener",  ET_REMOVE,    3, "TYPE LISTENER USE-CAPTURE"},
        {"removeLegacyListener", ET_REMLEGACY, 1, "TYPE"},
        {"runEvent",             ET_RUNEVENT,  4, "TYPE CAPTURE THIS JS-ARG"},
        {"setLegacyListener",    ET_LEGACY,    2, "TYPE LISTENER"},
        {"setLegacyScript",      ET_INLINE,    2, "TYPE JAVASCRIPT"},
        {0, 0, 0, 0}
    };

    int rc = TCL_OK;

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

    /* If this is an ADD, LEGACY, INLINE, RUNEVENT or REMOVE operation, 
     * search for an EventType that matches objv[2]. If it is an ADD, 
     * LEGACY or INLINE, create the EventType if it does not already exist. 
     */
    eChoice = aSubCommand[iChoice].eSymbol;
    if (
        eChoice == ET_REMOVE || eChoice == ET_RUNEVENT || 
        eChoice == ET_ADD || eChoice == ET_LEGACY || eChoice == ET_INLINE ||
        eChoice == ET_REMLEGACY
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
        pListener = findOrCreateObject(p->pTclSeeInterp, objv[3], 0);
    }
    if (eChoice == ET_INLINE) {
        struct SEE_input *pInputCode;
        struct SEE_input *pInputParam;
        pInputCode = SEE_input_utf8(pSeeInterp, Tcl_GetString(objv[3]));
        pInputParam = SEE_input_utf8(pSeeInterp, "event");
        pListener = SEE_Function_new(pSeeInterp, 0, pInputParam, pInputCode);
        SEE_INPUT_CLOSE(pInputCode);
        SEE_INPUT_CLOSE(pInputParam);
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

            if (!pType) break;
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
        case ET_REMLEGACY:
            assert(pType || (!pListener && eChoice == ET_REMLEGACY));
            if (pType) {
                pType->pLegacyListener = pListener;
            }
            break;

        /*
         * $eventtarget runEvent TYPE IS-CAPTURE THIS EVENT
         * 
         *     The return value of this command is one of the following:
         *
         *         "prevent" - A legacy handler returned false, indicating
         *                     that the default browser action should
         *                     be cancelled.
         *
         *         "ok"      - Event handlers were run.
         *
         *         ""        - There were no event handlers to run.
         *
         *     If an error or unhandled exception occurs in the javascript,
         *     a Tcl exception is thrown.
         */
        case ET_RUNEVENT: {
            ListenerContainer *pL;

            /* The result of this Tcl command. eRes is set to an 
             * index into azRes. 
             */
            char const *azRes[] = {"", "ok", "prevent"};
            int eRes = 0;

	    /* TODO: Try-catch context to execute the javascript callbacks in.
             * At the moment only a single try-catch is used for all
             * W3C and legacy listeners. So if one throws an exception
             * the rest will not run. This is probably wrong, but fixing
	     * it means figuring out some way to return the error information
	     * to the browser. i.e. the current algorithm is:
             *
             *     TRY {
             *         FOREACH (W3C listener) { ... }
             *         IF (legacy listener) { ... }
             *     } CATCH (...) { ... }
             *
             */
            SEE_try_context_t try_ctxt;

            struct SEE_object *pThis;

            /* The event object passed as an argument */
            struct SEE_object *pArg;
            struct SEE_value sArgValue;
            struct SEE_value *pArgV;

            int isCapture;
            if (TCL_OK != Tcl_GetBooleanFromObj(interp, objv[3], &isCapture)) {
                return TCL_ERROR;
            }
            isCapture = (isCapture ? 1 : 0);

            pThis = findOrCreateObject(p->pTclSeeInterp, objv[4], 0);
            pArg = findOrCreateObject(p->pTclSeeInterp, objv[5], 0);
            SEE_SET_OBJECT(&sArgValue, pArg);
            pArgV= &sArgValue;

            if (pType) {
                SEE_TRY (pSeeInterp, try_ctxt) {
                    struct SEE_object *pLegacy = pType->pLegacyListener;
    
                    for (pL = pType->pListenerList; pL; pL = pL->pNext) {
                        if (isCapture == pL->isCapture) {
                            struct SEE_value res;
                            struct SEE_object *p2 = pL->pListener;
                            SEE_OBJECT_CALL(
                                pSeeInterp, p2, pThis, 1, &pArgV, &res
                            );
                            eRes = MAX(eRes, 1);
                        }
                    }
        
                    if (!isCapture && pLegacy) {
                        struct SEE_value r;
                        SEE_OBJECT_CALL(
                            pSeeInterp, pLegacy, pThis, 1, &pArgV, &r
                        );
                        eRes = MAX(eRes, 1);
                        switch (SEE_VALUE_GET_TYPE(&r)) {
                            case SEE_BOOLEAN:
                                if (0 == r.u.boolean) eRes = MAX(eRes, 2);
                                break;
        
                            case SEE_NUMBER:
                                if (0 == ((int)r.u.number)) eRes = MAX(eRes, 2);
                                break;
        
                            default:
                                break;
                        }
                    }
                }
            }

            if (pType && SEE_CAUGHT(try_ctxt)) {
                rc = handleJavascriptError(p->pTclSeeInterp, &try_ctxt);
            } else {
                /* Note: The SEE_OBJECT_CALL() above may end up executing
                 * Tcl code in our main interpreter. Therefore it is important
                 * to set the command result here, after SEE_OBJECT_CALL().
                 *
                 * This was causing a bug earlier.
                 */
                Tcl_Obj *pRes = Tcl_NewStringObj(azRes[eRes], -1);
                Tcl_SetObjResult(interp, pRes);
            }
            break;
        }

        case ET_DESTROY:
            Tcl_DeleteCommand(interp, Tcl_GetString(objv[0]));
            break;

        case ET_DUMP:
            eventTargetDump(interp, p);
            break;

        default: assert(!"Can't happen");
    }

    return rc;
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
