/*
 * htmlprop.h --
 *
 *     This header file contains the definition of the HtmlComputedValues
 *     structure, which stores a set of CSS2 properties output by the
 *     styler. This information is used by the layout engine in
 *     htmllayout.c to create the runtime model of the document. 
 *
 * -----------------------------------------------------------------------
 *     TODO: Copyright.
 */

#ifndef __HTMLPROP_H__
#define __HTMLPROP_H__

/*
 * We need <limits.h> to get the INT_MIN symbol (the most negative number that
 * can be stored in a C "int".
 */
#include <limits.h>

typedef struct HtmlFourSides HtmlFourSides;
typedef struct HtmlFont HtmlFont;
typedef struct HtmlComputedValues HtmlComputedValues;
typedef struct HtmlComputedValuesCreator HtmlComputedValuesCreator;
typedef struct HtmlFontKey HtmlFontKey;
typedef struct HtmlColor HtmlColor;

/* 
 * This structure is used to group four padding, margin or border-width
 * values together. When we get around to it, it will be used for the
 * position properties too ('top', 'right', 'bottom' and 'left').
 */
struct HtmlFourSides {
    int iTop;
    int iLeft;
    int iBottom;
    int iRight;
};

/*
 * The HtmlFont structure is used to store a font in use by the current
 * document. The following properties are used to determine the Tk
 * font to load:
 * 
 *     'font-size'
 *     'font-family'
 *     'font-style'
 *     'font-weight'
 *
 * HtmlFont structures are stored in the HtmlTree.aFonts hash table. The hash
 * table uses a custom key type (struct HtmlFontKey) implemented in htmlhash.c. 
 */
struct HtmlFontKey {
    int iFontSize;           /* Font size in points */
    const char *zFontFamily; /* Name of font family (i.e. "Serif") */
    unsigned char isItalic;  /* True if the font is italic */
    unsigned char isBold;    /* True if the font is bold */
};
struct HtmlFont {
    int nRef;              /* Number of pointers to this structure */
    HtmlFontKey *pKey;     /* Pointer to corresponding HtmlFontKey structure */
    char *zFont;           /* Name of font */
    Tk_Font tkfont;        /* The Tk font */

    int em_pixels;         /* Pixels per 'em' unit */
    int ex_pixels;         /* Pixels per 'ex' unit */
    int space_pixels;      /* Pixels per space (' ') in text */
    Tk_FontMetrics metrics;
};

/*
 * An HtmlColor structure is used to store each color in use by the current
 * document. HtmlColor structures are stored in the HtmlTree.aColors hash
 * table. The hash table uses string keys (the name of the color).
 */
struct HtmlColor {
    int nRef;              /* Number of pointers to this structure */
    char *zColor;          /* Name of color */
    XColor *xcolor;        /* The XColor* */
};

