
#include "css.h"
#include "cssInt.h"

#include <tcl.h>
#include <string.h>
#include <assert.h>

#define TRACE_PARSER_CALLS 0
#define TRACE_STYLE_APPLICATION 0

/*
 *---------------------------------------------------------------------------
 *
 * constantToString --
 *
 *     Transform an integer constant to it's string representation (for
 *     debugging). All of the constants that start with CSS_* are
 *     supported.
 *
 * Results:
 *     Pointer to a static string.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static const char *constantToString(int c){
    switch( c ){
        case CSS_SELECTORCHAIN_DESCENDANT: 
            return "CSS_SELECTORCHAIN_DESCENDANT";
        case CSS_SELECTORCHAIN_CHILD: 
            return "CSS_SELECTORCHAIN_CHILD";
        case CSS_SELECTORCHAIN_ADJACENT: 
            return "CSS_SELECTORCHAIN_ADJACENT";
        case CSS_SELECTOR_UNIVERSAL: 
            return "CSS_SELECTOR_UNIVERSAL";
        case CSS_SELECTOR_TYPE: 
            return "CSS_SELECTOR_TYPE";
        case CSS_SELECTOR_ATTR: 
            return "CSS_SELECTOR_ATTR";
        case CSS_SELECTOR_ATTRVALUE: 
            return "CSS_SELECTOR_ATTRVALUE";
        case CSS_SELECTOR_ATTRLISTVALUE: 
            return "CSS_SELECTOR_ATTRLISTVALUE";
        case CSS_SELECTOR_ATTRHYPHEN: 
            return "CSS_SELECTOR_ATTRHYPHEN";
        case CSS_PSEUDOCLASS_LANG: 
            return "CSS_PSEUDOCLASS_LANG";
        case CSS_PSEUDOCLASS_FIRSTCHILD: 
            return "CSS_PSEUDOCLASS_FIRSTCHILD";
        case CSS_PSEUDOCLASS_LINK:  
            return "CSS_PSEUDOCLASS_LINK";
        case CSS_PSEUDOCLASS_UNVISITED: 
            return "CSS_PSEUDOCLASS_UNVISITED";
        case CSS_PSEUDOCLASS_ACTIVE:  
            return "CSS_PSEUDOCLASS_ACTIVE";
        case CSS_PSEUDOCLASS_HOVER: 
            return "CSS_PSEUDOCLASS_HOVER";
        case CSS_PSEUDOCLASS_FOCUS: 
            return "CSS_PSEUDOCLASS_FOCUS";
        case CSS_PSEUDOELEMENT_FIRSTLINE: 
            return "CSS_PSEUDOELEMENT_FIRSTLINE";
        case CSS_PSEUDOELEMENT_FIRSTLETTER: 
            return "CSS_PSEUDOELEMENT_FIRSTLETTER";
        case CSS_PSEUDOELEMENT_BEFORE: 
            return "CSS_PSEUDOELEMENT_BEFORE";
        case CSS_PSEUDOELEMENT_AFTER: 
            return "CSS_PSEUDOELEMENT_AFTER";
        case CSS_MEDIA_ALL: 
            return "CSS_MEDIA_ALL";
        case CSS_MEDIA_AURAL: 
            return "CSS_MEDIA_AURAL";
        case CSS_MEDIA_BRAILLE: 
            return "CSS_MEDIA_BRAILLE";
        case CSS_MEDIA_EMBOSSED: 
            return "CSS_MEDIA_EMBOSSED";
        case CSS_MEDIA_HANDHELD: 
            return "CSS_MEDIA_HANDHELD";
        case CSS_MEDIA_PRINT: 
            return "CSS_MEDIA_PRINT";
        case CSS_MEDIA_PROJECTION: 
            return "CSS_MEDIA_PROJECTION";
        case CSS_MEDIA_SCREEN: 
            return "CSS_MEDIA_SCREEN";
        case CSS_MEDIA_TTY: 
            return "CSS_MEDIA_TTY";
        case CSS_MEDIA_TV: 
            return "CSS_MEDIA_TV";
    }
    return "unknown";
}

/*--------------------------------------------------------------------------
 *
 * tokenToString --
 *
 *     This function returns a null-terminated string (allocated by ckalloc)
 *     populated with the contents of the supplied token.
 *
 * Results:
 *     Null-terminated string. Caller is responsible for calling ckfree()
 *     on it.
 *
 * Side effects:
 *     None.
 *
 *--------------------------------------------------------------------------
 */
