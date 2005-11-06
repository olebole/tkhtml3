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

#ifndef NDEBUG
#define HTML_DEBUG
#endif

#include <tk.h>
#include <string.h>
#include <assert.h>

#include "htmltokens.h"

#ifdef HTML_MACROS
#include "htmlmacros.h"
#endif

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
typedef struct HtmlScaledImage HtmlScaledImage;
typedef struct HtmlTokenMap HtmlTokenMap;
typedef struct HtmlCanvas HtmlCanvas;
typedef struct HtmlCanvasItem HtmlCanvasItem;
typedef struct HtmlFloatList HtmlFloatList;
typedef struct HtmlPropertyCache HtmlPropertyCache;
typedef struct HtmlNodeReplacement HtmlNodeReplacement;
typedef struct HtmlCallback HtmlCallback;
typedef struct HtmlNodeCmd HtmlNodeCmd;

#include "css.h"
#include "htmlprop.h"

typedef int (*HtmlContentTest)(HtmlNode *, int);

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

#define HTMLTAG_END         0x01
#define HTMLTAG_INLINE      0x02
#define HTMLTAG_BLOCK       0x04
#define HTMLTAG_EMPTY       0x08

#define TAG_CLOSE 1
#define TAG_PARENT 2
#define TAG_OK 3

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

/*
 * For a replaced node, the HtmlNode.pReplacement variable points to an
 * instance * of the following structure. The member objects are the name of
 * the replaced object (an image or widget handle), the configure script if
 * any, and the delete script if any. i.e. in Tcl:
 *
 *     $nodeHandle replace $pReplace \
 *             -configurecmd $pConfigure -deletecmd $pDelete
 *    
 */
struct HtmlNodeReplacement {
    Tcl_Obj *pReplace;            /* Image or window name */
    Tcl_Obj *pConfigure;          /* Script passed to -configurecmd */
    Tcl_Obj *pDelete;             /* Script passed to -deletecmd */
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

    HtmlPropertyValues *pPropertyValues;     /* CSS property values */
    HtmlNodeReplacement *pReplacement;       /* Replaced object, if any */
    HtmlNodeCmd *pNodeCmd;                   /* Tcl command for this node */

    /* Variables used by the layout engine */
    int iBlockWidth;
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
    HtmlCanvasItem *pWindow;
    Tcl_Obj *pPrimitives;
};

struct HtmlOptions {
    int width;
    int height;
    int xscrollincrement;
    int yscrollincrement;

    Tcl_Obj *yscrollcommand;
    Tcl_Obj *xscrollcommand;
    Tcl_Obj *defaultstyle;
    Tcl_Obj *imagecmd;

#ifndef NDEBUG
    Tcl_Obj *logcmd;
#endif
};

#ifndef NDEBUG
void HtmlLog(HtmlTree *, CONST char *, CONST char *, ...);
#else
#define HtmlLog(...)
#endif


/*
 * Widget state information information is stored in an instance of this
 * structure, which is a part of the HtmlTree. The variables within control the
 * behaviour of the idle callback scheduled to update the display.
 *
 * eCallbackAction may take the following values:
 *
 *     HTML_CALLBACK_NONE
 *     HTML_CALLBACK_DAMAGE
 *     HTML_CALLBACK_LAYOUT
 *     HTML_CALLBACK_STYLE
 */
struct HtmlCallback {
    int eCallbackAction;            /* Action to take in next callback */

    int x1, y1;                     /* Top-left corner of damaged region */
    int x2, y2;                     /* Bottom-right corner of damaged region */
};

#define HTML_CALLBACK_NONE    0
#define HTML_CALLBACK_DAMAGE  1
#define HTML_CALLBACK_LAYOUT  2
#define HTML_CALLBACK_STYLE   3

/* 
 * The Tk-window used by the widget is stored in variable tkwin.
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
     * The following variables are used to stored the text of the current
     * document (i.e. the *.html file).
     * 
     * Variable 'iCol' stores the number of characters tokenized since the last
     * newline encountered in the document source. When we encounter a TAB
     * character, it is converted to (8-(iCol%8)) spaces. This makes text
     * inside a block with the 'white-space' property set to "pre" look good
     * even if the input contains tabs. 
     *
     */
    Tcl_Obj *pDocument;             /* Text of the html document */
    int nParsed;                    /* Bytes of the html document tokenized */
    int iCol;                       /* Current column in document */
    int parseFinished;              /* True if the html parse is finished */

    HtmlToken *pFirst;              /* First token parsed */
    HtmlToken *pLast;               /* Last token parsed */
    HtmlNode *pCurrent;             /* The node currently being built. */
    HtmlNode *pRoot;                /* The root-node of the document. */

    Tcl_HashTable aScriptHandler;   /* Script handler callbacks. */
    Tcl_HashTable aNodeHandler;     /* Script handler callbacks. */

    CssStyleSheet *pStyle;          /* Style sheet configuration */

    Tcl_HashTable aImage;           /* All images used by document (by name) */ 
    HtmlOptions options;            /* Configurable options */
    Tk_OptionTable optionTable;     /* Option table */

    HtmlCanvas canvas;              /* Canvas to render into */
    int iCanvasWidth;               /* Width of window for canvas */

    /* 
     * Tables managed by code in htmlprop.c. Initialised in function
     * HtmlPropertyValuesSetupTables().
     */
    Tcl_HashTable aColor;
    Tcl_HashTable aFont;
    Tcl_HashTable aValues;
    int aFontSizeTable[7];

    /*
     * Todo: Have to think seriously about these before any API freeze.
     */
    Tcl_HashTable aVar;             /* Tcl state data dictionary. */
    Tcl_HashTable aCmd;             /* Map of sub-commands implemented in Tcl */

    HtmlCallback cb;                /* See structure definition comments */
};

