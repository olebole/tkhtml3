
typedef struct BridgeObject BridgeObject;
typedef struct BridgeEnum BridgeEnum;

struct BridgeObject {
  struct SEE_object object;
  struct SEE_interpreter *i;
  struct SEE_object *pObj;
};

struct BridgeEnum {
  struct SEE_enum base;
  struct SEE_interpreter *i;
  struct SEE_enum *pEnum;
};

static struct SEE_objectclass *getBridgeVtbl();
static struct SEE_enumclass *getBridgeEnumVtbl();

/*
 *---------------------------------------------------------------------------
 *
 * createBridgeObject --
 *
 *   This is the only public interface in this file. Return a pointer to
 *   a wrapper (bridge) object that can be used in interpreter pInterp
 *   to access an object pForiegnObj that was created in interpreter
 *   pForiegnInterp.
 *
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
struct SEE_object *
createBridgeObject(pInterp, pForiegnInterp, pForiegnObj)
    struct SEE_interpreter *pInterp;
    struct SEE_interpreter *pForiegnInterp;
    struct SEE_object *pForiegnObj;
{
    BridgeObject *p;
    if (!pForiegnObj || pForiegnObj->objectclass==getBridgeVtbl()) {
        return pForiegnObj;
    }
    p = SEE_NEW(pInterp, BridgeObject);
    memset(p, 0, sizeof(BridgeObject));
    p->object.Prototype = 0;
    p->object.objectclass = getBridgeVtbl();
    p->i = pForiegnInterp;
    p->pObj = pForiegnObj;
    return (struct SEE_object *)p;
}

static void
copyBridgeValue(pInterp, pValue, pForiegnInterp, pForiegnValue)
    struct SEE_interpreter *pInterp;              /* Source interpreter */
    struct SEE_value *pValue;                     /* Destination value */
    struct SEE_interpreter *pForiegnInterp;       /* Source interpreter */
    struct SEE_value *pForiegnValue;              /* Source value */
{
    int eType = SEE_VALUE_GET_TYPE(pForiegnValue);

    if( eType==SEE_OBJECT ){
        struct SEE_object *o = pForiegnValue->u.object;
        SEE_SET_OBJECT(pValue, createBridgeObject(pInterp, pForiegnInterp, o));
        assert(SEE_VALUE_GET_TYPE(pValue) == eType);
    }else{
        SEE_VALUE_COPY(pValue, pForiegnValue);
    }
}

static void
handleBridgeException(pInterp, pForiegnInterp, pForiegnTry)
    struct SEE_interpreter *pInterp;
    struct SEE_interpreter *pForiegnInterp;
    SEE_try_context_t *pForiegnTry;
{
    struct SEE_value *pForiegnVal = SEE_CAUGHT(*pForiegnTry);
    if (pForiegnVal) {
        struct SEE_value exception;
        struct SEE_traceback *pTrace;
#if 1
printf("throw: ");
SEE_PrintValue(pForiegnInterp, pForiegnVal, stdout);
printf("\n");
SEE_ToString(pForiegnInterp, pForiegnVal, &exception);
SEE_PrintValue(pForiegnInterp, &exception, stdout);
printf("\n");
#endif
#if 0
        for (pTrace = pForiegnTry->traceback; pTrace; pTrace = pTrace->prev) {
            if (!pTrace->prev) {
                pTrace->prev = pInterp->traceback;
                pInterp->traceback = pForiegnTry->traceback;
            }
        }
#endif
        copyBridgeValue(pInterp, &exception, pForiegnInterp, pForiegnVal);
        pInterp->try_location = pForiegnInterp->try_location;
        SEE_THROW(pInterp, &exception);
    }
}


static void 
Bridge_Get(pInterp, pObj, pProp, pRes)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_string *pProp;
    struct SEE_value *pRes;
{
    BridgeObject *p = (BridgeObject *)pObj;
    if (p->i == pInterp) {
        SEE_OBJECT_GET(pInterp, p->pObj, pProp, pRes);
        return;
    } else {
        SEE_try_context_t try_ctxt;
        struct SEE_value val;
        pProp = SEE_intern(p->i, pProp);
        SEE_TRY(p->i, try_ctxt) {
            SEE_OBJECT_GET(p->i, p->pObj, pProp, &val);
            copyBridgeValue(pInterp, pRes, p->i, &val);
        }
        handleBridgeException(pInterp, p->i, &try_ctxt);
    }
}

static void 
Bridge_Put(pInterp, pObj, pProp, pValue, flags)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_string *pProp;
    struct SEE_value *pValue;
    int flags;
{
    BridgeObject *p = (BridgeObject *)pObj;
    if (p->i == pInterp) {
        SEE_OBJECT_PUT(pInterp, p->pObj, pProp, pValue, flags);
        return;
    } else {
        SEE_try_context_t try_ctxt;
        struct SEE_value val;
        pProp = SEE_intern(p->i, pProp);
        copyBridgeValue(p->i, &val, pInterp, pValue);

        SEE_TRY(p->i, try_ctxt) {
            SEE_OBJECT_PUT(p->i, p->pObj, pProp, &val, flags);
        }
        handleBridgeException(pInterp, p->i, &try_ctxt);
    }
}

static int 
Bridge_CanPut(pInterp, pObj, pProp)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_string *pProp;
{
    BridgeObject *p = (BridgeObject *)pObj;
    pProp = SEE_intern(p->i, pProp);
    return SEE_OBJECT_CANPUT(p->i, p->pObj, pProp);
}

static int 
Bridge_HasProperty(pInterp, pObj, pProp)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_string *pProp;
{
    BridgeObject *p = (BridgeObject *)pObj;
    pProp = SEE_intern(p->i, pProp);
    return SEE_OBJECT_HASPROPERTY(p->i, p->pObj, pProp);
}