static char *tokenToString(CssToken *pToken){
    char *zRet;
    if( !pToken || pToken->n<=0 ){
         return 0;
    }
    zRet = (char *)ckalloc(pToken->n+1);
    memcpy(zRet, pToken->z, pToken->n);
    zRet[pToken->n] = '\0';
    return zRet;
}

/*--------------------------------------------------------------------------
 *
 * PROPERTY_MASK_SET --
 * PROPERTY_MASK_GET --
 * PROPERTY_MASK_CNT --
 *
 *     Macros to deal with property set masks.
 *
 * Results:
 *
 * Side effects:
 *     None.
 *
 *--------------------------------------------------------------------------
 */
#define PROPERTY_MASK_SET(p,i) ((p)->a[(i>>5)&0x00000003]|=(1<<(i&0x0000001F)))
#define PROPERTY_MASK_GET(p,i) ((p)->a[(i>>5)&0x00000003]&(1<<(i&0x0000001F)))
#define PROPERTY_MASK_CNT(p,i) propertyMaskCnt(p, i)
static int propertyMaskCnt(p, i)
    CssPropertyMask *p;
    int i;
{
    int j;
    int r = 0;
    for(j=0; j<i; j++){
        if( PROPERTY_MASK_GET(p, j) ) r++;
    }
    return r;
}


/*--------------------------------------------------------------------------
 *
 * propertySetNew --
 *
 *     Allocate a new (empty) property set. The caller should eventually
 *     delete the property set using propertySetFree().
 *
 * Results:
 *     An empty property set.
 *
 * Side effects:
 *     None.
 *
 *--------------------------------------------------------------------------
 */
static CssPropertySet *propertySetNew(){
    CssPropertySet *p = (CssPropertySet *)ckalloc(sizeof(CssPropertySet));
    if( p ){
        memset(p, 0, sizeof(CssPropertySet));
    }
    return p;
}

/*--------------------------------------------------------------------------
 *
 * propertySetGet --
 *
 *     Retrieve CSS property 'i' if present in the property-set. 
 *
 * Results:
 *
 *     Return NULL if the property is not present, or a pointer to it's 
 *     string value if it is.
 *
 * Side effects:
 *     None.
 *
 *--------------------------------------------------------------------------
 */
static const char *propertySetGet(p, i)
    CssPropertySet *p;         /* Property set */
    int i;                     /* Property id (i.e CSS_PROPERTY_WIDTH) */
{
    char const *zRet = 0;
    assert( i<128 && i>=0 );
    if( PROPERTY_MASK_GET(&p->mask, i) ){
        zRet = p->aProp[PROPERTY_MASK_CNT(&p->mask, i)];
    }
    return zRet;
}

/*--------------------------------------------------------------------------
 *
 * propertySetAdd --
 *
 *     Insert or replace a value into a property set.
 *
 * Results:
 *
 *     Return NULL if the property is not present, or a pointer to it's 
 *     string value if it is.
 *
 * Side effects:
 *     None.
 *
 *--------------------------------------------------------------------------
 */
static void propertySetAdd(p, i, v)
    CssPropertySet *p;         /* Property set. */
    int i;                     /* Property id (i.e CSS_PROPERTY_WIDTH). */
    CssToken *v;               /* Value for property. */
{
    int n;
    assert( i<128 && i>=0 );

    n = PROPERTY_MASK_CNT(&p->mask, i);
    PROPERTY_MASK_SET(&p->mask, i);
    if( n>=p->nProp ){
        p->aProp = (char **)ckrealloc((char *)(p->aProp),(n+1)*(sizeof(char*)));
        memset(&p->aProp[p->nProp], 0, ((n+1)-p->nProp)*sizeof(char *));
    }
    ckfree(p->aProp[n]);
    p->aProp[n] = tokenToString(v);
}

/*--------------------------------------------------------------------------
 *
 * propertySetFree --
 *
 *     Delete a property set and it's contents.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *--------------------------------------------------------------------------
 */
static void propertySetFree(CssPropertySet *p){
    int i;
    if( !p ) return;
    for(i=0; i<p->nProp; i++){
        ckfree(p->aProp[i]);
    }
    ckfree((void *)p);
}