#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))

void HtmlFinishNodeHandlers(HtmlTree *);
void HtmlAddToken(HtmlTree *, HtmlToken *);
int HtmlTreeBuild(ClientData, Tcl_Interp *, int, Tcl_Obj *CONST []);
Tcl_ObjCmdProc HtmlTreeCollapseWhitespace;
Tcl_ObjCmdProc HtmlStyleApply;
Tcl_ObjCmdProc HtmlStyleSyntaxErrs;
Tcl_ObjCmdProc HtmlLayoutForce;
Tcl_ObjCmdProc HtmlLayoutSize;
Tcl_ObjCmdProc HtmlLayoutNode;
Tcl_ObjCmdProc HtmlLayoutImage;
Tcl_ObjCmdProc HtmlLayoutPrimitives;
Tcl_ObjCmdProc HtmlLayoutBbox;
Tcl_ObjCmdProc HtmlWidgetMapControls;

int HtmlWidgetScroll(HtmlTree *, int, int);
int HtmlWidgetPaint(HtmlTree *, int, int, int, int, int, int);

int HtmlStyleParse(HtmlTree *, Tcl_Interp*, Tcl_Obj *, Tcl_Obj *, Tcl_Obj *);
void HtmlTokenizerAppend(HtmlTree *, const char *, int);
int HtmlNameToType(void *, char *);
Html_u8 HtmlMarkupFlags(int);

void HtmlTreeFree(HtmlTree *p);
int HtmlWalkTree(HtmlTree *, int (*)(HtmlTree *, HtmlNode *));
int HtmlTreeClear(HtmlTree *);
int         HtmlNodeNumChildren(HtmlNode *);
HtmlNode *  HtmlNodeChild(HtmlNode *, int);
HtmlNode *  HtmlNodeRightSibling(HtmlNode *);
HtmlNode *  HtmlNodeParent(HtmlNode *);
char CONST *HtmlNodeTagName(HtmlNode *);
char CONST *HtmlNodeAttr(HtmlNode *, char CONST *);
char *      HtmlNodeToString(HtmlNode *);

#ifndef HTML_MACROS
int         HtmlNodeIsText(HtmlNode *);
Html_u8     HtmlNodeTagType(HtmlNode *);
#endif

Tcl_Obj *HtmlNodeCommand(Tcl_Interp *interp, HtmlTree *, HtmlNode *pNode);

CssProperty *HtmlNodeGetProperty(Tcl_Interp *, HtmlNode *, int);
void HtmlNodeGetDefault(HtmlNode *, int , CssProperty *);
void HtmlDeletePropertyCache(HtmlPropertyCache *);

Tcl_Obj *HtmlResizeImage(HtmlTree *, CONST char *, int *, int *, int);
Tcl_Obj *HtmlXImageToImage(HtmlTree *, XImage *, int, int);
int HtmlClearImageArray(HtmlTree*);

void HtmlDrawCleanup(HtmlCanvas *);
void HtmlDrawDeleteControls(HtmlTree *, HtmlCanvas *);

void HtmlDrawCanvas(HtmlCanvas*,HtmlCanvas*,int,int,HtmlNode*);
void HtmlDrawText(HtmlCanvas*,Tcl_Obj*,int,int,int,int,Tk_Font,XColor*,int);
void HtmlDrawImage(HtmlCanvas *, Tcl_Obj *, int, int, int, int, int);
void HtmlDrawWindow(HtmlCanvas *, Tcl_Obj *, int, int, int, int, int);
void HtmlDrawBackground(HtmlCanvas *, XColor *, int);
void HtmlDrawQuad(HtmlCanvas*,int,int,int,int,int,int,int,int,XColor*,int);
int HtmlDrawIsEmpty(HtmlCanvas *);

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

HtmlPropertyCache * HtmlNewPropertyCache();
void HtmlDeletePropertyCache(HtmlPropertyCache *pCache);
void HtmlSetPropertyCache(HtmlPropertyCache *, int, CssProperty *);
void HtmlAttributesToPropertyCache(HtmlNode *pNode);

Tcl_HashKeyType * HtmlCaseInsenstiveHashType();
Tcl_HashKeyType * HtmlFontKeyHashType();
Tcl_HashKeyType * HtmlPropertyValuesHashType();

CONST char *HtmlDefaultTcl();
CONST char *HtmlDefaultCss();

#ifdef HTML_DEBUG
void HtmlDrawComment(HtmlCanvas *, CONST char *zComment, int);
#else
#define HtmlDrawComment(x, y, z)
#endif

#endif

