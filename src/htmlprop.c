
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
 * mapBackgroundColor --
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
mapBackgroundColor(pNode, pOut)
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
            return mapBackgroundColor(pParent, pOut);
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
 * A special value that the eType field of an initial property value can
 * take. If eType==CSS_TYPE_SAMEASCOLOR, then the value of the 'color'
 * property is used as the initial value for the property.
 */
#define CSS_TYPE_COPYCOLOR -1

/* 
 * This macro just makes the array definition below format more neatly.
 */
#define CSS_NONE {CSS_TYPE_STRING, "none"}

struct PropMapEntry {
    int property;
    int inherit;
    int attrinherit;
    int(*xAttrmap)(HtmlNode *, CssProperty *);
    CssProperty initial;
};
typedef struct PropMapEntry PropMapEntry;

static PropMapEntry propmapdata[] = {
    {CSS_PROPERTY_DISPLAY, 0, 0, 0, {CSS_TYPE_STRING, "inline"}},

    {CSS_PROPERTY_BACKGROUND_COLOR, 0, 0, mapBackgroundColor, 
            {CSS_TYPE_STRING, "transparent"}},
    {CSS_PROPERTY_COLOR, 1, 0, mapColor, {CSS_TYPE_STRING, "black"}},
    {CSS_PROPERTY_WIDTH, 0, 0, mapWidth, {CSS_TYPE_NONE, 0}},
    {CSS_PROPERTY_HEIGHT, 0, 0, mapHeight, {CSS_TYPE_NONE, 0}},

    /* Font and text related properties */
    {CSS_PROPERTY_TEXT_DECORATION, 1, 0, 0,     {CSS_TYPE_NONE, 0}},
    {CSS_PROPERTY_FONT_SIZE, 0, 0, mapFontSize, {CSS_TYPE_NONE, 0}},
    {CSS_PROPERTY_WHITE_SPACE, 1, 0, 0, {CSS_TYPE_NONE, 0}},
    {CSS_PROPERTY_VERTICAL_ALIGN,0,0,mapVAlign,{CSS_TYPE_STRING, "baseline"}},

    /* Width of borders. */
    {CSS_PROPERTY_BORDER_TOP_WIDTH,    0, 0, mapBorderWidth, {CSS_TYPE_PX, 0}},
    {CSS_PROPERTY_BORDER_BOTTOM_WIDTH, 0, 0, mapBorderWidth, {CSS_TYPE_PX, 0}},
    {CSS_PROPERTY_BORDER_LEFT_WIDTH,   0, 0, mapBorderWidth, {CSS_TYPE_PX, 0}},
    {CSS_PROPERTY_BORDER_RIGHT_WIDTH,  0, 0, mapBorderWidth, {CSS_TYPE_PX, 0}},

    /* Color of borders. */
    {CSS_PROPERTY_BORDER_TOP_COLOR,    0, 0, 0, {CSS_TYPE_COPYCOLOR, 0}},
    {CSS_PROPERTY_BORDER_BOTTOM_COLOR, 0, 0, 0, {CSS_TYPE_COPYCOLOR, 0}},
    {CSS_PROPERTY_BORDER_LEFT_COLOR,   0, 0, 0, {CSS_TYPE_COPYCOLOR, 0}},
    {CSS_PROPERTY_BORDER_RIGHT_COLOR,  0, 0, 0, {CSS_TYPE_COPYCOLOR, 0}},

    /* Style of borders. */
    {CSS_PROPERTY_BORDER_TOP_STYLE,    0, 0, mapBorderStyle, CSS_NONE},
    {CSS_PROPERTY_BORDER_BOTTOM_STYLE, 0, 0, mapBorderStyle, CSS_NONE},
    {CSS_PROPERTY_BORDER_LEFT_STYLE,   0, 0, mapBorderStyle, CSS_NONE},
    {CSS_PROPERTY_BORDER_RIGHT_STYLE,  0, 0, mapBorderStyle, CSS_NONE},

    /* Padding */
    {CSS_PROPERTY_PADDING_TOP,    0, 0, mapPadding, {CSS_TYPE_PX, 0}},
    {CSS_PROPERTY_PADDING_LEFT,   0, 0, mapPadding, {CSS_TYPE_PX, 0}},
    {CSS_PROPERTY_PADDING_RIGHT,  0, 0, mapPadding, {CSS_TYPE_PX, 0}},
    {CSS_PROPERTY_PADDING_BOTTOM, 0, 0, mapPadding, {CSS_TYPE_PX, 0}},

    /* Margins */
    {CSS_PROPERTY_MARGIN_TOP,    0, 0, 0, {CSS_TYPE_PX, 0}},
    {CSS_PROPERTY_MARGIN_LEFT,   0, 0, 0, {CSS_TYPE_PX, 0}},
    {CSS_PROPERTY_MARGIN_RIGHT,  0, 0, 0, {CSS_TYPE_PX, 0}},
    {CSS_PROPERTY_MARGIN_BOTTOM, 0, 0, 0, {CSS_TYPE_PX, 0}},

    /* Custom Tkhtml properties */
    {CSS_PROPERTY__TKHTML_REPLACE, 0, 0, 0, {CSS_TYPE_NONE, 0}},
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
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void getProperty(interp, pNode, pEntry, inheriting, pOut)
    Tcl_Interp *interp;
    HtmlNode *pNode;
    PropMapEntry *pEntry;
    int inheriting;
    CssProperty *pOut;
{
    CssProperty *pProp = 0;
    HtmlNode *pN;
    int spec = -1;
    int sheet = -1;
    int prop = pEntry->property;
    HtmlCachedProperty *pCache;

    /* Before looking anywhere else, see if this property is present in the
     * nodes local cache. This can happen if a Tcl script was invoked to
     * find the value of the same property earlier.
     */
    for (pCache=pNode->pCache; pCache; pCache=pCache->pNext) {
        if (pCache->iProp==prop) {
            if (pCache->pProp) {
                *pOut = *pCache->pProp;
            } else {
                *pOut = pEntry->initial;
            }
            goto getproperty_out;
        }
    }

    pN = pNode;
    while (pN) {
        int sh;
        int sp;
        pProp = HtmlCssPropertiesGet2(pN->pProperties, prop, &sh, &sp);
        if (pProp && (sh>sheet || (sh==sheet && sp>spec))) {
            *pOut = *pProp;
            sheet = sh;
            spec = sp;
        }
        if (pEntry->inherit) {
            pN = HtmlNodeParent(pN);
        } else {
            pN = 0;
        }
    }
    if (sheet>1 || (sheet==1&&spec>10000)) {
        goto getproperty_out;
    }

    /* See if the property was specified as part of a 'style' attribute. */
    pN = pNode;
    while (pN) {
        if (pN->pStyle) {
            pProp = HtmlCssPropertiesGet(pN->pStyle, prop);
            if (pProp) {
                *pOut = *pProp;
                goto getproperty_out;
            }
        }
        if (pEntry->inherit) {
            pN = HtmlNodeParent(pN);
        } else {
            pN = 0;
        }
    }
    if (sheet>0) {
        goto getproperty_out;
    }

    /* See if we can get the property by translating an HTML attribute. */
    pN = pNode;
    while (pN && pEntry->xAttrmap) {
        if (pEntry->xAttrmap(pN, pOut)) {
            goto getproperty_out;
        }
        if (pEntry->attrinherit) {
            pN = HtmlNodeParent(pN);
        } else {
            pN = 0;
        }
    }

    /* None of the above worked, return the default property value. */
    if (sheet<0) {
        if (pEntry->initial.eType==CSS_TYPE_COPYCOLOR) {
            HtmlNodeGetProperty(interp, pNode, CSS_PROPERTY_COLOR, pOut);
        } else {
            *pOut = pEntry->initial;
        }
    }

getproperty_out:
    /* At this point a property has been assigned and copied to *pOut. This
     * step checks if the property is of type CSS_TYPE_TCL. If so, we need
     * to invoke a Tcl script to retrieve the value of this property.
     */
    if (pOut->eType==CSS_TYPE_TCL) {
        int rc;
        Tcl_Obj *pNodeCmd = HtmlNodeCommand(interp, pNode);
        Tcl_Obj *pEval = Tcl_NewStringObj(pOut->v.zVal, -1);

        Tcl_IncrRefCount(pEval);
        Tcl_ListObjAppendElement(0, pEval, pNodeCmd);
        rc = Tcl_EvalObjEx(interp, pEval, TCL_EVAL_GLOBAL|TCL_EVAL_DIRECT);
        Tcl_DecrRefCount(pEval);

        pCache = (HtmlCachedProperty *)ckalloc(sizeof(HtmlCachedProperty));
        pCache->iProp = prop;
        pCache->pNext = pNode->pCache;
        pNode->pCache = pCache;

        if (rc==TCL_OK) {
            /* If the Tcl script was successful, interpret the returned
             * string as a property value and return it. Put the
             * interpreted property value in the nodes property cache so
             * that we don't have to execute the Tcl script again.
             */
            Tcl_Obj *pResult;
            CssProperty *pProp;

            pResult = Tcl_GetObjResult(interp);
            pProp = HtmlCssStringToProperty(Tcl_GetString(pResult), -1);
            pCache->pProp = pProp;
            if (pProp->eType==CSS_TYPE_TCL) {
                *pProp = pEntry->initial;
            }
            *pOut = *pProp;
        } else {
            /* The Tcl script failed. Return the default property value.
	     * Set the pointer in the cache to NULL so that we don't
	     * execute this script next time.
            */
            pCache->pProp = 0;
            *pOut = pEntry->initial;
        }
    }
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
    getProperty(interp, pNode, pEntry, 0, pOut);
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