/*--------------------------------------------------------------------------
 *
 * propertySetFree --
 *
 *     Delete a linked list of CssSelector structs, including the 
 *     CssSelector.zValue and CssSelector.zAttr fields.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *--------------------------------------------------------------------------
 */
static void selectorFree(pSelector)
    CssSelector *pSelector;
{
    if( !pSelector ) return;
    selectorFree(pSelector->pNext);
    ckfree(pSelector->zValue);
    ckfree(pSelector->zAttr);
    ckfree((char *)pSelector);
}

/*
** Return the id of the next CSS token in the string pointed to by z, length
** n. The length of the token is written to *pLen. 0 is returned if there
** are no complete tokens remaining.
*/
static int cssGetToken(const char *z, int n, int *pLen){
    if( n<=0 ){
      return 0;
    }

    *pLen = 1;
    switch( z[0] ){
        case ' ':
        case '\n':
        case '\t': return CT_SPACE;
        case '{':  return CT_LP;
        case '}':  return CT_RP;
        case ')':  return CT_RRP;
        case '[':  return CT_LSP;
        case ']':  return CT_RSP;
        case ';':  return CT_SEMICOLON;
        case ',':  return CT_COMMA;
        case ':':  return CT_COLON;
        case '+':  return CT_PLUS;
        case '>':  return CT_GT;
        case '*':  return CT_STAR;
        case '.':  return CT_DOT;
        case '#':  return CT_HASH;
        case '=':  return CT_EQUALS;
        case '~':  return CT_TILDE;
        case '|':  return CT_PIPE;
        case '/':  return CT_SLASH;

        case '"': case '\'': {
            char delim = z[0];
            char c;
            int i;
            for(i=1; i<n; i++){
                c = z[i];
                if( c=='\\' ){
                    i++;
                }
                if( c==delim ){
                    *pLen = i+1; 
                    return CT_STRING;
                }
            }
            *pLen = n;
            return -1;
        }

        case '@': {
            struct AtKeyWord {
                const char *z;
                int n;
                int t;
            } atkeywords[] = {
                {"import", 6, CT_IMPORT_SYM},
                {"page", 4, CT_PAGE_SYM},
                {"media", 5, CT_MEDIA_SYM},
                {"font-face", 9, CT_FONT_SYM},
                {"charset", 7, CT_CHARSET_SYM},
            };
            int i;
            for(i=0; i<sizeof(atkeywords)/sizeof(struct AtKeyWord); i++){
                if( 0==strncmp(&z[1], atkeywords[i].z, atkeywords[i].n) ){
                    *pLen = atkeywords[i].n + 1;
                    return atkeywords[i].t;
                }
            }
            goto bad_token;
        }
        case '!': {
            if( 0==strncmp(&z[1], "important", 9) ){
                 *pLen = 9 + 1;
                 return CT_IMPORTANT_SYM;
            }
            goto bad_token;
        }

        default: {
            /* This must be either an identifier or a function. For the
            ** ASCII character range 0-127, the 'charmap' array is 1 for
            ** characters allowed in an identifier or function name, 0
            ** for characters not allowed. Allowed characters are a-z, 
	    ** 0-9, '-', '%' and '\'. All unicode characters outside the
            ** ASCII range are allowed.
            */
            static u8 charmap[128] = {
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x00-0x0F */
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x10-0x1F */
                0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, /* 0x20-0x2F */
                1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, /* 0x30-0x3F */
                0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x40-0x4F */
                1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, /* 0x50-0x5F */
                0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x60-0x6F */
                1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0  /* 0x70-0x7F */
            };
            int i;
            for(i=0; i<n && (z[i]<0 || charmap[(int)z[i]]); i++) /* empty */ ;
            if( i==0 ) goto bad_token;
            if( i<n && z[i]=='(' ){
                int t = -1;
                int tlen;
                i++;
                while( i!=n && t!=0 && t!=CT_RRP ){
                    t = cssGetToken(&z[i], n-i, &tlen);
                    i += tlen;
                }
                if( t!=CT_RRP ) goto bad_token;
                *pLen = i;
                return CT_FUNCTION;
            }
            *pLen = i;
            return CT_IDENT;
        }
             
    }

bad_token:
    *pLen = 1;
    return -1;
}

/* Versions of ckalloc() and ckfree() that are always functions (not macros). 
*/
static void * xCkalloc(size_t n){
    return ckalloc(n);
}
static void xCkfree(void *p){
    ckfree(p);
}

