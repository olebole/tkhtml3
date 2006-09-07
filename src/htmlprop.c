
/*
 * htmlprop.c ---
 *
 *     This file implements the mapping between HTML attributes and CSS
 *     properties.
 *
 *--------------------------------------------------------------------------
 * Copyright (c) 2005 Eolas Technologies Inc.
 * All rights reserved.
 *
 * This Open Source project was made possible through the financial support
 * of Eolas Technologies Inc.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <ORGANIZATION> nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
static const char rcsid[] = "$Id: htmlprop.c,v 1.93 2006/09/07 14:41:52 danielk1977 Exp $";

#include "html.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* #define ACCEPT_UNITLESS_LENGTHS */

#define LOG if (p->pTree->options.logcmd && p->pNode)

/*
 * Convert a double value from a CssProperty to an integer.
 */
#define INTEGER(x) ((int)((x) + ((x > 0.0) ? 0.49 : -0.49)))

/*
 * The properties table. This data structure describes the way in
 * which each individual property should be handled in the following
 * use cases:
 *
 *     HtmlNodeGetProperty()       - Format property value as string
 *     HtmlNodeProperties()        - Format property value as string
 *
 *     HtmlComputedValuesCompare() - Compare property values and determine if
 *                                   relayout or repainting is required by the
 *                                   changes.
 *     HtmlComputedValuesInit()    - Initialise property value.
 *
 *     HtmlComputedValuesRelease() - Delete ref to property value.
 *     HtmlComputedValuesSet()     - Set property value.
 */

enum PropertyValueType {
    ENUM, COLOR, LENGTH, IMAGE, BORDERWIDTH, CUSTOM
};

typedef struct PropertyDef PropertyDef;
struct PropertyDef {
    enum PropertyValueType eType;
    int eProp;
    int iOffset;
    int mask;
    int iDefault;              /* For LENGTH and BORDERWIDTH */

    int setsizemask;           /* If eType==LENGTH, mask for SetSize() */
    int (*xSet)(HtmlComputedValuesCreator *, CssProperty *);
    int isInherit;             /* True to inherit by default */
    int isNolayout;            /* Can be changed without relayout */
};

#define PROPDEF(w, x, y) {                                   \
  w, CSS_PROPERTY_ ## x, Tk_Offset(HtmlComputedValues, y), 0 \
}
#define PROPDEFM(w, x, y, z) {                              \
  w, CSS_PROPERTY_ ## x, Tk_Offset(HtmlComputedValues, y),  \
  PROP_MASK_ ## x, z                                        \
}

static PropertyDef propdef[] = {
  PROPDEF(ENUM, BACKGROUND_ATTACHMENT, eBackgroundAttachment),
  PROPDEF(ENUM, BACKGROUND_REPEAT,     eBackgroundRepeat),
  PROPDEF(ENUM, BORDER_BOTTOM_STYLE,   eBorderBottomStyle),
  PROPDEF(ENUM, BORDER_LEFT_STYLE,     eBorderLeftStyle),
  PROPDEF(ENUM, BORDER_RIGHT_STYLE,    eBorderRightStyle),
  PROPDEF(ENUM, BORDER_TOP_STYLE,      eBorderTopStyle),
  PROPDEF(ENUM, CLEAR,                 eClear), 
  PROPDEF(ENUM, DISPLAY,               eDisplay), 
  PROPDEF(ENUM, FLOAT,                 eFloat), 
  PROPDEF(ENUM, LIST_STYLE_POSITION,   eListStylePosition),
  PROPDEF(ENUM, LIST_STYLE_TYPE,       eListStyleType),
  PROPDEF(ENUM, OUTLINE_STYLE,         eOutlineStyle),
  PROPDEF(ENUM, OVERFLOW,              eOverflow),
  PROPDEF(ENUM, POSITION,              ePosition),
  PROPDEF(ENUM, TEXT_ALIGN,            eTextAlign), 
  PROPDEF(ENUM, TEXT_DECORATION,       eTextDecoration), 
  PROPDEF(ENUM, WHITE_SPACE,           eWhitespace), 
  PROPDEF(ENUM, BORDER_COLLAPSE,       eBorderCollapse),
  PROPDEF(ENUM, DIRECTION,             eDirection),
  PROPDEF(ENUM, CAPTION_SIDE,          eCaptionSide),
  PROPDEF(ENUM, EMPTY_CELLS,           eEmptyCells),
  PROPDEF(ENUM, FONT_VARIANT,          eFontVariant),
  PROPDEF(ENUM, TABLE_LAYOUT,          eTableLayout),
  PROPDEF(ENUM, TEXT_TRANSFORM,        eTextTransform),
  PROPDEF(ENUM, UNICODE_BIDI,          eUnicodeBidi),
  PROPDEF(ENUM, VISIBILITY,            eVisibility),

  /* Note: The CSS2 property 'border-spacing' can be set to
   * either a single or pair of length values. Only a single
   * value is supported at the moment, which is enough to support
   * the html 4.01 cellspacing attribute.
   */
  PROPDEFM(LENGTH, BORDER_SPACING,        iBorderSpacing,    0),
  PROPDEFM(LENGTH, BACKGROUND_POSITION_X, iBackgroundPositionX, 0),
  PROPDEFM(LENGTH, BACKGROUND_POSITION_Y, iBackgroundPositionY, 0),
  PROPDEFM(LENGTH, BOTTOM,                position.iBottom,  PIXELVAL_AUTO),
  PROPDEFM(LENGTH, HEIGHT,                iHeight,           PIXELVAL_AUTO),
  PROPDEFM(LENGTH, LEFT,                  position.iLeft,    PIXELVAL_AUTO),
  PROPDEFM(LENGTH, MARGIN_BOTTOM,         margin.iBottom,    0),
  PROPDEFM(LENGTH, MARGIN_LEFT,           margin.iLeft,      0),
  PROPDEFM(LENGTH, MARGIN_RIGHT,          margin.iRight,     0),
  PROPDEFM(LENGTH, MARGIN_TOP,            margin.iTop,       0),
  PROPDEFM(LENGTH, MAX_HEIGHT,            iMaxHeight,        PIXELVAL_NONE),
  PROPDEFM(LENGTH, MAX_WIDTH,             iMaxWidth,         PIXELVAL_NONE),
  PROPDEFM(LENGTH, MIN_HEIGHT,            iMinHeight,        0),
  PROPDEFM(LENGTH, MIN_WIDTH,             iMinWidth,         0),
  PROPDEFM(LENGTH, PADDING_BOTTOM,        padding.iBottom,   0),
  PROPDEFM(LENGTH, PADDING_LEFT,          padding.iLeft,     0),
  PROPDEFM(LENGTH, PADDING_RIGHT,         padding.iRight,    0),
  PROPDEFM(LENGTH, PADDING_TOP,           padding.iTop,      0),
  PROPDEFM(LENGTH, RIGHT,                 position.iRight,   PIXELVAL_AUTO),
  PROPDEFM(LENGTH, TEXT_INDENT,           iTextIndent,       0),
  PROPDEFM(LENGTH, TOP,                   position.iTop,     PIXELVAL_AUTO),
  PROPDEFM(LENGTH, WIDTH,                 iWidth,            PIXELVAL_AUTO),
  PROPDEFM(LENGTH, WORD_SPACING,          iWordSpacing,      PIXELVAL_NORMAL),
  PROPDEFM(LENGTH, LETTER_SPACING,        iLetterSpacing,    PIXELVAL_NORMAL),

  PROPDEF(COLOR, BACKGROUND_COLOR,        cBackgroundColor),
  PROPDEF(COLOR, COLOR,                   cColor),
  PROPDEF(COLOR, BORDER_TOP_COLOR,        cBorderTopColor),
  PROPDEF(COLOR, BORDER_RIGHT_COLOR,      cBorderRightColor),
  PROPDEF(COLOR, BORDER_LEFT_COLOR,       cBorderLeftColor),
  PROPDEF(COLOR, BORDER_BOTTOM_COLOR,     cBorderBottomColor),
  PROPDEF(COLOR, OUTLINE_COLOR,           cOutlineColor),

  PROPDEF(IMAGE, _TKHTML_REPLACEMENT_IMAGE, imReplacementImage),
  PROPDEF(IMAGE, BACKGROUND_IMAGE,          imBackgroundImage),
  PROPDEF(IMAGE, LIST_STYLE_IMAGE,          imListStyleImage),

  PROPDEFM(BORDERWIDTH, BORDER_TOP_WIDTH,    border.iTop,    2),
  PROPDEFM(BORDERWIDTH, BORDER_LEFT_WIDTH,   border.iLeft,   2),
  PROPDEFM(BORDERWIDTH, BORDER_RIGHT_WIDTH,  border.iRight,  2),
  PROPDEFM(BORDERWIDTH, BORDER_BOTTOM_WIDTH, border.iBottom, 2),
  PROPDEFM(BORDERWIDTH, OUTLINE_WIDTH,       iOutlineWidth,  2),

  PROPDEF(CUSTOM, VERTICAL_ALIGN,            iVerticalAlign),
  PROPDEF(CUSTOM, LINE_HEIGHT,               iLineHeight),
  PROPDEF(CUSTOM, Z_INDEX,                   iZIndex),

  PROPDEF(CUSTOM, FONT_SIZE,                 fFont),
  PROPDEF(CUSTOM, FONT_WEIGHT,               fFont),
  PROPDEF(CUSTOM, FONT_FAMILY,               fFont),
  PROPDEF(CUSTOM, FONT_STYLE,                fFont),

  PROPDEF(CUSTOM, CONTENT,                   fFont),
};

#define SZ_AUTO     0x00000001
#define SZ_INHERIT  0x00000002
#define SZ_NONE     0x00000004
#define SZ_PERCENT  0x00000008
#define SZ_NEGATIVE 0x00000010
#define SZ_NORMAL   0x00000020

#define SZMASKDEF(x, y) { CSS_PROPERTY_ ## x, y }
struct SizemaskDef {
  int eProp;
  int mask;
} sizemskdef[] = {
  SZMASKDEF(BORDER_SPACING,        0),
  SZMASKDEF(BACKGROUND_POSITION_X, SZ_INHERIT|SZ_PERCENT|SZ_NEGATIVE),
  SZMASKDEF(BACKGROUND_POSITION_Y, SZ_INHERIT|SZ_PERCENT|SZ_NEGATIVE),
  SZMASKDEF(BOTTOM,                SZ_INHERIT|SZ_PERCENT|SZ_AUTO|SZ_NEGATIVE),
  SZMASKDEF(HEIGHT,                SZ_INHERIT|SZ_PERCENT|SZ_AUTO),
  SZMASKDEF(LEFT,                  SZ_INHERIT|SZ_PERCENT|SZ_AUTO|SZ_NEGATIVE),
  SZMASKDEF(MARGIN_BOTTOM,         SZ_INHERIT|SZ_PERCENT|SZ_AUTO|SZ_NEGATIVE),
  SZMASKDEF(MARGIN_LEFT,           SZ_INHERIT|SZ_PERCENT|SZ_AUTO|SZ_NEGATIVE),
  SZMASKDEF(MARGIN_RIGHT,          SZ_INHERIT|SZ_PERCENT|SZ_AUTO|SZ_NEGATIVE),
  SZMASKDEF(MARGIN_TOP,            SZ_INHERIT|SZ_PERCENT|SZ_AUTO|SZ_NEGATIVE),
  SZMASKDEF(MAX_HEIGHT,            SZ_INHERIT|SZ_PERCENT|SZ_NONE),
  SZMASKDEF(MAX_WIDTH,             SZ_INHERIT|SZ_PERCENT|SZ_NONE),
  SZMASKDEF(MIN_HEIGHT,            SZ_INHERIT|SZ_PERCENT),
  SZMASKDEF(MIN_WIDTH,             SZ_INHERIT|SZ_PERCENT),
  SZMASKDEF(PADDING_BOTTOM,        SZ_INHERIT|SZ_PERCENT),
  SZMASKDEF(PADDING_LEFT,          SZ_INHERIT|SZ_PERCENT),
  SZMASKDEF(PADDING_RIGHT,         SZ_INHERIT|SZ_PERCENT),
  SZMASKDEF(PADDING_TOP,           SZ_INHERIT|SZ_PERCENT),
  SZMASKDEF(RIGHT,                 SZ_INHERIT|SZ_PERCENT|SZ_AUTO|SZ_NEGATIVE),
  SZMASKDEF(TEXT_INDENT,           SZ_INHERIT|SZ_PERCENT|SZ_NEGATIVE),
  SZMASKDEF(TOP,                   SZ_INHERIT|SZ_PERCENT|SZ_AUTO|SZ_NEGATIVE),
  SZMASKDEF(WIDTH,                 SZ_INHERIT|SZ_PERCENT|SZ_AUTO),
  SZMASKDEF(LETTER_SPACING,        SZ_INHERIT|SZ_NORMAL|SZ_NEGATIVE),
  SZMASKDEF(WORD_SPACING,          SZ_INHERIT|SZ_NORMAL|SZ_NEGATIVE),
};

