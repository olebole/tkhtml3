/*
 * cssInt.h - 
 *
 *     This header defines the internal structures and functions
 *     used internally by the tkhtml CSS module. 
 *
 * TODO: Copyright disclaimer.
 */

#ifndef __CSSINT_H__
#define __CSSINT_H__

#include "css.h"
#include "cssparse.h"
#include <tcl.h>

typedef struct CssSelector CssSelector;
typedef struct CssPropertySet CssPropertySet;
typedef struct CssPropertyMask CssPropertyMask;
typedef struct CssRule CssRule;
typedef struct CssParse CssParse;
typedef struct CssToken CssToken;
typedef struct CssPriority CssPriority;

typedef unsigned char u8;
typedef unsigned int u32;

/*
 * Ways in which simple selectors and pseudo-classes can be chained 
 * together to form complex selectors.
 */
#define CSS_SELECTORCHAIN_DESCENDANT     1    /* eg. "a b" */
#define CSS_SELECTORCHAIN_CHILD          2    /* eg. "a > b" */
#define CSS_SELECTORCHAIN_ADJACENT       3    /* eg. "a + b" */

/*
 * Simple selector types.
 */
#define CSS_SELECTOR_UNIVERSAL           4    /* eg. "*" */
#define CSS_SELECTOR_TYPE                5
#define CSS_SELECTOR_ATTR                7
#define CSS_SELECTOR_ATTRVALUE           8
#define CSS_SELECTOR_ATTRLISTVALUE       9
#define CSS_SELECTOR_ATTRHYPHEN          10

/*
** Psuedo-classes
*/
#define CSS_PSEUDOCLASS_LANG             11
#define CSS_PSEUDOCLASS_FIRSTCHILD       12
#define CSS_PSEUDOCLASS_LINK             13
#define CSS_PSEUDOCLASS_VISITED          14
#define CSS_PSEUDOCLASS_ACTIVE           15
#define CSS_PSEUDOCLASS_HOVER            16
#define CSS_PSEUDOCLASS_FOCUS            17

/*
** Pseudo-elements.
*/
#define CSS_PSEUDOELEMENT_FIRSTLINE      18
#define CSS_PSEUDOELEMENT_FIRSTLETTER    19
#define CSS_PSEUDOELEMENT_BEFORE         20
#define CSS_PSEUDOELEMENT_AFTER          21

/*
** CSS media types.
*/
#define CSS_MEDIA_ALL          22
#define CSS_MEDIA_AURAL        23
#define CSS_MEDIA_BRAILLE      24
#define CSS_MEDIA_EMBOSSED     25
#define CSS_MEDIA_HANDHELD     26
#define CSS_MEDIA_PRINT        27
#define CSS_MEDIA_PROJECTION   28
#define CSS_MEDIA_SCREEN       29
#define CSS_MEDIA_TTY          30
#define CSS_MEDIA_TV           31

#define CSS_SELECTOR_NEVERMATCH 32

/*
 * Before they are passed to the lemon-generated parser, the tokenizer
 * splits the output into tokens of the following type.
 */
struct CssToken {
    const char *z;
    int n;
};

/*
 * A CSS selector is stored as a linked list of the CssSelector structure.
 * The first element in the list is the rightmost simple-selector in the 
 * selector text. For example, the selector "h1 h2 > p" (match elements of
 * type <p> that is a child of an <h2> that is a descendant of an <h1>)
 * is stored as [p]->[h2]->[h1].
 *
 * See the function selectorTest() in css.c for details of how this is
 * used.
 */
struct CssSelector {
    u8 eSelector;     /* CSS_SELECTOR* or CSS_PSEUDO* value */
    char *zAttr;      /* The attribute queried, if any. */
    char *zValue;     /* The value tested for, if any. */
    CssSelector *pNext;  /* Next simple-selector in chain */
};

struct CssPropertyMask {
  u32 a[4];
};

/*
** A collection of CSS2 properties and values.
**
** A CssPropertyMask is defined above. It is a bitmask with one bit
** for each CSS property, identifying the contents of this property-set.
** The first N entries in 'aProp', where N is the number of bits set in 
** 'mask', are pointers to property values. The entries are sorted 
** according to the value assigned to the property constant (CSS_PROPERTY_*
** symbol). nProp is the number of pointers allocated at aProp.
**
*/
struct CssPropertySet {
    CssPropertyMask mask;   /* Contents of property-set. */
    int nProp;              /* Allocated slots in aProp[] array. */
    int nRef;               /* Number of CssRule objects that point to this */
    CssProperty **aProp;    /* Array of pointers to property values. */
};