/*--------------------------------------------------------------------------
 *
 * HtmlCssParse --
 *
 *     Parse the stylesheet pointed to by z, length n bytes.
 *
 * Results:
 *
 *     Returns a CssStyleSheet pointer, written to *ppStyle.
 *
 * Side effects:
 *
 *--------------------------------------------------------------------------
 */
int HtmlCssParse(
    int n,
    const char *z,
    CssStyleSheet **ppStyle
){
    CssParse sParse;
    CssToken sToken;
    void *p;
    int t;
    int c = 0;

    /* Declarations for the parser generated by lemon. */
    void *tkhtmlCssParserAlloc(void *(*)(size_t));
    void tkhtmlCssParser(void *, int, CssToken, CssParse*);
    void tkhtmlCssParserFree(void *, void (*)(void *));

    memset(&sParse, 0, sizeof(CssParse));

    if( n<0 ){
        n = strlen(z);
    }
    p = tkhtmlCssParserAlloc(xCkalloc);

    sParse.pStyle = (CssStyleSheet *)ckalloc(sizeof(CssStyleSheet));
    memset(sParse.pStyle, 0, sizeof(CssStyleSheet));
    Tcl_InitHashTable(&sParse.pStyle->rules, TCL_STRING_KEYS);

    while( (t=cssGetToken(&z[c], n-c, &sToken.n)) ){
        sToken.z = &z[c];
        if( t>0 ){
            tkhtmlCssParser(p, t, sToken, &sParse);
        }
        c += sToken.n;
    }
    *ppStyle = sParse.pStyle;

    tkhtmlCssParserFree(p, xCkfree);
    return 0;
}

void HtmlCssStyleSheetFree(CssStyleSheet *pStyle){
    /* TODO: Cleanup! */
}

/*
** Return the number of syntax errors that occured while parsing the
** style-sheet.
*/
int HtmlCssStyleSheetSyntaxErrs(CssStyleSheet *pStyle){
    return pStyle->nSyntaxErr;
}

/*--------------------------------------------------------------------------
 *
 * tkhtmlCssDeclaration --
 *
 *     This function is called by the CSS parser when it parses a property 
 *     declaration (i.e "<property> : <expression>").
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *--------------------------------------------------------------------------
 */
void tkhtmlCssDeclaration(CssParse *pParse, CssToken *pProp, CssToken *pExpr){
    int prop; 

#if TRACE_PARSER_CALLS
    printf("tkhtmlCssDeclaration(%p, \"%.*s\", \"%.*s\")\n", 
        pParse,
        pProp?pProp->n:0, pProp?pProp->z:"", 
        pExpr?pExpr->n:0, pExpr?pExpr->z:""
    );
#endif

    /* Resolve the property name. If we don't recognize it, then ignore
     * the entire declaration (CSS2 spec says to do this).
     */
    prop = tkhtmlCssPropertyFromString(pProp->n, pProp->z);
    if( prop<0 ) return;

    if( !pParse->pPropertySet ){
        pParse->pPropertySet = propertySetNew();
    }
    propertySetAdd(pParse->pPropertySet, prop, pExpr);
}

/*--------------------------------------------------------------------------
 *
 * tkhtmlCssSelector --
 *
 *     This is called whenever a simple selector is parsed. 
 *     i.e. "H1" or ":before".
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *--------------------------------------------------------------------------
 */
void tkhtmlCssSelector(pParse, stype, pAttr, pValue)
    CssParse *pParse; 
    int stype; 
    CssToken *pAttr; 
    CssToken *pValue;
{
    CssSelector *pSelector;

#if TRACE_PARSER_CALLS
    /* I used this to make sure the parser was passing the components of
     * selectors to this function in the correct order. Once this was 
     * verified, it is not particularly useful trace output. But we'll leave
     * it here for the time being in case something comes up.
     */
    printf("tkhtmlCssSelector(%p, %s, \"%.*s\", \"%.*s\")\n", 
        pParse, constantToString(stype), 
        pAttr?pAttr->n:0, pAttr?pAttr->z:"", 
        pValue?pValue->n:0, pValue?pValue->z:""
    );
#endif

    pSelector = (CssSelector *)ckalloc(sizeof(CssSelector));
    memset(pSelector, 0, sizeof(CssSelector));
    pSelector->eSelector = stype;
    pSelector->zValue = tokenToString(pValue);
    pSelector->zAttr = tokenToString(pAttr);
    pSelector->pNext = pParse->pSelector;
    pParse->pSelector = pSelector;
}