static int propertyValuesSetFontSize(HtmlComputedValuesCreator*,CssProperty*);
static int propertyValuesSetLineHeight(HtmlComputedValuesCreator*,CssProperty*);
static int propertyValuesSetVerticalAlign(HtmlComputedValuesCreator*,CssProperty*);
static int propertyValuesSetFontStyle(HtmlComputedValuesCreator*,CssProperty*);
static int propertyValuesSetFontFamily(HtmlComputedValuesCreator*,CssProperty*);
static int propertyValuesSetFontWeight(HtmlComputedValuesCreator*,CssProperty*);

static int propertyValuesSetContent(HtmlComputedValuesCreator*,CssProperty*);
static int propertyValuesSetZIndex(HtmlComputedValuesCreator*,CssProperty*);

static struct CustomDef {
  int eProp;
  int (*xSet)(HtmlComputedValuesCreator *, CssProperty *);
} customdef[] = {
  {CSS_PROPERTY_VERTICAL_ALIGN, propertyValuesSetVerticalAlign},
  {CSS_PROPERTY_LINE_HEIGHT,    propertyValuesSetLineHeight},
  {CSS_PROPERTY_FONT_SIZE,      propertyValuesSetFontSize},
  {CSS_PROPERTY_FONT_WEIGHT,    propertyValuesSetFontWeight},
  {CSS_PROPERTY_FONT_STYLE,     propertyValuesSetFontStyle},
  {CSS_PROPERTY_FONT_FAMILY,    propertyValuesSetFontFamily},
  {CSS_PROPERTY_CONTENT,        propertyValuesSetContent},
  {CSS_PROPERTY_Z_INDEX,        propertyValuesSetZIndex},
};

static int inheritlist[] = {
    CSS_PROPERTY_LIST_STYLE_TYPE, CSS_PROPERTY_WHITE_SPACE,
    CSS_PROPERTY_TEXT_ALIGN,      CSS_PROPERTY_COLOR,
    CSS_PROPERTY_BORDER_SPACING,  CSS_PROPERTY_LINE_HEIGHT,
    CSS_PROPERTY_FONT_SIZE,       CSS_PROPERTY_FONT_STYLE,
    CSS_PROPERTY_FONT_WEIGHT,     CSS_PROPERTY_FONT_FAMILY,
    CSS_PROPERTY_TEXT_INDENT,     CSS_PROPERTY_BORDER_COLLAPSE,
    CSS_PROPERTY_CAPTION_SIDE,    CSS_PROPERTY_DIRECTION,
    CSS_PROPERTY_EMPTY_CELLS,     CSS_PROPERTY_FONT_VARIANT,
    CSS_PROPERTY_LETTER_SPACING,  CSS_PROPERTY_LIST_STYLE_IMAGE,
    CSS_PROPERTY_LIST_STYLE_TYPE, CSS_PROPERTY_LIST_STYLE_POSITION,
    CSS_PROPERTY_TEXT_TRANSFORM,  CSS_PROPERTY_VISIBILITY,
    CSS_PROPERTY_WORD_SPACING,    CSS_PROPERTY_CURSOR,
    CSS_PROPERTY_QUOTES
};

static int nolayoutlist[] = {
    CSS_PROPERTY_TEXT_DECORATION,
    CSS_PROPERTY_BACKGROUND_ATTACHMENT,
    CSS_PROPERTY_BACKGROUND_REPEAT,
    CSS_PROPERTY_VISIBILITY,
    CSS_PROPERTY_BACKGROUND_POSITION_X,
    CSS_PROPERTY_BACKGROUND_POSITION_Y
};


/*
 *---------------------------------------------------------------------------
 *
 * getPropertyDef --
 *
 * Results:
 *
 * Side effects:
 *     This function is not threadsafe.
 *
 *---------------------------------------------------------------------------
 */
static PropertyDef *getPropertyDef(int eProp){
    static int isInit = 0;
    static PropertyDef *a[CSS_PROPERTY_MAX_PROPERTY+1];
 
    assert(eProp >= 0);
    assert(eProp <= CSS_PROPERTY_MAX_PROPERTY);

    if (0 == isInit) {
        int i;
        memset(a, 0, (CSS_PROPERTY_MAX_PROPERTY+1) * sizeof(PropertyDef *));
        for (i = 0; i < sizeof(propdef)/sizeof(PropertyDef); i++){
            int eCss = propdef[i].eProp;
            assert(eCss >= 0 && eCss <= CSS_PROPERTY_MAX_PROPERTY);
            a[eCss] = &propdef[i];
        } 
        for (i = 0; i < sizeof(sizemskdef)/sizeof(struct SizemaskDef); i++){
          a[sizemskdef[i].eProp]->setsizemask = sizemskdef[i].mask;
        }
        for (i = 0; i < sizeof(customdef)/sizeof(struct CustomDef); i++){
          a[customdef[i].eProp]->xSet = customdef[i].xSet;
        }
        for (i = 0; i < sizeof(inheritlist)/sizeof(int); i++){
            if (a[inheritlist[i]]) {
                a[inheritlist[i]]->isInherit = 1;
            }
        }
        for (i = 0; i < sizeof(nolayoutlist)/sizeof(int); i++){
            if (a[nolayoutlist[i]]) {
                a[nolayoutlist[i]]->isNolayout = 1;
            }
        }
        isInit = 1;
    }
    return a[eProp];
}


/*
 *---------------------------------------------------------------------------
 *
 * propertyToString --
 *
 *     This function is used for logging debugging info only.
 * 
 *     Return a pointer to a string representation of the CSS specified
 *     value contained in argument pProp. *pzFree is set to the value
 *     of a pointer (possibly NULL) that should be passed to HtmlFree(0, )
 *     when the returned string is no longer required. Example:
 *
 *         char *zFree;
 *         char *zString;
 * 
 *         zString = propertyToString(pProp, &zFree);
 *         // Use zString for something (i.e. print to stdout)
 *         HtmlFree(0, zFree);
 *
 * Results:
 *     Pointer to string representation of property pProp.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
char *
HtmlPropertyToString(pProp, pzFree)
    CssProperty *pProp;
    char **pzFree;
{
    char *zRet = (char *)HtmlCssPropertyGetString(pProp);
    *pzFree = 0;

    if (!zRet) {
        if (pProp->eType == CSS_TYPE_TCL || pProp->eType == CSS_TYPE_URL) {
            zRet = HtmlAlloc(0, strlen(pProp->v.zVal) + 6);
            sprintf(zRet, "%s(%s)", 
                    (pProp->eType==CSS_TYPE_TCL)?"tcl":"url", pProp->v.zVal
            );
        } else {
            char *zSym = 0;
            switch (pProp->eType) {
                case CSS_TYPE_EM:         zSym = "em"; break;
                case CSS_TYPE_PX:         zSym = "px"; break;
                case CSS_TYPE_PT:         zSym = "pt"; break;
                case CSS_TYPE_PC:         zSym = "pc"; break;
                case CSS_TYPE_EX:         zSym = "ex"; break;
                case CSS_TYPE_PERCENT:    zSym = "%"; break;
                case CSS_TYPE_FLOAT:      zSym = ""; break;
                case CSS_TYPE_CENTIMETER: zSym = "cm"; break;
                case CSS_TYPE_INCH:       zSym = "in"; break;
                case CSS_TYPE_MILLIMETER: zSym = "mm"; break;
                default:
                    assert(!"Unknown CssProperty.eType value");
            }

            zRet = HtmlAlloc(0, 128);
            sprintf(zRet, "%.2f%s", pProp->v.rVal, zSym);
        }
        *pzFree = zRet;
    }

    return zRet;
}

/*
 *---------------------------------------------------------------------------
 *
 * pixelsToPoints --
 *
 *     Convert a pixel length to points (1/72 of an inch). 
 *
 *     Note: An "inch" is an anachronism still in use in some of the more
 *           stubborn countries. It is equivalent to approximately 25.4
 *           millimeters. 
 *
 * Results:
 *     Integer length in points.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static float 
pixelsToPoints(p, pixels)
    HtmlComputedValuesCreator *p;
    int pixels;
{
    double mm;
    Tcl_Obj *pObj = Tcl_NewIntObj(pixels);
    Tcl_IncrRefCount(pObj);
    Tk_GetMMFromObj(p->pTree->interp, p->pTree->tkwin, pObj, &mm);
    Tcl_DecrRefCount(pObj);
    return (mm * 72.0 / 25.4);
}

/*
 *---------------------------------------------------------------------------
 *
 * physicalToPixels --
 *
 *     This function is a wrapper around Tk_GetPixels(), used to convert
 *     physical units to pixels. The first argument is the layout-context.
 *     The second argument is the distance in terms of the physical unit
 *     being converted from. The third argument determines the unit type,
 *     as follows:
 *
 *         Character          Unit
 *         ------------------------------
 *         'c'                Centimeters
 *         'i'                Inches
 *         'm'                Millimeters
 *         'p'                Points (1 point = 1/72 inches)
 *
 *     The value returned is the distance in pixels.
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
physicalToPixels(p, rVal, type)
    HtmlComputedValuesCreator *p;
    double rVal;
    char type;
{
    char zBuf[64];
    int pixels;
    sprintf(zBuf, "%f%c", rVal, type);
    Tk_GetPixels(p->pTree->interp, p->pTree->tkwin, zBuf, &pixels);
    return pixels;
}

/*
 *---------------------------------------------------------------------------
 *
 * propertyValuesSetFontStyle --
 *
 * Keywords 'italic' and 'oblique' map to a Tk italic font. Keyword
 * 'normal' maps to a non-italic font. Any other property value is
 * rejected as a type-mismatch.
 *
 * Results: 
 *     0 if value is successfully set. 1 if the value of *pProp is not a valid
 *     a value for the 'font-style' property.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
propertyValuesSetFontStyle(p, pProp)
    HtmlComputedValuesCreator *p;
    CssProperty *pProp;
{
    int eType = pProp->eType;
    if (eType == CSS_CONST_INHERIT) {
        int i = p->pParent->pPropertyValues->fFont->pKey->isItalic;;
        p->fontKey.isItalic = i;
    }else if (eType == CSS_CONST_ITALIC || eType == CSS_CONST_OBLIQUE) {
        p->fontKey.isItalic = 1;
    } else if (eType == CSS_CONST_NORMAL) {
        p->fontKey.isItalic = 0;
    } else {
        return 1;
    }
    return 0;
}

static int 
propertyValuesSetContent(p, pProp)
    HtmlComputedValuesCreator *p;
    CssProperty *pProp;
{
    if (pProp->eType == CSS_TYPE_STRING && p->pzContent) {
        int nBytes = strlen(pProp->v.zVal) + 1;
        *(p->pzContent) = HtmlAlloc(0, nBytes);
        strcpy(*(p->pzContent), pProp->v.zVal);
        return 0;
    }
    return 1;
}

static int
propertyValuesSetZIndex(p, pProp)
    HtmlComputedValuesCreator *p;
    CssProperty *pProp;
{
    if (pProp->eType == CSS_TYPE_FLOAT) {
        p->values.iZIndex = (int)pProp->v.rVal;
    }else if (pProp->eType == CSS_CONST_AUTO) {
        p->values.iZIndex = PIXELVAL_AUTO;
    }else{
        /* Type mismatch */
        return 1;
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * propertyValuesSetFontWeight --
 *
 *     Keywords 'bold' and 'bolder', and numeric values greater than 550 map to
 *     a Tk bold font. Keywords 'normal' and 'lighter', along with numeric
 *     values less than 550 map to a non-bold font. Any other property value is
 *     rejected as a type-mismatch.
 *
 * Results: 
 *     0 if value is successfully set. 1 if the value of *pProp is not a valid
 *     a value for the 'font-weight' property.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