/*
 * An instance of this structure stores a set of property values as assigned by
 * the styler process. The values are as far as I can tell "computed" values,
 * but in some cases I'm really only guessing.
 *
 * All values are stored as one of the following broad "types":
 *
 *     Variable Name       Type
 *     ------------------------------------------
 *         eXXX            Enumerated type values
 *         iXXX            Pixel type values
 *         cXXX            Color type values
 *         fXXX            Font type values
 *     ------------------------------------------
 *
 * Enumerated type values
 *
 *     Many properties can be stored as a single variable, for example the
 *     'display' property is stored in the HtmlComputedValues.eDisplay
 *     variable.  Members of the HtmlComputedValues structure with names that
 *     match the pattern "eXXX" contain a CSS constant value (one of the
 *     CSS_CONST_XXX #define symbols). These are defined in the header file
 *     cssprop.h, which is generated during compilation by the script in
 *     cssprop.tcl. 
 *
 *     Note: Since we use 'unsigned char' to store the eXXX variables:
 *
 *         assert(CSS_CONST_MIN_CONSTANT >= 0);
 *         assert(CSS_CONST_MAX_CONSTANT < 256);
 *
 * Color type values
 *
 * Font type values
 *
 * Pixel type values:
 *
 *     Most variables that match the pattern 'iXXX' contain pixel values - a
 *     length or size expressed in pixels. The only exception at the moment is
 *     HtmlFontKey.iFontSize. 
 *
 *     Percentage values
 *
 *         Some values, for example the 'width' property, may be either
 *         calculated to an exact number of pixels by the styler or left as a
 *         percentage value. In the first case, the 'int iXXX;' variable for
 *         the property contains the number of pixels. Otherwise, it contains
 *         the percentage value multiplied by 100. If the value is a
 *         percentage, then the PROP_MASK_XXX bit is set in the
 *         HtmlComputedValues.mask mask.  For example, given the width of the
 *         parent block in pixels, the following code determines the width in
 *         pixels contained by the HtmlComputedValues structure:
 *
 *             int iParentPixelWidth = <some assignment>;
 *             HtmlComputedValues Values = <some assignment>;
 *
 *             int iPixelWidth;
 *             if (Values.mask & PROP_MASK_WIDTH) {
 *                 iPixelWidth = (Values.iWidth * iParentPixelWidth / 10000);
 *             } else {
 *                 iPixelWidth = Values.iWidth;
 *             }
 *
 *     The 'auto', 'none' and 'normal' values:
 *
 *         If a pixel type value is set to 'auto' or 'none', the integer
 *         variable is set to the constant PIXELVAL_AUTO, PIXELVAL_NONE or
 *         PIXELVAL_NORMAL respectively. These are both very large negative
 *         numbers, unlikely to be confused with real pixel values.
 *
 *     iVerticalAlign:
 *
 *         The 'vertical-align' property, stored in iVerticalAlign is different
 *         from the other iXXX values. The styler output for vertical align is
 *         either a number of pixels or one of the constants 'baseline', 'sub'
 *         'super', 'top', 'text-top', 'middle', 'bottom', 'text-bottom'. The
 *         'vertical-align' property can be assigned a percentage value, but
 *         the styler can resolve it. (This matches the CSS 2.1 description of
 *         the computed value - section 10.8.1).
 *
 *         If 'vertical-align' is a constant value, it is stored in
 *         eVerticalAlign (as a CSS_CONST_XXX value). Otherwise, if it is a
 *         pixel value it is stored in iVerticalAlign and eVerticalAlign is set
 *         to 0.
 *
 *     iLineHeight:
 *         Todo: Note that inheritance is not done correctly for this property
 *         if it is set to <number>.
 */
struct HtmlComputedValues {
    int nRef;                         /* MUST BE FIRST (see htmlhash.c) */
    unsigned int mask;

    unsigned char eDisplay;           /* 'display' */
    unsigned char eFloat;             /* 'float' */
    unsigned char eClear;             /* 'clear' */

    HtmlColor *cColor;                /* 'color' */
    HtmlColor *cBackgroundColor;      /* 'background-color' */

    unsigned char eListStyleType;     /* 'list-style-type' */


    /* 'font-size', 'font-family', 'font-style', 'font-weight' */
    HtmlFont *fFont;

    unsigned char eTextDecoration;    /* 'text-decoration' */
    unsigned char eWhitespace;        /* 'white-space' */
    unsigned char eTextAlign;         /* 'text-align' */

    /* See above. iVerticalAlign is used only if (eVerticalAlign==0) */
    unsigned char eVerticalAlign;     /* 'vertical-align' */
    int iVerticalAlign;               /* 'vertical-align' (pixels) */

    int iBorderSpacing;               /* 'border-spacing' (pixels)            */
    HtmlFourSides border;             /* 'border-width'   (pixels)            */
    int iLineHeight;                  /* 'line-height'    (pixels, %, NORMAL) */
    int iWidth;                       /* 'width'          (pixels, %, AUTO)   */
    int iMinWidth;                    /* 'min-width'      (pixels, %)         */
    int iMaxWidth;                    /* 'max-height'     (pixels, %, NONE)   */
    int iHeight;                      /* 'height'         (pixels, %, AUTO)   */
    int iMinHeight;                   /* 'min-height'     (pixels, %)         */
    int iMaxHeight;                   /* 'max-height'     (pixels, %, NONE)   */
    HtmlFourSides padding;            /* 'padding'        (pixels, %)         */
    HtmlFourSides margin;             /* 'margin'         (pixels, %, AUTO)   */

