/*
 * htmlprop.h --
 *
 *     This header file contains the definition of the HtmlPropertyValues
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
typedef struct HtmlPropertyValues HtmlPropertyValues;
typedef struct HtmlPropertyValuesCreator HtmlPropertyValuesCreator;
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
    int em_pixels;         /* Pixels per 'em' unit */
    int ex_pixels;         /* Pixels per 'ex' unit */
    Tk_Font tkfont;        /* The Tk font */
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
 * but the CSS2 spec doesn't define exactly what this means for each property
 * so I'm really only guessing.
 *
 * Enumerated type values
 *
 *     Many properties can be stored as a single variable, for example the
 *     'display' property is stored in the HtmlPropertyValues.eDisplay
 *     variable.  Members of the HtmlPropertyValues structure with names that
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
 *         HtmlPropertyValues.mask mask.  For example, given the width of the
 *         parent block in pixels, the following code determines the width in
 *         pixels contained by the HtmlPropertyValues structure:
 *
 *             int iParentPixelWidth = <some assignment>;
 *             HtmlPropertyValues Values = <some assignment>;
 *
 *             int iPixelWidth;
 *             if (Values.mask & PROP_MASK_WIDTH) {
 *                 iPixelWidth = (Values.iWidth * iParentPixelWidth / 10000);
 *             } else {
 *                 iPixelWidth = Values.iWidth;
 *             }
 *
 *     The 'auto', 'none' and 'inherit' values
 *
 *         If a pixel type value is set to 'auto' or 'none', the integer
 *         variable is set to the constant PIXELVAL_AUTO or PIXELVAL_NONE
 *         respectively. These are both very large negative numbers, unlikely
 *         to be confused with real pixel values.
 *
 *         If a pixel type value is set to 'inherit', and it is one of those
 *         that may also be expressed as a percentage, then the integer
 *         variable is set to PIXELVAL_INHERIT. This is because the inheritance
 *         has to be done on the actual value, which may not be known until the
 *         layout engine runs.
 *
 *         Section 4.3.3 of CSS spec 2.1 contains a statement regarding
 *         inheritance and percentage values.
 *
 * iVerticalAlign:
 *
 *         The 'vertical-align' property, stored in iVerticalAlign is different
 *         from the other iXXX values. The styler output for vertical align is
 *         either a number of pixels or one of the constants 'baseline', 'sub'
 *         'super', 'top', 'text-top', 'middle', 'bottom', 'text-bottom'. The
 *         'vertical-align' property can be assigned a percentage value, but
 *         the styler can resolve it. (This matches the CSS 2.1 description of
 *         the computed value - section 10.8.1).
 *
 *         If 'vertical-align' is a pixel value, the PROP_MASK_VERTICALALIGN
 *         bit is set in HtmlPropertyValues.mask and iVerticalAlign contains
 *         the absolute number of pixels. If 'vertical-align' is one of the
 *         constants, the PROP_MASK_VERTICALALIGN bit is cleared and
 *         iVerticalAlign is set to the corresponding CSS_CONST_XXX value.
 */
struct HtmlPropertyValues {
    int nRef;                         /* MUST BE FIRST (see htmlhash.c) */
    unsigned int mask;

    unsigned char eDisplay;           /* 'display' */
    unsigned char eFloat;             /* 'float' */
    unsigned char eClear;             /* 'clear' */

    HtmlColor *cColor;                /* 'color' */
    HtmlColor *cBackgroundColor;      /* 'background-color' */

    unsigned char eListStyleType;     /* 'list-style-type' */

    int iVerticalAlign;               /* 'vertical-align' (note: mask) */
    int iBorderSpacing;               /* 'border-spacing' */
    int iLineHeight;                  /* 'line-height' */

    /* 'font-size', 'font-family', 'font-style', 'font-weight' */
    HtmlFont *fFont;

    unsigned char eTextDecoration;    /* 'text-decoration' */
    unsigned char eWhitespace;        /* 'white-space' */
    unsigned char eTextAlign;         /* 'text-align' */

    int iWidth;                  /* 'width'      (note: mask, inherit_mask) */
    int iMinWidth;               /* 'min-width'  (note: mask, inherit_mask) */
    int iMaxWidth;               /* 'max-height' (note: mask, inherit_mask) */
    int iHeight;                 /* 'height'     (note: mask, inherit_mask) */
    int iMinHeight;              /* 'min-height' (note: mask, inherit_mask) */
    int iMaxHeight;              /* 'max-height' (note: mask, inherit_mask) */
    HtmlFourSides padding;       /* 'padding'    (note: mask, inherit_mask) */
    HtmlFourSides margin;        /* 'margin'     (note: mask, inherit_mask) */

