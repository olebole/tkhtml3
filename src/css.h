
/*
 * css.h --
 *  
 *     This header file contains the interface used by other modules to
 *     access the CSS parsing/cascade module.
 */

#ifndef __CSS_H__
#define __CSS_H__

#include <tcl.h>
#include "cssprop.h"

/*
 * This header file contains the public interface to the CSS2 parsing 
 * module. The module parses CSS2 and implements the cascade algorithm.
 * It does not interpret property values, except for short-cut properties, 
 * which are translated to the equivalent canonical properties.
 *
 * This module uses a fairly complicated object-oriented interface. The 
 * objects have the following roles:
 *
 * CssProperty:      A single property value.
 *
 * CssProperties:    A collection of CSS2 properties. Can be queried for the
 *                   value or presence of a specific property.
 *
 * CssStyleSheet:    The parsed representation of a style-sheet document or
 *                   an HTML style attribute (i.e. from a tag like: 
 *                   '<h1 style="font : italic">').
 */
typedef struct CssStyleSheet CssStyleSheet;
typedef struct CssProperties CssProperties;
typedef struct CssProperty CssProperty;

/* Include html.h after we define our opaque types, because it includes
 * structures that contain pointers to them.
 */
#include "html.h"

#define CSS_TYPE_EM           1            /* Value in 'rVal' */
#define CSS_TYPE_PX           2            /* Value in 'iVal' */
#define CSS_TYPE_PT           3            /* Value in 'iVal' */
#define CSS_TYPE_PC           14           /* Value in 'rVal' */
#define CSS_TYPE_EX           15           /* Value in 'rVal' */
#define CSS_TYPE_STRING       4            /* Value in 'sVal' */
#define CSS_TYPE_PERCENT      5            /* Value in 'iVal' */
#define CSS_TYPE_FLOAT        6            /* Value in 'rVal' */
#define CSS_TYPE_NONE         7            /* No value */

#define CSS_TYPE_TCL          8            /* Value in 'zVal' */
#define CSS_TYPE_URL          9            /* Value in 'zVal' */

/* Physical units. */
#define CSS_TYPE_CENTIMETER   10           /* Value in 'rVal */
#define CSS_TYPE_INCH         11           /* Value in 'rVal */
#define CSS_TYPE_MILLIMETER   12           /* Value in 'rVal */

/* Magical types */
#define CSS_TYPE_XCOLOR       16           /* p points at XColor */

/*
 * A single CSS property is represented by an instance of the following
 * struct. The actual value is stored in one of the primitives inside
 * the union. The eType field is set to one of the CSS_TYPE_* constants
 * below. 
 */
struct CssProperty {
    int eType;
    union {
        int iVal;
        double rVal;
        char *zVal;
        void *p;
    } v;
};

/*
 * Retrieve the string value of a CSS property. This works with all
 * internally consistent CssProperty objects, regardless of the
 * CssProperty.eType value.
 */
EXTERN CONST char *HtmlCssPropertyGetString(CssProperty *pProp);

/*
 * Create a property from a value string (i.e. "20ex" or "yellow", 
 * not "h1 {font:large}").
 */
EXTERN CssProperty *HtmlCssStringToProperty(CONST char *z, int n);

/*
 * Functions to parse stylesheet and style data into CssStyleSheet objects.
 *
 * A CssStyleSheet object is created from either the text of a stylesheet
 * document (function HtmlCssParse()) or the value of a style="..." 
 * HTML attribute (function tkhtmlCssParseStyle()). The primary use
 * of a CssStyleSheet object is to incorporate it into a CssCascade
 * object.
 *
 * HtmlCssStyleSheetSyntaxErrs() returns the number of syntax errors
 * that occured while parsing the stylesheet or style attribute.
 *
 * tkhtmlCssStyleSheetFree() frees the memory used to store a stylesheet
 * object internally.
 */
EXTERN int HtmlCssParse(Tcl_Obj *, int, Tcl_Obj *, CssStyleSheet **);
EXTERN int HtmlCssParseStyle(int, CONST char *, CssProperties **);
EXTERN int HtmlCssStyleSheetSyntaxErrs(CssStyleSheet *);
EXTERN void HtmlCssStyleSheetFree(CssStyleSheet *);

/*
 * Function to apply a stylesheet to a document node.
 */
EXTERN void HtmlCssStyleSheetApply(CssStyleSheet *, HtmlNode *);

/*
 * Functions to interface with the results of a style application.
 */
#define CSS_ORIGIN_AGENT  1
#define CSS_ORIGIN_USER   2
#define CSS_ORIGIN_AUTHOR 3
EXTERN void HtmlCssPropertiesFree(CssProperties *);
EXTERN CssProperty *HtmlCssPropertiesGet(CssProperties *, int, int*, int*);

#if 0

/* Future interface for :before and :after pseudo-elements. Need this to 
 * handle the <br> tag most elegantly.
 */
EXTERN CssProperties *HtmlCssPropertiesGetBefore(CssProperties *);
EXTERN CssProperties *HtmlCssPropertiesGetAfter(CssProperties *);

#define CSS_VISITED   0x01
#define CSS_HOVER     0x02
#define CSS_ACTIVE    0x04
#define CSS_FOCUS     0x08

#endif

#endif