/*
 *---------------------------------------------------------------------------
 *
 * cssSelectorPropertySetPair --
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
cssSelectorPropertySetPair(pParse, pSelector, pPropertySet)
    CssParse *pParse;
    CssSelector *pSelector;
    CssPropertySet *pPropertySet;
{
    CssStyleSheet *pStyle = pParse->pStyle;

    CssRule *pRule = (CssRule *)ckalloc(sizeof(CssRule));
    memset(pRule, 0, sizeof(CssRule));

    pPropertySet->nRef++;
    pRule->pImportant = 0;

    /* TODO: Set up pRule->eMedia. Later: Maybe this should be filtered
     * on the way in? */
    /* TODO: Calculate specificity and copy to pPropertySet. */

    if( pSelector->eSelector==CSS_SELECTOR_TYPE ){
        Tcl_HashEntry *pEntry; 
        int n;         /* True if we add a a new hash table entry */
        assert( pSelector->zValue );
        pEntry = Tcl_CreateHashEntry(&pStyle->rules, pSelector->zValue, &n);
        pRule->pNext = Tcl_GetHashValue(pEntry);
        assert( (n && !pRule->pNext) || (!n && pRule->pNext) );
        Tcl_SetHashValue(pEntry, pRule);
    }else{
        pRule->pNext = pStyle->pUniversalRules;
        pStyle->pUniversalRules = pRule;
    }

    pRule->pSelector = pSelector;
    pRule->pPropertySet = pPropertySet;
}


/*--------------------------------------------------------------------------
 *
 * tkhtmlCssRule --
 *
 *     This is called when the parser has parsed an entire rule.
 *
 * Results:
 *     None.
 *
 * Side effects:
 * 
 *     If the parse was successful, then add the rule to the stylesheet.
 *     If unsuccessful, delete anything that was built up by calls to 
 *     tkhtmlCssDeclaration() or tkhtmlCssSelector().
 *
 *--------------------------------------------------------------------------
 */
void tkhtmlCssRule(pParse, success)
    CssParse *pParse;
    int success;
{
    CssSelector *pSelector = pParse->pSelector;
    CssPropertySet *pPropertySet = pParse->pPropertySet;
    CssSelector **apXtraSelector = pParse->apXtraSelector;
    int nXtra = pParse->nXtra;
    int i;

    pParse->pSelector = 0;
    pParse->pPropertySet = 0;
    pParse->apXtraSelector = 0;
    pParse->nXtra = 0;

    if( success && pSelector && pPropertySet ){
        cssSelectorPropertySetPair(pParse, pSelector, pPropertySet);
        for (i = 0; i < nXtra; i++){
            cssSelectorPropertySetPair(pParse, apXtraSelector[i], pPropertySet);
        }
    }else{
        /* Some sort of a parse error has occured. We won't be including
         * this rule, so just free these structs so we don't leak memory.
         */ 
        selectorFree(pSelector);
        propertySetFree(pPropertySet);
        for (i = 0; i < nXtra; i++){
            selectorFree(apXtraSelector);
        }
    }

    if( apXtraSelector ){
        ckfree((char *)apXtraSelector);
    }
}

/*--------------------------------------------------------------------------
 *
 * attrTest --
 *
 *     Test if an attribute value matches a string. The three modes of 
 *     comparing attribute values specified in CSS are supported.
 *
 * Results:
 *     Non-zero is returned if the match is true.
 *
 * Side effects:
 *     None.
 *
 *--------------------------------------------------------------------------
 */
