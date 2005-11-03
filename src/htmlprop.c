
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
static char rcsid[] = "@(#) $Id:";

#include "html.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

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
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
pixelsToPoints(p, pixels)
    HtmlPropertyValuesCreator *p;
    int pixels;
{
    double mm;
    Tcl_Obj *pObj = Tcl_NewIntObj(pixels);
    Tcl_IncrRefCount(pObj);
    Tk_GetMMFromObj(p->pTree->interp, p->pTree->tkwin, pObj, &mm);
    Tcl_DecrRefCount(pObj);
    return (int) ((mm * 72.0 / 25.4) + 0.5);
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
    HtmlPropertyValuesCreator *p;
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
 * propertyValuesSetFontSize --
 *
 *     This function sets the HtmlPropertyValuesCreator.fontKey.iFontSize
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
    HtmlPropertyValuesCreator *p;
    CssProperty *pProp;
{
    int iPoints = 0;
    int iPixels = 0;
    double iScale = -1.0;
    assert(pProp);

    if (pProp->eType == CSS_CONST_INHERIT) {
        HtmlNode *pParent = HtmlNodeParent(p->pNode);
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
            HtmlNode *pParent = HtmlNodeParent(p->pNode);
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
            iScale = (double)pProp->v.iVal * 0.01;
            break;
        case CSS_CONST_SMALLER:
            iScale = 0.8333;
            break;
        case CSS_CONST_LARGER:
            iScale = 1.2;
            break;

        /* Font-size is in terms of the font-size table */
        case CSS_CONST_XX_SMALL:
            iPoints = p->pTree->aFontSizeTable[0];
            break;
        case CSS_CONST_X_SMALL:
            iPoints = p->pTree->aFontSizeTable[1];
            break;
        case CSS_CONST_SMALL:
            iPoints = p->pTree->aFontSizeTable[2];
            break;
        case CSS_CONST_MEDIUM:
            iPoints = p->pTree->aFontSizeTable[3];
            break;
        case CSS_CONST_LARGE:
            iPoints = p->pTree->aFontSizeTable[4];
            break;
        case CSS_CONST_X_LARGE:
            iPoints = p->pTree->aFontSizeTable[5];
            break;
        case CSS_CONST_XX_LARGE:
            iPoints = p->pTree->aFontSizeTable[6];
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
            iPixels = pProp->v.iVal;
            break;

        /* Font-size is already in points or picas*/
        case CSS_TYPE_PC:
            iPoints = (int)(pProp->v.rVal / 12.0);
            break;
        case CSS_TYPE_PT:
            iPoints = pProp->v.iVal;
            break;

        default:   /* Type-mismatch error */
            return 1;
    }

    if (iPixels > 0) {
        p->fontKey.iFontSize = pixelsToPoints(p, iPixels);
    } else if (iPoints > 0) {
        p->fontKey.iFontSize = iPoints;
    } else if (iScale > 0.0) {
       HtmlNode *pParent = HtmlNodeParent(p->pNode);
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
    HtmlPropertyValuesCreator *p;
    unsigned char *pVar;
{
    const int values_offset = Tk_Offset(HtmlPropertyValuesCreator, values);
    const int fontkey_offset = Tk_Offset(HtmlPropertyValuesCreator, fontKey);
    const int values_end = values_offset + sizeof(HtmlPropertyValues);
    const int fontkey_end = fontkey_offset + sizeof(HtmlFontKey);

    int offset = pVar - (unsigned char *)p;
    HtmlNode *pParent = HtmlNodeParent(p->pNode);

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
 *     aOptions is a 0-terminated list of integers (all CSS_CONST_XXX values).
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
    HtmlPropertyValuesCreator *p;
    unsigned char *pEVar;
    int *aOptions;
    CssProperty *pProp;
{
    int val = pProp->eType;
    int *pOpt;

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
 *     HtmlPropertyValues structure) to point to the corresponding entry in the 
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
    HtmlPropertyValuesCreator *p;
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
        color = Tk_GetColor(pTree->interp, pTree->tkwin, zColor);
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
            cVal = (HtmlColor *)ckalloc(sizeof(HtmlColor) + strlen(zColor) + 1);
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
propertyValuesSetLength(p, pIVal, em_mask, pProp)
    HtmlPropertyValuesCreator *p;
    int *pIVal;
    unsigned int em_mask;
    CssProperty *pProp;
{
    switch (pProp->eType) {

        case CSS_TYPE_EM:
            if (em_mask == 0) return 1;
            *pIVal = (int)(pProp->v.rVal * 100.0);
            p->em_mask |= em_mask;
            break;
        case CSS_TYPE_EX:
            if (em_mask == 0) return 1;
            *pIVal = (int)(pProp->v.rVal * 100.0);
            p->ex_mask |= em_mask;
            break;

        case CSS_TYPE_PX:
            *pIVal = pProp->v.iVal;
            break;

        case CSS_TYPE_PT:
            *pIVal = physicalToPixels(p, (double)pProp->v.iVal, 'p');
            break;
        case CSS_TYPE_PC:
            *pIVal = physicalToPixels(p, pProp->v.rVal * 12.0, 'p');
            break;
        case CSS_TYPE_CENTIMETER:
            *pIVal = physicalToPixels(p, pProp->v.rVal, 'c');
            break;
        case CSS_TYPE_INCH:
            *pIVal = physicalToPixels(p, pProp->v.rVal, 'i');
            break;
        case CSS_TYPE_MILLIMETER:
            *pIVal = physicalToPixels(p, pProp->v.rVal, 'm');
            break;

        default:
            return 1;
    }

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
    HtmlPropertyValuesCreator *p;
    CssProperty *pProp;
{
    static const unsigned int MASK = PROP_MASK_VERTICALALIGN;
    int rc = 0;

    switch (pProp->eType) {
        case CSS_CONST_INHERIT: {
            HtmlNode *pParent = HtmlNodeParent(p->pNode);
            HtmlPropertyValues *pPV;

            assert(pParent && pParent->pPropertyValues);
            pPV = pParent->pPropertyValues;

            p->values.mask = (p->values.mask & (~MASK)) | (pPV->mask & MASK);
            p->values.iVerticalAlign = pPV->iVerticalAlign;

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
            p->values.iVerticalAlign = pProp->eType;

            p->eVerticalAlignPercent = 0;
            p->em_mask &= (~MASK);
            p->ex_mask &= (~MASK);
            break;

        case CSS_TYPE_PERCENT: {
            p->values.mask |= MASK;
            p->values.iVerticalAlign = 100 * pProp->v.iVal;

            p->eVerticalAlignPercent = 1;
            p->em_mask &= (~MASK);
            p->ex_mask &= (~MASK);
            break;
        }

        default: {
            /* Try to treat the property as a <length> */
            int *pIVal = &p->values.iVerticalAlign;
            rc = propertyValuesSetLength(p, &pIVal, MASK, pProp);
            if (rc == 0) {
                p->values.mask |= MASK;
                p->eVerticalAlignPercent = 0;
            }
            return 0;
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
propertyValuesSetSize(p, pIVal, p_mask, pProp)
    HtmlPropertyValuesCreator *p;
    int *pIVal;
    unsigned int p_mask;
    CssProperty *pProp;
{
    assert(p_mask != 0);

    /* Clear the bits in the inherit and percent masks for this property */
    p->values.mask &= ~p_mask;
    p->em_mask &= ~p_mask;
    p->ex_mask &= ~p_mask;

    switch (pProp->eType) {

        /* TODO Percentages are still stored as integers - this is wrong */
        case CSS_TYPE_PERCENT:
            p->values.mask |= p_mask;
            *pIVal = (pProp->v.iVal * 100);
            return 0;

        case CSS_CONST_INHERIT:
            *pIVal = PIXELVAL_INHERIT;
            return 0;

        case CSS_CONST_AUTO:
            *pIVal = PIXELVAL_AUTO;
            return 0;

        case CSS_CONST_NONE:
            *pIVal = PIXELVAL_NONE;
            return 0;

        case CSS_TYPE_FLOAT:
            *pIVal = pProp->v.rVal;
            return 0;

        default:
            return propertyValuesSetLength(p, pIVal, p_mask, pProp);
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * propertyValuesSetBorderWidth --
 *
 *     pIVal points to an integer to store the value of a 'border-width-xxx'
 *     property in pixels in (i.e. HtmlPropertyValues.border.iTop). This
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
    HtmlPropertyValuesCreator *p;
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
    }

    /* If it is not one of the above keywords, then the border-width may 
     * be expressed as a CSS <length>.
     */
    if (0 == propertyValuesSetLength(p, pIVal, em_mask, pProp)) {
        return 0;
    }

    /* Not one of the keywords or a length -> type-mismatch error */
    return 1;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlPropertyValuesInit --
 *   
 *     Initialise an HtmlPropertyValuesCreator structure.
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
HtmlPropertyValuesInit(pTree, pNode, p)
    HtmlTree *pTree;
    HtmlNode *pNode;
    HtmlPropertyValuesCreator *p;
{
    static CssProperty Medium = {CSS_CONST_MEDIUM, {(int)"medium"}};
    static CssProperty Transparent = {
        CSS_CONST_TRANSPARENT, {(int)"transparent"}
    };
    int rc;

    HtmlNode *pParent = HtmlNodeParent(pNode);
    assert(p && pTree && pNode);

    memset(p, 0, sizeof(HtmlPropertyValuesCreator));
    p->pTree = pTree;
    p->pNode = pNode;

    /* The following properties are inherited by default. So the initial values
     * depend on whether or not there is a parent node:
     *
     *     'list-style-type', 'white-space', 'text-align', 'color',
     *     'border-spacing', 'line-height', 'font-size', 'font-style',
     *     'font-weight', 'font-family'.
     * 
     * There are more of these, but we don't support them yet.
     */
    if (!pParent) {
        static CssProperty Black   = {CSS_CONST_BLACK, {(int)"black"}};

        /* Regular HtmlPropertyValues properties */
        p->values.eListStyleType  = CSS_CONST_DISC;     /* 'list-style-type' */
        p->values.eWhitespace     = CSS_CONST_NORMAL;   /* 'white-space' */
        p->values.eTextAlign      = CSS_CONST_LEFT;     /* 'text-align' */ 
        p->values.iBorderSpacing = 0;                   /* 'border-spacing' */
        p->values.iLineHeight = PIXELVAL_NORMAL;        /* 'line-height' */
        rc = propertyValuesSetColor(p, &p->values.cColor, &Black); /* 'color' */
        assert(rc == 0);

        /* The font properties */
        propertyValuesSetFontSize(p, &Medium);          /* 'font-size'  */
        p->fontKey.zFontFamily = "Helvetica";           /* 'font-family'      */
        p->fontKey.isItalic = 0;                        /* 'font-style'       */
        p->fontKey.isBold = 0;                          /* 'font-weight'      */
    } else {
        static CssProperty Inherit = {CSS_CONST_INHERIT, {(int)"inherit"}};
        HtmlPropertyValues *pV = pParent->pPropertyValues;
        HtmlFontKey *pFK = pV->fFont->pKey;

        /* The font properties */
        memcpy(&p->fontKey, pFK, sizeof(HtmlFontKey));

        p->values.eListStyleType = pV->eListStyleType;  /* 'list-style-type' */
        p->values.eWhitespace = pV->eWhitespace;        /* 'white-space' */
        p->values.eTextAlign = pV->eTextAlign;          /* 'text-align' */ 
        p->values.iBorderSpacing = pV->iBorderSpacing;  /* 'border-spacing' */
        p->values.iLineHeight = pV->iLineHeight;        /* 'line-height' */
        rc = propertyValuesSetColor(p, &p->values.cColor, &Inherit); 
        assert(rc == 0);
    }

    /* Assign the initial values to other properties. */
    p->values.eDisplay        = CSS_CONST_INLINE;     /* 'display' */
    p->values.eFloat          = CSS_CONST_NONE;       /* 'float' */
    p->values.eClear          = CSS_CONST_NONE;       /* 'clear' */
    p->values.eTextDecoration = CSS_CONST_NONE;       /* 'text-decoration' */
    rc = propertyValuesSetColor(p, &p->values.cBackgroundColor, &Transparent);
    assert(rc == 0);
    p->values.iWidth = PIXELVAL_AUTO;
    p->values.iHeight = PIXELVAL_AUTO;
    p->values.iMinWidth = 0;
    p->values.iMaxWidth = PIXELVAL_NONE;
    p->values.iMinHeight = 0;
    p->values.iMaxHeight = PIXELVAL_NONE;
    p->values.padding.iTop = 0;
    p->values.padding.iRight = 0;
    p->values.padding.iBottom = 0;
    p->values.padding.iLeft = 0;
    p->values.margin.iTop = 0;
    p->values.margin.iRight = 0;
    p->values.margin.iBottom = 0;
    p->values.margin.iLeft = 0;
    p->values.eBorderTopStyle = CSS_CONST_NONE;
    p->values.eBorderRightStyle = CSS_CONST_NONE;
    p->values.eBorderBottomStyle = CSS_CONST_NONE;
    p->values.eBorderLeftStyle = CSS_CONST_NONE;
    propertyValuesSetBorderWidth(p, &p->values.border.iTop, 0, &Medium);
    propertyValuesSetBorderWidth(p, &p->values.border.iRight, 0, &Medium);
    propertyValuesSetBorderWidth(p, &p->values.border.iBottom, 0, &Medium);
    propertyValuesSetBorderWidth(p, &p->values.border.iLeft, 0, &Medium);
    p->values.cBorderTopColor = 0;
    p->values.cBorderRightColor = 0;
    p->values.cBorderBottomColor = 0;
    p->values.cBorderLeftColor = 0;

    /* TODO: Look at this one. It's trickier than the others. */
    p->values.iVerticalAlign = CSS_CONST_BASELINE;   /* 'vertical-align' */
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlPropertyValuesInit --
 *   
 *     Initialise an HtmlPropertyValuesCreator structure.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Initialises *p.
 *
 *---------------------------------------------------------------------------
 */
static int 
propertyValuesTclScript(p, eProp, zScript)
    HtmlPropertyValuesCreator *p;
    int eProp;
    const char *zScript;
{
    int rc;
    const char *zRes;
    CssProperty *pVal;
    Tcl_Interp *interp = p->pTree->interp;
    Tcl_Obj *pCommand = HtmlNodeCommand(interp, p->pNode);

    Tcl_SetVar2Ex(interp, "N", 0, pCommand, 0);
    rc = Tcl_Eval(interp, zScript);
    zRes = Tcl_GetStringResult(interp);
    if (rc == TCL_ERROR) {
        Tcl_BackgroundError(interp);
        return 1;
    }

    zRes = Tcl_GetStringResult(interp);
    assert(zRes);
    pVal = HtmlCssStringToProperty(zRes, -1);

    if (HtmlPropertyValuesSet(p, eProp, pVal)) {
        ckfree((char *)pVal);
        assert(0);
        return 1;
    }

    /* Now that we've successfully called HtmlPropertyValuesSet(), the
     * CssProperty structure (it's associated string data is what matters)
     * cannot be ckfree()d until after HtmlPropertyValuesFinish() is called. So
     * we make a linked list of such structures at p->pDeleteList using
     * CssProperty.v.p as the pNext pointer.
     * 
     * HtmlPropertyValuesFinish() deletes the list when it is called.
     */
    pVal->v.p = (void *)p->pDeleteList;
    p->pDeleteList = pVal;

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlPropertyValuesSet --
 *
 *     One or more calls to HtmlPropertyValuesSet() take place between the
 *     HtmlPropertyValuesInit() and HtmlPropertyValuesFinish() calls (see
 *     comments above HtmlPropertyValuesInit() for an API summary). The value
 *     of property eProp (one of the CSS_PROPERTY_XXX values) in either the
 *     HtmlPropertyValues or HtmlFontKey structure is set to the value
 *     contained by pProp.
 *
 *     Note: If pProp contains a pointer to a string, then the string must
 *           remain valid until HtmlPropertyValuesFinish() is called (see the
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
HtmlPropertyValuesSet(p, eProp, pProp)
    HtmlPropertyValuesCreator *p;
    int eProp;
    CssProperty *pProp;
{
    static int border_style_options[] = { 
	CSS_CONST_NONE,    CSS_CONST_HIDDEN,    CSS_CONST_DOTTED,  
        CSS_CONST_DASHED,  CSS_CONST_SOLID,     CSS_CONST_DOUBLE,
        CSS_CONST_GROOVE,  CSS_CONST_RIDGE,     CSS_CONST_INSET,
        CSS_CONST_OUTSET,  0
    };

    /* Silently ignore any attempt to set a root-node property to 'inherit'.
     * It's not a type-mismatch, we just want to leave the value unchanged.
     */
    if (pProp->eType == CSS_CONST_INHERIT && !HtmlNodeParent(p->pNode)) {
        return 0;
    }

    /* Special case - a Tcl script to evaluate */
    if (pProp->eType == CSS_TYPE_TCL) {
        return propertyValuesTclScript(p, eProp, pProp->v.zVal);
    }

    switch (eProp) {

        /* Simple enumerated type values:
         * 
	 *     'display', 'float', 'clear', 'text-decoration', 'white-space',
	 *     'text-align', 'list-style-type'.
         *
         * These are all handled by function propertyValuesSetEnum().
         */
        case CSS_PROPERTY_DISPLAY: {
            int options[] = {
                CSS_CONST_BLOCK,      CSS_CONST_INLINE,  CSS_CONST_TABLE,
                CSS_CONST_LIST_ITEM,  CSS_CONST_NONE,    CSS_CONST_TABLE_CELL,
                CSS_CONST_TABLE_ROW,  0
            };
            unsigned char *pEVar = &(p->values.eDisplay);
            return propertyValuesSetEnum(p, pEVar, options, pProp);
        }
        case CSS_PROPERTY_FLOAT: {
            int options[] = {
                CSS_CONST_LEFT, CSS_CONST_RIGHT, CSS_CONST_NONE, 0
            };
            unsigned char *pEVar = &(p->values.eFloat);
            return propertyValuesSetEnum(p, pEVar, options, pProp);
        }
        case CSS_PROPERTY_CLEAR: {
	    int options[] = { 
                CSS_CONST_LEFT,    CSS_CONST_RIGHT,    CSS_CONST_NONE,    
                CSS_CONST_BOTH,    0
            };
            unsigned char *pEVar = &(p->values.eClear);
            return propertyValuesSetEnum(p, pEVar, options, pProp);
        }
        case CSS_PROPERTY_TEXT_DECORATION: {
            int options[] = {
                CSS_CONST_UNDERLINE,     CSS_CONST_OVERLINE, 
                CSS_CONST_LINE_THROUGH,  CSS_CONST_NONE,      0 
            };
            unsigned char *pEVar = &(p->values.eTextDecoration);
            return propertyValuesSetEnum(p, pEVar, options, pProp);
        }
        case CSS_PROPERTY_WHITE_SPACE: {
            int options[] = {
                CSS_CONST_PRE,  CSS_CONST_NORMAL,  CSS_CONST_NOWRAP,  0
            };
            unsigned char *pEVar = &(p->values.eWhitespace);
            return propertyValuesSetEnum(p, pEVar, options, pProp);
        }
        case CSS_PROPERTY_TEXT_ALIGN: {
            int options[] = {
                CSS_CONST_LEFT,      CSS_CONST_RIGHT,  CSS_CONST_CENTER,
                CSS_CONST_JUSTIFY,   0
            };
            unsigned char *pEVar = &(p->values.eTextAlign);
            return propertyValuesSetEnum(p, pEVar, options, pProp);
        }
        case CSS_PROPERTY_LIST_STYLE_TYPE: {
            int options[] = {
                CSS_CONST_DISC,      CSS_CONST_CIRCLE,  CSS_CONST_SQUARE,
                CSS_CONST_NONE,      0
            };
            unsigned char *pEVar = &(p->values.eListStyleType);
            return propertyValuesSetEnum(p, pEVar, options, pProp);
        }
        case CSS_PROPERTY_BORDER_TOP_STYLE: {
            unsigned char *pEVar = &(p->values.eBorderTopStyle);
            return propertyValuesSetEnum(p, pEVar, border_style_options, pProp);
        }
        case CSS_PROPERTY_BORDER_BOTTOM_STYLE: {
            unsigned char *pEVar = &(p->values.eBorderBottomStyle);
            return propertyValuesSetEnum(p, pEVar, border_style_options, pProp);
        }
        case CSS_PROPERTY_BORDER_LEFT_STYLE: {
            unsigned char *pEVar = &(p->values.eBorderLeftStyle);
            return propertyValuesSetEnum(p, pEVar, border_style_options, pProp);
        }
        case CSS_PROPERTY_BORDER_RIGHT_STYLE: {
            unsigned char *pEVar = &(p->values.eBorderRightStyle);
            return propertyValuesSetEnum(p, pEVar, border_style_options, pProp);
        }


        /* 
         * Color properties: 
         *
	 *     'background-color', 'color', 'border-top-color',
	 *     'border-right-color', 'border-bottom-color',
	 *     'border-left-color'
         * 
         * Handled by propertyValuesSetColor().
         */
        case CSS_PROPERTY_BACKGROUND_COLOR: {
            HtmlColor **pCVar = &(p->values.cBackgroundColor);
            return propertyValuesSetColor(p, pCVar, pProp);
        }
        case CSS_PROPERTY_COLOR: {
            HtmlColor **pCVar = &(p->values.cColor);
            return propertyValuesSetColor(p, pCVar, pProp);
        }
        case CSS_PROPERTY_BORDER_TOP_COLOR: {
            HtmlColor **pCVar = &(p->values.cBorderTopColor);
            return propertyValuesSetColor(p, pCVar, pProp);
        }
        case CSS_PROPERTY_BORDER_BOTTOM_COLOR: {
            HtmlColor **pCVar = &(p->values.cBorderBottomColor);
            return propertyValuesSetColor(p, pCVar, pProp);
        }
        case CSS_PROPERTY_BORDER_LEFT_COLOR: {
            HtmlColor **pCVar = &(p->values.cBorderLeftColor);
            return propertyValuesSetColor(p, pCVar, pProp);
        }
        case CSS_PROPERTY_BORDER_RIGHT_COLOR: {
            HtmlColor **pCVar = &(p->values.cBorderRightColor);
            return propertyValuesSetColor(p, pCVar, pProp);
        }

        /* Pixel values that may be percentages or inherit from percentages */
        case CSS_PROPERTY_WIDTH: 
            return propertyValuesSetSize(p, &(p->values.iWidth),
                PROP_MASK_WIDTH, pProp
            );
        case CSS_PROPERTY_MIN_WIDTH:
            return propertyValuesSetSize(p, &(p->values.iMinWidth),
                PROP_MASK_MINWIDTH, pProp
            );
        case CSS_PROPERTY_MAX_WIDTH:
            return propertyValuesSetSize(p, &(p->values.iMaxWidth),
                PROP_MASK_MAXWIDTH, pProp
            );
        case CSS_PROPERTY_HEIGHT: 
            return propertyValuesSetSize(p, &(p->values.iHeight),
                PROP_MASK_HEIGHT, pProp
            );
        case CSS_PROPERTY_MIN_HEIGHT:
            return propertyValuesSetSize(p, &(p->values.iMinHeight),
                PROP_MASK_MINHEIGHT, pProp
            );
        case CSS_PROPERTY_MAX_HEIGHT:
            return propertyValuesSetSize(p, &(p->values.iMaxHeight),
                PROP_MASK_MAXHEIGHT, pProp
            );
        case CSS_PROPERTY_PADDING_TOP:
            return propertyValuesSetSize(p, &(p->values.padding.iTop),
                PROP_MASK_PADDINGTOP, pProp
            );
        case CSS_PROPERTY_PADDING_LEFT:
            return propertyValuesSetSize(p, &(p->values.padding.iLeft),
                PROP_MASK_PADDINGLEFT, pProp
            );
        case CSS_PROPERTY_PADDING_RIGHT:
            return propertyValuesSetSize(p, &(p->values.padding.iRight),
                PROP_MASK_PADDINGRIGHT, pProp
            );
        case CSS_PROPERTY_PADDING_BOTTOM:
            return propertyValuesSetSize(p, &(p->values.padding.iBottom),
                PROP_MASK_PADDINGBOTTOM, pProp
            );
        case CSS_PROPERTY_MARGIN_TOP:
            return propertyValuesSetSize(p, &(p->values.margin.iTop),
                PROP_MASK_MARGINTOP, pProp
            );
        case CSS_PROPERTY_MARGIN_LEFT:
            return propertyValuesSetSize(p, &(p->values.margin.iLeft),
                PROP_MASK_MARGINLEFT, pProp
            );
        case CSS_PROPERTY_MARGIN_RIGHT:
            return propertyValuesSetSize(p, &(p->values.margin.iRight),
                PROP_MASK_MARGINRIGHT, pProp
            );
        case CSS_PROPERTY_MARGIN_BOTTOM:
            return propertyValuesSetSize(p, &(p->values.margin.iBottom),
                PROP_MASK_MARGINBOTTOM, pProp
            );

        /* 'vertical-align', special case:
         */
        case CSS_PROPERTY_VERTICAL_ALIGN:
            return propertyValuesSetVerticalAlign(p, pProp);

        case CSS_PROPERTY_LINE_HEIGHT: {
            /* TODO */
            return 0;
        }
        case CSS_PROPERTY_BORDER_SPACING: {
            /* TODO */
            return 0;
        }

        /* Property 'font-size' */
        case CSS_PROPERTY_FONT_SIZE: 
            return propertyValuesSetFontSize(p, pProp);

        /*
         * Property 'font-family'
         * 
         * Just copy the pointer, not the string.
         */
        case CSS_PROPERTY_FONT_FAMILY: {
            const char *z = HtmlCssPropertyGetString(pProp);
            if (!z) {
                return 1;
            }
            p->fontKey.zFontFamily = z;
            return 0;
        }

        /*
	 * Property 'font-style': 
         *
	 * Keywords 'italic' and 'oblique' map to a Tk italic font. Keyword
	 * 'normal' maps to a non-italic font. Any other property value is
	 * rejected as a type-mismatch.
         */
        case CSS_PROPERTY_FONT_STYLE: {
            int eType = pProp->eType;
            if (eType == CSS_CONST_ITALIC || CSS_CONST_OBLIQUE) {
                p->fontKey.isItalic = 1;
            } else if (eType == CSS_CONST_NORMAL) {
                p->fontKey.isItalic = 0;
            } else {
                return 1;
            }
            return 0;
        }

        /*
	 * Property 'font-weight': 
         *
	 * Keywords 'bold' and 'bolder', and numeric values greater than 550
	 * map to a Tk bold font. Keywords 'normal' and 'lighter', along with
	 * numeric values less than 550 map to a non-bold font. Any other
	 * property value is rejected as a type-mismatch.
         */
	case CSS_PROPERTY_FONT_WEIGHT: {
            int eType = pProp->eType;
            if (eType == CSS_CONST_INHERIT) {
                HtmlNode *pParent = HtmlNodeParent(p->pNode);
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

        case CSS_PROPERTY_BORDER_TOP_WIDTH:
            return propertyValuesSetBorderWidth(p, 
                &(p->values.border.iTop), PROP_MASK_BORDERWIDTHTOP, pProp
            );
        case CSS_PROPERTY_BORDER_BOTTOM_WIDTH:
            return propertyValuesSetBorderWidth(p, 
                &(p->values.border.iBottom), PROP_MASK_BORDERWIDTHBOTTOM, pProp
            );
        case CSS_PROPERTY_BORDER_LEFT_WIDTH:
            return propertyValuesSetBorderWidth(p, 
                &(p->values.border.iLeft), PROP_MASK_BORDERWIDTHLEFT, pProp
            );
        case CSS_PROPERTY_BORDER_RIGHT_WIDTH:
            return propertyValuesSetBorderWidth(p, 
                &(p->values.border.iRight), PROP_MASK_BORDERWIDTHRIGHT, pProp
            );

        default:
            /* Unknown property */
            return 0;
    }
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
allocateNewFont(interp, tkwin, pFontKey)
    Tcl_Interp *interp;
    Tk_Window tkwin;
    HtmlFontKey *pFontKey;
{
    Tk_Font tkfont = 0;

    int iFontSize = pFontKey->iFontSize;
    const char *zFamily = pFontKey->zFontFamily;
    int isItalic = pFontKey->isItalic;
    int isBold = pFontKey->isBold;

    char zTkFontName[256];      /* Tk font name */
    HtmlFont *pFont;
    Tk_FontMetrics metrics;

    int iFallback = -1;
    struct FamilyMap {
        CONST char *cssFont;
        CONST char *tkFont;
    } familyMap [] = {
        {"serif",      "Times"},
        {"sans-serif", "Helvetica"},
        {"monospace",  "Courier"}
    };

    do {
        const char *zF;      /* Pointer to tk font family name */
        int iF = 0;          /* Length of tk font family name in bytes */

        if (0 == *zFamily) {
            if (iF < 0) {
                iF = 1;     /* End of the line default font: Helvetica */
            }
            zF = familyMap[iFallback].tkFont;
            iF = strlen(zF);
        } else {
            zF = zFamily;
            while (*zFamily && *zFamily != ',') zFamily++;
            iF = (zFamily - zF);
            if (*zFamily == ',') {
                iF--;
                zFamily++;
            }
            while (*zFamily == ' ') zFamily++;

            /* Trim spaces from the beginning and end of the string */
            while (iF > 0 && zF[iF] == ' ') iF--;
            while (iF > 0 && *zF == ' ') {
                iF--;
                zF++;
            }

            if (iF < 0) {
                const int n = sizeof(familyMap)/sizeof(struct FamilyMap);
                int i;
                for (i = 0; i < n; i++) {
                    if (
                        iF == strlen(familyMap[i].cssFont) && 
                        0 == strncmp(zF, familyMap[i].cssFont, iF)
                    ) {
                        iFallback = i;
                        break;
                    }
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
        if (!tkfont && *zFamily == 0) {
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

    pFont = (HtmlFont *)ckalloc(sizeof(HtmlFont) + strlen(zTkFontName) + 1);
    pFont->nRef = 0;
    pFont->tkfont = tkfont;
    pFont->zFont = (char *)&pFont[1];
    strcpy(pFont->zFont, zTkFontName);

    Tk_GetFontMetrics(tkfont, &metrics);
    pFont->em_pixels = metrics.ascent + metrics.descent;
    pFont->ex_pixels = Tk_TextWidth(tkfont, "x", 1);

    return pFont;
}
    

/*
 *---------------------------------------------------------------------------
 *
 * HtmlPropertyValuesFinish --
 *
 * Results: 
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
HtmlPropertyValues *
HtmlPropertyValuesFinish(p)
    HtmlPropertyValuesCreator *p;
{
    Tcl_HashEntry *pEntry;
    int ne;                /* New Entry */
    HtmlFont *pFont;
    int ii;
    HtmlPropertyValues *pValues = 0;
    HtmlColor *pColor;

#define OFFSET(x) Tk_Offset(HtmlPropertyValues, x)
    struct EmExMap {
        unsigned int mask;
        int offset;
    } emexmap[] = {
        {PROP_MASK_WIDTH,             OFFSET(iWidth)},
        {PROP_MASK_MINWIDTH,          OFFSET(iMinWidth)},
        {PROP_MASK_MAXWIDTH,          OFFSET(iMaxWidth)},
        {PROP_MASK_HEIGHT,            OFFSET(iHeight)},
        {PROP_MASK_MINHEIGHT,         OFFSET(iMinHeight)},
        {PROP_MASK_MAXHEIGHT,         OFFSET(iMaxHeight)},
        {PROP_MASK_MARGINTOP,         OFFSET(margin.iTop)},
        {PROP_MASK_MARGINRIGHT,       OFFSET(margin.iRight)},
        {PROP_MASK_MARGINBOTTOM ,     OFFSET(margin.iBottom)},
        {PROP_MASK_MARGINLEFT,        OFFSET(margin.iLeft)},
        {PROP_MASK_PADDINGTOP,        OFFSET(padding.iTop)},
        {PROP_MASK_PADDINGRIGHT,      OFFSET(padding.iRight)},
        {PROP_MASK_PADDINGBOTTOM,     OFFSET(padding.iBottom)},
        {PROP_MASK_PADDINGLEFT,       OFFSET(padding.iLeft)},
        {PROP_MASK_VERTICALALIGN,     OFFSET(iVerticalAlign)},
        {PROP_MASK_BORDERWIDTHTOP,    OFFSET(border.iTop)},
        {PROP_MASK_BORDERWIDTHRIGHT,  OFFSET(border.iRight)},
        {PROP_MASK_BORDERWIDTHBOTTOM, OFFSET(border.iBottom)},
        {PROP_MASK_BORDERWIDTHLEFT,   OFFSET(border.iLeft)}
    };
#undef OFFSET

    /* Find the font to use. If there is not a matching font in the aFont hash
     * table already, allocate a new one.
     */
    pEntry = Tcl_CreateHashEntry(&p->pTree->aFont, (char *)&p->fontKey, &ne);
    if (ne) {
        pFont = allocateNewFont(p->pTree->interp, p->pTree->tkwin, &p->fontKey);
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

    /* TODO: Deal with 'line-height' property */
    p->values.iLineHeight = pFont->em_pixels;

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

    /* Deal with the 'vertical-align' property */
    if (p->eVerticalAlignPercent) {
        /* TODO: Calculate as a % of 'line-height' */
    }
    if (p->values.eDisplay == CSS_CONST_TABLE_CELL && (
           (p->values.mask & PROP_MASK_VERTICALALIGN) || (
               p->values.iVerticalAlign != CSS_CONST_TOP &&
               p->values.iVerticalAlign != CSS_CONST_BOTTOM &&
               p->values.iVerticalAlign != CSS_CONST_MIDDLE))
    ) {
        p->values.mask &= ~PROP_MASK_VERTICALALIGN;
        p->values.iVerticalAlign = CSS_CONST_BASELINE;
    }

    /* Look the values structure up in the hash-table. */
    pEntry = Tcl_CreateHashEntry(&p->pTree->aValues, (char *)&p->values, &ne);
    pValues = (HtmlPropertyValues *)Tcl_GetHashKey(&p->pTree->aValues, pEntry);
    if (!ne) {
	/* If this is not a new entry, we need to decrement the reference count
         * on the font and all the color values.
         */
        pValues->fFont->nRef--;
        pValues->cColor->nRef--;
        pValues->cBackgroundColor->nRef--;
        pValues->cBorderTopColor->nRef--;
        pValues->cBorderRightColor->nRef--;
        pValues->cBorderBottomColor->nRef--;
        pValues->cBorderLeftColor->nRef--;
    }

    /* Delete any CssProperty structures allocated for Tcl properties */
    if (p->pDeleteList) {
        CssProperty *p1 = p->pDeleteList;
        while (p1) {
            CssProperty *p2 = (CssProperty *)p1->v.p;
            ckfree((char *)p1);
            p1 = p2;
        }
        p->pDeleteList = 0;
    }

    pValues->nRef++;
    return pValues;
}

void 
HtmlPropertyValuesRelease(pValues)
    HtmlPropertyValues *pValues;
{
    if (pValues) {
        pValues->nRef--;
        assert(pValues->nRef >= 0);
    }

    if (pValues->nRef == 0) {
        /* Clean up structure, decrement ref-counts on colors and the font */
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlPropertyValuesSetupTables --
 * 
 *     This function is called during widget initialisation to initialise the
 *     three hash-tables used by code in this file:
 *
 *         HtmlTree.aColor
 *         HtmlTree.aFont
 *         HtmlTree.aValues
 *
 *     and the font-size lookup table:
 * 
 *         HtmlTree.aFontSizeTable
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
HtmlPropertyValuesSetupTables(pTree)
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

    pType = HtmlPropertyValuesHashType();
    Tcl_InitCustomHashTable(&pTree->aValues, TCL_CUSTOM_TYPE_KEYS, pType);

    /* Initialise the color table */
    for (ii = 0; ii < sizeof(color_map)/sizeof(struct CssColor); ii++) {
        pColor = (HtmlColor *)ckalloc(sizeof(HtmlColor));
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
    pColor = (HtmlColor *)ckalloc(sizeof(HtmlColor));
    pColor->zColor = "transparent";
    pColor->nRef = 1;
    pColor->xcolor = 0;
    Tcl_SetHashValue(pEntry, pColor);
   
    /* Initialise the font-size table */
    pTree->aFontSizeTable[3] = 10;            /* medium */
    pTree->aFontSizeTable[4] = pTree->aFontSizeTable[3] * (1.2);
    pTree->aFontSizeTable[5] = pTree->aFontSizeTable[3] * (1.2 * 1.2);
    pTree->aFontSizeTable[6] = pTree->aFontSizeTable[3] * (1.2 * 1.2 * 1.2);
    pTree->aFontSizeTable[2] = pTree->aFontSizeTable[3] / (1.2);
    pTree->aFontSizeTable[1] = pTree->aFontSizeTable[3] / (1.2 * 1.2);
    pTree->aFontSizeTable[0] = pTree->aFontSizeTable[3] / (1.2 * 1.2 * 1.2);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlPropertyValuesCleanupTables --
 * 
 *     This function is called during widget destruction to deallocate
 *     resources allocated by HtmlPropertyValuesSetupTables(). This should be
 *     called after HtmlPropertyValues references have been released (otherwise
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
 *     Cleans up resources allocated by HtmlPropertyValuesSetupTables().
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlPropertyValuesCleanupTables(pTree)
    HtmlTree *pTree;
{
    assert(0);
}


