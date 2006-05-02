/*
 * html.h --
 *
 *----------------------------------------------------------------------------
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

#ifndef __HTMLTREE_H__
#define __HTMLTREE_H__

/*
 * Without exception the tkhtml code uses the wrapper functions HtmlAlloc(),
 * HtmlFree() and HtmlRealloc() in place of the regular Tcl ckalloc(), ckfree()
 * and ckrealloc() functions.
 */
#ifdef HTML_DEBUG
    #include "restrack.h"
    #define HtmlAlloc Rt_Alloc
    #define HtmlFree(x) Rt_Free((char *)(x))
    #define HtmlRealloc(x, n) Rt_Realloc((char *)(x), (n))
#else
    #define HtmlAlloc ckalloc
    #define HtmlFree(x) ckfree((char *)(x))
    #define HtmlRealloc(x, n) ckrealloc((char *)(x), n)
#endif

/* HtmlClearAlloc() is a version of HtmlAlloc() that returns zeroed memory */
#define HtmlClearAlloc(x) ((char *)memset(HtmlAlloc((x)), 0, (x)))

#include <tk.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "htmltokens.h"

/*
 * Version information for the package mechanism.
 */
#define HTML_PKGNAME "Tkhtml"
#define HTML_PKGVERSION "3.0"

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

typedef struct HtmlOptions HtmlOptions;
typedef struct HtmlTree HtmlTree;
typedef struct HtmlNode HtmlNode;
typedef struct HtmlToken HtmlToken;
typedef struct HtmlTokenMap HtmlTokenMap;
typedef struct HtmlCanvas HtmlCanvas;
typedef struct HtmlCanvasItem HtmlCanvasItem;
typedef struct HtmlFloatList HtmlFloatList;
typedef struct HtmlPropertyCache HtmlPropertyCache;
typedef struct HtmlNodeReplacement HtmlNodeReplacement;
typedef struct HtmlCallback HtmlCallback;
typedef struct HtmlNodeCmd HtmlNodeCmd;
typedef struct HtmlLayoutCache HtmlLayoutCache;

typedef struct HtmlImageServer HtmlImageServer;
typedef struct HtmlImage2 HtmlImage2;

#include "css.h"
#include "htmlprop.h"

typedef int (*HtmlContentTest)(HtmlTree *, HtmlNode *, int);

#define FLOAT_LEFT       CSS_CONST_LEFT
#define FLOAT_RIGHT      CSS_CONST_RIGHT
#define FLOAT_NONE       CSS_CONST_NONE

#define CLEAR_NONE       CSS_CONST_NONE
#define CLEAR_LEFT       CSS_CONST_LEFT
#define CLEAR_RIGHT      CSS_CONST_RIGHT
#define CLEAR_BOTH       CSS_CONST_BOTH

struct HtmlTokenMap {
  char *zName;                    /* Name of a markup */
  Html_16 type;                   /* Markup type code */
  Html_u8 flags;                  /* Combination of HTMLTAG values */
  HtmlContentTest xClose;         /* Function to identify close tag */
  HtmlTokenMap *pCollide;         /* Hash table collision chain */
};

#define HTMLTAG_END         0x01  /* Set for a closing tag (i.e. </p>) */
#define HTMLTAG_INLINE      0x02  /* Set for an HTML inline tag */
#define HTMLTAG_BLOCK       0x04  /* Set for an HTML block tag */
#define HTMLTAG_EMPTY       0x08  /* Set for an empty tag (i.e. <img>) */

#define TAG_CLOSE    1
#define TAG_PARENT   2
#define TAG_OK       3
#define TAG_IMPLICIT 4

struct HtmlToken {
    HtmlToken *pNext;           /* Next input token in a list of them all */
    HtmlToken *pPrev;           /* Previous token in a list of them all   */
    Html_u8 type;
    Html_16 count;
    union {
        int newline;
        char *zText;
        char **zArgs;
    } x;
};

/*
 * For a replaced node, the HtmlNode.pReplacement variable points to an
 * instance of the following structure. The member objects are the name of
 * the replaced object (widget handle), the configure script if any, and the
 * delete script if any. i.e. in Tcl:
 *
 *     $nodeHandle replace $pReplace \
 *             -configurecmd $pConfigure -deletecmd $pDelete
 *
 * The iOffset variable holds the integer returned by any -configurecmd
 * script (default value 0). This is assumed to be the number of pixels
 * between the bottom of the replacement window and the point that should
 * be aligned with the bottom of a line-box. i.e. equivalent to the 
 * "descent" property of a font.
 */
