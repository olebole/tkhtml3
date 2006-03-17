
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

/* Add box (i.e. replaced) inline elements to the inline context */
void HtmlInlineContextAddBox(InlineContext*,HtmlNode*,HtmlCanvas*,int,int,int);

/* Test to see if the inline-context contains any more boxes */
int HtmlInlineContextIsEmpty(InlineContext *);

/* Retrieve the next line-box from an inline context */
int HtmlInlineContextGetLineBox(
LayoutContext *, InlineContext*,int*,int,HtmlCanvas*,int*, int*);

/* Manage inline borders and text-decoration */
InlineBorder *HtmlGetInlineBorder(LayoutContext *, HtmlNode *, int);
int HtmlInlineContextPushBorder(InlineContext *, InlineBorder *);
void HtmlInlineContextPopBorder(InlineContext *, InlineBorder *);

/* End of htmllayoutinline.c interface
 *-------------------------------------------------------------------------*/

#define DRAW_CANVAS(a, b, c, d, e) \
HtmlDrawCanvas(a, b, c, d, e)
#define DRAW_WINDOW(a, b, c, d, e, f) \
HtmlDrawWindow(a, b, c, d, e, f, pLayout->minmaxTest)
#define DRAW_BACKGROUND(a, b) \
HtmlDrawBackground(a, b, pLayout->minmaxTest)
#define DRAW_QUAD(a, b, c, d, e, f, g, h, i, j) \
HtmlDrawQuad(a, b, c, d, e, f, g, h, i, j, pLayout->minmaxTest)

/* The following flags may be passed as the 4th argument to
 * HtmlInlineContextGetLineBox().
 */
#define LINEBOX_FORCELINE          0x01
#define LINEBOX_FORCEBOX           0x02
#define LINEBOX_CLOSEBORDERS       0x04


/*
 * A seperate BoxContext struct is used for each block box layed out.
 *
 *     HtmlTableLayout()
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
void nodeGetMargins(LayoutContext *, HtmlNode *, int, MarginProperties *);

int  blockMinMaxWidth(LayoutContext *, HtmlNode *, int *, int *);
void borderLayout(LayoutContext*, HtmlNode*, BoxContext*, int, int, int, int);

/*--------------------------------------------------------------------------*
 * htmltable.c --
 *
 *     htmlTableLayout.c contains code to layout HTML/CSS tables.
 */
int HtmlTableLayout(LayoutContext*, BoxContext*, HtmlNode*);

/* End of htmlTableLayout.c interface
 *-------------------------------------------------------------------------*/

int HtmlLayoutTableCell(LayoutContext *, BoxContext *, HtmlNode *, int);

#endif
