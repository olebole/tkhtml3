
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
 *
 * CssNodeInterface: An interface implemented by the application and used by 
 *                   this module to access document nodes and their attributes
 *                   when applying the cascade algorithm. The theory is that
 *                   the module is completely agnostic as to the way the
 *                   document tree is implemented.
 */
typedef struct CssStyleSheet CssStyleSheet;
typedef struct CssProperties CssProperties;
typedef struct CssProperty CssProperty;
typedef struct CssNodeInterface CssNodeInterface;

/*
 * A single CSS property is represented by an instance of the following
 * struct. The actual value is stored in one of the primitives inside
 * the union. The eType field is set to one of the CSS_TYPE_* constants
 * below. 
 */
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

/* CSS2 physical units. */
#define CSS_TYPE_CENTIMETER   10           /* Value in 'rVal */
#define CSS_TYPE_INCH         11           /* Value in 'rVal */
#define CSS_TYPE_MILLIMETER   12           /* Value in 'rVal */

/* CSS2 Keywords */
#define CSS_TYPE_INHERIT      13           /* No value */

struct CssProperty {
    int eType;
    union {
        int iVal;
        double rVal;
        char *zVal;
    } v;
};

EXTERN CONST char *HtmlCssPropertyGetString(CssProperty *pProp);

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
int HtmlCssParseStyle(int, const char *, CssProperties **);
int HtmlCssStyleSheetSyntaxErrs(CssStyleSheet *);
void HtmlCssStyleSheetFree(CssStyleSheet *);

/*
 * Functions (and a structure) to apply a stylesheet to a document node.
 *
 * xType:      Return the type of document node. (eg. "h1" or "p")
 * xAttr:      Return the value of the specified node attribute. Or NULL, if
 *             the attribute is not defined.
 * xParent:    Return the parent node. Or NULL, if the node is document root.
 * xNumChild:  Return the number of children the node has.
 * xChild:     Return the nth child of the node, 0 indexed from left to right.
 * xLang:      Return the language (i.e. "english") of the node.
 * xParendIdx: Return the index of the node in it's parent, or -1 if 
 *             the node is the document root.
 * xProperties: Return the CSS properties associated with the node.
 */
struct CssNodeInterface {
    const char * (*xType)(void *);
    const char * (*xAttr)(void *, const char *);
    void * (*xParent)(void *);
    int (*xNumChild)(void *);
    void * (*xChild)(void *, int);
    const char * (*xLang)(void *);
    int (*xParentIdx)(void *);
    CssProperties *(*xProperties)(void *);
};
void HtmlCssStyleSheetApply
(CssStyleSheet *, CssNodeInterface CONST *, void *, CssProperties**);

/*
 * Functions to interface with the results of a style application.
 */
#define CSS_ORIGIN_AGENT  1
#define CSS_ORIGIN_USER   2
#define CSS_ORIGIN_AUTHOR 3
void HtmlCssPropertiesFree(CssProperties *);
CssProperty *HtmlCssPropertiesGet(CssProperties *, int);
CssProperty *HtmlCssPropertiesGet2(CssProperties *, int, int*, int*);

/* Future interface for :before and :after pseudo-elements. Need this to 
 * handle the <br> tag most elegantly.
 */
CssProperties *HtmlCssPropertiesGetBefore(CssProperties *);
CssProperties *HtmlCssPropertiesGetAfter(CssProperties *);

#define CSS_VISITED   0x01
#define CSS_HOVER     0x02
#define CSS_ACTIVE    0x04
#define CSS_FOCUS     0x08

CssProperty *HtmlCssStringToProperty(CONST char *z, int n);

/*
** Register the TCL interface with the supplied interpreter.
*/
#include <tcl.h>
int tkhtmlCssTclInterface(Tcl_Interp *);

#endif