struct HtmlNodeReplacement {
    Tcl_Obj *pReplace;            /* Replacement window name */
    Tk_Window win;                /* Replacement window */
    Tcl_Obj *pConfigure;          /* Script passed to -configurecmd */
    Tcl_Obj *pDelete;             /* Script passed to -deletecmd */
    int iOffset;                  /* See above */

    HtmlNodeReplacement *pNext;   /* Next element in HtmlTree.pMapped list */
    int iCanvasX;                 /* Current X canvas coordinate of window */
    int iCanvasY;                 /* Current Y canvas coordinate of window */
};

/*
 * When a Tcl command representing a node-handle is created, an instance of the 
 * following structure is allocated.
 */
struct HtmlNodeCmd {
    Tcl_Obj *pCommand;
    HtmlTree *pTree;
};

/* 
 * Each node of the document tree is represented as an HtmlNode structure.
 * This structure carries no information to do with the node itself, it is
 * simply used to build the tree structure. All the information for the
 * node is stored in the HtmlToken object.
 */
struct HtmlNode {
    HtmlNode *pParent;             /* Parent of this node */
    HtmlToken *pToken;             /* Html element associated with node */
    int nChild;                    /* Number of child nodes */
    HtmlNode **apChildren;         /* Array of pointers to children nodes */

    CssProperties *pStyle;     /* The CSS properties from style attribute */

    HtmlComputedValues *pPropertyValues;     /* CSS property values */
    HtmlComputedValues *pPreviousValues;     /* Previous CSS property values */
    CssDynamic *pDynamic;                    /* CSS dynamic conditions */
    Html_u8 flags;                           /* HTML_DYNAMIC_XXX flags */

    HtmlLayoutCache *pLayoutCache;           /* Cached layout, if any */

    HtmlNodeReplacement *pReplacement;       /* Replaced object, if any */
    HtmlNodeCmd *pNodeCmd;                   /* Tcl command for this node */
    int iNode;                               /* Node index */
};

/* Values for HtmlNode.flags. These may be set and cleared via the Tcl
 * interface on the node command: [$node dynamic set|clear ...]
 */
#define HTML_DYNAMIC_HOVER    0x01
#define HTML_DYNAMIC_FOCUS    0x02
#define HTML_DYNAMIC_ACTIVE   0x04
#define HTML_DYNAMIC_LINK     0x08
#define HTML_DYNAMIC_VISITED  0x10
#define HTML_DYNAMIC_USERFLAG 0x20

struct HtmlCanvas {
    int left;
    int right;
    int top;
    int bottom;
    HtmlCanvasItem *pFirst;
    HtmlCanvasItem *pLast;
};

struct HtmlOptions {
    int width;
    int height;
    int xscrollincrement;
    int yscrollincrement;

    int shrink;
    int layoutcache;

    XColor *selectforeground;
    XColor *selectbackground;

    Tcl_Obj *yscrollcommand;
    Tcl_Obj *xscrollcommand;
    Tcl_Obj *defaultstyle;
    Tcl_Obj *imagecmd;
    Tcl_Obj *encoding;

    Tcl_Obj *fonttable;

    Tcl_Obj *logcmd;
    Tcl_Obj *timercmd;
};

void HtmlLog(HtmlTree *, CONST char *, CONST char *, ...);
void HtmlTimer(HtmlTree *, CONST char *, CONST char *, ...);


/*
 * Widget state information information is stored in an instance of this
 * structure, which is a part of the HtmlTree. The variables within control 
 * the behaviour of the idle callback scheduled to update the display.
 */
struct HtmlCallback {
    int flags;                  /* Comb. of HTML_XXX bitmasks defined below */
    int inProgress;             /* Prevent recursive invocation */

    /* HTML_DYNAMIC */
    HtmlNode *pDynamic;         /* Recalculate dynamic CSS for this node */

    /* HTML_DAMAGE */
    int x, y;                   /* Top-left corner of damaged region */
    int w, h;                   /* Bottom-right corner of damaged region */

    /* HTML_RESTYLE */
    HtmlNode *pRestyle;         /* Restyle this node */