    unsigned char eBorderTopStyle;    /* 'border-top-style' */
    unsigned char eBorderRightStyle;  /* 'border-right-style' */
    unsigned char eBorderBottomStyle; /* 'border-bottom-style' */
    unsigned char eBorderLeftStyle;   /* 'border-left-style' */
    HtmlColor *cBorderTopColor;       /* 'border-top-color' */
    HtmlColor *cBorderRightColor;     /* 'border-right-color' */
    HtmlColor *cBorderBottomColor;    /* 'border-bottom-color' */
    HtmlColor *cBorderLeftColor;      /* 'border-left-color' */
};

struct HtmlComputedValuesCreator {
    HtmlComputedValues values;
    HtmlFontKey fontKey;
    HtmlTree *pTree;
    HtmlNode *pNode;
    unsigned int em_mask;
    unsigned int ex_mask;
    int eVerticalAlignPercent;       /* True if 'vertical-align' is a % */
    CssProperty *pDeleteList;
};

/*
 * Percentage masks.
 * 
 * According to the spec, the following CSS2 properties can also be set to
 * percentages:
 *
 *     Unsupported properties:
 *         'background-position'
 *         'bottom', 'top', 'left', 'right'
 *         'text-indent'
 *
 *     These can be set to percentages, but the styler can resolve them:
 *         'font-size', 'line-height', 'vertical-align'
 *
 * The HtmlComputedValues.mask mask also contains the
 * CONSTANT_MASK_VERTICALALIGN bit. If this bit is set, then
 * HtmlComputedValues.iVerticalAlign should be interpreted as a constant value
 * (like an HtmlComputedValues.eXXX variable).
 */
#define PROP_MASK_WIDTH          0x00000001
#define PROP_MASK_MIN_WIDTH       0x00000002
#define PROP_MASK_MAX_WIDTH       0x00000004
#define PROP_MASK_HEIGHT         0x00000008
#define PROP_MASK_MIN_HEIGHT      0x00000010
#define PROP_MASK_MAX_HEIGHT      0x00000020
#define PROP_MASK_MARGIN_TOP      0x00000040
#define PROP_MASK_MARGIN_RIGHT    0x00000080
#define PROP_MASK_MARGIN_BOTTOM   0x00000100
#define PROP_MASK_MARGIN_LEFT     0x00000200
#define PROP_MASK_PADDING_TOP     0x00000400
#define PROP_MASK_PADDING_RIGHT   0x00000800
#define PROP_MASK_PADDING_BOTTOM  0x00001000
#define PROP_MASK_PADDING_LEFT    0x00002000
#define PROP_MASK_VERTICAL_ALIGN     0x00004000
#define PROP_MASK_BORDER_TOP_WIDTH    0x00008000
#define PROP_MASK_BORDER_RIGHT_WIDTH  0x00010000
#define PROP_MASK_BORDER_BOTTOM_WIDTH 0x00020000
#define PROP_MASK_BORDER_LEFT_WIDTH   0x00040000

/*
 * Pixel values in the HtmlComputedValues struct may also take the following
 * special values. These are all very large negative numbers, unlikely to be
 * confused with real pixel counts. INT_MIN comes from <limits.h>, which is
 * supplied by Tcl if the operating system doesn't have it.
 */
#define PIXELVAL_AUTO       (2 + (int)INT_MIN)
#define PIXELVAL_NONE       (3 + (int)INT_MIN)
#define PIXELVAL_NORMAL     (4 + (int)INT_MIN)
#define MAX_PIXELVAL        (5 + (int)INT_MIN)

