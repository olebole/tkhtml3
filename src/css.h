
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
typedef struct CssNodeInterface CssNodeInterface;

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
int HtmlCssParse(int, const char *, CssStyleSheet **);
int tkhtmlCssParseStyle(int, const char *, CssStyleSheet **);
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
 */
struct CssNodeInterface {
    const char * (*xType)(void *);
    const char * (*xAttr)(void *, const char *);
    void * (*xParent)(void *);
    int (*xNumChild)(void *);
    void * (*xChild)(void *, int);
    const char * (*xLang)(void *);
    int (*xParentIdx)(void *);
};
void HtmlCssStyleSheetApply
(CssStyleSheet *, CssNodeInterface *, void *, CssProperties**);

/*
** Functions to interface with the results of a style application.
*/
void HtmlCssPropertiesFree(CssProperties *);
const char *tkhtmlCssPropertiesGet(CssProperties *, int);
CssProperties *tkhtmlCssPropertiesGetBefore(CssProperties *);
CssProperties *tkhtmlCssPropertiesGetAfter(CssProperties *);
Tcl_Obj * HtmlCssPropertiesTclize(CssProperties *pProperties);

#define CSS_VISITED   0x01
#define CSS_HOVER     0x02
#define CSS_ACTIVE    0x04
#define CSS_FOCUS     0x08

/*
** Register the TCL interface with the supplied interpreter.
*/
#include <tcl.h>
int tkhtmlCssTclInterface(Tcl_Interp *);

#endif