propertyValuesSetFontWeight(p, pProp)
    HtmlComputedValuesCreator *p;
    CssProperty *pProp;
{
    int eType = pProp->eType;
    if (eType == CSS_CONST_INHERIT) {
        HtmlNode *pParent = p->pParent;
        if (pParent) {
            int i = pParent->pPropertyValues->fFont->pKey->isBold;
            p->fontKey.isBold = i;
        }
    }
    else if (
        eType == CSS_CONST_BOLD || 
        eType == CSS_CONST_BOLDER ||
        (eType == CSS_TYPE_FLOAT && pProp->v.rVal > 550.0)
    ) {
        p->fontKey.isBold = 1;
    }
    else if (
        eType == CSS_CONST_NORMAL || 
        eType == CSS_CONST_LIGHTER ||
        (eType == CSS_TYPE_FLOAT && pProp->v.rVal < 550.0)
    ) {
        p->fontKey.isBold = 0;
    } else {
        return 1;
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * propertyValuesSetFontFamily --
 *
 *     Just copy the pointer, not the string.
 *
 * Results: 
 *     0 if value is successfully set. 1 if the value of *pProp is not a valid
 *     a value for the 'font-family' property.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
propertyValuesSetFontFamily(p, pProp)
    HtmlComputedValuesCreator *p;
    CssProperty *pProp;
{
    const char *z;

    /* Handle 'inherit' */
    if (pProp->eType == CSS_CONST_INHERIT) {
        HtmlNode *pParent = p->pParent;
        if (pParent) {
            z = pParent->pPropertyValues->fFont->pKey->zFontFamily;
            p->fontKey.zFontFamily = z;
        }
        return 0;
    }

    z = HtmlCssPropertyGetString(pProp);
    if (!z) {
        return 1;
    }
    p->fontKey.zFontFamily = z;
    return 0;
}
 

/*
 *---------------------------------------------------------------------------
 *
 * propertyValuesSetFontSize --
 *
 *     This function sets the HtmlComputedValuesCreator.fontKey.iFontSize
 *     variable in *p based on the value stored in property *pProp. This
 *     function handles the value 'inherit'.
 *
 * Results: 
 *     0 if value is successfully set. 1 if the value of *pProp is not a valid
 *     a value for the 'font-size' property.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
propertyValuesSetFontSize(p, pProp)
    HtmlComputedValuesCreator *p;
    CssProperty *pProp;
{
    int iPoints = 0;
    int iPixels = 0;
    double iScale = -1.0;
    assert(pProp);

    /* Handle 'inherit' separately. */
    if (pProp->eType == CSS_CONST_INHERIT) {
        HtmlNode *pParent = p->pParent;
        if (pParent) {
            int i = pParent->pPropertyValues->fFont->pKey->iFontSize;
            p->fontKey.iFontSize = i;
        }
        return 0;
    }

    switch (pProp->eType) {
        /* Font-size is in terms of parent node font size */
        case CSS_TYPE_EM:
            iScale = (double)pProp->v.rVal;
            break;
        case CSS_TYPE_EX: {
            HtmlNode *pParent = p->pParent;
            if (pParent) {
                HtmlFont *pFont = pParent->pPropertyValues->fFont;
                iScale = (double)pProp->v.rVal * 
                    ((double)(pFont->ex_pixels) / (double)(pFont->em_pixels));
            } else {
                iScale = 1.0;    /* Just to prevent type-mismatch error */
            }
            break;
        }
        case CSS_TYPE_PERCENT:
            iScale = pProp->v.rVal * 0.01;
            break;

        case CSS_CONST_SMALLER: {
            HtmlNode *pParent = p->pParent;
            if (pParent) {
                int ii;
                int *aSize = p->pTree->aFontSizeTable;
                int ps = pParent->pPropertyValues->fFont->pKey->iFontSize;
                int points = ps / HTML_IFONTSIZE_SCALE;
                for (ii = 1; ii < 7 && aSize[ii] < points; ii++);
                iPoints = ps + (aSize[ii-1] - aSize[ii]) * HTML_IFONTSIZE_SCALE;
            } else {
                iPoints = p->pTree->aFontSizeTable[2] * HTML_IFONTSIZE_SCALE;
            }
            break;
        }
        case CSS_CONST_LARGER: {
            HtmlNode *pParent = p->pParent;
            if (pParent) {
                int ii;
                int *aSize = p->pTree->aFontSizeTable;
                int ps = pParent->pPropertyValues->fFont->pKey->iFontSize;
                int points = ps / HTML_IFONTSIZE_SCALE;
                for (ii = 0; ii < 6 && aSize[ii] < points; ii++);
                iPoints = ps + (aSize[ii+1] - aSize[ii]) * HTML_IFONTSIZE_SCALE;
            } else {
                iPoints = p->pTree->aFontSizeTable[2] * HTML_IFONTSIZE_SCALE;
            }
            break;
        }

        /* Font-size is in terms of the font-size table */
        case CSS_CONST_XX_SMALL:
            iPoints = p->pTree->aFontSizeTable[0];
            iPoints = iPoints * HTML_IFONTSIZE_SCALE;
            break;
        case CSS_CONST_X_SMALL:
            iPoints = p->pTree->aFontSizeTable[1];
            iPoints = iPoints * HTML_IFONTSIZE_SCALE;
            break;
        case CSS_CONST_SMALL:
            iPoints = p->pTree->aFontSizeTable[2];
            iPoints = iPoints * HTML_IFONTSIZE_SCALE;
            break;
        case CSS_CONST_MEDIUM:
            iPoints = p->pTree->aFontSizeTable[3];
            iPoints = iPoints * HTML_IFONTSIZE_SCALE;
            break;
        case CSS_CONST_LARGE:
            iPoints = p->pTree->aFontSizeTable[4];
            iPoints = iPoints * HTML_IFONTSIZE_SCALE;
            break;
        case CSS_CONST_X_LARGE:
            iPoints = p->pTree->aFontSizeTable[5];
            iPoints = iPoints * HTML_IFONTSIZE_SCALE;
            break;
        case CSS_CONST_XX_LARGE:
            iPoints = p->pTree->aFontSizeTable[6];
            iPoints = iPoints * HTML_IFONTSIZE_SCALE;
            break;

        /* Font-size is in physical units (except points or picas) */
        case CSS_TYPE_CENTIMETER:
            iPixels = physicalToPixels(p, pProp->v.rVal, 'c');
            break;
        case CSS_TYPE_MILLIMETER:
            iPixels = physicalToPixels(p, pProp->v.rVal, 'm');
            break;
        case CSS_TYPE_INCH:
            iPixels = physicalToPixels(p, pProp->v.rVal, 'i');
            break;

        /* Font-size is in pixels */
        case CSS_TYPE_PX:
            iPixels = INTEGER(pProp->v.rVal);
            break;

        /* Font-size is already in points or picas*/
        case CSS_TYPE_PC:
            iPoints = (int)(pProp->v.rVal * HTML_IFONTSIZE_SCALE / 12.0);
            break;
        case CSS_TYPE_PT:
            iPoints = INTEGER(HTML_IFONTSIZE_SCALE * pProp->v.rVal);
            break;

        default:   /* Type-mismatch error */
            return 1;
    }

    if (iPixels > 0) {
        p->fontKey.iFontSize = HTML_IFONTSIZE_SCALE * pixelsToPoints(p,iPixels);
    } else if (iPoints > 0) {
        p->fontKey.iFontSize = iPoints;
    } else if (iScale > 0.0) {
       HtmlNode *pParent = p->pParent;
       if (pParent) {
           HtmlFont *pFont = pParent->pPropertyValues->fFont;
           p->fontKey.iFontSize = pFont->pKey->iFontSize * iScale;
       }
    } else {
        return 1;
    }

    return 0;
}

static unsigned char *
getInheritPointer(p, pVar)
    HtmlComputedValuesCreator *p;
    unsigned char *pVar;
{
    const int values_offset = Tk_Offset(HtmlComputedValuesCreator, values);
    const int fontkey_offset = Tk_Offset(HtmlComputedValuesCreator, fontKey);
    const int values_end = values_offset + sizeof(HtmlComputedValues);

#ifndef NDEBUG
    const int fontkey_end = fontkey_offset + sizeof(HtmlFontKey);
#endif

    int offset = pVar - (unsigned char *)p;
    HtmlNode *pParent = p->pParent;

    assert(
        values_offset >= 0 &&
        fontkey_offset >= 0 &&
        values_end > 0 && values_end > values_offset &&
        fontkey_end > 0 && fontkey_end > fontkey_offset
    );

    assert(offset >= 0);
    assert(
        (offset >= values_offset && offset < values_end) ||
        (offset >= fontkey_offset && offset < fontkey_end)
    );

    if (pParent) {
        unsigned char *pV; 

        if (offset >= values_offset && offset < values_end) {
            pV = (unsigned char *)pParent->pPropertyValues;
            assert(pV);
            return (pV + (offset - values_offset));
        } else {
            pV = (unsigned char *)(pParent->pPropertyValues->fFont->pKey);
            assert(pV);
            return (pV + (offset - fontkey_offset));
        }
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * propertyValuesSetEnum --
 *
 *     aOptions is a 0-terminated list of uchars (all CSS_CONST_XXX values).
 *     If pProp contains a constant string that matches an entry in aOptions,
 *     copy the constant value to *pEVar and return 0. Otherwise return 1 and
 *     leave *pEVar untouched.
 *
 * Results:
 *     See above.
 *
 * Side effects:
 *     May copy pProp->eType to *pEVar.
 *
 *---------------------------------------------------------------------------
 */
static int 
propertyValuesSetEnum(p, pEVar, aOptions, pProp)
    HtmlComputedValuesCreator *p;
    unsigned char *pEVar;
    unsigned char *aOptions;
    CssProperty *pProp;
{
    int val = pProp->eType;
    unsigned char *pOpt;

    if (val == CSS_CONST_INHERIT) {
        unsigned char *pInherit = getInheritPointer(p, pEVar);
        if (pInherit) {
            *pEVar = *pInherit;
        }
        return 0;
    }

    for (pOpt = aOptions; *pOpt; pOpt++) {
        if (*pOpt == val) {
            *pEVar = (unsigned char)val;
            return 0;
        }
    }
    return 1;
}

/*
 *---------------------------------------------------------------------------
 *
 * propertyValuesSetColor --
 *
 *     Css property pProp contains a color-name. Set *pCVar (part of an
 *     HtmlComputedValues structure) to point to the corresponding entry in the 
 *     pTree->aColor array. The entry may be created if required.
 *
 * Results: 
 *     0 if *pCVar is set correctly. If pProp cannot be parsed as a color name,
 *     1 is returned and *pCVar remains unmodified.
 *
 * Side effects:
 *     May set *pCVar.
 *
 *---------------------------------------------------------------------------
 */
static int 
propertyValuesSetColor(p, pCVar, pProp)
    HtmlComputedValuesCreator *p;
    HtmlColor **pCVar;
    CssProperty *pProp;
{
    Tcl_HashEntry *pEntry;
    int newEntry = 0;
    CONST char *zColor;
    HtmlColor *cVal = 0;
    HtmlTree *pTree = p->pTree;

    if (pProp->eType == CSS_CONST_INHERIT) {
        HtmlColor **pInherit = (HtmlColor **)getInheritPointer(p, pCVar);
        assert(pInherit);
        cVal = *pInherit;
        goto setcolor_out;
    }

    zColor = HtmlCssPropertyGetString(pProp);
    if (!zColor) return 1;

    pEntry = Tcl_CreateHashEntry(&pTree->aColor, zColor, &newEntry);
    if (newEntry) {
        XColor *color;

        if (zColor[0] == '#' && strlen(zColor) == 4) {
	    /* Tk interprets a color value of "#ABC" as the same as "#A0B0C0".
             * But CSS implementations generally assume that it is equivalent
             * to "#AABBCC".
             */
	    char zBuf[8];
            zBuf[0] = '#';
            zBuf[1] = zColor[1]; zBuf[2] = zColor[1];
            zBuf[3] = zColor[2]; zBuf[4] = zColor[2];
            zBuf[5] = zColor[3]; zBuf[6] = zColor[3];
            zBuf[7] = '\0';
            color = Tk_GetColor(pTree->interp, pTree->tkwin, zBuf);
        } else {
            color = Tk_GetColor(pTree->interp, pTree->tkwin, zColor);
        }

        if (!color && strlen(zColor) <= 12) {
            /* Old versions of netscape used to support hex colors
             * without the '#' character (i.e. "FFF" is the same as
	     * "#FFF"). So naturally this has become a defacto standard, even
	     * though it is obviously wrong. At any rate, if Tk_GetColor()
	     * cannot parse the color-name as it stands, put a '#' character in
	     * front of it and give it another go.
             */
            char zBuf[14];
            sprintf(zBuf, "#%s", zColor);
            color = Tk_GetColor(pTree->interp, pTree->tkwin, zBuf);
        }

        if (color) {
            cVal = (HtmlColor *)HtmlAlloc(0, sizeof(HtmlColor)+strlen(zColor)+1);
            cVal->nRef = 0;
            cVal->xcolor = color;
            cVal->zColor = (char *)(&cVal[1]);
            strcpy(cVal->zColor, zColor);
            Tcl_SetHashValue(pEntry, cVal);
        } else {
            Tcl_DeleteHashEntry(pEntry);
            return 1;
        }
    } else {
        cVal = (HtmlColor *)Tcl_GetHashValue(pEntry);
    }

setcolor_out:
    assert(cVal);
    if (*pCVar) {
        (*pCVar)->nRef--;
    }
    cVal->nRef++;
    *pCVar = cVal;
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * propertyValuesSetLength --
 *
 *     This function attempts to interpret *pProp as a CSS <length>. A <length>
 *     is a numeric quantity with one of the following units:
 *
 *         em: font-size of the relevant font
 *         ex: x-height of the relevant font
 *         px: pixels
 *         in: inches
 *         cm: centimeters
 *         mm: millimeters
 *         pt: points
 *         pc: picas
 *
 *     If the widget mode is "quirks", then an integer without units is 
 *     considered to be in pixels. If the mode is "standards" or "almost
 *     standards", then a number without any units is a type mismatch for a
 *     length.
 *
 *     If *pProp is not a numeric quantity with one of the above units, 1 is
 *     returned and *pIVar is not written.  
 *
 *     If *pProp is an 'em' or 'ex' value, then *pIVar is set to the numeric
 *     value of the property * 100.0 and the em_mask bit in either p->em_mask
 *     or p->ex_mask is set. If an 'em' or 'ex' value is encountered but
 *     (em_mask==0), then 1 is returned and *pIVar is not written.
 *
 *     Note that unlike most of the other propertyValuesSetXXX() functions,
 *     this function does *not* handle the value 'inherit'. 
 *
 *
 * Results:  
 *     If successful, 0 is returned. If pProp cannot be parsed as a <length> 1
 *     is returned and *pIVar remains unmodified.
 *
 * Side effects:
 *     May set *pIVar, may modify p->em_mask or p->ex_mask.
 *
 *---------------------------------------------------------------------------
 */
static int 
propertyValuesSetLength(p, pIVal, em_mask, pProp, allowNegative)
    HtmlComputedValuesCreator *p;
    int *pIVal;
    unsigned int em_mask;
    CssProperty *pProp;
    int allowNegative;
{
    int iVal;
    switch (pProp->eType) {

        case CSS_TYPE_EM:
            if (em_mask == 0) return 1;
            iVal = (int)(pProp->v.rVal * 100.0);
            break;
        case CSS_TYPE_EX:
            if (em_mask == 0) return 1;
            iVal = (int)(pProp->v.rVal * 100.0);
            break;

        case CSS_TYPE_PX:
            iVal = INTEGER(pProp->v.rVal);
            break;

        case CSS_TYPE_PT:
            iVal = physicalToPixels(p, pProp->v.rVal, 'p');
            break;
        case CSS_TYPE_PC:
            iVal = physicalToPixels(p, pProp->v.rVal * 12.0, 'p');
            break;
        case CSS_TYPE_CENTIMETER:
            iVal = physicalToPixels(p, pProp->v.rVal, 'c');
            break;
        case CSS_TYPE_INCH:
            iVal = physicalToPixels(p, pProp->v.rVal, 'i');
            break;
        case CSS_TYPE_MILLIMETER:
            iVal = physicalToPixels(p, pProp->v.rVal, 'm');
            break;

        case CSS_TYPE_FLOAT: {
	    /* There are two cases where a unitless number is a legal
             * value for a CSS %length%. Other than the following, it 
             * is a type mismatch:
             *
             * 1. From section 4.3.2 of CSS 2.1: "After a zero length, 
             *    the unit identifier is optional.". 
             * 2. In quirks mode, no unit means pixels.
             */
            iVal = INTEGER(pProp->v.rVal);
            if (iVal && p->pTree->options.mode != HTML_MODE_QUIRKS) return 1;
            break;
        }

        default:
            return 1;
    }

    if (iVal < MAX_PIXELVAL || iVal >= 0 || allowNegative) {
        *pIVal = iVal;
        if (pProp->eType == CSS_TYPE_EM) {
            p->em_mask |= em_mask;
        } else if (pProp->eType == CSS_TYPE_EX) {
            p->ex_mask |= em_mask;
        }
    } else {
        return 1;
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * propertyValuesSetLineHeight --
 *
 * Results: 
 *     0 if value is successfully set. 1 if the value of *pProp is not a valid
 *     a value for the 'line-height' property.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int
propertyValuesSetLineHeight(p, pProp)
    HtmlComputedValuesCreator *p;
    CssProperty *pProp;
{
    int rc = 1;

    switch (pProp->eType) {
        case CSS_CONST_INHERIT: {
            p->values.iLineHeight = p->pParent->pPropertyValues->iLineHeight;
            rc = 0;
            break;
        }
        case CSS_CONST_NORMAL: {
            /* p->values.iLineHeight = -100; */
            p->values.iLineHeight = PIXELVAL_NORMAL;
            rc = 0;
            break;
        }
        case CSS_TYPE_PERCENT: {
            int iVal = INTEGER(pProp->v.rVal);
            if (iVal > 0) {
                p->values.iLineHeight = iVal;
                p->em_mask |= PROP_MASK_LINE_HEIGHT;
                rc = 0;
            }
            break;
        }
        case CSS_TYPE_FLOAT: {
            double rVal = pProp->v.rVal;
            if (rVal > 0) {
                rc = 0;
                p->values.iLineHeight = (-100.0 * rVal);
            }
            break;
        }
        default: {
            /* Try to treat the property as a <length> */
            int i = p->values.iLineHeight;
            int *pIVal = &p->values.iLineHeight;
            rc = propertyValuesSetLength(p,pIVal,PROP_MASK_LINE_HEIGHT,pProp,0);
            if (*pIVal < 0) {
                rc = 1;
                *pIVal = i;
            }
            break;
        }
    }
    return rc;
}

/*
 *---------------------------------------------------------------------------
 *
 * propertyValuesSetImage --
 *
 * Results: 
 *     0 if value is successfully set. 1 if the value of *pProp is not a valid
 *     value for an image property.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int
propertyValuesSetImage(p, pImVar, pProp)
    HtmlComputedValuesCreator *p;
    HtmlImage2 **pImVar;
    CssProperty *pProp;
{
    HtmlImage2 *pNew = 0;
    CONST char *zUrl = 0;

    switch (pProp->eType) {
        case CSS_CONST_INHERIT: {
            unsigned char *v = (unsigned char *)pImVar;
            HtmlImage2 **pInherit = (HtmlImage2 **)getInheritPointer(p, v);
            *pImVar = *pInherit;
            HtmlImageRef(*pImVar);
            return 0;
        }

        case CSS_CONST_NONE:
            break;

        case CSS_TYPE_URL:
        case CSS_TYPE_STRING: 
            zUrl = pProp->v.zVal;
            break;
 
        default:
            return 1;
    }

    if (zUrl) {
        pNew = HtmlImageServerGet(p->pTree->pImageServer, zUrl);
    }
    if (*pImVar) {
        HtmlImageFree(*pImVar);
    }
    *pImVar = pNew;
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * propertyValuesSetVerticalAlign --
 *
 * Results: 
 *     0 if value is successfully set. 1 if the value of *pProp is not a valid
 *     a value for the 'vertical-align' property.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int
propertyValuesSetVerticalAlign(p, pProp)
    HtmlComputedValuesCreator *p;
    CssProperty *pProp;
{
    static const unsigned int MASK = PROP_MASK_VERTICAL_ALIGN;
    int rc = 0;

    switch (pProp->eType) {
        case CSS_CONST_INHERIT: {
            HtmlNode *pParent = p->pParent;
            HtmlComputedValues *pPV;

            assert(pParent && pParent->pPropertyValues);
            pPV = pParent->pPropertyValues;

            p->values.iVerticalAlign = pPV->iVerticalAlign;
            p->values.eVerticalAlign = pPV->eVerticalAlign;

            p->eVerticalAlignPercent = 0;
            p->em_mask &= (~MASK);
            p->ex_mask &= (~MASK);

            break;
        }

        case CSS_CONST_BASELINE:
        case CSS_CONST_SUB:
        case CSS_CONST_SUPER:
        case CSS_CONST_TOP:
        case CSS_CONST_TEXT_TOP:
        case CSS_CONST_MIDDLE:
        case CSS_CONST_BOTTOM:
        case CSS_CONST_TEXT_BOTTOM:
            p->values.mask &= (~MASK);
            p->values.eVerticalAlign = pProp->eType;
            p->values.iVerticalAlign = 0;

            p->eVerticalAlignPercent = 0;
            p->em_mask &= (~MASK);
            p->ex_mask &= (~MASK);
            break;

        case CSS_TYPE_PERCENT: {
            p->values.mask |= MASK;
            p->values.iVerticalAlign = INTEGER(100.0 * pProp->v.rVal);
            p->values.eVerticalAlign = 0;

            p->eVerticalAlignPercent = 1;
            p->em_mask &= (~MASK);
            p->ex_mask &= (~MASK);
            break;
        }

        default: {
            /* Try to treat the property as a <length> */
            int *pIVal = &p->values.iVerticalAlign;
            rc = propertyValuesSetLength(p, pIVal, MASK, pProp, 1);
            if (rc == 0) {
                p->values.mask |= MASK;
                p->eVerticalAlignPercent = 0;
                p->values.eVerticalAlign = 0;
            }
            break;
        }
    }

    return rc;
}

/*
 *---------------------------------------------------------------------------
 *
 * propertyValuesSetSize --
 *
 * Results: 
 *     0 if *pIVar is set correctly. If pProp cannot be parsed as a size,
 *     1 is returned and *pIVar remains unmodified.
 *
 * Side effects:
 *     May set *pIVar and set or clear bits in various *p masks.
 *
 *---------------------------------------------------------------------------
 */
static int 
propertyValuesSetSize(p, pIVal, p_mask, pProp, allow_mask)
    HtmlComputedValuesCreator *p;
    int *pIVal;
    unsigned int p_mask;
    CssProperty *pProp;
    unsigned int allow_mask;
{
    assert(p_mask != 0);

    /* Clear the bits in the inherit and percent masks for this property */
    p->values.mask &= ~p_mask;
    p->em_mask &= ~p_mask;
    p->ex_mask &= ~p_mask;

    switch (pProp->eType) {

        /* TODO Percentages are still stored as integers - this is wrong */
        case CSS_TYPE_PERCENT: {
            int iVal = INTEGER(pProp->v.rVal * 100.0);
            if (
                (allow_mask & SZ_PERCENT) && 
                (iVal >= 0 || allow_mask & SZ_NEGATIVE) 
            ) {
                p->values.mask |= p_mask;
                *pIVal = iVal;
                return 0;
            }
            return 1;
        }

        case CSS_CONST_INHERIT:
            if (allow_mask & SZ_INHERIT) {
                HtmlNode *pParent = p->pParent;
                int *pInherit = (int *)getInheritPointer(p, pIVal);
                assert(pInherit);
                assert(pParent);

                *pIVal = *pInherit;
                p->values.mask |= (pParent->pPropertyValues->mask & p_mask);
                return 0;
            }
            return 1;

        case CSS_CONST_AUTO:
            if (allow_mask & SZ_AUTO) {
                *pIVal = PIXELVAL_AUTO;
                return 0;
            }
            return 1;

        case CSS_CONST_NONE:
            if (allow_mask & SZ_NONE) {
                *pIVal = PIXELVAL_NONE;
                return 0;
            }
            return 1;

        case CSS_CONST_NORMAL:
            if (allow_mask & SZ_NORMAL) {
                *pIVal = PIXELVAL_NONE;
                return 0;
            }
            return 1;

        case CSS_TYPE_FLOAT: {
#if 0
            if (iVal >= 0 || allow_mask & SZ_NEGATIVE) {
                *pIVal = iVal;
                return 0;
            }
            return 1;
#endif
        }

        default:
            return propertyValuesSetLength(
                p, pIVal, p_mask, pProp, allow_mask & SZ_NEGATIVE);
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * propertyValuesSetBorderWidth --
 *
 *     pIVal points to an integer to store the value of a 'border-width-xxx'
 *     property in pixels in (i.e. HtmlComputedValues.border.iTop). This
 *     function attempts to interpret *pProp as a <border-width> and writes the
 *     number of pixels to render the border as to *pIVal.
 *
 *     Border-widths are a little different from properties dealt with by
 *     SetSize() in that they may not expressed as percentages. Hence they can
 *     be resolved to an integer number of pixels during the styler phase (i.e.
 *     now).
 *
 * Results:  
 *     If successful, 0 is returned. If pProp cannot be parsed as a
 *     border-width, 1 is returned and *pIVar remains unmodified.
 *
 * Side effects:
 *     May set *pIVar.
 *
 *---------------------------------------------------------------------------
 */
static int 
propertyValuesSetBorderWidth(p, pIVal, em_mask, pProp)
    HtmlComputedValuesCreator *p;
    int *pIVal;
    unsigned int em_mask;
    CssProperty *pProp;
{
    int eType = pProp->eType;

    /* Check for one of the keywords "thin", "medium" and "thick". TODO: We
     * should use some kind of table here, rather than constant values.
     *
     * Also handle "inherit" seperately here too.
     */
    switch (eType) {
        case CSS_CONST_INHERIT: {
            int *pInherit = (int *)getInheritPointer(p, (unsigned char*)pIVal);
            if (pInherit) {
                *pIVal = *pInherit;
            }
            return 0;
        }
        case CSS_CONST_THIN:
            *pIVal = 1;
            return 0;
        case CSS_CONST_MEDIUM:
            *pIVal = 2;
            return 0;
        case CSS_CONST_THICK:
            *pIVal = 4;
            return 0;
        case CSS_TYPE_FLOAT:
            *pIVal = pProp->v.rVal;
            return 0;
    }

    /* If it is not one of the above keywords, then the border-width may 
     * be expressed as a CSS <length>.
     */
    if (0 == propertyValuesSetLength(p, pIVal, em_mask, pProp, 0)) {
        return 0;
    }

    /* Not one of the keywords or a length -> type-mismatch error */
    return 1;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlComputedValuesInit --
 *   
 *     Initialise an HtmlComputedValuesCreator structure.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Initialises *p.
 *
 *---------------------------------------------------------------------------
 */
void
HtmlComputedValuesInit(pTree, pNode, pParent, p)
    HtmlTree *pTree;
    HtmlNode *pNode;                 /* Node to use for LOG blocks */
    HtmlNode *pParent;               /* Node to inherit properties from */
    HtmlComputedValuesCreator *p;
{
    static CssProperty Black       = {CSS_CONST_BLACK, {"black"}};
    static CssProperty Medium      = {CSS_CONST_MEDIUM, {"medium"}};
    static CssProperty Transparent = {CSS_CONST_TRANSPARENT, {"transparent"}};
    static CssProperty Inherit     = {CSS_CONST_INHERIT, {"inherit"}};

    int i;

    HtmlComputedValues *pValues = &p->values;
    char *values = (char *)pValues;

    if (0 == pParent) {
        pParent = HtmlNodeParent(pNode);
    }

    memset(p, 0, sizeof(HtmlComputedValuesCreator));
    p->pTree = pTree;
    p->pParent = pParent;

    /* Initialise the CUSTOM properties */
    pValues->eVerticalAlign = CSS_CONST_BASELINE;
    pValues->iLineHeight    = PIXELVAL_NORMAL;
    propertyValuesSetFontSize(p, &Medium);
    p->fontKey.zFontFamily = "Helvetica";

    /* Initialise the 'color' and 'background-color' properties */
    propertyValuesSetColor(p, &p->values.cColor, &Black);
    propertyValuesSetColor(p, &p->values.cBackgroundColor, &Transparent);

    for (i = 0; i < sizeof(propdef) / sizeof(PropertyDef); i++) {
        PropertyDef *pDef = &propdef[i];
        if (pParent && pDef->isInherit) {
            HtmlComputedValuesSet(p, pDef->eProp, &Inherit);
        } else {
            switch (pDef->eType) {
                case LENGTH:
                case BORDERWIDTH: {
                    int *pVal = (int *)(values + pDef->iOffset);
                    *pVal = pDef->iDefault;
                    break;
                }
    
                case ENUM: {
                    /* Default for enum values is the first value in
                     * the list returned by HtmlCssEnumeratedValues().
                     */
                    unsigned char *opt = HtmlCssEnumeratedValues(pDef->eProp);
                    *(unsigned char *)(values + pDef->iOffset) = *opt;
                    assert(*opt);
                    break;
                }
                default: /* do nothing */
                    break;
            }
        }
    }

    p->pNode = pNode;
}

/*
 *---------------------------------------------------------------------------
 *
 * propertyValuesTclScript --
 *   
 * Results:
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
propertyValuesTclScript(p, eProp, zScript)
    HtmlComputedValuesCreator *p;
    int eProp;
    const char *zScript;
{
    int rc;
    const char *zRes;
    CssProperty *pVal;
    Tcl_Interp *interp = p->pTree->interp;
    Tcl_Obj *pCommand = HtmlNodeCommand(p->pTree, p->pNode);

    Tcl_SetVar2Ex(interp, "N", 0, pCommand, 0);
    rc = Tcl_Eval(interp, zScript);
    zRes = Tcl_GetStringResult(interp);
    if (rc == TCL_ERROR) {
        if (*zRes) {
    	    /* A tcl() script has returned a value that caused a type-mismatch
             * error. Run the -logcmd script if one exists.
             */
            LOG {
                HtmlLog(p->pTree, "STYLEENGINE", "%s "
                    "tcl() script error: %s",
                    Tcl_GetString(HtmlNodeCommand(p->pTree, p->pNode)), zRes
                );
            }
        }
        return 1;
    }

    assert(zRes);
    pVal = HtmlCssStringToProperty(zRes, -1);

    if (HtmlComputedValuesSet(p, eProp, pVal)) {
	/* A tcl() script has returned a value that caused a type-mismatch
         * error. Run the -logcmd script if one exists.
         */
        LOG {
            HtmlLog(p->pTree, "STYLEENGINE", "%s "
                "tcl() script result is type mismatch for property '%s'",
                Tcl_GetString(HtmlNodeCommand(p->pTree, p->pNode)),
                HtmlCssPropertyToString(eProp)
            );
        }
        HtmlFree(0, (char *)pVal);
        return 1;
    }

    /* Now that we've successfully called HtmlComputedValuesSet(), the
     * CssProperty structure (it's associated string data is what matters)
     * cannot be HtmlFree(0, )d until after HtmlComputedValuesFinish() is called.
     * So we make a linked list of such structures at p->pDeleteList using
     * CssProperty.v.p as the pNext pointer.
     * 
     * HtmlComputedValuesFinish() deletes the list when it is called.
     */
    HtmlComputedValuesFreeProperty(p, pVal);

    return 0;
}

void HtmlComputedValuesFreeProperty(p, pProp)
    HtmlComputedValuesCreator *p;
    CssProperty *pProp;
{
    pProp->v.p = (void *)p->pDeleteList;
    p->pDeleteList = pProp;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlComputedValuesSet --
 *
 *     One or more calls to HtmlComputedValuesSet() take place between the
 *     HtmlComputedValuesInit() and HtmlComputedValuesFinish() calls (see
 *     comments above HtmlComputedValuesInit() for an API summary). The value
 *     of property eProp (one of the CSS_PROPERTY_XXX values) in either the
 *     HtmlComputedValues or HtmlFontKey structure is set to the value
 *     contained by pProp.
 *
 *     Note: If pProp contains a pointer to a string, then the string must
 *           remain valid until HtmlComputedValuesFinish() is called (see the
 *           'font-family property handling below). 
 *
 * Results: 
 *     Zero to indicate the value was set successfully, or non-zero to
 *     indicate a type-mismatch.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlComputedValuesSet(p, eProp, pProp)
    HtmlComputedValuesCreator *p;
    int eProp;
    CssProperty *pProp;
{
    PropertyDef *pDef = getPropertyDef(eProp);

    if (!pProp) {
        return 0;
    }

    LOG {
        char *zFree;
        char *zPropVal = HtmlPropertyToString(pProp, &zFree);
        HtmlLog(p->pTree, "STYLEENGINE", "%s %s -> %s",
                Tcl_GetString(HtmlNodeCommand(p->pTree, p->pNode)),
                HtmlCssPropertyToString(eProp), zPropVal
        );
        if (zFree) HtmlFree(0, zFree);
    }

    /* Silently ignore any attempt to set a root-node property to 'inherit'.
     * It's not a type-mismatch, we just want to leave the value unchanged.
     */
    if (pProp->eType == CSS_CONST_INHERIT && !p->pParent) {
        return 0;
    }

    /* Special case - a Tcl script to evaluate */
    if (pProp->eType == CSS_TYPE_TCL) {
        return propertyValuesTclScript(p, eProp, pProp->v.zVal);
    }

    if (pDef) {
        switch (pDef->eType) {
            case ENUM: {
                unsigned char *options = HtmlCssEnumeratedValues(eProp);
                unsigned char *pEVar;
                pEVar = (unsigned char *)&p->values + pDef->iOffset;
                return propertyValuesSetEnum(p, pEVar, options, pProp);
            }
            case LENGTH: {
                int *pIVar = (int*)((unsigned char*)&p->values + pDef->iOffset);
                int setsizemask = pDef->setsizemask;
                return propertyValuesSetSize(
                    p, pIVar, pDef->mask, pProp, setsizemask
                );
            }
            case BORDERWIDTH: {
                int *pBVar = (int*)((unsigned char*)&p->values + pDef->iOffset);
                return propertyValuesSetBorderWidth(
                    p, pBVar, pDef->mask, pProp
                );
            }
            case CUSTOM: {
                return pDef->xSet(p, pProp);
            }
            case COLOR: {
                HtmlColor **pCVar; 
                pCVar = (HtmlColor **)
                    ((unsigned char *)&p->values + pDef->iOffset);
                return propertyValuesSetColor(p, pCVar, pProp);
            }
            case IMAGE: {
                HtmlImage2 **pI2Var; 
                pI2Var = (HtmlImage2 **)
                    ((unsigned char *)&p->values + pDef->iOffset);
                return propertyValuesSetImage(p, pI2Var, pProp);
            }
        }
    }
    return 1;
}

/*
 *---------------------------------------------------------------------------
 *
 * allocateNewFont --
 *
 *     Allocate a new HtmlFont structure and populate it with the font
 *     described by *pFontKey. The HtmlFont.nRef counter is set to 0 when this
 *     function returns.
 *
 * Results: 
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static HtmlFont * 
allocateNewFont(pTree, tkwin, pFontKey)
    HtmlTree *pTree;
    Tk_Window tkwin;
    HtmlFontKey *pFontKey;
{
    Tcl_Interp *interp = pTree->interp;
    int isForceFontMetrics = pTree->options.forcefontmetrics;
    Tk_Font tkfont = 0;
    const char *DEFAULT_FONT_FAMILY = "Helvetica";

    const char *zFamily = pFontKey->zFontFamily;
    int isItalic = pFontKey->isItalic;
    int isBold = pFontKey->isBold;

    char zTkFontName[256];      /* Tk font name */
    HtmlFont *pFont;

    struct FamilyMap {
        CONST char *cssFont;
        CONST char *tkFont;
    } familyMap [] = {
        {"serif",      "Times"},
        {"sans-serif", "Helvetica"},
        {"monospace",  "Courier"}
    };

    /* Local variable iFontSize is in points - not thousandths */
    int iFontSize;
    float fontsize = ((float)pFontKey->iFontSize / (float)HTML_IFONTSIZE_SCALE);
    fontsize = fontsize * pTree->options.fontscale;

#if 0
    if (isForceFontMetrics) {
        iFontSize = INTEGER(fontsize * 0.9);
    } else {
        iFontSize = INTEGER(fontsize);
    }
#else
    iFontSize = INTEGER(fontsize);
#endif

    do {
        const char *zF;      /* Pointer to tk font family name */
        int iF = 0;          /* Length of tk font family name in bytes */

        if (0 == *zFamily) {
            /* End of the line default font: Helvetica */
            zF = DEFAULT_FONT_FAMILY;
            iF = strlen(zF);
        } else {
            int i;
            zF = zFamily;
            while (*zFamily && *zFamily != ',') zFamily++;
            iF = (zFamily - zF);
            if (*zFamily == ',') {
                zFamily++;
            }
            while (*zFamily == ' ') zFamily++;

            /* Trim spaces from the beginning and end of the string */
            while (iF > 0 && zF[iF-1] == ' ') iF--;
            while (iF > 0 && *zF == ' ') {
                iF--;
                zF++;
            }

            for (i = 0; i < sizeof(familyMap)/sizeof(struct FamilyMap); i++) {
                if (
                    iF == strlen(familyMap[i].cssFont) && 
                    0 == strncmp(zF, familyMap[i].cssFont, iF)
                ) {
                    zF = familyMap[i].tkFont;
                    iF = strlen(zF);
                    break;
                }
            }
        }

        sprintf(zTkFontName, "%.*s %d%.8s%.8s", 
             ((iF > 64) ? 64 : iF), zF,
             iFontSize,
             isItalic ? " italic" : "", 
             isBold ? " bold" : ""
        );

        tkfont = Tk_GetFont(interp, tkwin, zTkFontName);
        if (!tkfont && zF == DEFAULT_FONT_FAMILY) {
            if (isItalic) {
                isItalic = 0;
            } else if (isBold) {
                isBold = 0;
            } else if (iFontSize != 10) {
                iFontSize = 10;
            } else {
                assert(0);
                return 0;
            }
        }

    } while (0 == tkfont);

    pFont = (HtmlFont *)HtmlAlloc(0, sizeof(HtmlFont) + strlen(zTkFontName)+1);
    pFont->nRef = 0;
    pFont->tkfont = tkfont;
    pFont->zFont = (char *)&pFont[1];
    strcpy(pFont->zFont, zTkFontName);

    Tk_GetFontMetrics(tkfont, &pFont->metrics);
    pFont->ex_pixels = Tk_TextWidth(tkfont, "x", 1);
    pFont->space_pixels = Tk_TextWidth(tkfont, " ", 1);

    /* Set the number of pixels to be used for 1 "em" unit for this font.
     * Setting the em-pixels to the ascent + the descent worked Ok for
     * the old X11 fonts. However the value turns out to be too large with 
     * the new Xft fonts (Tk 8.5). So for now, use the font-ascent as the
     * em-pixels value. I'm not entirely satisfied with this.
     */
    /* pFont->em_pixels = pFont->metrics.ascent + pFont->metrics.descent; */

    if (isForceFontMetrics) {
        float ratio;
        Tk_FontMetrics *pMet = &pFont->metrics;

        char zBuf[24];
        sprintf(zBuf, "%.3fp", fontsize);
        Tk_GetPixels(interp, tkwin, zBuf, &pFont->em_pixels);

        pMet->linespace = pMet->ascent + pMet->descent;
        if (pFont->em_pixels < pMet->linespace) {
            ratio = (float)pFont->em_pixels / (float)pMet->linespace;
            pMet->ascent = INTEGER((float)pMet->ascent * ratio);
            pMet->descent = pFont->em_pixels - pMet->ascent;
            pMet->linespace = pFont->em_pixels;
        }
    } else {
        pFont->em_pixels = pFont->metrics.ascent;
    }

    return pFont;
}
    
/*
 *---------------------------------------------------------------------------
 *
 * setDisplay97 --
 *
 *     Modify the value of the 'display' property in the structure pointed 
 *     to by argument p according to the table in section 9.7 of the CSS 
 *     2.1 spec.
 *
 * Results: 
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void
setDisplay97(p)
    HtmlComputedValuesCreator *p;
{
    switch (p->values.eDisplay) {
        case CSS_CONST_INLINE_TABLE:
            p->values.eDisplay = CSS_CONST_TABLE;
            break;
        case CSS_CONST_INLINE:
        case CSS_CONST_INLINE_BLOCK:
        case CSS_CONST_RUN_IN:
        case CSS_CONST_TABLE_ROW_GROUP:
        case CSS_CONST_TABLE_COLUMN:
        case CSS_CONST_TABLE_COLUMN_GROUP:
        case CSS_CONST_TABLE_HEADER_GROUP:
        case CSS_CONST_TABLE_FOOTER_GROUP:
        case CSS_CONST_TABLE_ROW:
        case CSS_CONST_TABLE_CELL:
        case CSS_CONST_TABLE_CAPTION:
            p->values.eDisplay = CSS_CONST_BLOCK;
            break;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlComputedValuesFinish --
 *
 * Results: 
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
HtmlComputedValues *
HtmlComputedValuesFinish(p)
    HtmlComputedValuesCreator *p;
{
    Tcl_HashEntry *pEntry;
    int ne;                /* New Entry */
    HtmlFont *pFont;
    int ii;
    HtmlComputedValues *pValues = 0;
    HtmlColor *pColor;

#define OFFSET(x) Tk_Offset(HtmlComputedValues, x)
    struct EmExMap {
        unsigned int mask;
        int offset;
    } emexmap[] = {
        {PROP_MASK_WIDTH,               OFFSET(iWidth)},
        {PROP_MASK_MIN_WIDTH,           OFFSET(iMinWidth)},
        {PROP_MASK_MAX_WIDTH,           OFFSET(iMaxWidth)},
        {PROP_MASK_HEIGHT,              OFFSET(iHeight)},
        {PROP_MASK_MIN_HEIGHT,          OFFSET(iMinHeight)},
        {PROP_MASK_MAX_HEIGHT,          OFFSET(iMaxHeight)},
        {PROP_MASK_MARGIN_TOP,          OFFSET(margin.iTop)},
        {PROP_MASK_MARGIN_RIGHT,        OFFSET(margin.iRight)},
        {PROP_MASK_MARGIN_BOTTOM,       OFFSET(margin.iBottom)},
        {PROP_MASK_MARGIN_LEFT,         OFFSET(margin.iLeft)},
        {PROP_MASK_PADDING_TOP,         OFFSET(padding.iTop)},
        {PROP_MASK_PADDING_RIGHT,       OFFSET(padding.iRight)},
        {PROP_MASK_PADDING_BOTTOM,      OFFSET(padding.iBottom)},
        {PROP_MASK_PADDING_LEFT,        OFFSET(padding.iLeft)},
        {PROP_MASK_VERTICAL_ALIGN,      OFFSET(iVerticalAlign)},
        {PROP_MASK_BORDER_TOP_WIDTH,    OFFSET(border.iTop)},
        {PROP_MASK_BORDER_RIGHT_WIDTH,  OFFSET(border.iRight)},
        {PROP_MASK_BORDER_BOTTOM_WIDTH, OFFSET(border.iBottom)},
        {PROP_MASK_BORDER_LEFT_WIDTH,   OFFSET(border.iLeft)},
        {PROP_MASK_LINE_HEIGHT,         OFFSET(iLineHeight)},
        {PROP_MASK_OUTLINE_WIDTH,       OFFSET(iOutlineWidth)},
        {PROP_MASK_TOP,                 OFFSET(position.iTop)},
        {PROP_MASK_BOTTOM,              OFFSET(position.iBottom)},
        {PROP_MASK_LEFT,                OFFSET(position.iLeft)},
        {PROP_MASK_RIGHT,               OFFSET(position.iRight)},
        {PROP_MASK_TEXT_INDENT,         OFFSET(iTextIndent)}
    };
#undef OFFSET

    /* Find the font to use. If there is not a matching font in the aFont hash
     * table already, allocate a new one.
     */
    pEntry = Tcl_CreateHashEntry(&p->pTree->aFont, (char *)&p->fontKey, &ne);
    if (ne) {
        pFont = allocateNewFont(p->pTree, p->pTree->tkwin, &p->fontKey);
        assert(pFont);
        Tcl_SetHashValue(pEntry, pFont);
        pFont->pKey = (HtmlFontKey *)Tcl_GetHashKey(&p->pTree->aFont, pEntry);
    } else {
        pFont = Tcl_GetHashValue(pEntry);
    }
    pFont->nRef++;
    p->values.fFont = pFont;
    pEntry = 0;
    ne = 0;

    /* Now that we have the font, update all the property values that are
     * currently stored in 'em' or 'ex' form so that they are in pixels.
     */
    for (ii = 0; ii < sizeof(emexmap)/sizeof(struct EmExMap); ii++) {
        struct EmExMap *pMap = &emexmap[ii];
        if (p->em_mask & pMap->mask) {
            int *pVal = (int *)(((unsigned char *)&p->values) + pMap->offset);
            *pVal = (*pVal * pFont->em_pixels) / 100;
        } else if (p->ex_mask & pMap->mask) {
            int *pVal = (int *)(((unsigned char *)&p->values) + pMap->offset);
            *pVal = (*pVal * pFont->ex_pixels) / 100;
        }
    }

    /* If no value has been assigned to any of the 'border-xxx-color'
     * properties, then copy the value of the 'color' property. 
     */
    pColor = p->values.cColor;
    if (!p->values.cBorderTopColor) {
        p->values.cBorderTopColor = pColor;
        pColor->nRef++;
    }
    if (!p->values.cBorderRightColor) {
        p->values.cBorderRightColor = pColor;
        pColor->nRef++;
    }
    if (!p->values.cBorderBottomColor) {
        p->values.cBorderBottomColor = pColor;
        pColor->nRef++;
    }
    if (!p->values.cBorderLeftColor) {
        p->values.cBorderLeftColor = pColor;
        pColor->nRef++;
    }
    if (!p->values.cOutlineColor) {
        p->values.cOutlineColor = pColor;
        pColor->nRef++;
    }

    /* Post-processing for the 'vertical-align' property:
     * 
     *     1. If the value was specified as a percentage, transform it
     *        to a pixel length here. This is tricky, because the computed 
     *        value of 'line-height' (the property percentages are calculated
     *        relative to) may be stored as a multiple of the font em-pixels.
     *
     *        The reason iLineHeight is not always stored as an absolute
     *        pixel value is that if it is specified as a <number>, it
     *        is equivalent to an em-pixels value for the current node, but 
     *        the inherited value is as specified.
     *
     *     2. If the node is a table-cell, then the only valid values are 
     *        "baseline", "top", "middle" and "bottom". If anything else has
     *        been specified, set the property to "baseline" instead.
     */
    if (p->eVerticalAlignPercent) {
        int line_height = p->values.iLineHeight;
        if (line_height == PIXELVAL_NORMAL) {
            line_height = -100;
        }
        if (line_height < 0) {
            line_height = (line_height * pFont->em_pixels) / -100;
        }
        p->values.iVerticalAlign = (p->values.iVerticalAlign*line_height)/10000;
    }
    if (p->values.eDisplay == CSS_CONST_TABLE_CELL && 
        p->values.eVerticalAlign != CSS_CONST_TOP &&
        p->values.eVerticalAlign != CSS_CONST_BOTTOM &&
        p->values.eVerticalAlign != CSS_CONST_MIDDLE
    ) {
        p->values.eVerticalAlign = CSS_CONST_BASELINE;
    }

    /* The following block implements section 9.7 of the CSS 2.1 
     * specification. Refer there for details.
     */
    if (
        p->values.ePosition == CSS_CONST_ABSOLUTE || 
        p->values.ePosition == CSS_CONST_FIXED
    ) {
        p->values.eFloat = CSS_CONST_NONE;
        setDisplay97(p);
    }
    else if (p->values.eFloat != CSS_CONST_NONE) {
        setDisplay97(p);
    }
    else if (p->pNode == p->pTree->pRoot) {
        setDisplay97(p);
    }

    /* This is section 9.4.3 of the same document. Massaging 'left',
     * 'right', 'top' and 'bottom' if the 'position' property is set to
     * "relative".
     */
    if (p->values.ePosition == CSS_CONST_RELATIVE) {
        /* First for 'left' and 'right' */
        if (p->values.position.iLeft == PIXELVAL_AUTO) {
            if (p->values.position.iRight == PIXELVAL_AUTO) {
                p->values.position.iRight = 0;
                p->values.position.iLeft = 0;
            } else {
                p->values.position.iLeft = -1 * p->values.position.iRight;
                p->values.mask = 
                    (p->values.mask & ~(PROP_MASK_LEFT)) |
                    ((p->values.mask & PROP_MASK_RIGHT) ? PROP_MASK_LEFT : 0);
            }
        } else {
            p->values.position.iRight = -1 * p->values.position.iLeft;
            p->values.mask = 
                (p->values.mask & ~(PROP_MASK_RIGHT)) |
                ((p->values.mask & PROP_MASK_LEFT) ? PROP_MASK_RIGHT : 0);
        }

        /* Then for 'top' and 'bottom' */
        if (p->values.position.iTop == PIXELVAL_AUTO) {
            if (p->values.position.iBottom == PIXELVAL_AUTO) {
                p->values.position.iBottom = 0;
                p->values.position.iTop = 0;
            } else {
                p->values.position.iTop = -1 * p->values.position.iBottom;
                p->values.mask = 
                    (p->values.mask & ~(PROP_MASK_TOP)) |
                    ((p->values.mask & PROP_MASK_BOTTOM) ? PROP_MASK_TOP : 0);
            }
        } else {
            p->values.position.iBottom = -1 * p->values.position.iTop;
            p->values.mask = 
                (p->values.mask & ~(PROP_MASK_BOTTOM)) |
                ((p->values.mask & PROP_MASK_TOP) ? PROP_MASK_BOTTOM : 0);
        }
    }

    if (
        p->values.eDisplay == CSS_CONST_TABLE_CAPTION
        || p->values.eDisplay == CSS_CONST_RUN_IN
        /* || p->values.eDisplay == CSS_CONST_INLINE_BLOCK */
    ) {
        p->values.eDisplay = CSS_CONST_BLOCK;
    }
    if (p->values.eDisplay == CSS_CONST_INLINE_TABLE) {
        p->values.eDisplay = CSS_CONST_TABLE;
    }

    /* Look the values structure up in the hash-table. */
    pEntry = Tcl_CreateHashEntry(&p->pTree->aValues, (char *)&p->values, &ne);
    pValues = (HtmlComputedValues *)Tcl_GetHashKey(&p->pTree->aValues, pEntry);
    if (!ne) {
	/* If this is not a new entry, we need to decrement the reference count
         * on the font, image and color values.
         */
        pValues->fFont->nRef--;
        pValues->cColor->nRef--;
        pValues->cBackgroundColor->nRef--;
        pValues->cBorderTopColor->nRef--;
        pValues->cBorderRightColor->nRef--;
        pValues->cBorderBottomColor->nRef--;
        pValues->cBorderLeftColor->nRef--;
        pValues->cOutlineColor->nRef--;
        HtmlImageFree(pValues->imReplacementImage);
        HtmlImageFree(pValues->imBackgroundImage);
        HtmlImageFree(pValues->imListStyleImage);
    } else if (
        pValues->eBackgroundAttachment == CSS_CONST_FIXED ||
        pValues->ePosition == CSS_CONST_FIXED
    ) {
	/* If this is a new entry and the 'background-attachment' or 
         * 'position' property computes to "fixed", then increment
	 * HtmlTree.nFixedBackground.
         */
        p->pTree->nFixedBackground++;
    }
    HtmlImageCheck(pValues->imReplacementImage);
    HtmlImageCheck(pValues->imBackgroundImage);
    HtmlImageCheck(pValues->imListStyleImage);

    /* Delete any CssProperty structures allocated for Tcl properties */
    if (p->pDeleteList) {
        CssProperty *p1 = p->pDeleteList;
        while (p1) {
            CssProperty *p2 = (CssProperty *)p1->v.p;
            HtmlFree(0, (char *)p1);
            p1 = p2;
        }
        p->pDeleteList = 0;
    }

    pValues->nRef++;
    return pValues;
}

static void 
decrementFontRef(pTree, pFont)
    HtmlTree *pTree;
    HtmlFont *pFont;
{
    pFont->nRef--;
    assert(pFont->nRef >= 0);
    if (pFont->nRef == 0) {
        CONST char *pKey = (CONST char *)pFont->pKey;
        Tcl_HashEntry *pEntry = Tcl_FindHashEntry(&pTree->aFont, pKey);
        Tcl_DeleteHashEntry(pEntry);
        Tk_FreeFont(pFont->tkfont);
        HtmlFree(0, (char *)pFont);
    }
}

static void 
decrementColorRef(pTree, pColor)
    HtmlTree *pTree;
    HtmlColor *pColor;
{
    pColor->nRef--;
    assert(pColor->nRef >= 0);
    if (pColor->nRef == 0) {
        Tcl_HashEntry *pEntry;
        pEntry = Tcl_FindHashEntry(&pTree->aColor, pColor->zColor);
        Tcl_DeleteHashEntry(pEntry);
        if (pColor->xcolor) {
            Tk_FreeColor(pColor->xcolor);
        }
        HtmlFree(0, (char *)pColor);
    }
}

void 
HtmlComputedValuesRelease(pTree, pValues)
    HtmlTree *pTree;
    HtmlComputedValues *pValues;
{
    if (pValues) {
        pValues->nRef--;
        assert(pValues->nRef >= 0);

        /* If the reference count on this values structure has reached 0, then
         * decrement the reference counts on the font and colors and delete the
         * values structure hash entry.
         */
        if (pValues->nRef == 0) {
            Tcl_HashEntry *pEntry;
    
            pEntry = Tcl_FindHashEntry(&pTree->aValues, (CONST char *)pValues);
            assert(pEntry);
    
            decrementFontRef(pTree, pValues->fFont);
            decrementColorRef(pTree, pValues->cColor);
            decrementColorRef(pTree, pValues->cBackgroundColor);
            decrementColorRef(pTree, pValues->cBorderTopColor);
            decrementColorRef(pTree, pValues->cBorderRightColor);
            decrementColorRef(pTree, pValues->cBorderBottomColor);
            decrementColorRef(pTree, pValues->cBorderLeftColor);
            decrementColorRef(pTree, pValues->cOutlineColor);
            HtmlImageFree(pValues->imReplacementImage);
            HtmlImageFree(pValues->imBackgroundImage);
            HtmlImageFree(pValues->imListStyleImage);
            if (
                pValues->eBackgroundAttachment == CSS_CONST_FIXED ||
                pValues->ePosition == CSS_CONST_FIXED
            ) {
                pTree->nFixedBackground--;
                assert(pTree->nFixedBackground >= 0);
            }
            Tcl_DeleteHashEntry(pEntry);
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlComputedValuesSetupTables --
 * 
 *     This function is called during widget initialisation to initialise the
 *     three hash-tables used by code in this file:
 *
 *         HtmlTree.aColor
 *         HtmlTree.aFont
 *         HtmlTree.aValues
 *
 *     The aColor array is pre-loaded with 16 colors - the colors defined by
 *     the CSS standard. This is because the RGB definitions of these colors in
 *     CSS may be different than Tk's definition. If we preload all 16 and
 *     leave them in the color-cache permanently, we can be sure that the CSS
 *     defintions will always be used.
 *
 *     The aFont and aValues hash tables are initialised empty.
 *
 * Results: 
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlComputedValuesSetupTables(pTree)
    HtmlTree *pTree;
{
    static struct CssColor {
        char *css;
        char *tk;
    } color_map[] = {
        {"silver",  "#C0C0C0"},
        {"gray",    "#808080"},
        {"white",   "#FFFFFF"},
        {"maroon",  "#800000"},
        {"red",     "#FF0000"},
        {"purple",  "#800080"},
        {"fuchsia", "#FF00FF"},
        {"green",   "#008000"},
        {"lime",    "#00FF00"},
        {"olive",   "#808000"},
        {"yellow",  "#FFFF00"},
        {"navy",    "#000080"},
        {"blue",    "#0000FF"},
        {"teal",    "#008080"},
        {"aqua",    "#00FFFF"}
    };
    int ii;
    Tcl_HashEntry *pEntry;
    Tcl_Interp *interp = pTree->interp;
    Tcl_HashKeyType *pType;
    HtmlColor *pColor;
    int n;

    pType = HtmlCaseInsenstiveHashType();
    Tcl_InitCustomHashTable(&pTree->aColor, TCL_CUSTOM_TYPE_KEYS, pType);

    pType = HtmlFontKeyHashType();
    Tcl_InitCustomHashTable(&pTree->aFont, TCL_CUSTOM_TYPE_KEYS, pType);

    pType = HtmlComputedValuesHashType();
    Tcl_InitCustomHashTable(&pTree->aValues, TCL_CUSTOM_TYPE_KEYS, pType);

    Tcl_InitHashTable(&pTree->aImage, TCL_STRING_KEYS);

    /* Initialise the color table */
    for (ii = 0; ii < sizeof(color_map)/sizeof(struct CssColor); ii++) {
        pColor = (HtmlColor *)HtmlAlloc(0, sizeof(HtmlColor));
        pColor->zColor = color_map[ii].css;
        pColor->nRef = 1;
        pColor->xcolor = Tk_GetColor(interp, pTree->tkwin, color_map[ii].tk);
        assert(pColor->xcolor);
        pEntry = Tcl_CreateHashEntry(&pTree->aColor, pColor->zColor, &n);
        assert(pEntry && n);
        Tcl_SetHashValue(pEntry, pColor);
    }

    /* Add the "transparent" color */
    pEntry = Tcl_CreateHashEntry(&pTree->aColor, "transparent", &n);
    assert(pEntry && n);
    pColor = (HtmlColor *)HtmlAlloc(0, sizeof(HtmlColor));
    pColor->zColor = "transparent";
    pColor->nRef = 1;
    pColor->xcolor = 0;
    Tcl_SetHashValue(pEntry, pColor);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlComputedValuesCleanupTables --
 * 
 *     This function is called during widget destruction to deallocate
 *     resources allocated by HtmlComputedValuesSetupTables(). This should be
 *     called after HtmlComputedValues references have been released (otherwise
 *     an assertion will fail).
 *
 *     Resources are currently:
 *
 *         - The entries in aColor for the 15 CSS defined colors.
 *         - The entry in aColor for "transparent".
 *
 * Results: 
 *     None.
 *
 * Side effects:
 *     Cleans up resources allocated by HtmlComputedValuesSetupTables().
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlComputedValuesCleanupTables(pTree)
    HtmlTree *pTree;
{
    CONST char **pzCursor;
   
    CONST char *azColor[] = {
        "silver",
        "gray",
        "white",
        "maroon",
        "red",
        "purple",
        "fuchsia",
        "green",
        "lime",
        "olive",
        "yellow",
        "navy",
        "blue",
        "teal",
        "aqua",
        "transparent",
        0
    };

    for (pzCursor = azColor; *pzCursor; pzCursor++) {
        HtmlColor *pColor;
        Tcl_HashEntry *pEntry = Tcl_FindHashEntry(&pTree->aColor, *pzCursor);
        assert(pEntry);
        pColor = (HtmlColor *)Tcl_GetHashValue(pEntry);
        decrementColorRef(pTree, pColor);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeGetProperty --
 *
 *     Obtain a Tcl (string) representation of the computed value of
 *     a specified property.
 *
 * Results: 
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlNodeGetProperty(interp, pProp, pValues)
    Tcl_Interp *interp;                 /* Interpreter to set result in */
    Tcl_Obj *pProp;                     /* Property name */
    HtmlComputedValues *pValues;        /* Read value from here */
{
    PropertyDef *pDef;

    int nProp;
    const char *zProp = Tcl_GetStringFromObj(pProp, &nProp);
    int eProp = HtmlCssPropertyLookup(nProp, zProp);

    assert(eProp <= CSS_PROPERTY_MAX_PROPERTY);
    if (eProp < 0) {
        Tcl_AppendResult(interp, "no such property: ", zProp, 0);
        return TCL_ERROR;
    }

    /* If the following returns NULL, then the specified property name
     * is a legal one, but not one that Tkhtml3 handles. Return an
     * empty string in this case.
     */
    pDef = getPropertyDef(eProp);
    Tcl_ResetResult(interp);
    if (pDef) {
        Tcl_Obj *pValue = 0;
        unsigned char *v = (unsigned char *)pValues;
        switch (pDef->eType) {
            case ENUM: {
                int eValue = (int)*(unsigned char *)(v + pDef->iOffset);
                CONST char *zValue = HtmlCssConstantToString(eValue);
                pValue = Tcl_NewStringObj(zValue, -1);
                break;
            }

            case COLOR: {
                HtmlColor *pColor = *(HtmlColor **)(v + pDef->iOffset);
                pValue = Tcl_NewStringObj(pColor->zColor, -1);
                break;
            }
        }

        if (!pValue) {
            Tcl_AppendResult(interp, "TODO!", 0);
        } else {
            Tcl_SetObjResult(interp, pValue);
        }
    }

    return TCL_OK;
}

int 
HtmlNodeProperties(interp, pValues)
    Tcl_Interp *interp;
    HtmlComputedValues *pValues;
{
    Tcl_Obj *pRet;
    int ii;
    char *v = (char *)pValues;

    int eValue = pValues->eVerticalAlign;
    char zBuf[256];
    int iFontSize;
    Tcl_Obj *pValue = 0;

    pRet = Tcl_NewObj();
    Tcl_IncrRefCount(pRet);

    for (ii = 0; ii < sizeof(propdef) / sizeof(propdef[0]); ii++) {
        PropertyDef *pDef = &propdef[ii];
        CONST char *zName = HtmlCssPropertyToString(pDef->eProp);

        if (
            pDef->eProp == CSS_PROPERTY_FONT_FAMILY ||
            pDef->eProp == CSS_PROPERTY_FONT_SIZE ||
            pDef->eProp == CSS_PROPERTY_FONT_STYLE ||
            pDef->eProp == CSS_PROPERTY_FONT_VARIANT ||
            pDef->eProp == CSS_PROPERTY_FONT_WEIGHT  ||
            pDef->eProp == CSS_PROPERTY_CONTENT 
        ) continue;

        Tcl_ListObjAppendElement(interp, pRet, Tcl_NewStringObj(zName, -1));
        switch (pDef->eType) {
            case ENUM: {
                int eValue = (int)*(unsigned char *)(v + pDef->iOffset);
                CONST char *zValue = HtmlCssConstantToString(eValue);
                pValue = Tcl_NewStringObj(zValue, -1);
                break;
            }

            case COLOR: {
                HtmlColor *pColor = *(HtmlColor **)(v + pDef->iOffset);
                pValue = Tcl_NewStringObj(pColor->zColor, -1);
                break;
            }

            case CUSTOM:
                if (pDef->eProp != CSS_PROPERTY_LINE_HEIGHT) break;
            case BORDERWIDTH:
            case LENGTH: {
                int iValue = *(int *)(v + pDef->iOffset);
                switch (iValue) {
                    case PIXELVAL_NONE:
                        pValue = Tcl_NewStringObj("none", -1);
                        break;
                    case PIXELVAL_AUTO:
                        pValue = Tcl_NewStringObj("auto", -1);
                        break;
                    case PIXELVAL_NORMAL:
                        pValue = Tcl_NewStringObj("normal", -1);
                        break;
                    default: {
                        char zBuf[64];
                        if (pDef->mask & pValues->mask) {
                            sprintf(zBuf, "%.2f%%", (double)iValue/100.0);
                        } else 

                        if (
                            pDef->eProp == CSS_PROPERTY_LINE_HEIGHT && 
                            iValue < 0
                        ) {
                            sprintf(zBuf, "%.2fem", (double)iValue / -100.0);
                        } else {
                            sprintf(zBuf, "%dpx", iValue);
                        }
                        pValue = Tcl_NewStringObj(zBuf, -1);
                        break;
                    }
                }
                break;
            }

            case IMAGE: {
                HtmlImage2 *imValue = *(HtmlImage2 **)(v + pDef->iOffset);
                if (imValue) {
                    pValue = Tcl_NewStringObj(HtmlImageUrl(imValue), -1);
                } else {
                    pValue = Tcl_NewStringObj("none", 4);
                }
                break;
            }
        }
        Tcl_ListObjAppendElement(interp, pRet, pValue);
    }

    /* vertical-align */
    eValue = pValues->eVerticalAlign;
    pValue = 0;
    if (0 == eValue) {
        char zBuf[64];
        int iValue = pValues->iVerticalAlign;
        sprintf(zBuf, "%dpx", iValue);
        pValue = Tcl_NewStringObj(zBuf, -1);
    } else {
        CONST char *zValue = HtmlCssConstantToString(eValue);
        pValue = Tcl_NewStringObj(zValue, -1);
    }
    Tcl_ListObjAppendElement(0, pRet, Tcl_NewStringObj("vertical-align", -1));
    Tcl_ListObjAppendElement(0, pRet, pValue);
    
    /* font */
    pValue = Tcl_NewStringObj(pValues->fFont->zFont, -1);
    Tcl_ListObjAppendElement(0, pRet, Tcl_NewStringObj("font", -1));
    Tcl_ListObjAppendElement(0, pRet, pValue);

    /* font-size */
    iFontSize = pValues->fFont->pKey->iFontSize;
    sprintf(zBuf, "%.3fpts", (float)iFontSize / 1000);
    Tcl_ListObjAppendElement(0, pRet, Tcl_NewStringObj("font-size", -1));
    Tcl_ListObjAppendElement(0, pRet, Tcl_NewStringObj(zBuf, -1));

    /* em-pixels */
    sprintf(zBuf, "%dpx", pValues->fFont->em_pixels);
    Tcl_ListObjAppendElement(0, pRet, Tcl_NewStringObj("em-pixels", -1));
    Tcl_ListObjAppendElement(0, pRet, Tcl_NewStringObj(zBuf, -1));

    /* nRef */
    Tcl_ListObjAppendElement(interp, pRet, Tcl_NewStringObj("nRef", -1));
    Tcl_ListObjAppendElement(interp, pRet, Tcl_NewIntObj(pValues->nRef));

    /* z-index */
    Tcl_ListObjAppendElement(interp, pRet, Tcl_NewStringObj("z-index", -1));
    if (pValues->iZIndex > MAX_PIXELVAL) {
        Tcl_ListObjAppendElement(interp, pRet, Tcl_NewIntObj(pValues->iZIndex));
    } else {
        Tcl_ListObjAppendElement(interp, pRet, Tcl_NewStringObj("auto", 4));
    }

    Tcl_SetObjResult(interp, pRet);
    Tcl_DecrRefCount(pRet);
    return TCL_OK;
}

#define HTML_REQUIRE_LAYOUT 2
#define HTML_REQUIRE_PAINT  1
#define HTML_OK     0
int 
HtmlComputedValuesCompare(pV1, pV2) 
    HtmlComputedValues *pV1;
    HtmlComputedValues *pV2;
{
    unsigned char *v1 = (unsigned char *)pV1;
    unsigned char *v2 = (unsigned char *)pV2;
    int ii;

    if (pV1 == pV2) {
        return HTML_OK;
    }

    /* 
     * Check for changes in the following properties:
     *
     *     '-tkhtml-replacement-image'
     *     'list-style-image'
     *     'font'
     *     'vertical-align'
     */
    if (
        !pV1 || !pV2 ||
        pV1->imReplacementImage != pV2->imReplacementImage ||
        pV1->imListStyleImage != pV2->imListStyleImage     ||
        pV1->fFont != pV2->fFont ||
        pV1->eVerticalAlign != pV2->eVerticalAlign ||
        (!pV1->eVerticalAlign && pV1->iVerticalAlign != pV1->iVerticalAlign)
    ) {
        return HTML_REQUIRE_LAYOUT;
    }

    for (ii = 0; ii < sizeof(propdef) / sizeof(propdef[0]); ii++){
        PropertyDef *pDef = &propdef[ii];
        if (pDef->isNolayout) continue;
        switch (pDef->eType) {

            case ENUM: {
                if (*(v1 + pDef->iOffset) != *(v2 + pDef->iOffset)) {
                    return HTML_REQUIRE_LAYOUT;
                }
                break;
            }

            case BORDERWIDTH:
            case LENGTH: {
                int *pL1 = (int *)(v1 + pDef->iOffset);
                int *pL2 = (int *)(v2 + pDef->iOffset);
 
                if (
                    *pL1 != *pL2 || 
                    ((pDef->mask & pV1->mask) != (pDef->mask & pV2->mask))
                ) {
                    return HTML_REQUIRE_LAYOUT;
                }
 
                break;
            }

            case COLOR:
            case IMAGE:
            case CUSTOM:
                break;
        }
    }

    return HTML_REQUIRE_PAINT;
}