    /* HTML_SCROLL */
    int iScrollX;               /* New HtmlTree.iScrollX value */
    int iScrollY;               /* New HtmlTree.iScrollY value */
};

/* Values for HtmlCallback.flags */
#define HTML_DYNAMIC    0x01
#define HTML_DAMAGE     0x02
#define HTML_RESTYLE    0x04
#define HTML_LAYOUT     0x08
#define HTML_SCROLL     0x10

/* 
 * Functions used to schedule callbacks and set the HtmlCallback state. 
 */
void HtmlCallbackSchedule(HtmlTree *, int);
void HtmlCallbackExtents(HtmlTree *, int, int, int, int);

void HtmlCallbackForce(HtmlTree *);
void HtmlCallbackDynamic(HtmlTree *, HtmlNode *);
void HtmlCallbackDamage(HtmlTree *, int, int, int, int);
void HtmlCallbackLayout(HtmlTree *, HtmlNode *);
void HtmlCallbackRestyle(HtmlTree *, HtmlNode *);

void HtmlCallbackScrollX(HtmlTree *, int);
void HtmlCallbackScrollY(HtmlTree *, int);

struct HtmlTree {

    /*
     * The interpreter and main window hosting this widget instance.
     */
    Tcl_Interp *interp;             /* Tcl interpreter */
    Tk_Window win;                  /* Main window of interp */

    /*
     * The widget window.
     */
    Tk_Window tkwin;           /* Widget window */
    int iScrollX;              /* Number of pixels offscreen to the left */
    int iScrollY;              /* Number of pixels offscreen to the top */
    int eVisibility;           /* Most recent XVisibilityEvent.state */

    /*
     * The widget command.
     */
    Tcl_Command cmd;           /* Widget command */
    int isDeleted;             /* True once the widget-delete has begun */

    /*
     * The image server object.
     */
    HtmlImageServer *pImageServer;

    /*
     * The following variables are used to stored the text of the current
     * document (i.e. the *.html file).
     * 
     * Variable 'iCol' stores the number of characters tokenized since the last
     * newline encountered in the document source. When we encounter a TAB
     * character, it is converted to (8-(iCol%8)) spaces. This makes text
     * inside a block with the 'white-space' property set to "pre" look good
     * even if the input contains tabs. 
     */
    Tcl_Obj *pDocument;             /* Text of the html document */
    int nParsed;                    /* Bytes of the html document tokenized */
    int iCol;                       /* Current column in document */
    int isParseFinished;            /* True if the html parse is finished */

    HtmlToken *pFirst;              /* First token parsed */
    HtmlToken *pLast;               /* Last token parsed */
    HtmlNode *pCurrent;             /* The node currently being built. */
    HtmlNode *pRoot;                /* The root-node of the document. */
    HtmlNode *pBgRoot;              /* The node for the canvas bg */
    int nFixedBackground;           /* Number of nodes with fixed backgrounds */

    HtmlToken *pTextLast;           /* Currently parsing text node */
    HtmlToken *pTextFirst;          /* Currently parsing text node */

    /*
     * Handler callbacks configured by the [$widget handler] command.
     *
     * The aScriptHandler hash table contains entries representing
     * script-handler callbacks. Each entry maps a tag-type (i.e. Html_P)
     * to a Tcl_Obj* that contains the Tcl script to call when the tag type is
     * encountered. A single argument is appended to the script - all the text
     * between the start and end tag. The ref-count of the Tcl_Obj* should be
     * decremented if it is removed from the hash table.
     */
    Tcl_HashTable aScriptHandler;   /* Script handler callbacks. */
    Tcl_HashTable aNodeHandler;     /* Script handler callbacks. */

    CssStyleSheet *pStyle;          /* Style sheet configuration */

    Tcl_HashTable aScaledImage;     /* All images used by document (by name) */ 
    HtmlOptions options;            /* Configurable options */
    Tk_OptionTable optionTable;     /* Option table */

    /*
     * Internal representation of a completely layed-out document.
     */
    HtmlCanvas canvas;              /* Canvas to render into */
    int iCanvasWidth;               /* Width of window for canvas */

    /* Linked list of currently mapped replacement objects */
    HtmlNodeReplacement *pMapped;

