
/*
 * htmllayout.h --
 *
 *     This header file is included by the files involved in layout only:
 *
 *         htmllayout.c
 *         htmllayoutinline.c
 *         htmllayouttable.c
 *
 *----------------------------------------------------------------------------
 */
#ifndef __HTML_LAYOUT_H
#define __HTML_LAYOUT_H

#include "html.h"

typedef struct LayoutContext LayoutContext;

/*
 * A single Layout context object is allocated for use throughout
 * the entire layout process. It contains global resources required
 * by the drawing routines.
 */
struct LayoutContext {
    HtmlTree *pTree;       /* The Html widget. */
    HtmlNode *pTop;          /* Top level node rendered (<body>). */
    Tk_Window tkwin;
    Tcl_Interp *interp;      /* The interpreter */

    Tcl_HashTable widthCache;
    int minmaxTest;          /* Currently figuring out min/max widths */
};

typedef struct BoxProperties BoxProperties;
typedef struct BorderProperties BorderProperties;
typedef struct MarginProperties MarginProperties;

struct BoxProperties {
    int padding_top;
    int padding_left;
    int padding_bottom;
    int padding_right;
    int border_top;
    int border_left;
    int border_bottom;
    int border_right;
};

struct BorderProperties {
    XColor *color_top;
    XColor *color_left;
    XColor *color_bottom;
    XColor *color_right;
    XColor *color_bg;
    HtmlImage2 *pBgImage;
    unsigned char eBgRepeat;
    int iPositionX;
    int iPositionY;
    unsigned char isPositionPercent;
};

struct MarginProperties {
    int margin_top;
    int margin_left;
    int margin_bottom;
    int margin_right;
    int leftAuto;        /* True if ('margin-left' == "auto") */
    int rightAuto;       /* True if ('margin-right' == "auto") */
};


/*--------------------------------------------------------------------------*
 * htmllayoutinline.c --
 *
 *     htmllayoutinline.c contains the implementation of the InlineContext
 *     object.
 */
typedef struct InlineContext InlineContext;
typedef struct InlineBorder InlineBorder;

/* Allocate and deallocate InlineContext structures */
InlineContext *HtmlInlineContextNew(HtmlNode *, int);
void HtmlInlineContextCleanup(InlineContext *);

/* Add a text node to the inline context */
int HtmlInlineContextAddText(InlineContext*, HtmlNode *);

/* Test to see if the inline-context contains any more boxes */
int HtmlInlineContextIsEmpty(InlineContext *);

/* Enter and exit nodes (that may have borders or underlining) */
void HtmlInlineContextEnter(InlineContext *, HtmlNode *);
void HtmlInlineContextExit(InlineContext *, HtmlNode *);

/* Add box (i.e. replaced) inline elements to the inline context */
void HtmlInlineContextAddBox(InlineContext*,HtmlNode*,HtmlCanvas*,int,int,int);

/* Retrieve the next line-box from an inline context TODO: Look at this... */
int HtmlInlineContextGetLineBox(InlineContext*,int*,int,HtmlCanvas*,int*, int*);

int inlineContextGetLineBox(
LayoutContext *, InlineContext*,int*,int,HtmlCanvas*,int*, int*);
int inlineContextIsEmpty(InlineContext *);

InlineBorder *inlineContextGetBorder(LayoutContext *, HtmlNode *, int);
int inlineContextPushBorder(InlineContext *, InlineBorder *);
void inlineContextPopBorder(InlineContext *, InlineBorder *);

/* End of htmllayoutinline.c interface
 *-------------------------------------------------------------------------*/

#define DRAW_TEXT(a, b, c, d, e, f, g) \
HtmlDrawText(a, b, c, d, e, f, g, pLayout->minmaxTest, 0, 0)
#define DRAW_CANVAS(a, b, c, d, e) \
HtmlDrawCanvas(a, b, c, d, e)
#define DRAW_IMAGE(a, b, c, d, e, f) \
HtmlDrawImage(a, b, c, d, e, f, pLayout->minmaxTest)
#define DRAW_WINDOW(a, b, c, d, e, f) \
HtmlDrawWindow(a, b, c, d, e, f, pLayout->minmaxTest)
#define DRAW_BACKGROUND(a, b) \
HtmlDrawBackground(a, b, pLayout->minmaxTest)
#define DRAW_QUAD(a, b, c, d, e, f, g, h, i, j) \
HtmlDrawQuad(a, b, c, d, e, f, g, h, i, j, pLayout->minmaxTest)
#define DRAW_COMMENT(a, b) \
HtmlDrawComment(a, b, pLayout->minmaxTest)


/* The following flags may be passed as the 4th argument to
 * inlineContextGetLineBox().
 */
#define LINEBOX_FORCELINE          0x01
#define LINEBOX_FORCEBOX           0x02
#define LINEBOX_CLOSEBORDERS       0x04


/*
 * A seperate BoxContext struct is used for each block box layed out.
 *
 *     tableLayout()
 */
typedef struct BoxContext BoxContext;
struct BoxContext {
    int iContaining;        /* DOWN:    Width of containing block. */

    HtmlFloatList *pFloat;  /* UP/DOWN: Floating margins. */

    int height;             /* UP: Generated box height. */
    int width;              /* UP: Generated box width. */
    HtmlCanvas vc;          /* UP: Canvas to draw the block on. */
};

void nodeGetBoxProperties(LayoutContext *, HtmlNode *, int, BoxProperties *);
void nodeGetBorderProperties(LayoutContext *, HtmlNode *, BorderProperties *);
void nodeGetMargins(LayoutContext *, HtmlNode *, int, MarginProperties *);

int  blockMinMaxWidth(LayoutContext *, HtmlNode *, int *, int *);
void borderLayout(LayoutContext*, HtmlNode*, BoxContext*, int, int, int, int);

/*--------------------------------------------------------------------------*
 * htmlTableLayout.c --
 *
 *     htmlTableLayout.c contains code to layout HTML/CSS tables.
 */
int tableLayout(LayoutContext*, BoxContext*, HtmlNode*);

/* End of htmlTableLayout.c interface
 *-------------------------------------------------------------------------*/

int HtmlLayoutTableCell(LayoutContext *, BoxContext *, HtmlNode *, int);

#endif