static int attrTest(eType, zString, zAttr)
    u8 eType;
    const char *zString;
    const char *zAttr;
{
    switch( eType ){
        /* True if the specified attribute exists */
        case CSS_SELECTOR_ATTR:
            return (zAttr?1:0);

        /* True if the specified attribute exists and the value matches
         * the string exactly.
         */
        case CSS_SELECTOR_ATTRVALUE:
            return ((zAttr && 0==strcmp(zAttr, zString))?1:0);

	/* Treat the attribute value (if it exists) as a space seperated list.
         * Return true if zString exists in the list.
         */
        case CSS_SELECTOR_ATTRLISTVALUE: {
            const char *pAttr = zAttr;
            while( pAttr && pAttr[0] ){
                char *pSpace = strchr(zAttr, ' ');
                if( pSpace && 0==strncmp(pAttr, zString, pSpace-pAttr) ){
                    return 1;
                }
                while( pSpace && *pSpace==' ' ){
                    pSpace++;
                }
                pAttr = pSpace;
            }
            return 0;
        }

        /* True if the attribute exists and matches zString up to the
         * first '-' character in the attribute value.
         */
        case CSS_SELECTOR_ATTRHYPHEN: {
            if( zAttr ){
                char *pHyphen = strchr(zAttr, '-');
                if( pHyphen && 0==strncmp(zAttr, zString, pHyphen-zAttr) ){
                    return 1;
                }
            }
            return 0;
        }
    }

    assert(!"Impossible");
    return 0;
}

/*--------------------------------------------------------------------------
 *
 * selectorTest --
 *
 *     Test if a selector matches a document node.
 *
 * Results:
 *     Non-zero is returned if the Selector does match the node.
 *
 * Side effects:
 *     None.
 *
 *--------------------------------------------------------------------------
 */
#define NODE_TYPE(x)        (pNodeInterface->xType(x))
#define NODE_ATTR(x,y)      (pNodeInterface->xAttr(x,y))
#define NODE_PARENT(x)      (pNodeInterface->xParent(x))
#define NODE_NUMCHILDREN(x) (pNodeInterface->xNumChildren(x))
#define NODE_CHILD(x,y)     (pNodeInterface->xChild(x,y))
#define NODE_PARENTIDX(x)   (pNodeInterface->xParentIdx(x))
#define NODE_LANG(x)        (pNodeInterface->xLang(x))
static int selectorTest(pSelector, pNodeInterface, pNode)
    CssSelector *pSelector;
    CssNodeInterface *pNodeInterface;
    void *pNode;
{
    CssSelector *p = pSelector;
    void *x = pNode;
    while( p && x ){

        switch( p->eSelector ){
            case CSS_SELECTOR_UNIVERSAL:
                break;

            case CSS_SELECTOR_TYPE:
                if( strcmp(NODE_TYPE(x), p->zValue) ) return 0;
                break;

            case CSS_SELECTOR_ATTR:
            case CSS_SELECTOR_ATTRVALUE:
            case CSS_SELECTOR_ATTRLISTVALUE:
            case CSS_SELECTOR_ATTRHYPHEN:
                if( !attrTest(p->eSelector, p->zValue, NODE_ATTR(x,p->zAttr)) ){
                    return 0;
                }
                break;

            case CSS_SELECTORCHAIN_DESCENDANT:
            case CSS_SELECTORCHAIN_CHILD:
                x = NODE_PARENT(x);
                break;
            case CSS_SELECTORCHAIN_ADJACENT: {
                int n = NODE_PARENTIDX(x);
                if( n<=0 ) return 0;
                x = NODE_CHILD(NODE_PARENT(x), n-1);
                break;
            }
                
            /* TODO: Support pseudo elements and classes. */
            case CSS_PSEUDOCLASS_LANG:
            case CSS_PSEUDOCLASS_FIRSTCHILD:
            case CSS_PSEUDOCLASS_LINK:
            case CSS_PSEUDOCLASS_UNVISITED:
            case CSS_PSEUDOCLASS_ACTIVE:
            case CSS_PSEUDOCLASS_HOVER:
            case CSS_PSEUDOCLASS_FOCUS:
            case CSS_PSEUDOELEMENT_FIRSTLINE:
            case CSS_PSEUDOELEMENT_FIRSTLETTER:
            case CSS_PSEUDOELEMENT_BEFORE:
            case CSS_PSEUDOELEMENT_AFTER:
                return 0;

            default:
                assert(!"Impossible");
        }
        p = p->pNext;
    }

    return (x && !p)?1:0;
}

static void propertiesAdd(ppProperties, pPropertySet)
    CssProperties **ppProperties;
    CssPropertySet *pPropertySet;
{
    CssProperties *pProperties = *ppProperties;
    int n = (pProperties?pProperties->nPropertySet:0) + 1;
    int nAlloc = sizeof(CssProperties) + n*sizeof(CssPropertySet *);

    assert( pPropertySet );

    pProperties = (CssProperties *)ckrealloc((char *)pProperties, nAlloc);
    pProperties->nPropertySet = n;
    pProperties->apPropertySet = (CssPropertySet **)&pProperties[1];
    pProperties->apPropertySet[n-1] = pPropertySet;

    *ppProperties = pProperties;
}