    /* 
     * Tables managed by code in htmlprop.c. Initialised in function
     * HtmlComputedValuesSetupTables(), except for aFontSizeTable[], which is
     * set via the -fonttable option. 
     */
    Tcl_HashTable aColor;
    Tcl_HashTable aFont;
    Tcl_HashTable aValues;
    Tcl_HashTable aImage;
    int aFontSizeTable[7];

    /*
     * These variables store the persistent data for the [widget select]
     * command.
     */
    HtmlNode *pFromNode;
    int iFromIndex;
    HtmlNode *pToNode;
    int iToIndex;
    int iNextNode;       /* Next node index to allocate */

    /*
     * Todo: Have to think seriously about these before any API freeze.
     */
    Tcl_HashTable aVar;             /* Tcl state data dictionary. */
    Tcl_HashTable aCmd;             /* Map of sub-commands implemented in Tcl */

    HtmlCallback cb;                /* See structure definition comments */
};

#define MAX(x,y)  ((x)>(y)?(x):(y))
#define MIN(x,y)  ((x)<(y)?(x):(y))

void HtmlFinishNodeHandlers(HtmlTree *);
void HtmlAddToken(HtmlTree *, HtmlToken *);
int HtmlTreeBuild(ClientData, Tcl_Interp *, int, Tcl_Obj *CONST []);
Tcl_ObjCmdProc HtmlTreeCollapseWhitespace;
Tcl_ObjCmdProc HtmlStyleSyntaxErrs;
Tcl_ObjCmdProc HtmlLayoutSize;
Tcl_ObjCmdProc HtmlLayoutNode;
Tcl_ObjCmdProc HtmlLayoutImage;
Tcl_ObjCmdProc HtmlLayoutPrimitives;
Tcl_ObjCmdProc HtmlLayoutBbox;
Tcl_ObjCmdProc HtmlCssStyleConfigDump;

char *HtmlPropertyToString(CssProperty *, char **);

int HtmlStyleApply(HtmlTree *, HtmlNode *);

int HtmlLayout(HtmlTree *);

int HtmlStyleParse(HtmlTree*, Tcl_Interp*, Tcl_Obj*,Tcl_Obj*,Tcl_Obj*,Tcl_Obj*);
void HtmlTokenizerAppend(HtmlTree *, const char *, int, int);
int HtmlNameToType(void *, char *);
Html_u8 HtmlMarkupFlags(int);


#define HTML_WALK_ABANDON            4
#define HTML_WALK_DESCEND            5
#define HTML_WALK_DO_NOT_DESCEND     6
typedef int (*html_walk_tree_cb)(HtmlTree*,HtmlNode*,ClientData);
int HtmlWalkTree(HtmlTree*, HtmlNode *, html_walk_tree_cb, ClientData);

void HtmlTreeFree(HtmlTree *p);
int HtmlTreeClear(HtmlTree *);
int         HtmlNodeNumChildren(HtmlNode *);
HtmlNode *  HtmlNodeChild(HtmlNode *, int);
HtmlNode *  HtmlNodeRightSibling(HtmlNode *);
HtmlNode *  HtmlNodeLeftSibling(HtmlNode *);
HtmlNode *  HtmlNodeParent(HtmlNode *);
char CONST *HtmlNodeTagName(HtmlNode *);
char CONST *HtmlNodeAttr(HtmlNode *, char CONST *);
char *      HtmlNodeToString(HtmlNode *);
HtmlNode *  HtmlNodeGetPointer(HtmlTree *, char CONST *);

int         HtmlNodeIsText(HtmlNode *);
Html_u8     HtmlNodeTagType(HtmlNode *);
int         HtmlNodeIsWhitespace(HtmlNode *);

Tcl_Obj *HtmlNodeCommand(HtmlTree *, HtmlNode *pNode);

CssProperty *HtmlNodeGetProperty(Tcl_Interp *, HtmlNode *, int);
void HtmlNodeGetDefault(HtmlNode *, int , CssProperty *);
void HtmlDeletePropertyCache(HtmlPropertyCache *);

void HtmlDrawCleanup(HtmlTree *, HtmlCanvas *);
void HtmlDrawDeleteControls(HtmlTree *, HtmlCanvas *);

void HtmlDrawCanvas(HtmlCanvas*,HtmlCanvas*,int,int,HtmlNode*);
void HtmlDrawText(HtmlCanvas*,Tcl_Obj*,int,int,int,int,HtmlNode*,int);