    unsigned char eBorderTopStyle;    /* 'border-top-style' */
    unsigned char eBorderRightStyle;  /* 'border-right-style' */
    unsigned char eBorderBottomStyle; /* 'border-bottom-style' */
    unsigned char eBorderLeftStyle;   /* 'border-left-style' */
    HtmlFourSides border;             /* 'border-width' */
    HtmlColor *cBorderTopColor;       /* 'border-top-color' */
    HtmlColor *cBorderRightColor;     /* 'border-right-color' */
    HtmlColor *cBorderBottomColor;    /* 'border-bottom-color' */
    HtmlColor *cBorderLeftColor;      /* 'border-left-color' */
};

struct HtmlPropertyValuesCreator {
    HtmlPropertyValues values;
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
 * The HtmlPropertyValues.mask mask also contains the
 * CONSTANT_MASK_VERTICALALIGN bit. If this bit is set, then
 * HtmlPropertyValues.iVerticalAlign should be interpreted as a constant value
 * (like an HtmlPropertyValues.eXXX variable).
 */
#define PROP_MASK_WIDTH          0x00000001
#define PROP_MASK_MINWIDTH       0x00000002
#define PROP_MASK_MAXWIDTH       0x00000004
#define PROP_MASK_HEIGHT         0x00000008
#define PROP_MASK_MINHEIGHT      0x00000010
#define PROP_MASK_MAXHEIGHT      0x00000020
#define PROP_MASK_MARGINTOP      0x00000040
#define PROP_MASK_MARGINRIGHT    0x00000080
#define PROP_MASK_MARGINBOTTOM   0x00000100
#define PROP_MASK_MARGINLEFT     0x00000200
#define PROP_MASK_PADDINGTOP     0x00000400
#define PROP_MASK_PADDINGRIGHT   0x00000800
#define PROP_MASK_PADDINGBOTTOM  0x00001000
#define PROP_MASK_PADDINGLEFT    0x00002000
#define PROP_MASK_VERTICALALIGN     0x00004000
#define PROP_MASK_BORDERWIDTHTOP    0x00008000
#define PROP_MASK_BORDERWIDTHRIGHT  0x00010000
#define PROP_MASK_BORDERWIDTHBOTTOM 0x00020000
#define PROP_MASK_BORDERWIDTHLEFT   0x00040000

/*
 * Pixel values in the HtmlPropertyValues struct may also take the following
 * special values. These are all very large negative numbers, unlikely to be
 * confused with real pixel counts. INT_MIN comes from <limits.h>, which is
 * supplied by Tcl if the operating system doesn't have it.
 */
#define PIXELVAL_INHERIT    (1 + (int)INT_MIN)
#define PIXELVAL_AUTO       (2 + (int)INT_MIN)
#define PIXELVAL_NONE       (3 + (int)INT_MIN)
#define PIXELVAL_NORMAL     (4 + (int)INT_MIN)
#define MAX_PIXELVAL        (5 + (int)INT_MIN)

/* 
 * API Notes for managing HtmlPropertyValues structures:
 *
 *     The following three functions are used by the styler phase to create and
 *     populate an HtmlPropertyValues structure (a set of property values for a
 *     node):
 *
 *         HtmlPropertyValuesInit()           (exactly one call)
 *         HtmlPropertyValuesSet()            (zero or more calls)
 *         HtmlPropertyValuesFinish()         (exactly one call)
 *
 *     To use this API, the caller allocates (either on the heap or the stack,
 *     doesn't matter) an HtmlPropertyValuesCreator struct. The contents are
 *     initialised by HtmlPropertyValuesInit().
 *
 *         HtmlPropertyValuesCreator sValues;
 *         HtmlPropertyValuesInit(pTree, pNode, &sValues);
 *
 *     This initialises the HtmlPropertyValuesCreator structure to contain the
 *     default (called "initial" in the CSS spec) value for each property. The
 *     default property values can be overwritten using the
 *     HtmlPropertyValuesSet() function (see comments above implementation
 *     below).
 * 
 *     Finally, HtmlPropertyValuesFinish() is called to obtain the populated 
 *     HtmlPropertyValues structure. This function returns a pointer to an
 *     HtmlPropertyValues structure, which should be associated with the node
 *     in question before it is passed to the layout engine:
 *
 *         p = HtmlPropertyValuesFinish(&sValues);
 *         assert(p);
 *         pNode->pPropertyValues = p;
 *
 *     Once an HtmlPropertyValues pointer returned by Finish() is no longer
 *     required (when the node is being restyled or deleted), it should be
 *     freed using:
 *
 *         HtmlPropertyValuesRelease(pNode->pPropertyValues);
 */
void HtmlPropertyValuesInit(HtmlTree*, HtmlNode*, HtmlPropertyValuesCreator*);
int HtmlPropertyValuesSet(HtmlPropertyValuesCreator *, int, CssProperty*);
HtmlPropertyValues *HtmlPropertyValuesFinish(HtmlPropertyValuesCreator *);
void HtmlPropertyValuesRelease(HtmlPropertyValues*);

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
void HtmlPropertyValuesSetupTables(HtmlTree *);
void HtmlPropertyValuesCleanupTables(HtmlTree *);

#endif