void HtmlCssPropertiesFree(pPropertySet)
    CssProperties *pPropertySet;
{
    ckfree((char *)pPropertySet);
}

/*--------------------------------------------------------------------------
 *
 * HtmlCssStyleSheetApply --
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------------------
 */
void HtmlCssStyleSheetApply(pStyle, pNodeInterface, pNode, ppProperties)
    CssStyleSheet * pStyle; 
    CssNodeInterface *pNodeInterface;
    void *pNode; 
    CssProperties **ppProperties;
{
    CssRule *pRule = 0;
    CssRule *pRule2 = 0;
    Tcl_HashEntry *pEntry;

    /* For each rule in the style-sheets hash table for the given node-type,
     * see if the selector matches the node. If so, add the rules properties
     * to the property set.
     */
    pEntry = Tcl_FindHashEntry(&pStyle->rules, NODE_TYPE(pNode));
    if( pEntry ){
        pRule = (CssRule *)Tcl_GetHashValue(pEntry);
    }
    pRule2 = pStyle->pUniversalRules;
    if( !pRule ){
        pRule = pRule2;
        pRule2 = 0;
    }

    while( pRule ){
        int i;
        int match = selectorTest(pRule->pSelector, pNodeInterface, pNode);
        CssPropertySet *pPropertySet = pRule->pPropertySet;

#if TRACE_STYLE_APPLICATION
        printf("Rule %p does %s node - ", pRule, match?"match":"NOT match");
        for(i=0; i<127; i++){
            const char *z = propertySetGet(pPropertySet, i);
            if( z ){
                printf("%s=\"%s\" ", tkhtmlCssPropertyToString(i), z);
            }
        }
        printf("\n");
#endif

        if (match) {
            propertiesAdd(ppProperties, pPropertySet);
        }

        /* Advance pRule to the next rule */
        pRule = pRule->pNext;
        if( !pRule ){
            pRule = pRule2;
            pRule2 = 0;
        }
    }
}

/*--------------------------------------------------------------------------
 *
 * tkhtmlCssPropertiesGet --
 *     Retrieve the value of a specified property from a CssProperties
 *     object, or NULL if the property is not defined.
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------------------
 */
const char *tkhtmlCssPropertiesGet(pProperties, prop)
    CssProperties * pProperties; 
    int prop;
{
    const char *zRet = 0;
    if( pProperties ){
        int i;
        for(i=0; i<pProperties->nPropertySet && !zRet; i++){
            CssPropertySet *pPropertySet = pProperties->apPropertySet[i];
            zRet = propertySetGet(pPropertySet, prop);
        }
    }
    return zRet;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssSelectorComma --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void HtmlCssSelectorComma(pParse)
    CssParse *pParse;
{
    int n = (pParse->nXtra + 1) * sizeof(CssSelector *);
    pParse->apXtraSelector = 
       (CssSelector **)ckrealloc((char *)pParse->apXtraSelector, n);
    pParse->apXtraSelector[pParse->nXtra] = pParse->pSelector;
    pParse->pSelector = 0;
    pParse->nXtra++;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssPropertiesTclize --
 * 
 *     Create a Tcl representation of a property set. This is returned
 *     as a new Tcl object with the ref-count set to 1. The caller must
 *     call Tcl_DecrRefCount() on the returned object at some point.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
Tcl_Obj * HtmlCssPropertiesTclize(pProperties)
    CssProperties * pProperties; 
{
    Tcl_Obj *pRet = Tcl_NewObj();
    int i;
    Tcl_IncrRefCount(pRet);
    if( pProperties ){
        for(i=0; i<127; i++){
            const char *z = tkhtmlCssPropertiesGet(pProperties, i);
            if( z ){
                Tcl_ListObjAppendElement(0, pRet, 
                    Tcl_NewStringObj(tkhtmlCssPropertyToString(i), -1));
                Tcl_ListObjAppendElement(0, pRet, 
                    Tcl_NewStringObj(z, -1));
            }
        }
    }
    return pRet;
}
