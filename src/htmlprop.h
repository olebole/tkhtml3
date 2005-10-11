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

typedef struct HtmlFourSides HtmlFourSides;
typedef struct HtmlPropertyValues HtmlPropertyValues;

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
 */
#define PERCENT_MASK_WIDTH          0x00000001
#define PERCENT_MASK_MINWIDTH       0x00000002
#define PERCENT_MASK_MAXWIDTH       0x00000004
#define PERCENT_MASK_HEIGHT         0x00000008
#define PERCENT_MASK_MINHEIGHT      0x00000010
#define PERCENT_MASK_MAXHEIGHT      0x00000020

#define PERCENT_MASK_MARGINTOP      0x00000040
#define PERCENT_MASK_MARGINRIGHT    0x00000080
#define PERCENT_MASK_MARGINBOTTOM   0x00000100
#define PERCENT_MASK_MARGINLEFT     0x00000200

#define PERCENT_MASK_PADDINGTOP     0x00000400
#define PERCENT_MASK_PADDINGRIGHT   0x00000800
#define PERCENT_MASK_PADDINGBOTTOM  0x00001000
#define PERCENT_MASK_PADDINGLEFT    0x00002000

#define CONSTANT_MASK_VERTICALALIGN 0x00004000


/*
 * If the following properties take the value "inherit", then inheritance
 * must be taken into account during layout. This is because the inherited
 * value is the "computed" value, not the "specified" value. The computed
 * value of these properties is not known until the layout engine runs.
 */
#define INHERIT_MASK_WIDTH          0x00000001
#define INHERIT_MASK_MINWIDTH       0x00000002
#define INHERIT_MASK_MAXWIDTH       0x00000004
#define INHERIT_MASK_HEIGHT         0x00000008
#define INHERIT_MASK_MINHEIGHT      0x00000010
#define INHERIT_MASK_MAXHEIGHT      0x00000020
#define INHERIT_MASK_MARGINTOP      0x00000040
#define INHERIT_MASK_MARGINRIGHT    0x00000080
#define INHERIT_MASK_MARGINBOTTOM   0x00000100
#define INHERIT_MASK_MARGINLEFT     0x00000200
#define INHERIT_MASK_PADDINGTOP     0x00000400
#define INHERIT_MASK_PADDINGRIGHT   0x00000800
#define INHERIT_MASK_PADDINGBOTTOM  0x00001000
#define INHERIT_MASK_PADDINGLEFT    0x00002000


/*
 * An instance of this structure stores a set of property values as
 * assigned by the styler process. The values are not consistently
 * "specified", "computed" or "actual" values as specified by the CSS2
 * spec. 
 *
 * Many properties can be stored as a single variable, for example the
 * 'display' property is stored in the HtmlPropertyValues.eDisplay
 * variable.
 *
 * Some values, for example the 'width' property, may be either calculated
 * to an exact number of pixels by the styler or left as a percentage
 * value. In the first case, the 'int iXXX;' variable for the property
 * contains the number of pixels. Otherwise, it contains the percentage
 * value multiplied by 100. If the value is a percentage, then the
 * PERCENT_MASK_XXX bit is set in the HtmlPropertyValues.mask mask.
 * For example, given the width of the parent block in pixels, the
 * following code determines the width in pixels contained by the
 * HtmlPropertyValues structure:
 *
 *     int iParentPixelWidth = <some assignment>;
 *     HtmlPropertyValues Values = <some assignment>;
 *
 *     int iPixelWidth;
 *     if (Values.mask & PERCENT_MASK_WIDTH) {
 *         iPixelWidth = (Values.iWidth * iParentPixelWidth / 10000);
 *     } else {
 *         iPixelWidth = Values.iWidth;
 *     }
 *
 *
 * TODO: The structure contains XColor* and Tk_Font variables. These should
 *       be changed to pointers to reference counted structs stored in
 *       hash-tables as part of the HtmlTree structure.
 */
struct HtmlPropertyValues {
    u32 mask;
    u32 inherit_mask;

    u8 eDisplay;                      /* 'display' */
    u8 eFloat;                        /* 'float' */
    u8 eClear;                        /* 'clear' */

    XColor *cColor;                   /* 'color' */
    XColor *cBackgroundColor;         /* 'background-color' */

    u8 eListStyleType;                /* 'list-style-type' */

    int iVerticalAlign;               /* 'vertical-align' (note: mask) */
    int iBorderSpacing;               /* 'border-spacing' */
    int iLineHeight;                  /* 'line-height' */

    /* 'font-size', 'font-family', 'font-style', 'font-weight' */
    Tk_Font font;

    u8 eTextDecoration;               /* 'text-decoration' */
    u8 eWhitespace;                   /* 'white-space' */
    u8 eTextAlign;                    /* 'text-align' */

    int iWidth;                       /* 'width'      (note: mask) */
    int iMinWidth;                    /* 'min-width'  (note: mask) */
    int iMaxWidth;                    /* 'max-height' (note: mask) */
    int iHeight;                      /* 'height'     (note: mask) */
    int iMinHeight;                   /* 'min-height' (note: mask) */
    int iMaxHeight;                   /* 'max-height' (note: mask) */

    HtmlFourSides padding;            /* 'padding' (note: mask) */
    HtmlFourSides margin;             /* 'margin'  (note: mask) */

    HtmlFourSides border;             /* 'border-width' + 'border-style' */
    XColor *cBorderTopColor;          /* 'border-top-color' */
    XColor *cBorderLeftColor;         /* 'border-left-color' */
    XColor *cBorderBottomColor;       /* 'border-bottom-color' */
    XColor *cBorderRightColor;        /* 'border-right-color' */
};

#endif