/* 
 * API Notes for managing HtmlComputedValues structures:
 *
 *     The following three functions are used by the styler phase to create and
 *     populate an HtmlComputedValues structure (a set of property values for a
 *     node):
 *
 *         HtmlComputedValuesInit()           (exactly one call)
 *         HtmlComputedValuesSet()            (zero or more calls)
 *         HtmlComputedValuesFinish()         (exactly one call)
 *
 *     To use this API, the caller allocates (either on the heap or the stack,
 *     doesn't matter) an HtmlComputedValuesCreator struct. The contents are
 *     initialised by HtmlComputedValuesInit().
 *
 *         HtmlComputedValuesCreator sValues;
 *         HtmlComputedValuesInit(pTree, pNode, &sValues);
 *
 *     This initialises the HtmlComputedValuesCreator structure to contain the
 *     default (called "initial" in the CSS spec) value for each property. The
 *     default property values can be overwritten using the
 *     HtmlComputedValuesSet() function (see comments above implementation
 *     below).
 * 
 *     Finally, HtmlComputedValuesFinish() is called to obtain the populated 
 *     HtmlComputedValues structure. This function returns a pointer to an
 *     HtmlComputedValues structure, which should be associated with the node
 *     in question before it is passed to the layout engine:
 *
 *         p = HtmlComputedValuesFinish(&sValues);
 *         assert(p);
 *         pNode->pPropertyValues = p;
 *
 *     Once an HtmlComputedValues pointer returned by Finish() is no longer
 *     required (when the node is being restyled or deleted), it should be
 *     freed using:
 *
 *         HtmlComputedValuesRelease(pNode->pPropertyValues);
 */
void HtmlComputedValuesInit(HtmlTree*, HtmlNode*, HtmlComputedValuesCreator*);
int HtmlComputedValuesSet(HtmlComputedValuesCreator *, int, CssProperty*);
HtmlComputedValues *HtmlComputedValuesFinish(HtmlComputedValuesCreator *);
void HtmlComputedValuesRelease(HtmlComputedValues*);

/*
 * The following two functions are used to initialise and destroy the following
 * tables used by code in htmlprop.c. They are called as part of the
 * initialisation and destruction of the widget.
 *
 *     HtmlTree.aColor
 *     HtmlTree.aFont
 *     HtmlTree.aValues
 *     HtmlTree.aFontSizeTable
 */
void HtmlComputedValuesSetupTables(HtmlTree *);
void HtmlComputedValuesCleanupTables(HtmlTree *);

#define HTML_COMPUTED_MARGIN_TOP      margin.iTop
#define HTML_COMPUTED_MARGIN_RIGHT    margin.iRight
#define HTML_COMPUTED_MARGIN_BOTTOM   margin.iBottom
#define HTML_COMPUTED_MARGIN_LEFT     margin.iLeft

#define HTML_COMPUTED_PADDING_TOP     padding.iTop
#define HTML_COMPUTED_PADDING_RIGHT   padding.iRight
#define HTML_COMPUTED_PADDING_BOTTOM  padding.iBottom
#define HTML_COMPUTED_PADDING_LEFT    padding.iLeft

#define HTML_COMPUTED_PADDING_TOP     padding.iTop
#define HTML_COMPUTED_PADDING_RIGHT   padding.iRight
#define HTML_COMPUTED_PADDING_BOTTOM  padding.iBottom
#define HTML_COMPUTED_PADDING_LEFT    padding.iLeft

#define HTML_COMPUTED_HEIGHT          iHeight
#define HTML_COMPUTED_WIDTH           iWidth
#define HTML_COMPUTED_MIN_HEIGHT      iMinHeight
#define HTML_COMPUTED_MIN_WIDTH       iMinWidth
#define HTML_COMPUTED_MAX_HEIGHT      iMaxHeight
#define HTML_COMPUTED_MAX_WIDTH       iMaxWidth

#define PIXELVAL(pV, prop, percent_of) \
    ((pV)->mask & PROP_MASK_ ## prop) ? \
        (((pV)-> HTML_COMPUTED_ ## prop * (percent_of)) / 10000) : \
        ((pV)-> HTML_COMPUTED_ ## prop)

#endif
