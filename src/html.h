/*
 * htmltree.h --
 *
 *     This file defines the tree structure used by the widget to store
 *     and traverse documents. Even though the data structure is in this
 *     header file, and therefore accessible by all files in the package,
 *     files other than htmltree.c usually use the functions listed
 *     below to access the tree.
 *
 *     In theory this might make it easier to reuse some of the CSS code
 *     with a different tree structure, but this is a long way off yet.
 *
 * COPYRIGHT.
 *
 */

#ifndef __HTMLTREE_H__
#define __HTMLTREE_H__

#define HTML_DEBUG

#include <tk.h>
#include "htmltokens.h"
#include "css.h"

/*
 * Version information for the package mechanism.
 */
#define HTML_PKGNAME "Tkhtml"
#define HTML_PKGVERSION "2.0"

/*
 * Various data types.  This code is designed to run on a modern
 * cached architecture where the CPU runs a lot faster than the
 * memory bus.  Hence we try to pack as much data into as small a space
 * as possible so that it is more likely to fit in cache.  The
 * extra CPU instruction or two needed to unpack the data is not
 * normally an issue since we expect the speed of the memory bus 
 * to be the limiting factor.
 */
typedef unsigned char  Html_u8;      /* 8-bit unsigned integer */
typedef short          Html_16;      /* 16-bit signed integer */
typedef unsigned short Html_u16;     /* 16-bit unsigned integer */
typedef int            Html_32;      /* 32-bit signed integer */

/*
 * Linux doesn't have a stricmp() function.
 */
#ifndef HAVE_STRICMP
# define stricmp strcasecmp
# define strnicmp strncasecmp
#endif

typedef struct HtmlTree HtmlTree;
typedef struct HtmlNode HtmlNode;
typedef struct HtmlToken HtmlToken;
typedef struct HtmlScaledImage HtmlScaledImage;
typedef struct HtmlCachedProperty HtmlCachedProperty;
typedef struct HtmlTokenMap HtmlTokenMap;
typedef struct HtmlCanvas HtmlCanvas;
typedef struct HtmlCanvasItem HtmlCanvasItem;
typedef struct HtmlFloatList HtmlFloatList;

typedef int (*HtmlContentTest)(HtmlNode *, int);

#define FLOAT_LEFT       1
#define FLOAT_RIGHT      2
#define FLOAT_NONE       3

#define CLEAR_NONE 1
#define CLEAR_LEFT 2
#define CLEAR_RIGHT 3
#define CLEAR_BOTH 4

struct HtmlTokenMap {
  char *zName;                    /* Name of a markup */
  Html_16 type;                   /* Markup type code */
  Html_u8 flags;                  /* Combination of HTMLTAG values */
  HtmlContentTest xClose;         /* Function to identify close tag */
  HtmlTokenMap *pCollide;         /* Hash table collision chain */
};

#define HTMLTAG_END         0x01
#define HTMLTAG_INLINE      0x02
#define HTMLTAG_BLOCK       0x04
#define HTMLTAG_EMPTY       0x08

struct HtmlToken {
    HtmlToken *pNext;           /* Next input token in a list of them all */
    HtmlToken *pPrev;           /* Previous token in a list of them all */
    Html_u8 type;
    Html_16 count;
    union {
        int newline;
        char *zText;
        char **zArgs;
    } x;
};

/* Stylesheets interpreted by Tkhtml may assign any property a value of the
 * form "tcl(TCL-SCRIPT)" (not normally valid CSS). When this property is
 * read for the first time during the layout process, the name of the node
 * object command is appended to the tcl-script and the result evaluated.
 * The return value is interpreted as a property string (i.e. "12px").
 *
 * The results of these callbacks are cached in the linked list starting at
 * HtmlNode.pCache. The cache is considered valid for the lifetime of
 * the tree.
 */
struct HtmlCachedProperty {
    int iProp;                 /* Property id - i.e. CSS_PROPERTY_WIDTH */
    CssProperty *pProp;        /* Pointer to property */
    HtmlCachedProperty *pNext; /* Nexted property cached by node */
};

