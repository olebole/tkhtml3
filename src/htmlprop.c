
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
static const char rcsid[] = "$Id: htmlprop.c,v 1.59 2006/04/12 13:14:12 danielk1977 Exp $";

#include "html.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define LOG if (p->pTree->options.logcmd)

/*
 * Convert a double value from a CssProperty to an integer.
 */
#define INTEGER(x) ((int)((x) + 0.49))

/*
 *---------------------------------------------------------------------------
 *
 * propertyToString --
 *
 *     This function is used for logging debugging info only.
 * 
 *     Return a pointer to a string representation of the CSS specified
 *     value contained in argument pProp. *pzFree is set to the value
 *     of a pointer (possibly NULL) that should be passed to HtmlFree()
 *     when the returned string is no longer required. Example:
 *
 *         char *zFree;
 *         char *zString;
 * 
 *         zString = propertyToString(pProp, &zFree);
 *         // Use zString for something (i.e. print to stdout)
 *         HtmlFree(zFree);
 *
 * Results:
 *     Pointer to string representation of property pProp.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static char *
propertyToString(pProp, pzFree)
    CssProperty *pProp;
    char **pzFree;
{
    char *zRet = (char *)HtmlCssPropertyGetString(pProp);
    *pzFree = 0;

    if (!zRet) {
        if (pProp->eType == CSS_TYPE_TCL || pProp->eType == CSS_TYPE_URL) {
            zRet = HtmlAlloc(strlen(pProp->v.zVal) + 6);
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

            zRet = HtmlAlloc(128);
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
static int 
pixelsToPoints(p, pixels)
    HtmlComputedValuesCreator *p;
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
            iScale = pProp->v.rVal * 0.01;
            break;

        case CSS_CONST_SMALLER: {
            HtmlNode *pParent = HtmlNodeParent(p->pNode);
            if (pParent) {
                int ii;
                int *aSize = p->pTree->aFontSizeTable;
                int ps = pParent->pPropertyValues->fFont->pKey->iFontSize;
                for (ii = 1; ii < 7 && aSize[ii] < ps; ii++);
                iPoints = ps + (aSize[ii-1] - aSize[ii]);
            } else {
                iPoints = p->pTree->aFontSizeTable[2];
            }
            break;
        }
        case CSS_CONST_LARGER: {
            HtmlNode *pParent = HtmlNodeParent(p->pNode);
            if (pParent) {
                int ii;
                int *aSize = p->pTree->aFontSizeTable;
                int ps = pParent->pPropertyValues->fFont->pKey->iFontSize;
                for (ii = 0; ii < 6 && aSize[ii] < ps; ii++);
                iPoints = ps + (aSize[ii+1] - aSize[ii]);
            } else {
                iPoints = p->pTree->aFontSizeTable[2];
            }
            break;
        }

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
            iPixels = INTEGER(pProp->v.rVal);
            break;

        /* Font-size is already in points or picas*/
        case CSS_TYPE_PC:
            iPoints = (int)(pProp->v.rVal / 12.0);
            break;
        case CSS_TYPE_PT:
            iPoints = INTEGER(pProp->v.rVal);
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
    HtmlComputedValuesCreator *p;
    unsigned char *pVar;
{
    const int values_offset = Tk_Offset(HtmlComputedValuesCreator, values);
    const int fontkey_offset = Tk_Offset(HtmlComputedValuesCreator, fontKey);
    const int values_end = values_offset + sizeof(HtmlComputedValues);
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
    HtmlComputedValuesCreator *p;
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
            cVal = (HtmlColor *)HtmlAlloc(sizeof(HtmlColor)+strlen(zColor)+1);
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
            p->values.iLineHeight = 
                HtmlNodeParent(p->pNode)->pPropertyValues->iLineHeight;
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
            HtmlNode *pParent = HtmlNodeParent(p->pNode);
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

#define SZ_AUTO     0x00000001
#define SZ_INHERIT  0x00000002
#define SZ_NONE     0x00000004
#define SZ_PERCENT  0x00000008
#define SZ_NEGATIVE 0x00000010

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
                HtmlNode *pParent = HtmlNodeParent(p->pNode);
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

        case CSS_TYPE_FLOAT: {
            int iVal = INTEGER(pProp->v.rVal);
            if (iVal >= 0 || allow_mask & SZ_NEGATIVE) {
                *pIVal = iVal;
                return 0;
            }
            return 1;
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
HtmlComputedValuesInit(pTree, pNode, p)
    HtmlTree *pTree;
    HtmlNode *pNode;
    HtmlComputedValuesCreator *p;
{
    static CssProperty Medium = {CSS_CONST_MEDIUM, {"medium"}};
    static CssProperty Transparent = {
        CSS_CONST_TRANSPARENT, {"transparent"}
    };
    int rc;

    HtmlNode *pParent = HtmlNodeParent(pNode);
    assert(p && pTree && pNode);

    memset(p, 0, sizeof(HtmlComputedValuesCreator));
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
        static CssProperty Black   = {CSS_CONST_BLACK, {"black"}};

        /* Regular HtmlComputedValues properties */
        p->values.eWhitespace     = CSS_CONST_NORMAL;   /* 'white-space' */
        p->values.eTextAlign      = CSS_CONST_LEFT;     /* 'text-align' */ 
        p->values.iBorderSpacing = 0;                   /* 'border-spacing' */
        p->values.iLineHeight = PIXELVAL_NORMAL;        /* 'line-height' */
        rc = propertyValuesSetColor(p, &p->values.cColor, &Black); /* 'color' */
        assert(rc == 0);

        /* List properties */
        p->values.eListStyleType  = CSS_CONST_DISC;     
        p->values.eListStylePosition = CSS_CONST_OUTSIDE;
        p->values.imListStyleImage = 0;

        /* The font properties */
        propertyValuesSetFontSize(p, &Medium);          /* 'font-size'  */
        p->fontKey.zFontFamily = "Helvetica";           /* 'font-family'      */
        p->fontKey.isItalic = 0;                        /* 'font-style'       */
        p->fontKey.isBold = 0;                          /* 'font-weight'      */
    } else {
        static CssProperty Inherit = {CSS_CONST_INHERIT, {"inherit"}};
        HtmlComputedValues *pV = pParent->pPropertyValues;
        HtmlFontKey *pFK = pV->fFont->pKey;

        /* The font properties */
        memcpy(&p->fontKey, pFK, sizeof(HtmlFontKey));

        /* List properties */
        p->values.eListStyleType = pV->eListStyleType;  /* 'list-style-type' */
        p->values.eListStylePosition = pV->eListStylePosition;
        rc = propertyValuesSetImage(p, &p->values.imListStyleImage, &Inherit); 
        assert(rc == 0);

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

    p->values.cOutlineColor = 0;
    propertyValuesSetBorderWidth(p, &p->values.iOutlineWidth, 0, &Medium);
    p->values.eOutlineStyle = CSS_CONST_NONE;

    p->values.imBackgroundImage = 0;
    p->values.eBackgroundRepeat = CSS_CONST_REPEAT;
    p->values.eBackgroundAttachment = CSS_CONST_SCROLL;
    p->values.iBackgroundPositionX = 0;
    p->values.iBackgroundPositionY = 0;

    p->values.imReplacementImage = 0;

    p->values.eVerticalAlign = CSS_CONST_BASELINE;   /* 'vertical-align' */
    p->values.iVerticalAlign = 0;

    p->values.ePosition = CSS_CONST_STATIC;
    p->values.position.iTop = PIXELVAL_AUTO;
    p->values.position.iBottom = PIXELVAL_AUTO;
    p->values.position.iLeft = PIXELVAL_AUTO;
    p->values.position.iRight = PIXELVAL_AUTO;
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
                "tcl() script returned \"%s\" - "
                "type mismatch for property '%s'",
                Tcl_GetString(HtmlNodeCommand(p->pTree, p->pNode)),
                zRes, HtmlCssPropertyToString(eProp)
            );
        }
        HtmlFree((char *)pVal);
        return 1;
    }

    /* Now that we've successfully called HtmlComputedValuesSet(), the
     * CssProperty structure (it's associated string data is what matters)
     * cannot be HtmlFree()d until after HtmlComputedValuesFinish() is called.
     * So we make a linked list of such structures at p->pDeleteList using
     * CssProperty.v.p as the pNext pointer.
     * 
     * HtmlComputedValuesFinish() deletes the list when it is called.
     */
    pVal->v.p = (void *)p->pDeleteList;
    p->pDeleteList = pVal;

    return 0;
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
    static int border_style_options[] = { 
	CSS_CONST_NONE,    CSS_CONST_HIDDEN,    CSS_CONST_DOTTED,  
        CSS_CONST_DASHED,  CSS_CONST_SOLID,     CSS_CONST_DOUBLE,
        CSS_CONST_GROOVE,  CSS_CONST_RIDGE,     CSS_CONST_INSET,
        CSS_CONST_OUTSET,  0
    };

    if (!pProp) {
        return 0;
    }

    LOG {
        char *zFree;
        char *zPropVal = propertyToString(pProp, &zFree);
        HtmlLog(p->pTree, "STYLEENGINE", "%s %s -> %s",
                Tcl_GetString(HtmlNodeCommand(p->pTree, p->pNode)),
                HtmlCssPropertyToString(eProp), zPropVal
        );
        if (zFree) HtmlFree(zFree);
    }

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
		CSS_CONST_NONE,      CSS_CONST_DECIMAL, CSS_CONST_LOWER_ALPHA,
		CSS_CONST_UPPER_ALPHA, CSS_CONST_LOWER_ROMAN,
                CSS_CONST_UPPER_ROMAN, 0 
            };
            unsigned char *pEVar = &(p->values.eListStyleType);
            return propertyValuesSetEnum(p, pEVar, options, pProp);
        }
        case CSS_PROPERTY_LIST_STYLE_POSITION: {
            int options[] = {
                CSS_CONST_OUTSIDE, CSS_CONST_INSIDE, 0
            };
            unsigned char *pEVar = &(p->values.eListStylePosition);
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
        case CSS_PROPERTY_BACKGROUND_REPEAT: {
            int options[] = {
                CSS_CONST_REPEAT,    CSS_CONST_REPEAT_X,  CSS_CONST_REPEAT_Y,
                CSS_CONST_NO_REPEAT, 0
            };
            unsigned char *pEVar = &(p->values.eBackgroundRepeat);
            return propertyValuesSetEnum(p, pEVar, options, pProp);
        }
        case CSS_PROPERTY_BACKGROUND_ATTACHMENT: {
            int options[] = {
                CSS_CONST_SCROLL, CSS_CONST_FIXED, 0
            };
            unsigned char *pEVar = &(p->values.eBackgroundAttachment);
            return propertyValuesSetEnum(p, pEVar, options, pProp);
        }

        case CSS_PROPERTY_OUTLINE_STYLE: {
            unsigned char *pEVar = &(p->values.eOutlineStyle);
            return propertyValuesSetEnum(p, pEVar, border_style_options, pProp);
        }
        case CSS_PROPERTY_OUTLINE_COLOR: {
            HtmlColor **pCVar = &(p->values.cOutlineColor);
            return propertyValuesSetColor(p, pCVar, pProp);
        }
        case CSS_PROPERTY_OUTLINE_WIDTH: {
            return propertyValuesSetBorderWidth(p, 
                &(p->values.iOutlineWidth), PROP_MASK_OUTLINE_WIDTH, pProp
            );
        }

        case CSS_PROPERTY_POSITION: {
            int options[] = {
                CSS_CONST_STATIC, CSS_CONST_RELATIVE, 
                CSS_CONST_FIXED, CSS_CONST_ABSOLUTE, 0
            };
            unsigned char *pEVar = &(p->values.ePosition);
            return propertyValuesSetEnum(p, pEVar, options, pProp);
        }
        case CSS_PROPERTY_TOP: 
            return propertyValuesSetSize(p, &(p->values.position.iTop),
                PROP_MASK_TOP, pProp, SZ_INHERIT|SZ_PERCENT|SZ_AUTO
            );
        case CSS_PROPERTY_BOTTOM: 
            return propertyValuesSetSize(p, &(p->values.position.iBottom),
                PROP_MASK_BOTTOM, pProp, SZ_INHERIT|SZ_PERCENT|SZ_AUTO
            );
        case CSS_PROPERTY_RIGHT: 
            return propertyValuesSetSize(p, &(p->values.position.iRight),
                PROP_MASK_RIGHT, pProp, SZ_INHERIT|SZ_PERCENT|SZ_AUTO
            );
        case CSS_PROPERTY_LEFT: 
            return propertyValuesSetSize(p, &(p->values.position.iLeft),
                PROP_MASK_LEFT, pProp, SZ_INHERIT|SZ_PERCENT|SZ_AUTO
            );

        /* 
         * Color properties: 
         *
	 *     'background-color', 'color', 'border-top-color',
	 *     'border-right-color', 'border-bottom-color',
	 *     'border-left-color',
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
                PROP_MASK_WIDTH, pProp, SZ_INHERIT|SZ_PERCENT|SZ_AUTO
            );
        case CSS_PROPERTY_MIN_WIDTH:
            return propertyValuesSetSize(p, &(p->values.iMinWidth),
                PROP_MASK_MIN_WIDTH, pProp, SZ_INHERIT|SZ_PERCENT
            );
        case CSS_PROPERTY_MAX_WIDTH:
            return propertyValuesSetSize(p, &(p->values.iMaxWidth),
                PROP_MASK_MAX_WIDTH, pProp, SZ_INHERIT|SZ_PERCENT|SZ_NONE
            );
        case CSS_PROPERTY_HEIGHT: 
            return propertyValuesSetSize(p, &(p->values.iHeight),
                PROP_MASK_HEIGHT, pProp, SZ_INHERIT|SZ_AUTO
            );
        case CSS_PROPERTY_MIN_HEIGHT:
            return propertyValuesSetSize(p, &(p->values.iMinHeight),
                PROP_MASK_MIN_HEIGHT, pProp, SZ_INHERIT|SZ_PERCENT
            );
        case CSS_PROPERTY_MAX_HEIGHT:
            return propertyValuesSetSize(p, &(p->values.iMaxHeight),
                PROP_MASK_MAX_HEIGHT, pProp, SZ_INHERIT|SZ_PERCENT|SZ_NONE
            );
        case CSS_PROPERTY_PADDING_TOP:
            return propertyValuesSetSize(p, &(p->values.padding.iTop),
                PROP_MASK_PADDING_TOP, pProp, SZ_INHERIT|SZ_PERCENT
            );
        case CSS_PROPERTY_PADDING_LEFT:
            return propertyValuesSetSize(p, &(p->values.padding.iLeft),
                PROP_MASK_PADDING_LEFT, pProp, SZ_INHERIT|SZ_PERCENT
            );
        case CSS_PROPERTY_PADDING_RIGHT:
            return propertyValuesSetSize(p, &(p->values.padding.iRight),
                PROP_MASK_PADDING_RIGHT, pProp, SZ_INHERIT|SZ_PERCENT
            );
        case CSS_PROPERTY_PADDING_BOTTOM:
            return propertyValuesSetSize(p, &(p->values.padding.iBottom),
                PROP_MASK_PADDING_BOTTOM, pProp, SZ_INHERIT|SZ_PERCENT
            );
        case CSS_PROPERTY_MARGIN_TOP:
            return propertyValuesSetSize(p, &(p->values.margin.iTop),
                PROP_MASK_MARGIN_TOP, pProp, 
                SZ_INHERIT|SZ_PERCENT|SZ_AUTO|SZ_NEGATIVE
            );
        case CSS_PROPERTY_MARGIN_LEFT:
            return propertyValuesSetSize(p, &(p->values.margin.iLeft),
                PROP_MASK_MARGIN_LEFT, pProp, 
                SZ_INHERIT|SZ_PERCENT|SZ_AUTO|SZ_NEGATIVE
            );
        case CSS_PROPERTY_MARGIN_RIGHT:
            return propertyValuesSetSize(p, &(p->values.margin.iRight),
                PROP_MASK_MARGIN_RIGHT, pProp, 
                SZ_INHERIT|SZ_PERCENT|SZ_AUTO|SZ_NEGATIVE
            );
        case CSS_PROPERTY_MARGIN_BOTTOM:
            return propertyValuesSetSize(p, &(p->values.margin.iBottom),
                PROP_MASK_MARGIN_BOTTOM, pProp, 
                SZ_INHERIT|SZ_PERCENT|SZ_AUTO|SZ_NEGATIVE
            );

        case CSS_PROPERTY_BACKGROUND_POSITION_X:
            return propertyValuesSetSize(p, &(p->values.iBackgroundPositionX),
                PROP_MASK_BACKGROUND_POSITION_X, pProp, SZ_INHERIT|SZ_PERCENT
            );
        case CSS_PROPERTY_BACKGROUND_POSITION_Y:
            return propertyValuesSetSize(p, &(p->values.iBackgroundPositionY),
                PROP_MASK_BACKGROUND_POSITION_Y, pProp, SZ_INHERIT|SZ_PERCENT
            );

        /* 'vertical-align', special case: */
        case CSS_PROPERTY_VERTICAL_ALIGN:
            return propertyValuesSetVerticalAlign(p, pProp);

        /* 'line-height', special case: */
        case CSS_PROPERTY_LINE_HEIGHT: 
            return propertyValuesSetLineHeight(p, pProp);

        case CSS_PROPERTY_BORDER_SPACING: {
            /* Note: The CSS2 property 'border-spacing' can be set to
             * either a single or pair of length values. Only a single
             * value is supported at the moment, which is enough to support
             * the html 4.01 cellspacing attribute.
             */
            return propertyValuesSetSize(p, &(p->values.iBorderSpacing),
                PROP_MASK_BORDER_SPACING, pProp, 0
            );
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
            if (eType == CSS_CONST_ITALIC || eType == CSS_CONST_OBLIQUE) {
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
                &(p->values.border.iTop), PROP_MASK_BORDER_TOP_WIDTH, pProp
            );
        case CSS_PROPERTY_BORDER_BOTTOM_WIDTH:
            return propertyValuesSetBorderWidth(p, 
                &(p->values.border.iBottom),PROP_MASK_BORDER_BOTTOM_WIDTH, pProp
            );
        case CSS_PROPERTY_BORDER_LEFT_WIDTH:
            return propertyValuesSetBorderWidth(p, 
                &(p->values.border.iLeft), PROP_MASK_BORDER_LEFT_WIDTH, pProp
            );
        case CSS_PROPERTY_BORDER_RIGHT_WIDTH:
            return propertyValuesSetBorderWidth(p, 
                &(p->values.border.iRight), PROP_MASK_BORDER_RIGHT_WIDTH, pProp
            );

        case CSS_PROPERTY__TKHTML_REPLACEMENT_IMAGE:
            return propertyValuesSetImage(p, 
                &p->values.imReplacementImage, pProp
            );
        case CSS_PROPERTY_BACKGROUND_IMAGE:
            return propertyValuesSetImage(p, 
                &p->values.imBackgroundImage, pProp
            );
        case CSS_PROPERTY_LIST_STYLE_IMAGE:
            return propertyValuesSetImage(p, 
                &p->values.imListStyleImage, pProp
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
    const char *DEFAULT_FONT_FAMILY = "Helvetica";

    int iFontSize = pFontKey->iFontSize;
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

    pFont = (HtmlFont *)HtmlAlloc(sizeof(HtmlFont) + strlen(zTkFontName) + 1);
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
    pFont->em_pixels = pFont->metrics.ascent;

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
        {PROP_MASK_WIDTH,              OFFSET(iWidth)},
        {PROP_MASK_MIN_WIDTH,          OFFSET(iMinWidth)},
        {PROP_MASK_MAX_WIDTH,          OFFSET(iMaxWidth)},
        {PROP_MASK_HEIGHT,             OFFSET(iHeight)},
        {PROP_MASK_MIN_HEIGHT,         OFFSET(iMinHeight)},
        {PROP_MASK_MAX_HEIGHT,         OFFSET(iMaxHeight)},
        {PROP_MASK_MARGIN_TOP,         OFFSET(margin.iTop)},
        {PROP_MASK_MARGIN_RIGHT,       OFFSET(margin.iRight)},
        {PROP_MASK_MARGIN_BOTTOM ,     OFFSET(margin.iBottom)},
        {PROP_MASK_MARGIN_LEFT,        OFFSET(margin.iLeft)},
        {PROP_MASK_PADDING_TOP,        OFFSET(padding.iTop)},
        {PROP_MASK_PADDING_RIGHT,      OFFSET(padding.iRight)},
        {PROP_MASK_PADDING_BOTTOM,     OFFSET(padding.iBottom)},
        {PROP_MASK_PADDING_LEFT,       OFFSET(padding.iLeft)},
        {PROP_MASK_VERTICAL_ALIGN,     OFFSET(iVerticalAlign)},
        {PROP_MASK_BORDER_TOP_WIDTH,    OFFSET(border.iTop)},
        {PROP_MASK_BORDER_RIGHT_WIDTH,  OFFSET(border.iRight)},
        {PROP_MASK_BORDER_BOTTOM_WIDTH, OFFSET(border.iBottom)},
        {PROP_MASK_BORDER_LEFT_WIDTH,   OFFSET(border.iLeft)},
        {PROP_MASK_LINE_HEIGHT,         OFFSET(iLineHeight)},
        {PROP_MASK_OUTLINE_WIDTH,       OFFSET(iOutlineWidth)},
        {PROP_MASK_TOP,                 OFFSET(position.iTop)},
        {PROP_MASK_BOTTOM,              OFFSET(position.iBottom)},
        {PROP_MASK_LEFT,                OFFSET(position.iLeft)},
        {PROP_MASK_RIGHT,               OFFSET(position.iRight)}
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
    } else if (pValues->eBackgroundAttachment == CSS_CONST_FIXED) {
        /* If this is a new entry and the 'background-attachment' property
         * computes to "fixed", then increment HtmlTree.nFixedBackground.
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
            HtmlFree((char *)p1);
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
        HtmlFree((char *)pFont);
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
        HtmlFree((char *)pColor);
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
            if (pValues->eBackgroundAttachment == CSS_CONST_FIXED) {
                pTree->nFixedBackground++;
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
        pColor = (HtmlColor *)HtmlAlloc(sizeof(HtmlColor));
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
    pColor = (HtmlColor *)HtmlAlloc(sizeof(HtmlColor));
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
        Tcl_HashEntry *pEntry = Tcl_FindHashEntry(&pTree->aColor, *pzCursor);
        assert(pEntry);
        HtmlColor *pColor = (HtmlColor *)Tcl_GetHashValue(pEntry);
        decrementColorRef(pTree, pColor);
    }
}

enum PropertyValueType {
    ENUM, COLOR, LENGTH, VERTICALALIGN, FONT, IMAGE, BACKGROUNDPOSITION
};

#define ENUMVAL(eProp, var) \
{ENUM, CSS_PROPERTY_ ## eProp, Tk_Offset(HtmlComputedValues, var), 0}

/* 
 * A Color value. Since a red pixel is on most displays the same size as a
 * green one, color values never change the layout of a document. 
 */
#define COLORVAL(eProp, var) \
{COLOR, CSS_PROPERTY_ ## eProp, Tk_Offset(HtmlComputedValues, var), 0}

/*
 * A length. Changing a length requires a re-layout.
 */
#define LENGTHVAL(eProp, var) \
{LENGTH, CSS_PROPERTY_ ## eProp, Tk_Offset(HtmlComputedValues, var), \
PROP_MASK_ ## eProp}

/*
 * An image. Changing a background image does not require a relayout, but
 * modifying a replacement or list-marker image does.
 */
#define IMAGEVAL(eProp, var) \
{IMAGE, CSS_PROPERTY_ ## eProp, Tk_Offset(HtmlComputedValues, var), 0}

/*
 * Property 'vertical-align' is in a catetory of it's own. It causes relayout.
 */
#define VERTICALALIGNVAL() {VERTICALALIGN, CSS_PROPERTY_VERTICAL_ALIGN, 0, 0}

/*
 * The font - HtmlComputedValues.fFont. Changing it causes relayout.
 */
#define FONTVAL() {FONT, CSS_SHORTCUTPROPERTY_FONT, 0, 0}

/*
 * The 'background-position' property. Changing it does not cause relayout.
 */
#define BACKGROUNDPOSITIONVAL() \
{BACKGROUNDPOSITION, CSS_SHORTCUTPROPERTY_BACKGROUND_POSITION, 0, 0}

struct PVDef {
    enum PropertyValueType eType;
    int eCssProperty;
    int iOffset;
    unsigned int mask;
} pvdef[] = {
    COLORVAL (BACKGROUND_COLOR, cBackgroundColor),
    COLORVAL (BORDER_BOTTOM_COLOR, cBorderBottomColor),
    ENUMVAL  (BORDER_BOTTOM_STYLE, eBorderBottomStyle),
    LENGTHVAL(BORDER_BOTTOM_WIDTH, border.iBottom),
    COLORVAL (BORDER_LEFT_COLOR, cBorderLeftColor),
    ENUMVAL  (BORDER_LEFT_STYLE, eBorderLeftStyle),
    LENGTHVAL(BORDER_LEFT_WIDTH, border.iLeft),
    COLORVAL (BORDER_RIGHT_COLOR, cBorderRightColor),
    ENUMVAL  (BORDER_RIGHT_STYLE, eBorderRightStyle),
    LENGTHVAL(BORDER_RIGHT_WIDTH, border.iRight),
    LENGTHVAL(BORDER_SPACING, iBorderSpacing),
    COLORVAL (BORDER_TOP_COLOR, cBorderTopColor),
    ENUMVAL  (BORDER_TOP_STYLE, eBorderTopStyle),
    LENGTHVAL(BORDER_TOP_WIDTH, border.iTop),

    ENUMVAL  (OUTLINE_STYLE, eOutlineStyle),
    COLORVAL (OUTLINE_COLOR, cOutlineColor),
    LENGTHVAL(OUTLINE_WIDTH, iOutlineWidth),

    ENUMVAL  (CLEAR, eClear),
    COLORVAL (COLOR, cColor),
    ENUMVAL  (DISPLAY, eDisplay),
    ENUMVAL  (FLOAT, eFloat),
    FONTVAL(),
    LENGTHVAL(HEIGHT, iHeight),
    LENGTHVAL(LINE_HEIGHT, iLineHeight),
    ENUMVAL  (LIST_STYLE_TYPE, eListStyleType),
    ENUMVAL  (LIST_STYLE_POSITION, eListStylePosition),

    LENGTHVAL(MARGIN_BOTTOM, margin.iBottom),
    LENGTHVAL(MARGIN_LEFT, margin.iLeft),
    LENGTHVAL(MARGIN_RIGHT, margin.iRight),
    LENGTHVAL(MARGIN_TOP, margin.iTop),

    ENUMVAL  (POSITION, ePosition),
    LENGTHVAL(BOTTOM, position.iBottom),
    LENGTHVAL(LEFT, position.iLeft),
    LENGTHVAL(RIGHT, position.iRight),
    LENGTHVAL(TOP, position.iTop),

    LENGTHVAL(MAX_HEIGHT, iMaxHeight),
    LENGTHVAL(MAX_WIDTH, iMaxWidth),
    LENGTHVAL(MIN_HEIGHT, iMinHeight),
    LENGTHVAL(MIN_WIDTH, iMinWidth),

    LENGTHVAL(PADDING_BOTTOM, padding.iBottom),
    LENGTHVAL(PADDING_LEFT, padding.iLeft),
    LENGTHVAL(PADDING_RIGHT, padding.iRight),
    LENGTHVAL(PADDING_TOP, padding.iTop),

    ENUMVAL  (TEXT_ALIGN, eTextAlign),
    ENUMVAL  (TEXT_DECORATION, eTextDecoration),

    VERTICALALIGNVAL(),

    ENUMVAL  (WHITE_SPACE, eWhitespace),
    LENGTHVAL(WIDTH, iWidth),

    IMAGEVAL(_TKHTML_REPLACEMENT_IMAGE, imReplacementImage),
    IMAGEVAL(BACKGROUND_IMAGE, imBackgroundImage),
    IMAGEVAL(LIST_STYLE_IMAGE, imListStyleImage),
    ENUMVAL (BACKGROUND_REPEAT, eBackgroundRepeat),
    ENUMVAL (BACKGROUND_ATTACHMENT, eBackgroundAttachment),
    BACKGROUNDPOSITIONVAL()
};

int 
HtmlNodeProperties(interp, pValues)
    Tcl_Interp *interp;
    HtmlComputedValues *pValues;
{
    Tcl_Obj *pRet;

    int ii;
    struct PVDef *pDef;
    char *v = (char *)pValues;

    pRet = Tcl_NewObj();
    Tcl_IncrRefCount(pRet);

    pDef = &pvdef[0];
    for (ii = 0; ii < sizeof(pvdef) / sizeof(pvdef[0]); ii++, pDef++) {
        Tcl_Obj *pValue;
        CONST char *zName = HtmlCssPropertyToString(pDef->eCssProperty);
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
                            pDef->eCssProperty == CSS_PROPERTY_LINE_HEIGHT && 
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
            case VERTICALALIGN: {
                int eValue = pValues->eVerticalAlign;
                if (0 == eValue) {
                    char zBuf[64];
                    int iValue = pValues->iVerticalAlign;
                    sprintf(zBuf, "%dpx", iValue);
                    pValue = Tcl_NewStringObj(zBuf, -1);
                } else {
                    CONST char *zValue = HtmlCssConstantToString(eValue);
                    pValue = Tcl_NewStringObj(zValue, -1);
                }
                break;
            }
            case FONT: {
                pValue = Tcl_NewStringObj(pValues->fFont->zFont, -1);
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

            case BACKGROUNDPOSITION: {
                char zBuf[128];
                if (pValues->mask & PROP_MASK_BACKGROUND_POSITION_X) {
                    assert(pValues->mask & PROP_MASK_BACKGROUND_POSITION_Y);
                    sprintf(zBuf, "%.2f%% %.2f%%", 
                        (double)pValues->iBackgroundPositionX/100.0,
                        (double)pValues->iBackgroundPositionY/100.0
                    );
                } else {
                    assert(!(pValues->mask & PROP_MASK_BACKGROUND_POSITION_Y));
                    sprintf(zBuf, "%dpx %dpx", 
                        pValues->iBackgroundPositionX,
                        pValues->iBackgroundPositionY
                    );
                }
                pValue = Tcl_NewStringObj(zBuf, -1);
                break;
            }

            default:
                assert(!"Not possible");
        }
        Tcl_ListObjAppendElement(interp, pRet, pValue);
    }

    Tcl_ListObjAppendElement(interp, pRet, Tcl_NewStringObj("nRef", -1));
    Tcl_ListObjAppendElement(interp, pRet, Tcl_NewIntObj(pValues->nRef));

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
    struct PVDef *pDef;
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

    pDef = &pvdef[0];
    for (ii = 0; ii < sizeof(pvdef) / sizeof(pvdef[0]); ii++, pDef++) {
        switch (pDef->eType) {
            case ENUM:
                if (pDef->eCssProperty != CSS_PROPERTY_TEXT_DECORATION) {
                    if (*(v1 + pDef->iOffset) != *(v2 + pDef->iOffset)) {
                        return HTML_REQUIRE_LAYOUT;
                    }
                }
                break;
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
        }
    }

    return HTML_REQUIRE_PAINT;
}