#define CANVAS_BOX_OPEN_LEFT    0x01      /* Open left-border */
#define CANVAS_BOX_OPEN_RIGHT   0x02      /* Open right-border */
void HtmlDrawBox(HtmlCanvas *, int, int, int, int, HtmlNode *, int, int);
void HtmlDrawLine(HtmlCanvas *, int, int, int, int, int, HtmlNode *, int);

void HtmlDrawWindow(HtmlCanvas *, HtmlNode *, int, int, int, int, int);
void HtmlDrawBackground(HtmlCanvas *, XColor *, int);
void HtmlDrawQuad(HtmlCanvas*,int,int,int,int,int,int,int,int,XColor*,int);
int  HtmlDrawIsEmpty(HtmlCanvas *);

void HtmlDrawImage(HtmlCanvas*, HtmlImage2*, int, int, int, int, HtmlNode*, int);
void HtmlDrawOrigin(HtmlCanvas*);
void HtmlDrawCopyCanvas(HtmlCanvas*, HtmlCanvas*);

HtmlCanvasItem *HtmlDrawAddMarker(HtmlCanvas*, int, int, int);
int HtmlDrawGetMarker(HtmlCanvas*, HtmlCanvasItem *, int*, int*);

void HtmlWidgetDamageText(HtmlTree *, int, int, int, int);
int HtmlWidgetNodeTop(HtmlTree *, int);

HtmlTokenMap *HtmlMarkup(int);
CONST char * HtmlMarkupName(int);
char * HtmlMarkupArg(HtmlToken *, CONST char *, char *);

void HtmlFloatListAdd(HtmlFloatList*, int, int, int, int);
HtmlFloatList *HtmlFloatListNew();
void HtmlFloatListDelete();
int HtmlFloatListPlace(HtmlFloatList*, int, int, int, int);
int HtmlFloatListClear(HtmlFloatList*, int, int);
void HtmlFloatListNormalize(HtmlFloatList*, int, int);
void HtmlFloatListMargins(HtmlFloatList*, int, int, int *, int *);
void HtmlFloatListLog(HtmlTree *, CONST char *, CONST char *, HtmlFloatList *);
int HtmlFloatListIsConstant(HtmlFloatList*, int, int);

HtmlPropertyCache * HtmlNewPropertyCache();
void HtmlDeletePropertyCache(HtmlPropertyCache *pCache);
void HtmlSetPropertyCache(HtmlPropertyCache *, int, CssProperty *);
void HtmlAttributesToPropertyCache(HtmlNode *pNode);

Tcl_HashKeyType * HtmlCaseInsenstiveHashType();
Tcl_HashKeyType * HtmlFontKeyHashType();
Tcl_HashKeyType * HtmlComputedValuesHashType();

CONST char *HtmlDefaultTcl();
CONST char *HtmlDefaultCss();

/* Functions from htmlimage.c */
void HtmlImageServerInit(HtmlTree *);
void HtmlImageServerShutdown(HtmlTree *);
HtmlImage2 *HtmlImageServerGet(HtmlImageServer *, const char *);
HtmlImage2 *HtmlImageScale(HtmlImage2 *, int *, int *, int);
Tcl_Obj *HtmlImageUnscaledName(HtmlImage2 *);
Tk_Image HtmlImageImage(HtmlImage2 *);
Tk_Image HtmlImageTile(HtmlImage2 *);
void HtmlImageFree(HtmlImage2 *);
void HtmlImageRef(HtmlImage2 *);
const char *HtmlImageUrl(HtmlImage2 *);
void HtmlImageCheck(HtmlImage2 *);
Tcl_Obj *HtmlXImageToImage(HtmlTree *, XImage *, int, int);
int HtmlImageAlphaChannel(HtmlTree *, HtmlImage2 *);

void HtmlLayoutPaintNode(HtmlTree *, HtmlNode *);
void HtmlLayoutInvalidateCache(HtmlTree *, HtmlNode *);
void HtmlWidgetNodeBox(HtmlTree *, HtmlNode *, int *, int *, int *, int *);

void HtmlWidgetSetViewport(HtmlTree *, int, int, int);
void HtmlWidgetRepair(HtmlTree *, int, int, int, int);

#endif

