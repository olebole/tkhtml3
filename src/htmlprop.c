
/*
 * htmlprop.c ---
 *
 *     This file implements the mapping between HTML attributes and CSS
 *     properties.
 *
 * TODO: Copyright.
 */
static char rcsid[] = "@(#) $Id:";

#include "html.h"
#include <assert.h>
#include <string.h>

/*
 * A special value that the eType field of an initial property value can
 * take. If eType==CSS_TYPE_SAMEASCOLOR, then the value of the 'color'
 * property is used as the initial value for the property.
 */
#define CSS_TYPE_COPYCOLOR -1
#define CSS_TYPE_FREEZVAL -1

typedef struct PropertyCacheEntry PropertyCacheEntry;
struct PropertyCacheEntry {
    CssProperty prop;
    PropertyCacheEntry *pNext;
};

/*
 * Each node has a property cache, used to cache properties derived from
 * the cascade algorithm. This is to avoid running the cascade more than
 * required.
 */
struct HtmlPropertyCache {
    PropertyCacheEntry *pStore;
    CssProperty *apProp[120];
};

/*
 *---------------------------------------------------------------------------
 *
 * newPropertyCache --
 *
 *     Allocate and return a new property cache.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static HtmlPropertyCache *
newPropertyCache()
{
    HtmlPropertyCache *pRet;
    pRet = (HtmlPropertyCache *)ckalloc(sizeof(HtmlPropertyCache));
    memset(pRet, 0, sizeof(HtmlPropertyCache));
    return pRet;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlDeletePropertyCache --
 *
 *     Delete a property cache previously returned by newPropertyCache().
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
HtmlDeletePropertyCache(pCache)
    HtmlPropertyCache *pCache; 
{
    if (pCache) {
        PropertyCacheEntry *pStore = pCache->pStore;
        PropertyCacheEntry *pStore2 = 0;

        while (pStore) {
            pStore2 = pStore->pNext;
            if (pStore->prop.eType == CSS_TYPE_FREEZVAL) {
                ckfree(pStore->prop.v.zVal);
            }
            ckfree((char *)pStore);
            pStore = pStore2;
        }
        ckfree((char *)pCache);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * setPropertyCache --
 *
 *     Set the value of property iProp in the property cache to pProp. The
 *     pointer to pProp is copied, so *pProp must exist for the lifetime of
 *     this property-cache.
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
setPropertyCache(pCache, iProp, pProp)
    HtmlPropertyCache *pCache; 
    int iProp;
    CssProperty *pProp;
{
    pCache->apProp[iProp] = pProp;
}

/*
 *---------------------------------------------------------------------------
 *
 * storePropertyCache --
 *
 *     Set the value of property iProp in the property cache to pProp. The
 *     value is copied and released when the property cache is destroyed.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static CssProperty *storePropertyCache(pCache, pProp)
    HtmlPropertyCache *pCache; 
    CssProperty *pProp;
{
    PropertyCacheEntry *pEntry;
    pEntry = (PropertyCacheEntry *)ckalloc(sizeof(PropertyCacheEntry));
    pEntry->prop = *pProp;
    pEntry->pNext = pCache->pStore;
    pCache->pStore = pEntry;
    return &pEntry->prop;
}

/*
 *---------------------------------------------------------------------------
 *
 * getPropertyCache --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static CssProperty *
getPropertyCache(pCache, iProp)
    HtmlPropertyCache *pCache; 
    int iProp;
{
    return pCache->apProp[iProp];
}

/*
 *---------------------------------------------------------------------------
 *
 * freeWithPropertyCache --
 *
 *     Call ckfree() on the supplied property pointer when this property
 *     cache is deleted. Presumably the same property is added to the
 *     lookup table using setPropertyCache().
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
freeWithPropertyCache(pCache, pProp)
    HtmlPropertyCache *pCache; 
    CssProperty *pProp;
{
    CssProperty freeprop;
    freeprop.eType = CSS_TYPE_FREEZVAL;
    freeprop.v.zVal = (char *)pProp;
    storePropertyCache(pCache, &freeprop);
}
    

/*
 *---------------------------------------------------------------------------
 *
 * lengthToProperty --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void lengthToProperty(zPadding, pOut)
    CONST char *zPadding;
    CssProperty *pOut;
{
    assert(zPadding);
    pOut->v.iVal = atoi(zPadding);
    if (zPadding[strlen(zPadding)-1]=='%') {
        pOut->eType = CSS_TYPE_PERCENT;
    } else {
        pOut->eType = CSS_TYPE_PX;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * mapVAlign --
 *
 *    Map from Html attributes to the 'vertical-align' property. The
 *    'vertical-align' property works on both inline elements and
 *    table-cells. This function only needs to deal with the table-cells
 *    case.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int
mapVAlign(pNode, pOut)
    HtmlNode *pNode;
    CssProperty *pOut;
{
    int tag = HtmlNodeTagType(pNode);
    if (tag==Html_TD || tag==Html_TH) {
        HtmlNode *pTmp = pNode;
        for (pTmp=pNode; 
             pTmp && HtmlNodeTagType(pTmp)!=Html_TABLE; 
             pTmp = HtmlNodeParent(pTmp)
        ) {
            CONST char *zAttr = HtmlNodeAttr(pTmp, "valign");
            if (zAttr) {
                pOut->v.zVal = (char *)zAttr;
                pOut->eType = CSS_TYPE_STRING;
                return 1;
            }
        }
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * mapColor --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
mapColor(pNode, pOut)
    HtmlNode *pNode;
    CssProperty *pOut;
{
    int tag;
    HtmlNode *pParent;
    CONST char *zColor = 0;

    /* See if we are inside any tag that specifies the "color" attribute.
     */
    for (pParent=pNode; pParent; pParent=HtmlNodeParent(pParent)) {
        tag = HtmlNodeTagType(pParent);

        /* If we are inside an <a> tag, look for a "link" attribute on the
         * <body> tag. Use this color if it exists. Todo: Support "vlink".
         */
        if (tag==Html_A && HtmlNodeAttr(pParent, "href")) {
            HtmlNode *pBody = HtmlNodeParent(pParent);
            while (pBody && HtmlNodeTagType(pBody)!=Html_BODY) {
                pBody = HtmlNodeParent(pBody);
            }
            if (pBody) {
                zColor = HtmlNodeAttr(pBody, "link");
                if (zColor) {
                    break;
                }
            }
        }
           
        zColor = HtmlNodeAttr(pParent, "color");
        if (zColor) {
            break;
        }
    }

    if (zColor) {
        pOut->eType = CSS_TYPE_STRING;
        pOut->v.zVal = (char *)zColor;
        return 1;
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * mapWidth --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int mapWidth(pNode, pOut)
    HtmlNode *pNode;
    CssProperty *pOut;
{
    CONST char *zWidth; 
    zWidth = HtmlNodeAttr(pNode, "width");

    if (zWidth && isdigit((int)zWidth[0])) {
        pOut->v.iVal = atoi(zWidth);
        if (zWidth[strlen(zWidth)-1]=='%') {
            pOut->eType = CSS_TYPE_PERCENT;
        } else {
            pOut->eType = CSS_TYPE_PX;
        }
        return 1;
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * mapHeight --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int mapHeight(pNode, pOut)
    HtmlNode *pNode;
    CssProperty *pOut;
{
    CONST char *zHeight; 
    zHeight = HtmlNodeAttr(pNode, "height");

    if (zHeight) {
        pOut->v.iVal = atoi(zHeight);
        if (zHeight[0] && zHeight[strlen(zHeight)-1]=='%') {
            pOut->eType = CSS_TYPE_PERCENT;
        } else {
            pOut->eType = CSS_TYPE_PX;
        }
    }
    return zHeight?1:0;
}

/*
 *---------------------------------------------------------------------------
 *
 * mapBgColor --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
mapBgColor(pNode, pOut)
    HtmlNode *pNode;
    CssProperty *pOut;
{
    int tag;
    CONST char *zBg; 

    zBg = HtmlNodeAttr(pNode, "bgcolor");
    if (zBg) {
        pOut->eType = CSS_TYPE_STRING;
        pOut->v.zVal = (char *)zBg;
        return 1;
    }

    tag = HtmlNodeTagType(pNode);
    if (tag==Html_TD || tag==Html_TH) {
        HtmlNode *pParent = HtmlNodeParent(pNode);
        if (pParent && HtmlNodeTagType(pParent)==Html_TR) {
            return mapBgColor(pParent, pOut);
        } 
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * mapFontSize --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
mapFontSize(pNode, pOut)
    HtmlNode *pNode;
    CssProperty *pOut;
{
    int tag;
    tag = HtmlNodeTagType(pNode);

    if (tag==Html_FONT || tag==Html_BASEFONT) {
        CONST char *zSize = HtmlNodeAttr(pNode, "size");
        if (zSize) {
            char *zStrings[7] = {"xx-small", "x-small", "small", 
                                 "medium", "large", "x-large", "xx-large"};
            int i;
            int iSize = atoi(zSize);
            if (iSize==0) return 0;
            if (zSize[0]=='+' || zSize[0]=='-') {
                iSize += 3;
            }

            if (iSize>7 || iSize<1) return 0;
            pOut->eType = CSS_TYPE_STRING;
            pOut->v.zVal = zStrings[iSize-1];
            return 1;
        }
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * mapBorderWidth --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int mapBorderWidth(pNode, pOut)
    HtmlNode *pNode;
    CssProperty *pOut;
{
    int tag;
    tag = HtmlNodeTagType(pNode);

    if (tag==Html_TABLE || tag==Html_TD || tag==Html_TH) {
        CONST char *zBorder;
        HtmlNode *p = pNode;

        while (p && Html_TABLE!=HtmlNodeTagType(p)) {
            p = HtmlNodeParent(p);
        }

        /* Attribute "border" may only take a value in pixels. */
        zBorder = HtmlNodeAttr(p, "border");
        if (zBorder) {
            int iBorder = atoi(zBorder);
            pOut->eType = CSS_TYPE_PX;

            /* This is a bit weird in my opinion. If a <TABLE> tag has a
             * border attribute with no value, then this means the table
             * has a border but it's up to the user-agent to pick a
             * formatting for it. So we treat the following as identical:
             *
             *     <table border>
             *     <table border="">
             *     <table border="1">
             *     <table border=1>
             *
             * See section 11.3 "Table formatting by visual user agents" of
             * the HTML 4.01 spec for details.
             */
            if (!zBorder[0]) {
                iBorder = 1;
            }

            if (iBorder>0 && (tag==Html_TD || tag==Html_TH)) {
                pOut->v.iVal = 1;
            } else {
                pOut->v.iVal = iBorder;
            }
            return 1;
        }

    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * mapBorderStyle --
 *
 *     If the "border" attribute of a table is set, then the border-style
 *     for the table and it's cells is set to 'solid'.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
mapBorderStyle(pNode, pOut)
    HtmlNode *pNode;
    CssProperty *pOut;
{
    int tag;
    tag = HtmlNodeTagType(pNode);

    if (tag==Html_TABLE || tag==Html_TD || tag==Html_TH) {
        CONST char *zBorder;
        HtmlNode *p = pNode;

        while (p && Html_TABLE!=HtmlNodeTagType(p)) {
            p = HtmlNodeParent(p);
        }

        /* Attribute "border" may only take a value in pixels. */
        zBorder = HtmlNodeAttr(p, "border");
        if (zBorder) {
            pOut->eType = CSS_TYPE_STRING;
            pOut->v.zVal = "solid";
            return 1;
        }

    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * mapPadding --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int mapPadding(pNode, pOut)
    HtmlNode *pNode;
    CssProperty *pOut;
{
    int tag = HtmlNodeTagType(pNode);

    /* For a table cell, check for HTML attribute "cellpadding" on the
     * parent <table> tag.
     */
    if (tag==Html_TD || tag==Html_TH) {
        HtmlNode *p = pNode;
        while (p && Html_TABLE!=HtmlNodeTagType(p)) {
            p = HtmlNodeParent(p);
        }

        if (p) {
            CONST char *zPadding = HtmlNodeAttr(p, "cellpadding");
            if (zPadding) {
                lengthToProperty(zPadding, pOut);
                return 1;
            }
        }
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * mapBorderSpacing --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
mapBorderSpacing(pNode, pOut)
    HtmlNode *pNode;
    CssProperty *pOut;
{
    int tag = HtmlNodeTagType(pNode);

    /* For a table or table cell, check for HTML attribute "cellspacing" on
     * the <table> tag.
     */
    if (tag==Html_TABLE || tag==Html_TD || tag==Html_TH) {
        HtmlNode *p = pNode;
        while (p && Html_TABLE!=HtmlNodeTagType(p)) {
            p = HtmlNodeParent(p);
        }

        if (p) {
            CONST char *zSpacing = HtmlNodeAttr(p, "cellspacing");
            if (zSpacing) {
                lengthToProperty(zSpacing, pOut);
                return 1;
            }
        }
    }

    return 0;
}

/* 
 * These macros just makes the array definition below format more neatly.
 */
#define CSSSTR(x) {CSS_TYPE_STRING, (int)x}
#define CSS_NONE CSSSTR("none")

struct PropMapEntry {
    int property;
    int inherit;
    int(*xAttrmap)(HtmlNode *, CssProperty *);
    CssProperty initial;
};
typedef struct PropMapEntry PropMapEntry;

static PropMapEntry propmapdata[] = {
    {CSS_PROPERTY_DISPLAY, 0, 0, CSSSTR("inline")},
    {CSS_PROPERTY_FLOAT, 0, 0, CSS_NONE},
    {CSS_PROPERTY_CLEAR, 0, 0, CSS_NONE},

    {CSS_PROPERTY_BACKGROUND_COLOR, 0, mapBgColor, CSSSTR("transparent")},
    {CSS_PROPERTY_COLOR, 1, mapColor, CSSSTR("black")},
    {CSS_PROPERTY_WIDTH, 0, mapWidth, CSSSTR("auto")},
    {CSS_PROPERTY_MIN_WIDTH, 0, 0, {CSS_TYPE_NONE, 0}},
    {CSS_PROPERTY_MAX_WIDTH, 0, 0, {CSS_TYPE_NONE, 0}},
    {CSS_PROPERTY_HEIGHT, 0, mapHeight, {CSS_TYPE_NONE, 0}},

    /* Font and text related properties */
    {CSS_PROPERTY_TEXT_DECORATION, 0, 0,     {CSS_TYPE_NONE, 0}},
    {CSS_PROPERTY_FONT_SIZE, 0, mapFontSize, {CSS_TYPE_NONE, 0}},
    {CSS_PROPERTY_WHITE_SPACE, 1, 0, {CSS_TYPE_NONE, 0}},
    {CSS_PROPERTY_VERTICAL_ALIGN, 0, mapVAlign, {CSS_TYPE_NONE, 0}},
    {CSS_PROPERTY_FONT_FAMILY, 1, 0, CSSSTR("Helvetica")},
    {CSS_PROPERTY_FONT_STYLE, 1, 0,  CSSSTR("normal")},
    {CSS_PROPERTY_FONT_WEIGHT, 1, 0, CSSSTR("normal")},
    {CSS_PROPERTY_TEXT_ALIGN, 1, 0,  CSSSTR("left")},
    {CSS_PROPERTY_LINE_HEIGHT, 0, 0, CSSSTR("normal")},

    /* Width of borders. */
    {CSS_PROPERTY_BORDER_TOP_WIDTH,    0, mapBorderWidth, CSSSTR("medium")},
    {CSS_PROPERTY_BORDER_BOTTOM_WIDTH, 0, mapBorderWidth, CSSSTR("medium")},
    {CSS_PROPERTY_BORDER_LEFT_WIDTH,   0, mapBorderWidth, CSSSTR("medium")},
    {CSS_PROPERTY_BORDER_RIGHT_WIDTH,  0, mapBorderWidth, CSSSTR("medium")},

    /* Color of borders. */
    {CSS_PROPERTY_BORDER_TOP_COLOR,    0, 0, {CSS_TYPE_COPYCOLOR, 0}},
    {CSS_PROPERTY_BORDER_BOTTOM_COLOR, 0, 0, {CSS_TYPE_COPYCOLOR, 0}},
    {CSS_PROPERTY_BORDER_LEFT_COLOR,   0, 0, {CSS_TYPE_COPYCOLOR, 0}},
    {CSS_PROPERTY_BORDER_RIGHT_COLOR,  0, 0, {CSS_TYPE_COPYCOLOR, 0}},

    /* Style of borders. */
    {CSS_PROPERTY_BORDER_TOP_STYLE,    0, mapBorderStyle, CSS_NONE},
    {CSS_PROPERTY_BORDER_BOTTOM_STYLE, 0, mapBorderStyle, CSS_NONE},
    {CSS_PROPERTY_BORDER_LEFT_STYLE,   0, mapBorderStyle, CSS_NONE},
    {CSS_PROPERTY_BORDER_RIGHT_STYLE,  0, mapBorderStyle, CSS_NONE},

    /* Padding */
    {CSS_PROPERTY_PADDING_TOP,    0, mapPadding, {CSS_TYPE_PX, 0}},
    {CSS_PROPERTY_PADDING_LEFT,   0, mapPadding, {CSS_TYPE_PX, 0}},
    {CSS_PROPERTY_PADDING_RIGHT,  0, mapPadding, {CSS_TYPE_PX, 0}},
    {CSS_PROPERTY_PADDING_BOTTOM, 0, mapPadding, {CSS_TYPE_PX, 0}},

    /* Margins */
    {CSS_PROPERTY_MARGIN_TOP,    0, 0, {CSS_TYPE_PX, 0}},
    {CSS_PROPERTY_MARGIN_LEFT,   0, 0, {CSS_TYPE_PX, 0}},
    {CSS_PROPERTY_MARGIN_RIGHT,  0, 0, {CSS_TYPE_PX, 0}},
    {CSS_PROPERTY_MARGIN_BOTTOM, 0, 0, {CSS_TYPE_PX, 0}},

    /* Custom Tkhtml properties */
    {CSS_PROPERTY__TKHTML_REPLACE, 0, 0, {CSS_TYPE_NONE, 0}},

    {CSS_PROPERTY_BORDER_SPACING, 1, mapBorderSpacing, {CSS_TYPE_PX, 0}},
    {CSS_PROPERTY_LIST_STYLE_TYPE, 1, 0, CSSSTR("disc")},
};

static int propMapisInit = 0;
static PropMapEntry *propmap[130];

/*
 *---------------------------------------------------------------------------
 *
 * getProperty --
 *
 *    Given a pointer to a PropMapEntry and a node, return the value of the
 *    property in *pOut.
 *
 *    This function implements the CSS "cascade":
 *
 *        1. Find all declarations that apply to the element/property in
 *           question.  Declarations apply if the selector matches the
 *           element in question. If no declarations apply, the inherited
 *           value is used. If there is no inherited value (this is the
 *           case for the 'HTML' element and for properties that do not
 *           inherit), the initial value is used. 
 *       
 *        2. Sort the declarations by explicit weight: declarations marked
 *           '!important' carry more weight than unmarked (normal)
 *           declarations.  
 * 
 *        3. Sort by origin: the author's style sheets override the
 *           reader's style sheet which override the UA's default values.
 *           An imported style sheet has the same origin as the style sheet
 *           from which it is imported. 
 *
 *        4. Sort by specificity of selector: more specific selectors will
 *           override more general ones. To find the specificity, count the
 *           number of ID attributes in the selector (a), the number of
 *           CLASS attributes in the selector (b), and the number of tag
 *           names in the selector (c). Concatenating the three numbers (in
 *           a number system with a large base) gives the specificity. Some
 *           examples:
 *
 *        5. Sort by order specified: if two rules have the same weight,
 *           the latter specified wins. Rules in imported style sheets are
 *           considered to be before any rules in the style sheet itself.
 *
 *     As well as rules specified as part of stylesheets, property values
 *     may also come from:
 * 
 *           * Html style attributes. (i.e. <p style="...">). These
 *             properties are treated as if they were specified at the end
 *             of the author stylesheet with a single "id" selector.
 *
 *           * Other html attributes. (i.e. <p font="...">). These
 *             properties are treated as if they occured at the start of
 *             the author stylesheet with a single "type" selector.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     The property value is copied to *pOut.
 *
 *---------------------------------------------------------------------------
 */
static CssProperty *
getProperty(interp, pNode, pEntry, inheriting, pOut)
    Tcl_Interp *interp;
    HtmlNode *pNode;
    PropMapEntry *pEntry;
    int inheriting;
    CssProperty *pOut;
{
    CssProperty *pProp = 0;
    CssProperty *pProp2 = 0;
    HtmlNode *pN;
    int spec = -1;
    int sheet = -1;
    int prop = pEntry->property;
    HtmlPropertyCache *pPropertyCache = pNode->pPropCache;

    if (!pPropertyCache) {
        pPropertyCache = newPropertyCache();
        pNode->pPropCache = pPropertyCache;
    }

    /* Before looking anywhere else, see if this property is present in the
     * nodes local cache. 
     */
    pProp = getPropertyCache(pPropertyCache, prop);
    if (pProp) {
        assert(pProp->eType != CSS_TYPE_TCL);
        sheet = 1;
        goto getproperty_out;
    }

    /* Query the style-sheet database. HtmlCssPropertiesGet() returns the
     * property value with the highest precedence (accounting for origin,
     * specificity and declaration order) for the node.
     *
     * If the property was specified as part of the author stylesheet with
     * a specifity greater than 10000 (a single id selector) then the style
     * or other html attributes may not override it, so we jump to the end
     * of this function immediately.
     */
    pProp2 = HtmlCssPropertiesGet(pNode->pProperties, prop, &sheet, &spec);
    if (pProp2) {
        pProp = pProp2;
    }
    if (sheet==CSS_ORIGIN_AUTHOR && spec>10000) {
        goto getproperty_out;
    }

    /* See if the property was specified as part of a 'style' attribute.
     * This overrides any style-sheet property, unless it has a specificity
     * of greater than an "id" selector (unusual but possible). If we find
     * the property in a style attribute, it may not be overriden by
     * another html attribute, so jump to th eend of the funtion now.
     */
    if (pNode->pStyle) {
        pProp2 = HtmlCssPropertiesGet(pNode->pStyle, prop, 0, 0);
        if (pProp2) {
            pProp = pProp2;
            sheet = 0;
            goto getproperty_out;
        }
    }

    /* Jump to the end of the function if we already have a property with a
     * higher precedence than a mapped html attribute. This is any property
     * specified as part of the author stylesheet, unless the selector is
     * the universal selector "*".
     */
    if (sheet==CSS_ORIGIN_AUTHOR && spec>0) {
        goto getproperty_out;
    }

    /* See if we can get the property by translating an HTML attribute. */
    if (pEntry->xAttrmap && pEntry->xAttrmap(pNode, pOut)) {
        pProp = storePropertyCache(pPropertyCache, pOut);
        sheet = 0;
    }

getproperty_out:

    /* If we have to inherit this property, either because of an explicit
     * "inherit" value, or because the property is inherited and no value
     * has been specified, then make a HtmlNodeGetProperty() call on the
     * parent node (if one exists).
     */ 
    if ((sheet < 0 && pEntry->inherit) || 
        (sheet >= 0 && pProp->eType == CSS_TYPE_INHERIT)
    ) {
        HtmlNode *pParent = HtmlNodeParent(pNode);
        if (pParent) {
            pProp = getProperty(interp, pParent, pEntry, 1, pOut);
            sheet = 0;
        }
    }
    
    /* If we still have nothing, use the properties initial value. */
    if (sheet < 0) {
        if (pEntry->initial.eType==CSS_TYPE_COPYCOLOR) {
            PropMapEntry *pE= propmap[CSS_PROPERTY_COLOR];
            pProp = getProperty(interp, pNode, pE, 0, pOut);
            sheet = 0;
        } else {
            pProp = &pEntry->initial;
        }
    }

    /* At this point a property has been assigned and copied to *pOut. This
     * step checks if the property is of type CSS_TYPE_TCL. If so, we need
     * to invoke a Tcl script to retrieve the value of this property.
     */
    if (pProp->eType==CSS_TYPE_TCL) {
        int rc;
        Tcl_Obj *pNodeCmd = HtmlNodeCommand(interp, pNode);
        Tcl_Obj *pEval = Tcl_NewStringObj(pProp->v.zVal, -1);

        Tcl_IncrRefCount(pEval);
        Tcl_ListObjAppendElement(0, pEval, pNodeCmd);
        rc = Tcl_EvalObjEx(interp, pEval, TCL_EVAL_GLOBAL|TCL_EVAL_DIRECT);
        Tcl_DecrRefCount(pEval);

        if (rc==TCL_OK) {
            /* If the Tcl script was successful, interpret the returned
             * string as a property value and return it. Put the
             * interpreted property value in the nodes property cache so
             * that we don't have to execute the Tcl script again.
             */
            Tcl_Obj *pResult;

            pResult = Tcl_GetObjResult(interp);
            pProp = HtmlCssStringToProperty(Tcl_GetString(pResult), -1);
            if (pProp->eType==CSS_TYPE_TCL) {
                pProp = &pEntry->initial;
            } else {
                freeWithPropertyCache(pPropertyCache, pProp);
            }
        } else {
            /* The Tcl script failed. Return the default property value. */
            pProp = &pEntry->initial;
            Tcl_BackgroundError(interp);
        }
    }

    if (pProp != pOut) {
        assert(pProp->eType != CSS_TYPE_TCL);
        setPropertyCache(pPropertyCache, prop, pProp);
    }
    return pProp;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeGetProperty --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void HtmlNodeGetProperty(interp, pNode, prop, pOut)
    Tcl_Interp *interp;
    HtmlNode *pNode; 
    int prop; 
    CssProperty *pOut;
{
    PropMapEntry *pEntry;
    CssProperty *pProp;

    /* If the property map is not already initialized, do so now. */
    if (!propMapisInit) {
        int i;
        memset(propmap, 0, sizeof(propmap));
        for (i=0; i<sizeof(propmapdata)/sizeof(PropMapEntry); i++) {
             PropMapEntry *p = &propmapdata[i];
             propmap[p->property] = p;
        }
        propMapisInit = 1;
    }

    pEntry = propmap[prop];
    assert(pEntry);
    pProp = getProperty(interp, pNode, pEntry, 0, pOut);
    if (pProp != pOut) {
        *pOut = *pProp;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeGetDefault --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void HtmlNodeGetDefault(pNode, prop, pOut)
    HtmlNode *pNode; 
    int prop; 
    CssProperty *pOut;
{
    PropMapEntry *pEntry = propmap[prop];
    assert(pEntry);
    *pOut = pEntry->initial;
}