/* Each node of the document tree is represented as an HtmlNode structure.
 * This structure carries no information to do with the node itself, it is
 * simply used to build the tree structure. All the information for the
 * node is stored in the HtmlElement object.
 */
struct HtmlNode {
    HtmlNode *pParent;             /* Parent of this node */
    HtmlToken *pToken;             /* Html element associated with node */
    int nChild;                    /* Number of child nodes */
    HtmlNode **apChildren;         /* Array of pointers to children nodes */

    CssProperties *pProperties;    /* The CSS properties from stylesheets */
    CssProperties *pStyle;         /* The CSS properties from style attribute */
    HtmlCachedProperty *pCache;    /* List of properties set by Tcl scripts. */

    Tcl_Obj *pCommand;             /* Tcl command for this node. */
};

struct HtmlScaledImage {
    Tk_Image image;
    Tcl_Obj *pImageName ;
    Tk_Image scaled_image;
    Tcl_Obj *pScaledImageName ;
};

struct HtmlCanvas {
    int left;
    int right;
    int top;
    int bottom;
    HtmlCanvasItem *pFirst;
    HtmlCanvasItem *pLast;
    Tcl_Obj *pPrimitives;
};

/* 
 * Variable 'iCol' stores the number of characters tokenized since the last
 * newline encountered in the document source. When we encounter a TAB
 * character, it is converted to (8-(iCol%8)) spaces. This makes text
 * inside a block with the 'white-space' property set to "pre" look good
 * even if the input contains tabs. 
 *
 * The aImage hash table maps from image name (i.e. the string returned by
 * [image create photo]) and a pointer to an HtmlScaledImage structure. The
 * HtmlScaledImage structure stores a scaled version of the image, if any
 * scaling was necessary.
 *
 * Each html tag to be treated as a script tag has an entry in the
 * 'aScriptHandler' hash table. The table maps from tag-type (i.e. Html_P)
 * to a Tcl_Obj* that contains the script to call when the tag type is
 * encountered. A single argument is appended to the script - all the text
 * between the start and end tag. The ref-count of the Tcl_Obj* should be
 * decremented if it is removed from the hash table.
 *
 * The aFontCache hash table maps from font-name to Tk_Font value.
 */
struct HtmlTree {
    Tcl_Interp *interp;             /* Tcl interpreter widget owned by */
    Tk_Window win;                  /* Main window of interpreter */

    Tcl_Obj *pDocument;             /* Text of the html document */
    int nParsed;                    /* Bytes of the html document tokenized */
    int iCol;                       /* Current column in document */

    HtmlToken *pFirst;              /* First token parsed */
    HtmlToken *pLast;               /* Last token parsed */
    HtmlNode *pCurrent;             /* The node currently being built. */
    HtmlNode *pRoot;                /* The root-node of the document. */

    Tcl_HashTable aScriptHandler;   /* Script handler callbacks. */
    Tcl_HashTable aNodeHandler;     /* Script handler callbacks. */

    CssStyleSheet *pStyle;          /* Style sheet configuration */

    Tcl_HashTable aFontCache;       /* All fonts used by canvas (by name) */
    Tcl_HashTable aImage;           /* All images used by document (by name) */ 
    HtmlCanvas canvas;              /* Canvas to render into */
};

#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))

EXTERN int HtmlTreeBuild(ClientData, Tcl_Interp *, int, Tcl_Obj *CONST []);
EXTERN int HtmlTreeRoot(ClientData, Tcl_Interp *, int, Tcl_Obj *CONST []);
EXTERN Tcl_ObjCmdProc HtmlTreeCollapseWhitespace;
EXTERN Tcl_ObjCmdProc HtmlStyleParse;
EXTERN Tcl_ObjCmdProc HtmlStyleApply;
EXTERN Tcl_ObjCmdProc HtmlStyleSyntaxErrs;
EXTERN Tcl_ObjCmdProc HtmlLayoutForce;
EXTERN Tcl_ObjCmdProc HtmlLayoutImage;
EXTERN Tcl_ObjCmdProc HtmlLayoutPrimitives;