struct CssProperties {
    int nRule;
    CssRule **apRule;
};

struct CssRule {
    u8 origin;               /* CSS_ORIGIN_* value */
    Tcl_Obj *pStyleId;       /* Second and subsequent parts of stylesheet id */

    int specificity;         /* Specificity of the selector */
    CssSelector *pSelector;  /* The selector-chain for this rule */
    int freePropertySets;          /* True to delete pPropertySet */
    CssPropertySet *pPropertySet;  /* Property values for the rule. */
    CssRule *pNext;                /* Next rule in this list. */
};

/*
 * A linked list of the following structures is stored in
 * CssStyleSheet.pPriority.
 *
 * Each time a call is made to [<widget> style] to add a new stylesheet to
 * the configuration, two instances of this structure are allocated using
 * ckalloc() and inserted into the list. The CssPriority.origin and
 * CssPriority.pIdTail variables are set to the origin and id-tail of the
 * new stylesheet (based on parsing the stylesheet-id) in both instances.
 * In one instance the CssPriority.important flag is set to true, in the
 * other false.
 *
 * The list is kept in order from highest priority to lowest priority based
 * on the values of CssPriority.important, CssPriority.origin and
 * CssPriority.pIdTail. Each time the list is reorganized, the values of
 * CssPriority.iPriority are set such that higher priority list members
 * have lower values of CssPriority.iPriority.
 *
 * Each CssRule structure has a pointer to it's associated CssPriority
 * structure.
 */
struct CssPriority {
    int important;           /* True if !IMPORTANT flag is set */
    int origin;              /* One of CSS_ORIGIN_AGENT, _AUTHOR or _USER */ 
    Tcl_Obj *pIdTail;        /* Tail of the stylesheet id */
    int iPriority;
    CssPriority *pNext;      /* Linked list pointer */
};

/*
 * A style-sheet contains zero or more rules. Depending on the nature of
 * the selector for the rule, it is either stored in a linked list starting
 * at CssStyleSheet.pUniversalRules, or in a linked list stored in the
 * hash table CssStyleSheet.rules. The CssStyleSheet.rules hash is indexed
 * by the tag type of the elements that the rule applies to.
 *
 * For example, the rule "H1 {text-decoration: bold}" is stored in a linked
 * list accessible by looking up "h1" in the rules hash table.
 */
struct CssStyleSheet {
    int nSyntaxErr;            /* Number of syntax errors during parsing */
    CssRule *pUniversalRules;
    CssPriority *pPriority;
#if 0
    Tcl_HashTable aByType;
    Tcl_HashTable aById;
    Tcl_HashTable aByClass;
#endif
};

/*
 * A single instance of this object is used for each parse. After the parse
 * is finished it is no longer required, the permanent record of the parsed
 * stylesheet is built up in CssParse.pStyle.
 */
struct CssParse {
    CssStyleSheet *pStyle;
    CssSelector *pSelector;         /* Selector currently being parsed */
    int nXtra;
    CssSelector **apXtraSelector;   /* Selectors also waiting for prop set. */
    CssPropertySet *pPropertySet;   /* Declarations being parsed. */
    CssPropertySet *pImportant;     /* !IMPORTANT declarations. */
    int origin;
    Tcl_Obj *pStyleId;
    CssPriority *pPriority1;
    CssPriority *pPriority2;
};

/*
 * These functions are called by the lemon-generated parser (see
 * cssparse.y). They add rules to the stylesheet.
 */
void HtmlCssDeclaration(CssParse *, CssToken *, CssToken *);
void HtmlCssSelector(CssParse *, int, CssToken *, CssToken *);
void HtmlCssRule(CssParse *, int);
void HtmlCssSelectorComma(CssParse *pParse);

/*
 * Called by the parser to transform between the name of a psuedo-class or
 * psuedo-selector to a CSS_PSEUDO... value that can be passed to
 * HtmlCssSelector().
 */
int HtmlCssPseudo(CssToken *);

#endif /* __CSS_H__ */