static int 
Bridge_Delete(pInterp, pObj, pProp)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_string *pProp;
{
    BridgeObject *p = (BridgeObject *)pObj;
    if (p->i == pInterp) {
        return SEE_OBJECT_DELETE(pInterp, p->pObj, pProp);
    } else {
        int ret;
        SEE_try_context_t try_ctxt;
        pProp = SEE_intern(p->i, pProp);
        SEE_TRY(p->i, try_ctxt) {
            ret = SEE_OBJECT_DELETE(p->i, p->pObj, pProp);
        }
        handleBridgeException(pInterp, p->i, &try_ctxt);
        return ret;
    }
}

static void 
Bridge_DefaultValue(pInterp, pObj, pHint, pRes)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_value *pHint;
    struct SEE_value *pRes;
{
    BridgeObject *p = (BridgeObject *)pObj;
    struct SEE_value val;
    SEE_OBJECT_DEFAULTVALUE(p->i, p->pObj, pHint, &val);
    copyBridgeValue(pInterp, pRes, p->i, &val);
}

static struct SEE_enum *
Bridge_Enumerator(pInterp, pObj)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
{
    BridgeObject *p = (BridgeObject *)pObj;
    BridgeEnum *pEnum;

    pEnum = SEE_NEW(pInterp, BridgeEnum);
    pEnum->base.enumclass = getBridgeEnumVtbl();
    pEnum->i = p->i;
    pEnum->pEnum = SEE_OBJECT_ENUMERATOR(p->i, p->pObj);

    return (struct SEE_enum *)pEnum;
}

static void 
bridgeCallOrConstruct(pInterp, pObj, pThis, argc, argv, pRes, isConstruct)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_object *pThis;
    int argc;
    struct SEE_value **argv;
    struct SEE_value *pRes;
    int isConstruct;
{
    BridgeObject *p = (BridgeObject *)pObj;

    if (p->i==pInterp) {
        if (isConstruct) {
            SEE_OBJECT_CONSTRUCT(pInterp, p->pObj, pThis, argc, argv, pRes);
        } else {
            SEE_OBJECT_CALL(pInterp, p->pObj, pThis, argc, argv, pRes);
        }
        return;
    }else{
        SEE_try_context_t try_ctxt;
        struct SEE_value **apValue;
        struct SEE_value *aValue;
        struct SEE_value val;

        int nByte;
        int i;
        nByte = (sizeof(struct SEE_value) + sizeof(struct SEE_value *)) * argc;
        aValue = SEE_malloc(pInterp, nByte);
        apValue = (struct SEE_value **)&aValue[argc];
        for(i = 0; i < argc; i++){
            copyBridgeValue(p->i, &aValue[i], pInterp, argv[i]);
            apValue[i] = &aValue[i];
        }
        pThis = createBridgeObject(p->i, pInterp, pThis);

        SEE_TRY(p->i, try_ctxt) {
            if (isConstruct) {
                SEE_OBJECT_CONSTRUCT(p->i, p->pObj, pThis, argc, apValue, &val);
            } else {
                SEE_OBJECT_CALL(p->i, p->pObj, pThis, argc, apValue, &val);
            }
	    copyBridgeValue(pInterp, pRes, p->i, &val);
        }

        handleBridgeException(pInterp, p->i, &try_ctxt);
    }
}

static void 
Bridge_Construct(pInterp, pObj, pThis, argc, argv, pRes)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_object *pThis;
    int argc;
    struct SEE_value **argv;
    struct SEE_value *pRes;
{
    return bridgeCallOrConstruct(pInterp, pObj, pThis, argc, argv, pRes, 1);
}

static void 
Bridge_Call(pInterp, pObj, pThis, argc, argv, pRes)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_object *pThis;
    int argc;
    struct SEE_value **argv;
    struct SEE_value *pRes;
{
    return bridgeCallOrConstruct(pInterp, pObj, pThis, argc, argv, pRes, 0);
}

static int 
Bridge_HasInstance(pInterp, pObj, pInstance)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_value *pInstance;
{
    BridgeObject *p = (BridgeObject *)pObj;
    struct SEE_value val;
    copyBridgeValue(p->i, &val, pInterp, pInstance);
    return SEE_OBJECT_HASINSTANCE(p->i, p->pObj, &val);
}

static void *
Bridge_GetSecDomain(pInterp, pObj)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
{
    BridgeObject *p = (BridgeObject *)pObj;
    return SEE_OBJECT_GET_SEC_DOMAIN(p->i, p->pObj);
}

static struct SEE_string *
BridgeEnum_Next(pSeeInterp, pEnum, pFlags)
    struct SEE_interpreter *pSeeInterp;
    struct SEE_enum *pEnum;
    int *pFlags;
{
    BridgeEnum *p = (BridgeEnum *)pEnum;
    return SEE_ENUM_NEXT(p->i, p->pEnum, pFlags);
}

static struct SEE_objectclass BridgeObjectVtbl = {
    "Bridge",
    Bridge_Get,
    Bridge_Put,
    Bridge_CanPut,
    Bridge_HasProperty,
    Bridge_Delete,
    Bridge_DefaultValue,
    Bridge_Enumerator,
    Bridge_Construct,
    Bridge_Call,
    Bridge_HasInstance,
    Bridge_GetSecDomain
};
static struct SEE_objectclass *getBridgeVtbl() {
    return &BridgeObjectVtbl;
}

static struct SEE_enumclass BridgeEnumVtbl = {
  0,  /* Unused */
  BridgeEnum_Next
};

static struct SEE_enumclass *getBridgeEnumVtbl() {
    return &BridgeEnumVtbl;
}