EXTERN void HtmlTreeFree(HtmlTree *p);
EXTERN int HtmlWalkTree(HtmlTree *, int (*)(HtmlTree *, HtmlNode *));
EXTERN int HtmlTreeClear(HtmlTree *);

EXTERN int         HtmlNodeNumChildren(HtmlNode *);
EXTERN HtmlNode *  HtmlNodeChild(HtmlNode *, int);
EXTERN HtmlNode *  HtmlNodeRightSibling(HtmlNode *);
EXTERN int         HtmlNodeIsText(HtmlNode *);
EXTERN HtmlNode *  HtmlNodeParent(HtmlNode *);
EXTERN Html_u8     HtmlNodeTagType(HtmlNode *);
EXTERN char CONST *HtmlNodeAttr(HtmlNode *, char CONST *);
EXTERN char *      HtmlNodeToString(HtmlNode *);

EXTERN Tcl_Obj *HtmlNodeCommand(Tcl_Interp *interp, HtmlNode *pNode);

EXTERN void HtmlNodeGetProperty(Tcl_Interp *, HtmlNode *, int , CssProperty *);
EXTERN void HtmlNodeGetDefault(HtmlNode *, int , CssProperty *);

EXTERN Tcl_Obj *HtmlResizeImage(HtmlTree *, CONST char *, int *, int *, int);
EXTERN Tcl_Obj *HtmlXImageToImage(HtmlTree *, XImage *, int, int);
EXTERN int HtmlClearImageArray(HtmlTree*);

EXTERN void HtmlDrawCleanup(HtmlCanvas *);
EXTERN void HtmlDrawCanvas(HtmlCanvas *, HtmlCanvas *, int, int);
EXTERN void HtmlDrawText(HtmlCanvas*, Tcl_Obj*, int, int, int, Tk_Font,XColor*);
EXTERN void HtmlDrawImage(HtmlCanvas *, Tcl_Obj *, int, int, int, int);
EXTERN void HtmlDrawWindow(HtmlCanvas *, Tcl_Obj *, int, int, int, int);
EXTERN void HtmlDrawBackground(HtmlCanvas *, XColor *);
EXTERN void HtmlDrawQuad(HtmlCanvas*,int,int,int,int,int,int,int,int,XColor*);
EXTERN int HtmlDrawIsEmpty(HtmlCanvas *);

EXTERN int HtmlEmptyContent(HtmlNode *, int);
EXTERN int HtmlInlineContent(HtmlNode *, int);
EXTERN int HtmlFlowContent(HtmlNode *, int);
EXTERN int HtmlColgroupContent(HtmlNode *, int);
EXTERN int HtmlDlContent(HtmlNode *, int);
EXTERN int HtmlUlContent(HtmlNode *, int);
EXTERN int HtmlLiContent(HtmlNode *, int);

EXTERN int HtmlTableSectionContent(HtmlNode *, int);
EXTERN int HtmlTableRowContent(HtmlNode *, int);
EXTERN int HtmlTableContent(HtmlNode *, int);
EXTERN int HtmlTableCellContent(HtmlNode *, int);

EXTERN HtmlTokenMap *HtmlMarkup(int);
EXTERN CONST char * HtmlMarkupName(int);
EXTERN char * HtmlMarkupArg(HtmlToken *, CONST char *, char *);

EXTERN void HtmlFloatListAdd(HtmlFloatList*, int, int, int, int);
EXTERN HtmlFloatList *HtmlFloatListNew();
EXTERN void HtmlFloatListDelete();
EXTERN int HtmlFloatListPlace(HtmlFloatList*, int, int, int, int);
EXTERN int HtmlFloatListClear(HtmlFloatList*, int, int);
EXTERN void HtmlFloatListNormalize(HtmlFloatList*, int, int);
EXTERN void HtmlFloatListMargins(HtmlFloatList*, int, int, int *, int *);

#ifdef HTML_DEBUG
EXTERN void HtmlDrawComment(HtmlCanvas *, CONST char *zComment);
#else
#define HtmlDrawComment(x, y)
#endif

#endif

