/*
** Structures and typedefs used by the HTML widget
** $Revision: 1.39 $
**
** This source code is released into the public domain by the author,
** D. Richard Hipp, on 2002 December 17.  Instead of a license, here
** is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
*/

#ifndef __HTML_H__
#define __HTML_H__

#include <tk.h>
#include "htmltokens.h"

/*
** Various data types.  This code is designed to run on a modern
** cached architecture where the CPU runs a lot faster than the
** memory bus.  Hence we try to pack as much data into as small a space
** as possible so that it is more likely to fit in cache.  The
** extra CPU instruction or two needed to unpack the data is not
** normally an issue since we expect the speed of the memory bus 
** to be the limiting factor.
*/
typedef unsigned char  Html_u8;      /* 8-bit unsigned integer */
typedef short          Html_16;      /* 16-bit signed integer */
typedef unsigned short Html_u16;     /* 16-bit unsigned integer */
typedef int            Html_32;      /* 32-bit signed integer */

typedef union HtmlElement HtmlElement;
typedef struct HtmlBaseElement HtmlBaseElement;
typedef struct HtmlTextElement HtmlTextElement;
typedef struct HtmlSpaceElement HtmlSpaceElement;
typedef struct HtmlMarkupElement HtmlMarkupElement;
typedef struct HtmlCell HtmlCell;
typedef struct HtmlTable HtmlTable;
typedef struct HtmlRef HtmlRef;
typedef struct HtmlLi HtmlLi;
typedef struct HtmlListStart HtmlListStart;
typedef struct HtmlExtensions HtmlExtensions;
typedef struct HtmlImageAnim HtmlImageAnim;
typedef struct HtmlImageMarkup HtmlImageMarkup;
typedef struct HtmlImage HtmlImage;
typedef struct HtmlInput HtmlInput;
typedef struct HtmlForm HtmlForm;
typedef struct HtmlHr HtmlHr;
typedef struct HtmlAnchor HtmlAnchor;
typedef struct HtmlScript HtmlScript;
typedef struct HtmlBlock HtmlBlock;
typedef struct HtmlStyleStack HtmlStyleStack;
typedef struct HtmlMargin HtmlMargin;
typedef struct HtmlLayoutContext HtmlLayoutContext;
typedef struct GcCache GcCache;
typedef struct HtmlIndex HtmlIndex;
typedef struct HtmlMapArea HtmlMapArea;
typedef struct HtmlWidget HtmlWidget;
typedef struct HtmlUserTag HtmlUserTag;
typedef struct HtmlStyle HtmlStyle;
typedef struct HtmlTokenMap HtmlTokenMap;
typedef struct HtmlTree HtmlTree;

#ifdef USE_DMALLOC
#define __malloc_and_calloc_defined
#define __need_malloc_and_calloc
#define _STRING_H
#define DMALLOC_FUNC_CHECK
#include <dmalloc.h>
#endif

/*
** Debug must be turned on for testing to work.
*/
#define DEBUG

/*
** Version information for the package mechanism.
*/
#define HTML_PKGNAME "Tkhtml"
#define HTML_PKGVERSION "2.0"

/*
** Sanity checking macros.
*/
#ifdef DEBUG
#define HtmlAssert(X) \
  if(!(X)){ \
    fprintf(stderr,"Assertion failed on line %d of %s\n",__LINE__,__FILE__); \
  }
#define HtmlCantHappen \
  fprintf(stderr,"Can't happen on line %d of %s\n",__LINE__,__FILE__);
#else
#define HtmlAssert(X)
#define HtmlCantHappen
#endif

#if defined(COVERAGE_TEST)
# define TestPoint(X)      {extern int HtmlTPArray[]; HtmlTPArray[X]++;}
# define UNTESTED          HtmlTPUntested(__FILE__,__LINE__)
# define CANT_HAPPEN       HtmlTPCantHappen(__FILE__,__LINE__)
# define HtmlVerifyLock(H) if((H)->locked==0)HtmlTPCantHappen(__FILE__,__LINE__)
#else
# define TestPoint(X)
# define UNTESTED
# define CANT_HAPPEN
# define HtmlVerifyLock(H)
#endif

#if INTERFACE
#define DLL_EXPORT
#endif
#if defined(USE_TCL_STUBS) && defined(__WIN32__)
# undef DLL_EXPORT
# define DLL_EXPORT __declspec(dllexport)
#endif

#ifndef DLL_EXPORT
#define DLL_EXPORT
#endif

/*
** The TRACE macro is used to print internal information about the
** HTML layout engine during testing and debugging.  The amount of
** information printed is governed by a global variable named
** HtmlTraceMask.  If bits in the first argument to the TRACE macro
** match any bits in HtmlTraceMask variable, then the trace message
** is printed.
**
** All of this is completely disabled, of course, if the DEBUG macro
** is not defined.
*/
#ifdef DEBUG
# define TRACE_INDENT  printf("%*s",HtmlDepth-3,"")
# define TRACE(Flag, Args) \
    if( (Flag)&HtmlTraceMask ){ \
       TRACE_INDENT; printf Args; fflush(stdout); \
    }
# define TRACE_PUSH(Flag)  if( (Flag)&HtmlTraceMask ){ HtmlDepth+=3; }
# define TRACE_POP(Flag)   if( (Flag)&HtmlTraceMask ){ HtmlDepth-=3; }
extern int HtmlDepth;
extern int HtmlTraceMask;
extern HtmlWidget *dbghtmlPtr;
#else
# define TRACE_INDENT
# define TRACE(Flag, Args)
# define TRACE_PUSH(Flag)
# define TRACE_POP(Flag)
#endif

/*
** Bitmasks for the HtmlTraceMask global variable
*/
#define HtmlTrace_Table1       0x00000001
#define HtmlTrace_Table2       0x00000002
#define HtmlTrace_Table3       0x00000004
#define HtmlTrace_Table4       0x00000008
#define HtmlTrace_Table5       0x00000010
#define HtmlTrace_Table6       0x00000020
#define HtmlTrace_GetLine      0x00000100
#define HtmlTrace_GetLine2     0x00000200
#define HtmlTrace_FixLine      0x00000400
#define HtmlTrace_BreakMarkup  0x00001000
#define HtmlTrace_Style        0x00002000
#define HtmlTrace_Input1       0x00004000


/*
** Macros to allocate and free memory.
*/
#ifndef USE_DMALLOC
#define HtmlAlloc(A)      ((void*)ckalloc(A))
#define HtmlFree(A)       ckfree((char*)(A))
#define HtmlRealloc(A,B)  ((void*)ckrealloc((A),(B)))
#define HtmlStrdup(A)     HtmlStrdupX(A)
#else
#define HtmlAlloc(A)      malloc(A)
#define HtmlFree(A)       free(A)
#define HtmlRealloc(A,B)  realloc(A,B)
#define HtmlStrdup(A)     strdup(A)
#endif

/*
** An instance of the following structure is used to record style
** information on each Html element.
*/
struct HtmlStyle {
  unsigned int font    : 6;      /* Font to use for display */
  unsigned int color   : 6;      /* Foreground color */
  signed int subscript : 4;      /* Positive for <sup>, negative for <sub> */
  unsigned int align   : 2;      /* Horizontal alignment */
  unsigned int bgcolor : 6;      /* Background color */
  unsigned int expbg   : 1;      /* Set to 1 if bgcolor explicitly set */
  unsigned int flags   : 7;      /* the STY_ flags below */
};

/*
** We allow 8 different font families:  Normal, Bold, Italic and 
** Bold-Italic in either variable or constant width.
** Within each family there can be up to 7 font sizes from 1
** (the smallest) up to 7 (the largest).  Hence, the widget can use
** a maximum of 56 fonts.  The ".font" field of the style is an integer
** between 0 and 55 which indicates which font to use.
*/
#define N_FONT_FAMILY     8
#define N_FONT_SIZE       7
#define N_FONT            (N_FONT_FAMILY*N_FONT_SIZE)
#define NormalFont(X)     (X)
#define BoldFont(X)       ((X)+N_FONT_SIZE)
#define ItalicFont(X)     ((X)+2*N_FONT_SIZE)
#define CWFont(X)         ((X)+4*N_FONT_SIZE)
#define FontSize(X)       ((X)%N_FONT_SIZE)
#define FontFamily(X)     (((X)/N_FONT_SIZE)*N_FONT_SIZE)
#define FONT_Any           -1
#define FONT_Default      3
#define FontSwitch(Size, Bold, Italic, Cw) \
                          ((Size)+(Bold+(Italic)*2+(Cw)*4)*N_FONT_SIZE)

/*
** Macros for manipulating the fontValid bitmap of an HtmlWidget structure.
*/
#define FontIsValid(H,I)     (((H)->fontValid[(I)>>3] & (1<<((I)&3)))!=0)
#define FontSetValid(H,I)    ((H)->fontValid[(I)>>3] |= (1<<((I)&3)))
#define FontClearValid(H,I)  ((H)->fontValid[(I)>>3] &= ~(1<<((I)&3)))

/*
** Information about available colors.
**
** The widget will use at most N_COLOR colors.  4 of these colors
** are predefined.  The rest are user selectable by options to
** various markups. (Ex:  <font color=red>)
**
** All colors are stored in the apColor[] array of the main widget
** structure.  The ".color" field of the HtmlStyle is an integer
** between 0 and N_COLOR-1 which indicates which of these colors 
** to use.
*/
#define N_COLOR              (sizeof(long long)*8)  /* Total number of colors */
#define COLOR_Normal         0      /* Index for normal color (black) */
#define COLOR_Unvisited      1      /* Index for unvisited hyperlinks */
#define COLOR_Visited        2      /* Color for visited hyperlinks */
#define COLOR_Selection      3      /* Background color for the selection */
#define COLOR_Background     4      /* Default background color */
#define N_PREDEFINED_COLOR   5      /* Number of predefined colors */

/*
** The "align" field of the style determines how text is justified
** horizontally.  ALIGN_None means that the alignment is not specified.
** (It should probably default to ALIGN_Left in this case.)
*/
#define ALIGN_Left   1
#define ALIGN_Right  2
#define ALIGN_Center 3
#define ALIGN_None   0

/*
** Possible value of the "flags" field of HtmlStyle are shown below.
**
**  STY_Preformatted       If set, the current text occurred within
**                         <pre>..</pre>
**
**  STY_StrikeThru         Draw a solid line thru the middle of this text.
**
**  STY_Underline          This text should drawn with an underline.
**
**  STY_NoBreak            This text occurs within <nobr>..</nobr>
**
**  STY_Anchor             This text occurs within <a href=X>..</a>.
**
**  STY_DT                 This text occurs within <dt>..</dt>.                 
**
**  STY_Invisible          This text should not appear in the main HTML
**                         window.  (For example, it might be within 
**                         <title>..</title> or <marquee>..</marquee>.)
*/
#define STY_Preformatted    0x001
#define STY_StrikeThru      0x002
#define STY_Underline       0x004
#define STY_NoBreak         0x008
#define STY_Anchor          0x010
#define STY_DT              0x020
#define STY_Invisible       0x040
#define STY_FontMask        (STY_StrikeThru|STY_Underline)

#define HTMLTAG_END         0x01
#define HTMLTAG_INLINE      0x02
#define HTMLTAG_BLOCK       0x04
#define HTMLTAG_EMPTY       0x08

struct HtmlTokenMap {
  char *zName;                /* Name of a markup */
  Html_16 type;               /* Markup type code */
  Html_16 extra;              /* Extra space needed above HtmlBaseElement */
  Html_u8 flags;              /* Combination of HTMLTAG values */
  HtmlTokenMap *pCollide;     /* Hash table collision chain */
};

/*
** Every element contains at least this much information:
*/
struct HtmlBaseElement {
  HtmlElement *pNext;         /* Next input token in a list of them all */
  HtmlElement *pPrev;         /* Previous token in a list of them all */
  HtmlStyle style;            /* The rendering style for this token */
  Html_u8 type;               /* The token type. */
  Html_u8 flags;              /* The HTML_ flags below */
  Html_16 count;              /* Various uses, depending on "type" */
  int id;		      /* Unique identifier */
  int offs;		      /* Offset within zText */
};

/*
** Bitmasks for the "flags" field of the HtmlBaseElement
*/
#define HTML_Visible   0x01     /* This element produces "ink" */
#define HTML_NewLine   0x02     /* type==Html_Space and ends with newline */
#define HTML_Selected  0x04     /* Some or all of this Html_Block is selected */

/*
** Each text element holds additional information as show here.  Notice
** that extra space is allocated so that zText[] will be large enough
** to hold the complete text of the element.  X and y coordinates are
** relative to the virtual canvas.  The y coordinate refers to the
** baseline.
*/
struct HtmlTextElement {
  HtmlBaseElement base;       /* All the base information */
  Html_32 y;                  /* y coordinate where text should be rendered */
  Html_16 x;                  /* x coordinate where text should be rendered */
  Html_16 w;                  /* width of this token in pixels */
  Html_u8 ascent;             /* height above the baseline */
  Html_u8 descent;            /* depth below the baseline */
  Html_u8 spaceWidth;         /* Width of one space in the current font */
  char *zText;              /* Text for this element.  Null terminated */
};

/*
** Each space element is represented like this:
*/
struct HtmlSpaceElement {
  HtmlBaseElement base;       /* All the base information */
  Html_16 w;                  /* Width of a single space in current font */
  Html_u8 ascent;             /* height above the baseline */
  Html_u8 descent;            /* depth below the baseline */
};

/*
** Most markup uses this structure.  Some markup extends this structure
** with additional information, but most use it as a base, at the very
** least. 
**
** If the markup doesn't have arguments (the "count" field of
** HtmlBaseElement is 0) then the extra "argv" field of this structure
** is not allocated and should not be used.
*/
struct HtmlMarkupElement {
  HtmlBaseElement base;
  char **argv;
};

/* Each <td> or <th> markup is represented by an instance of the 
** following structure.
**
** Drawing for a cell is a sunken 3D border with the border width given
** by the borderWidth field in the associated <table> structure.
*/
struct HtmlCell {
  HtmlMarkupElement markup;
  Html_16 rowspan;          /* Number of rows spanned by this cell */
  Html_16 colspan;          /* Number of columns spanned by this cell */
  Html_16 x;                /* X coordinate of left edge of border */
  Html_16 w;                /* Width of the border */
  Html_32 y;                /* Y coordinate of top of border indentation */
  Html_32 h;                /* Height of the border */
  HtmlElement *pTable;      /* Pointer back to the <table> */
  HtmlElement *pRow;	    /* Pointer back to the <tr> */
  HtmlElement *pEnd;        /* Element that ends this cell */
  Tk_Image bgimage;	    /* A background for the cell */
};

/*
** The maximum number of columns allowed in a table.  Any columns beyond
** this number are ignored.
*/
#define HTML_MAX_COLUMNS 40

/*
** This structure is used for each <table> element.
**
** In the minW[] and maxW[] arrays, the [0] element is the overall
** minimum and maximum width, including cell padding, spacing and 
** the "hspace".  All other elements are the minimum and maximum 
** width for the contents of individual cells without any spacing or
** padding.
*/
struct HtmlTable {
  HtmlMarkupElement markup;
  Html_u8 borderWidth;           /* Width of the border */
  Html_u8 nCol;                  /* Number of columns */
  Html_u16 nRow;                 /* Number of rows */
  Html_32 y;                     /* top edge of table border */
  Html_32 h;                     /* height of the table border */
  Html_16 x;                     /* left edge of table border */
  Html_16 w;                     /* width of the table border */
  int minW[HTML_MAX_COLUMNS+1];  /* minimum width of each column */
  int maxW[HTML_MAX_COLUMNS+1];  /* maximum width of each column */
  HtmlElement *pEnd;             /* Pointer to end tag element */
  Tk_Image bgimage;		 /* A background for the entire table */
  int hasbg;			 /* 1 if a table above has bgimage */
  int tktable; 			 /* 1 if table uses a tktable . */
};

/* This structure is used for </table>, </td>, <tr>, </tr> 
** and </th> elements.  It points back to the <table> element 
** that began the table.  It is also used by </a> to point back
** to the original <a>.  I'll probably think of other uses before
** all is said and done...
*/
struct HtmlRef {
  HtmlMarkupElement markup;
  HtmlElement *pOther;		/* Pointer to some other Html element */
  Tk_Image bgimage;		/* A background for the entire row */
};

/*
** An instance of the following structure is used to represent
** each <LI> markup.
*/
struct HtmlLi {
  HtmlMarkupElement markup;
  Html_u8 type;     /* What type of list is this? */
  Html_u8 ascent;   /* height above the baseline */
  Html_u8 descent;  /* depth below the baseline */
  Html_16 cnt;      /* Value for this element (if inside <OL>) */
  Html_16 x;        /* X coordinate of the bullet */
  Html_32 y;        /* Y coordinate of the bullet */
};

/*
** The .type field of an HtmlLi or HtmlListStart structure can take on 
** any of the following values to indicate what type of bullet to draw.
** The value in HtmlLi will take precedence over the value in HtmlListStart
** if the two values differ.
*/
#define LI_TYPE_Undefined 0     /* If in HtmlLi, use the HtmlListStart value */
#define LI_TYPE_Bullet1   1     /* A solid circle */
#define LI_TYPE_Bullet2   2     /* A hollow circle */
#define LI_TYPE_Bullet3   3     /* A hollow square */
#define LI_TYPE_Enum_1    4     /* Arabic numbers */
#define LI_TYPE_Enum_A    5     /* A, B, C, ... */
#define LI_TYPE_Enum_a    6     /* a, b, c, ... */
#define LI_TYPE_Enum_I    7     /* Capitalized roman numerals */
#define LI_TYPE_Enum_i    8     /* Lower-case roman numerals */

/*
** An instance of this structure is used for <UL> or <OL>
** markup.
*/
struct HtmlListStart {
  HtmlMarkupElement markup;
  Html_u8 type;            /* One of the LI_TYPE_ defines above */
  Html_u8 compact;         /* True if the COMPACT flag is present */
  Html_u16 cnt;            /* Next value for <OL> */
  Html_u16 width;          /* How much space to allow for indentation */
  HtmlElement *pPrev;      /* Next higher level list, or NULL */
};

/* Structure to chain extension data onto. */
struct HtmlExtensions {
  void *exts;
  int typ; int flags;
  struct HtmlExtensions *next;
};

/*
** Information about each image on the HTML widget is held in an
** instance of the following structure.  A pointer to this structure
** is the clientData for the image change callback.  All image structures
** are held on a list attached to the main widget structure.
**
** This structure is NOT an element.  The <IMG> element is represented
** by an HtmlImageMarkup structure below.  There is one HtmlImageMarkup
** for each <IMG> in the source HTML.  There is one of these structures
** for each unique image loaded.  (If two <IMG> specify the same image,
** there are still two HtmlImageMarkup structures but only one
** HtmlImage structure that is shared between them.)
*/
struct HtmlImageAnim {
  Tk_Image image;
  struct HtmlImageAnim* next;
};

struct HtmlImage {
  HtmlWidget *htmlPtr;     /* The owner of this image */
  Tk_Image image;          /* The Tk image token */
  Html_32 w;               /* Requested width of this image (0 if none) */
  Html_32 h;               /* Requested height of this image (0 if none) */
  char *zUrl;              /* The URL for this image. */
  char *zWidth, *zHeight;  /* Width and height in the <img> markup. */
  HtmlImage *pNext;        /* Next image on the list */
  HtmlElement *pList;      /* List of all <IMG> markups that use this 
                           ** same image */
  struct HtmlImageAnim* anims; /* For animated gifs, points to next image */
  int cur, num;	           /* Current and number of animated gif */
  struct HtmlExtensions *exts;
};

/* Each <img> markup is represented by an instance of the 
** following structure.
**
** If pImage==0, then we use the alternative text in zAlt.
*/
struct HtmlImageMarkup {
  HtmlMarkupElement markup;
  Html_u8 align;          /* Alignment.  See IMAGE_ALIGN_ defines below */
  Html_u8 textAscent;     /* Ascent of text font in force at the <IMG> */
  Html_u8 textDescent;    /* Descent of text font in force at the <IMG> */
  Html_u8 redrawNeeded;   /* Need to redraw this image because the image
                          ** content changed. */
  Html_16 h;              /* Actual height of the image */
  Html_16 w;              /* Actual width of the image */
  Html_16 ascent;         /* How far image extends above "y" */
  Html_16 descent;        /* How far image extends below "y" */
  Html_16 x;              /* X coordinate of left edge of the image */
  Html_32 y;              /* Y coordinate of image baseline */
  char *zAlt;             /* Alternative text */
  HtmlImage *pImage;      /* Corresponding HtmlImage structure */
  HtmlElement *pMap;	  /* usemap */
  HtmlElement *pNext;     /* Next markup using the same HtmlImage structure */
};

/*
** Allowed alignments for images.  These represent the allowed arguments
** to the "align=" field of the <IMG> markup.
*/
#define IMAGE_ALIGN_Bottom        0
#define IMAGE_ALIGN_Middle        1
#define IMAGE_ALIGN_Top           2
#define IMAGE_ALIGN_TextTop       3
#define IMAGE_ALIGN_AbsMiddle     4
#define IMAGE_ALIGN_AbsBottom     5
#define IMAGE_ALIGN_Left          6
#define IMAGE_ALIGN_Right         7

/*
** All kinds of form markup, including <INPUT>, <TEXTAREA> and <SELECT>
** are represented by instances of the following structure.
**
** (later...)  We also use this for the <APPLET> markup.  That way,
** the window we create for an <APPLET> responds to the HtmlMapControls()
** and HtmlUnmapControls() function calls.  For an <APPLET>, the
** pForm field is NULL.  (Later still...) <EMBED> works just like
** <APPLET> so it uses this structure too.
*/
struct HtmlInput {
  HtmlMarkupElement markup;
  HtmlElement *pForm;      /* The <FORM> to which this belongs */
  HtmlElement *pNext;      /* Next element in a list of all input elements */
  Tk_Window tkwin;         /* The window that implements this control */
  HtmlWidget *htmlPtr;     /* The whole widget.  Needed by geometry callbacks */
  HtmlElement *pEnd;       /* End tag for <TEXTAREA>, etc. */
  Html_u16 id;             /* Unique id for this element */
  Html_u16 subid;          /* For radio, an id, For select, Option count. */
  Html_32 y;               /* Baseline for this input element */
  Html_u16 x;              /* Left edge */
  Html_u16 w, h;           /* Width and height of this control */
  Html_u8 padLeft;         /* Extra padding on left side of the control */
  Html_u8 align;           /* One of the IMAGE_ALIGN_xxx  types */
  Html_u8 textAscent;      /* Ascent for the current font */
  Html_u8 textDescent;     /* descent for the current font */
  Html_u8 type;            /* What type of input is this? */
  Html_u8 sized;           /* True if this input has been sized already */
  Html_u16 cnt;            /* Used to derive widget name. 0 if no widget */
};

/*
** An input control can be one of the following types.  See the
** comment about <APPLET> on the HtmlInput structure insight into
** INPUT_TYPE_Applet.
*/
#define INPUT_TYPE_Unknown      0
#define INPUT_TYPE_Checkbox     1
#define INPUT_TYPE_File         2
#define INPUT_TYPE_Hidden       3
#define INPUT_TYPE_Image        4
#define INPUT_TYPE_Password     5
#define INPUT_TYPE_Radio        6
#define INPUT_TYPE_Reset        7
#define INPUT_TYPE_Select       8
#define INPUT_TYPE_Submit       9
#define INPUT_TYPE_Text        10
#define INPUT_TYPE_TextArea    11
#define INPUT_TYPE_Applet      12
#define INPUT_TYPE_Button      13
#define INPUT_TYPE_Tktable     14

/*
** There can be multiple <FORM> entries on a single HTML page.
** Each one must be given a unique number for identification purposes,
** and so we can generate unique state variable names for radiobuttons,
** checkbuttons, and entry boxes.
*/
struct HtmlForm {
  HtmlMarkupElement markup;
  Html_u16 id;             /* Unique number assigned to this form */
  unsigned int els;        /* number of elements */
  unsigned int hasctl;	   /* has controls */
  HtmlElement *pFirst;	   /* first form element. */
  HtmlElement *pEnd;       /* Pointer to end tag element */
};

/*
** Information used by a <HR> markup
*/
struct HtmlHr {
  HtmlMarkupElement markup;
  Html_32  y;              /* Baseline for this input element */
  Html_u16 x;              /* Left edge */
  Html_u16 w, h;           /* Width and height of this control */
  Html_u8 is3D;            /* Is it drawn 3D? */
};

/*
** Information used by a <A> markup
*/
struct HtmlAnchor {
  HtmlMarkupElement markup;
  Html_32  y;              /* Top edge for this element */
};

/*
** Information about the <SCRIPT> markup.  The parser treats <SCRIPT>
** specially.  All text between <SCRIPT> and </SCRIPT> is captured and
** is pointed to by the zScript field of this structure.
**
** Note that zScript is not null-terminated.   Instead, zScript just
** points to a spot in the zText field of the HtmlWidget structure.
** The nScript field determines how long the script is.
*/
struct HtmlScript {
  HtmlMarkupElement markup;
  char *zScript;           /* Complete text of this script */
  int nScript;             /* Number of characters of text */
};

/*
** A block is a single unit of display information.  This can be
** one or more text elements, or the border of table, or an
** image, etc.
**
** Blocks are used to improve display speed and to improve the
** speed of linear searchs through the token list.  A single
** block will typically contain enough information to display
** a dozen or more Text and Space elements all with a single
** call to Tk_DrawChars().  The blocks are linked together on
** their own list, so we can search them much faster then elements
** (since there are fewer of them.)
**
** Of course, you can construct pathological HTML that has as
** many Blocks as it has normal tokens.  But you haven't lost
** anything.  Using blocks just speeds things up in the common
** case.
**
** Much of the information needed for display is held in the
** original HtmlElement structures.  "base.pNext" points to the first
** structure in the list which can be used to find the "style"
** "x" and "y".
**
** If n==0, then "base.pNext" might point to a special HtmlElement that
** defines some other kind of drawing, like <LI> or <IMG> or <INPUT>.
*/
struct HtmlBlock {
  HtmlBaseElement base;      /* Superclass.  Must be first */
  char *z;                   /* Space to hold text when n>0 */
  int top, bottom;           /* Extremes of y coordinates */
  Html_u16 left, right;      /* Left and right boundry of this object */
  Html_u16 n;                /* Number of characters in z[] */
  HtmlBlock *pPrev, *pNext;  /* Linked list of all Blocks */
};

/*
** Linux doesn't have a stricmp() function.
*/
#ifndef HAVE_STRICMP
# define stricmp strcasecmp
# define strnicmp strncasecmp
#endif

/*
** A stack of these structures is used to keep track of nested font and
** style changes.  This allows us to easily revert to the previous style
** when we encounter and end-tag like </em> or </h3>.
**
** This stack is used to keep track of the current style while walking
** the list of elements.  After all elements have been assigned a style,
** the information in this stack is no longer used.
*/
struct HtmlStyleStack {
  HtmlStyleStack *pNext;   /* Next style on the stack */
  int type;                /* A markup that ends this style. Ex: Html_EndEM */
  HtmlStyle style;         /* The currently active style. */
};

/*
** A stack of the following structures is used to remember the
** left and right margins within a layout context.
*/
struct HtmlMargin {
  int indent;          /* Size of the current margin */
  int bottom;          /* Y value at which this margin expires */
  int tag;             /* Markup that will cancel this margin */
  HtmlMargin *pNext;   /* Previous margin */
};

/*
** How much space (in pixels) used for a single level of indentation due
** to a <UL> or <DL> or <BLOCKQUOTE>, etc.
*/
#define HTML_INDENT 36

/*
** A layout context holds all state information used by the layout
** engine.
*/
struct HtmlLayoutContext {
  HtmlWidget *htmlPtr;          /* The html widget undergoing layout */
  HtmlElement *pStart;          /* Start of elements to layout */
  HtmlElement *pEnd;            /* Stop when reaching this element */
  int headRoom;                 /* Extra space wanted above this line */
  int top;                      /* Absolute top of drawing area */
  int bottom;                   /* Bottom of previous line */
  int left, right;              /* Left and right extremes of drawing area */
  int pageWidth;                /* Width of the layout field, including
                                ** the margins */
  int maxX, maxY;               /* Maximum X and Y values of paint */
  HtmlMargin *leftMargin;       /* Stack of left margins */
  HtmlMargin *rightMargin;      /* Stack of right margins */
};

/*
** With 28 different fonts and 16 colors, we could in principle have
** as many as 448 different GCs.  But in practice, a single page of
** HTML will typically have much less than this.  So we won't try to
** keep all GCs on hand.  Instead, We'll keep around the most recently
** used GCs and allocate new ones as necessary.
**
** The following structure is used to build a cache of GCs in the
** main widget structure.
*/
#define N_CACHE_GC 32 /* 16 */
struct GcCache {
  GC gc;                /* The graphics context */
  Html_u8 font;         /* Font used for this context */
  Html_u8 color;        /* Color used for this context */
  Html_u8 index;        /* Index used for LRU replacement */
};

/*
** An HtmlIndex is a reference to a particular character within a
** particular Text or Space token.  
*/
struct HtmlIndex {
  HtmlElement *p;      /* The token containing the character */
  int i;               /* Index of the character */
};

#define MAP_RECT 1
#define MAP_CIRCLE 2
#define MAP_POLY 3

struct HtmlMapArea {
  HtmlMarkupElement base;       /* All the base information */
  int type;
  int *coords, num;
};

/*
** The first thing done with input HTML text is to parse it into
** HtmlElements.  All sizing and layout is done using these elements,
** so this is a very important structure.
**
** Elements are designed so that the common ones (Text and Space)
** require as little storage as possible, in order to increase
** the chance of memory cache hits.  (Turns out I didn't do a
** very good job of this.  This widget is a pig for memory.  But
** the speed is good, so I'm not going to change it right now...)
**
** Some elements require more memory than Text and Space (ex: <IMG>).
** An HtmlElement is therefore represented as a union of many other 
** structures all of different sizes.  That way we can have a pointer 
** to a generic element without having to worry about how big that 
** element is.  The ".base.type" field (which is found in all elements) 
** will tell us what type of element we are dealing with.
**
** NOTE:  This trick will only work on compilers that align all elements
** of a union to the lowest memory address in that union.  This is true
** for every C compiler I've ever seen, but isn't guarenteed for ANSI-C.
*/
union HtmlElement {
  HtmlElement *pNext;
  HtmlBaseElement base;
  HtmlTextElement text;
  HtmlSpaceElement space;
  HtmlMarkupElement markup;
  HtmlCell cell;
  HtmlTable table;
  HtmlRef ref;
  HtmlLi li;
  HtmlListStart list;
  HtmlImageMarkup image;
  HtmlInput input;
  HtmlForm form;
  HtmlHr hr;
  HtmlAnchor anchor;
  HtmlScript script;
  HtmlBlock block;
  HtmlMapArea area;
};


/*
** A single instance of the following structure (together with various
** other structures to which this structure points) contains complete 
** state information for a single HTML widget.  The clientData for
** the widget command is a pointer to this structure.
**
** The HTML widget is really a mega-widget.  It consists of two nested
** windows.  The outer window (tkwin) contains the focus highlight border,
** the 3D border and the padding between the border and the text.  All
** text that results from the HTML is drawn into the clipping window
** (clipwin).  The clipping window is a child of the main
** window and has the name "x".  We have to use a clipping window so
** that subwindows required by <FORM> will be clipped properly and won't
** overlap with the borders.
**
** Two primary coordinate systems are used in this widget.
**
**   Window coordinates      In this system, (0,0) is the upper left-hand
**                           corner of the clipping window.  This coordinates
**                           apply only to objects which is visible on screen.
**
**   Virtual canvas          The virtual canvas is an imaginary canvas holding
**                           the entire document.  Typically, part of the
**                           virtual canvas will show thru the clipping 
**                           window to become visible.  The mapping from
**                           window to virtual canvas coordinates is
**                           governed by the "xOffset" and "yOffset" fields
**                           of the widget structure.
**
** 
*/
struct HtmlWidget {
  Tk_Window tkwin;              /* The main window for this widget */
  Tk_Window clipwin;            /* The clipping window in which all text is
                                ** rendered. */
  char *zClipwin;               /* Name of the clipping window. */
  Display *display;             /* The X11 Server that contains tkwin */
  Tcl_Interp *interp;           /* The interpreter in which the widget lives */
  char *zCmdName;               /* Name of the command */
  HtmlElement *pFirst;          /* First HTML token on a list of them all */
  HtmlElement *pLast;           /* Last HTML token on the list */
  int nToken;                   /* Number of HTML tokens on the list.
                                 * Html_Block tokens don't count. */
  HtmlElement *lastSized;       /* Last HTML element that has been sized */
  HtmlElement *nextPlaced;      /* Next HTML element that needs to be 
                                 * positioned on canvas. */
  HtmlBlock *firstBlock;        /* List of all HtmlBlock tokens */
  HtmlBlock *lastBlock;         /* Last HtmlBlock in the list */
  HtmlElement *firstInput;      /* First <INPUT> element */
  HtmlElement *lastInput;       /* Last <INPUT> element */
  int nInput;                   /* The number of <INPUT> elements */
  int nForm;                    /* The number of <FORM> elements */
  int varId;                    /* Used to construct a unique name for a
                                ** global array used by <INPUT> elements */
  int inputIdx;			/* Unique input index */
  int radioIdx;			/* Unique radio index */

  /*
   * Information about the selected region of text
   */
  HtmlIndex selBegin;           /* Start of the selection */
  HtmlIndex selEnd;             /* End of the selection */
  HtmlBlock *pSelStartBlock;    /* Block in which selection starts */
  Html_16 selStartIndex;        /* Index in pSelStartBlock of first selected
                                 * character */
  Html_16 selEndIndex;          /* Index of last selecte char in pSelEndBlock */
  HtmlBlock *pSelEndBlock;      /* Block in which selection ends */

  /*
   * Information about the insertion cursor 
   */
  int insOnTime;                /* How long the cursor states one (millisec) */
  int insOffTime;               /* How long it is off (milliseconds) */
  int insStatus;                /* Is it visible? */
  Tcl_TimerToken insTimer;      /* Timer used to flash the insertion cursor */
  HtmlIndex ins;                /* The insertion cursor position */
  HtmlBlock *pInsBlock;         /* The HtmlBlock containing the cursor */
  int insIndex;                 /* Index in pInsBlock of the cursor */

  /*
   * The following fields hold state information used by
   * the tokenizer.
   */
  char *zText;                  /* Complete text of the unparsed HTML */
  int nText;                    /* Number of characters in zText */
  int nAlloc;                   /* Space allocated for zText */
  int nComplete;                /* How much of zText has actually been
                                 * converted into tokens */
  int iCol;                     /* The column in which zText[nComplete]
                                 * occurs.  Used to resolve tabs in input */
  int iPlaintext;               /* If not zero, this is the token type that
                                 * caused us to go into plaintext mode.  One
                                 * of Html_PLAINTEXT, Html_LISTING or
                                 * Html_XMP */
  HtmlScript *pScript;            /* <SCRIPT> currently being parsed */
  char *zHandler[Html_TypeCount]; /* If not NULL, this is a TCL routine that
                                 * is used to process tokens of the given 
                                 * type */
  /*
   * Information used when parsing HTML input:
   */
  int iSentencePadding;         /* TRUE if we should compensate SPC between
				   sentences. */
  

  /*
   * These fields hold state information used by the HtmlAddStyle routine.
   * We have to store this state information here since HtmlAddStyle
   * operates incrementally.  This information must be carried from
   * one incremental execution to the next.
   */
  HtmlStyleStack *styleStack;   /* The style stack */
  int paraAlignment;            /* Justification associated with <p> */
  int rowAlignment;             /* Justification associated with <tr> */
  int anchorFlags;              /* Style flags associated with <A>...</A> */
  int inDt;                     /* Style flags associated with <DT>...</DT> */
  int inTr;                     /* True if within <tr>..</tr> */
  int inTd;                     /* True if within <td>..</td> or <th>..</th> */
  HtmlElement *anchorStart;     /* Most recent <a href=...> */
  HtmlElement *formStart;       /* Most recent <form> */
  HtmlElement *formElemStart;   /* Most recent <textarea> or <select> */
  HtmlElement *formElemLast;    /* Most recent <input> <textarea> or <select> */
  HtmlElement *innerList;       /* The inner most <OL> or <UL> */
  HtmlElement *LOendPtr;        /* How far HtmlAddStyle has gone to. */
  HtmlElement *LOformStart;     /* For HtmlAddStyle. */

  /*
   * These fields are used to hold the state of the layout engine.
   * Because the layout is incremental, this state must be held for
   * the life of the widget.
   */
  HtmlLayoutContext layoutContext;

  /*
   * Information used when displaying the widget:
   */
  Tk_3DBorder border;		/* Background color */
  int borderWidth;		/* Width of the border. */
  int topmargin, leftmargin, marginwidth, marginheight;
  int relief;			/* 3-D effect: TK_RELIEF_RAISED, etc. */
  int highlightWidth;		/* Width in pixels of highlight to draw
				 * around widget when it has the focus.
				 * <= 0 means don't draw a highlight. */
  XColor *highlightBgColorPtr;  /* Color for drawing traversal highlight
				 * area when highlight is off. */
  XColor *highlightColorPtr;	/* Color for drawing traversal highlight. */
  int inset;			/* Total width of highlight and 3-D border */
  Tk_Font aFont[N_FONT];	/* Information about all screen fonts */
  char fontValid[(N_FONT+7)/8]; /* If bit N%8 of work N/8 of this field is 0
                                 * if aFont[N] needs to be reallocated before
                                 * being used. */
  XColor *apColor[N_COLOR];     /* Information about all colors */
  long long colorUsed;          /* bit N is 1 if color N is in use.  Only
                                ** applies to colors that aren't predefined */
  int iDark[N_COLOR];           /* Dark 3D shadow of color K is iDark[K] */
  int iLight[N_COLOR];          /* Light 3D shadow of color K is iLight[K] */
  XColor *fgColor;              /* Color of normal text. apColor[0] */
  XColor *newLinkColor;         /* Color of unvisitied links. apColor[1] */
  XColor *oldLinkColor;         /* Color of visitied links. apColor[2] */
  XColor *selectionColor;       /* Background color for selections */
  GcCache aGcCache[N_CACHE_GC]; /* A cache of GCs for general use */
  int lastGC;                   /* Index of recently used GC */
  HtmlImage *imageList;         /* A list of all images */
  Tk_Image bgimage;             /* Background image */
  int width, height;		/* User-requested size of the usable drawing
                                 * area, in pixels.   Borders and padding
                                 * make the actual window a little larger */
  int realWidth, realHeight;    /* The actual physical size of tkwin as
                                 * reported in the most recent ConfigureNotify
                                 * event. */
  int padx, pady;               /* Separation between the edge of the window
                                 * and rendered HTML.  */
  int formPadding;		/* Amount to pad form elements by */
  int overrideFonts;            /* TRUE if we should override fonts */
  int overrideColors;           /* TRUE if we should override colors */
  int underlineLinks;           /* TRUE if we should underline hyperlinks */
  int HasScript;		/* TRUE if we can do scripts for this page */
  int HasFrames;		/* TRUE if we can do frames for this page */
  int HasTktables;		/* TRUE if we can do tktables for this page */
  int AddEndTags;		/* TRUE if we add /LI etc. */
  int TableBorderMin;		/* Force tables to have min border size. */
  int varind;			/* Index suffix for unique global var name. */

  /* Information about the selection
  */
  int exportSelection;          /* True if the selection is automatically
                                 * exported to the clipboard */

  /* Callback commands.  The HTML parser will invoke callbacks from time
  ** to time to find out information it needs to complete formatting of
  ** the document.  The following fields define the callback commands.
  */
  char *zIsVisited;             /* Command to tell if a hyperlink has already
                                ** been visited */
  char *zGetImage;              /* Command to get an image from a URL */
  char *zGetBGImage;            /* Command to get an BG image from a URL */
  char *zFrameCommand;          /* Command for handling <frameset> markup */
  char *zAppletCommand;         /* Command to process applets */
  char *zResolverCommand;       /* Command to resolve URIs */
  char *zFormCommand;           /* When user presses Submit */
  char *zHyperlinkCommand;      /* Invoked when a hyperlink is clicked */
  char *zFontCommand;           /* Invoked to find font names */
  char *zScriptCommand;         /* Invoked for each <SCRIPT> markup */
  char *zImgIdxCommand;         /* Command to get image frame (animations) */

   /*
    * Miscellaneous information:
    */
  int tableRelief;              /* 3d effects on <TABLE> */
  int ruleRelief;               /* 3d effects on <HR> */
  char *zBase;                  /* The base URI */
  char *zBaseHref;              /* zBase as modified by <BASE HREF=..> markup */
  Tk_Cursor cursor;		/* Current cursor for window, or None. */
  char *takeFocus;		/* Value of -takefocus option;  not used in
				 * the C code, but used by keyboard traversal
				 * scripts.  Malloc'ed, but may be NULL. */
  char *yScrollCmd;		/* Command prefix for communicating with
				 * vertical scrollbar.  NULL means no command
				 * to issue.  Malloc'ed. */
  char *xScrollCmd;		/* Command prefix for communicating with
				 * horizontal scrollbar.  NULL means no command
				 * to issue.  Malloc'ed. */
  int xOffset, yOffset;         /* Current scroll position.  These form the
                                 * coordinate in the virtual canvas that
                                 * corresponds to (0,0) on the physical screen
                                 * in window tkwin */
  int maxX, maxY;               /* Maximum extent of any "paint" that appears
                                 * on the virtual canvas.  Used to compute 
                                 * scrollbar positions. */
  int dirtyLeft, dirtyTop;      /* Top left corner of region to redraw.  These
                                 * are physical screen coordinates relative to
                                 * clipwin, not tkwin. */
  int dirtyRight, dirtyBottom;  /* Bottom right corner of region to redraw */
  int locked;                   /* Number of locks on this structure. Don't
                                ** delete until it reaches zero. */
  int flags;			/* Various flags;  see below for
				 * definitions. */
  int idind;
  int inParse;			/* Prevent update if parsing. */
  char *zGoto;			/* Label to goto right after layout. */
  int TclHtml;			/* Set to 1 if is not using Tk */
  Tcl_HashTable tokenHash;	/* To support user definable tokens */
  int tokenCnt;			/* Number of tokens, incl user defined */
  int FontAdjust;		/* Add this quantity to each font size */
  char *FontFamily;		/* Default font family to use. */
  HtmlExtensions *exts;		/* Pointer to user extension data */

  HtmlTree *pTree;              /* Document tree */
};

struct HtmlUserTag {
  char *zHandler;
  HtmlTokenMap tokenMap;
};

/*
 * Flag bits "flags" field of the Html widget:
 *
 * REDRAW_PENDING         A DoWhenIdle handler has already been queued to 
 *                        call HtmlRedrawCallback() function.
 *
 * GOT_FOCUS              This widget currently has input focus.
 *
 * HSCROLL                Horizontal scrollbar position needs to be
 *                        recomputed.
 *
 * VSCROLL                Vertical scrollbar position needs to be
 *                        recomputed.
 *
 * RELAYOUT               We need to reposition every element on the 
 *                        virtual canvas.  (This happens, for example,
 *                        when the size of the widget changes and we
 *                        need to recompute the line breaks.)
 *
 * RESIZE_ELEMENTS        We need to recompute the size of every element.
 *                        This happens, for example, when the fonts
 *                        change.
 *
 * REDRAW_FOCUS           We need to repaint the focus highlight border.
 *
 * REDRAW_TEXT            Everything in the clipping window needs to be redrawn.
 *
 * REDRAW_BORDER          Everything outside the clipping window needs
 *                        to be redrawn.
 *
 * RESIZE_CLIPWIN         The size and position of the clipping window 
 *                        needs to be adjusted using Tk_MoveResizeWindow().
 *
 * STYLER_RUNNING         There is a call to HtmlAddStyle() in process.
 *                        Used to prevent a recursive call to HtmlAddStyle().
 *
 * INSERT_FLASHING        True if there is a timer callback pending that will
 *                        toggle the state of the insertion cursor.
 *
 * REDRAW_IMAGES          One or more HtmlImageMarkup structures have
 *                        their redrawNeeded flag set.
 */

#define REDRAW_PENDING	     0x000001
#define GOT_FOCUS            0x000002
#define HSCROLL              0x000004
#define VSCROLL              0x000008
#define RELAYOUT             0x000010
#define RESIZE_ELEMENTS      0x000020
#define REDRAW_FOCUS         0x000040
#define REDRAW_TEXT          0x000080
#define REDRAW_BORDER        0x000100
#define EXTEND_LAYOUT        0x000200
#define RESIZE_CLIPWIN       0x000400
#define STYLER_RUNNING       0x000800
#define INSERT_FLASHING      0x001000
#define REDRAW_IMAGES        0x002000
#define ANIMATE_IMAGES       0x004000

/*
** Macros to set, clear or test bits of the "flags" field.
*/
#define HtmlHasFlag(A,F)      (((A)->flags&(F))==(F))
#define HtmlHasAnyFlag(A,F)   (((A)->flags&(F))!=0)
#define HtmlSetFlag(A,F)      ((A)->flags|=(F))
#define HtmlClearFlag(A,F)    ((A)->flags&=~(F))

/*
** No coordinate is every as big as this number
*/
#define LARGE_NUMBER 100000000

/*
** Default values for configuration options
*/
#define DEF_HTML_BG_COLOR             DEF_FRAME_BG_COLOR
#define DEF_HTML_BG_MONO              DEF_FRAME_BG_MONO
#define DEF_HTML_BORDER_WIDTH         "2"
#define DEF_HTML_CALLBACK             ""
#define DEF_HTML_CURSOR               DEF_FRAME_CURSOR
#define DEF_HTML_EXPORT_SEL           "yes"
#define DEF_HTML_FG                   DEF_BUTTON_FG
#define DEF_HTML_HEIGHT               "400"
#define DEF_HTML_HIGHLIGHT_BG         DEF_BUTTON_HIGHLIGHT_BG
#define DEF_HTML_HIGHLIGHT            DEF_BUTTON_HIGHLIGHT
#define DEF_HTML_HIGHLIGHT_WIDTH      "0"
#define DEF_HTML_INSERT_OFF_TIME      "300"
#define DEF_HTML_INSERT_ON_TIME       "600"
#define DEF_HTML_PADX                 "5"
#define DEF_HTML_PADY                 "5"
#define DEF_HTML_RELIEF               "raised"
#define DEF_HTML_SCROLL_COMMAND       ""
#define DEF_HTML_SELECTION_COLOR      "skyblue"
#define DEF_HTML_TAKE_FOCUS           "0"
#define DEF_HTML_UNVISITED            "blue1"
#define DEF_HTML_VISITED              "blue3"
#define DEF_HTML_WIDTH                "600"

#ifdef NAVIGATOR_TABLES

#define DEF_HTML_TABLE_BORDER             "0"
#define DEF_HTML_TABLE_CELLPADDING        "2"
#define DEF_HTML_TABLE_CELLSPACING        "5"
#define DEF_HTML_TABLE_BORDER_LIGHT_COLOR "gray80"
#define DEF_HTML_TABLE_BORDER_DARK_COLOR  "gray40"

#endif /* NAVIGATOR_TABLES */

/* htmltcl.c */
EXTERN Tcl_ObjCmdProc HtmlWidgetObjCommand;

/* htmlcmd.c */
EXTERN Tcl_ObjCmdProc HtmlCgetObjCmd;
EXTERN Tcl_ObjCmdProc HtmlParseCmd;
EXTERN Tcl_ObjCmdProc HtmlGetCmd;
EXTERN Tcl_ObjCmdProc HtmlGetCmd;
EXTERN Tcl_ObjCmdProc HtmlConfigCmd;
EXTERN Tcl_ObjCmdProc HtmlWidgetObjCommand;
EXTERN Tcl_ObjCmdProc HtmlObjCommand;
EXTERN Tcl_ObjCmdProc HtmlHrefCmd;

EXTERN Tcl_CmdProc HtmlTextHtmlCmd;
EXTERN Tcl_CmdProc HtmlClearCmd;
EXTERN Tcl_CmdProc HtmlResolveCmd;
EXTERN Tcl_CmdProc HtmlNamesCmd;
EXTERN Tcl_CmdProc HtmlLayoutCmd;
EXTERN Tcl_CmdProc HtmlIndexCmd;
EXTERN Tcl_CmdProc HtmlInsertCmd;
EXTERN Tcl_CmdProc HtmlTextAsciiCmd;
EXTERN Tcl_CmdProc HtmlTextInsertCmd;
EXTERN Tcl_CmdProc HtmlTextDeleteCmd;
EXTERN Tcl_CmdProc HtmlTextFindCmd;
EXTERN Tcl_CmdProc HtmlCmd;
EXTERN Tcl_CmdProc HtmlTextInsertCmd;
EXTERN Tcl_CmdProc HtmlTextOffsetCmd;
EXTERN Tcl_CmdProc HtmlTextTable;
EXTERN Tcl_CmdProc HtmlTokenDeleteCmd;
EXTERN Tcl_CmdProc HtmlTokenDefineCmd;
EXTERN Tcl_CmdProc HtmlTokenFindCmd;
EXTERN Tcl_CmdProc HtmlTokenGetCmd;
EXTERN Tcl_CmdProc HtmlTokenHandlerCmd;
EXTERN Tcl_CmdProc HtmlTokenInsertCmd;
EXTERN Tcl_CmdProc HtmlTokenListCmd;
EXTERN Tcl_CmdProc HtmlTokenMarkupCmd;
EXTERN Tcl_CmdProc HtmlTokenDomCmd;
EXTERN Tcl_CmdProc HtmlTokenGetEnd;
EXTERN Tcl_CmdProc HtmlTokenAttr;
EXTERN Tcl_CmdProc HtmlTokenAttrSearch;
EXTERN Tcl_CmdProc HtmlTokenUnique;
EXTERN Tcl_CmdProc HtmlTokenOnEvents;
EXTERN Tcl_CmdProc HtmlDomCmd;
EXTERN Tcl_CmdProc HtmlIdToDomCmd;
EXTERN Tcl_CmdProc HtmlDomTreeCmd;
EXTERN Tcl_CmdProc HtmlDomName;
EXTERN Tcl_CmdProc HtmlDomRadio;
EXTERN Tcl_CmdProc HtmlDomFormElIndex;
EXTERN Tcl_CmdProc HtmlDomName2Index;
EXTERN Tcl_CmdProc HtmlDomRadio2Index;
EXTERN Tcl_CmdProc HtmlSelectionSetCmd;
EXTERN Tcl_CmdProc HtmlXviewCmd;
EXTERN Tcl_CmdProc HtmlYviewCmd;
EXTERN Tcl_CmdProc HtmlImageBgCmd;
EXTERN Tcl_CmdProc HtmlPostscriptCmd;
EXTERN Tcl_CmdProc HtmlAttrOverCmd;
EXTERN Tcl_CmdProc HtmlOverCmd;
EXTERN Tcl_CmdProc HtmlImageAtCmd;
EXTERN Tcl_CmdProc HtmlImageSetCmd;
EXTERN Tcl_CmdProc HtmlImageUpdateCmd;
EXTERN Tcl_CmdProc HtmlOnScreen;
EXTERN Tcl_CmdProc HtmlFormInfo;
EXTERN Tcl_CmdProc HtmlCoordsCmd;
EXTERN Tcl_CmdProc HtmlRefreshCmd;
EXTERN Tcl_CmdProc HtmlBP;
EXTERN Tcl_CmdProc HtmlSizeWindow;
EXTERN Tcl_CmdProc HtmlDebugDumpCmd;
EXTERN Tcl_CmdProc HtmlDebugTestPtCmd;
EXTERN Tcl_CmdProc HtmlSelectionClearCmd;
EXTERN Tcl_CmdProc HtmlImageAddCmd;
EXTERN Tcl_CmdProc HtmlImagesListCmd;

EXTERN int Tclhtml_Init(Tcl_Interp *interp);

/* htmlparse.c */
EXTERN HtmlTokenMap* HtmlGetMarkupMap _ANSI_ARGS_((HtmlWidget *htmlPtr, int n));
HtmlTokenMap *HtmlHashLookup(HtmlWidget *htmlPtr, CONST char *zType);
void HtmlAppendArglist(Tcl_DString *str, HtmlElement *pElem);
char *HtmlGetTokenName(HtmlWidget *htmlPtr, HtmlElement *p);

/* htmltcl.c */
EXTERN int (*HtmlFetchSelectionPtr)(ClientData , int, char *, int );
EXTERN int (*htmlReformatCmdPtr)(Tcl_Interp *interp, char *str, char *dtype);
HtmlElement *HtmlGetMap(HtmlWidget *htmlPtr, char *name);

/* htmlimage.c */
EXTERN int tkhtmlexiting;
HtmlImage *HtmlGetImage(HtmlWidget *htmlPtr, HtmlElement *p);

/* htmlwidget.c */
Tk_ConfigSpec *HtmlConfigSpec(void);
char *HtmlGetHref(HtmlWidget *htmlPtr, int x, int y, char **target);
Tk_Font HtmlGetFont(HtmlWidget *htmlPtr, int iFont);
GC HtmlGetGC(HtmlWidget *htmlPtr, int color, int font);
void HtmlScheduleRedraw(HtmlWidget *htmlPtr);

/* htmlsizer.c */
char *HtmlMarkupArg(HtmlElement *p, const char *tag, char *zDefault);
void HtmlTableBgImage(HtmlWidget *htmlPtr, HtmlElement *p);

/* htmlurl.c */
char *HtmlResolveUri(HtmlWidget *htmlPtr, char *zUri);

/* htmlcmd.c */
HtmlElement *HtmlAttrElem(  HtmlWidget *htmlPtr, char *name, CONST char *value);

/* htmldraw.c */
void HtmlDrawImage(HtmlWidget *htmlPtr, HtmlElement *pElem, Drawable drawable, 
                   int drawableLeft, int drawableTop, int drawableRight, 
                   int drawableBottom);

/* htmltable.c */
HtmlElement *HtmlTableLayout(HtmlLayoutContext *pLC, HtmlElement *pTable);

/* htmlexts.c */
HtmlElement *HtmlFindEndNest(HtmlWidget *htmlPtr, HtmlElement *sp, 
                             int en, HtmlElement *lp);
char *Clr2Name(CONST char *str);
void HtmlRemoveElements(HtmlWidget *p, HtmlElement* pElem, HtmlElement* pLast);

/* htmlindex.c */
HtmlElement *HtmlTokenByIndex(HtmlWidget *htmlPtr, int N, int flag);

/* htmltree.c */
EXTERN Tcl_ObjCmdProc HtmlTreeTclize;

/* htmltagdb.c */
Html_u8 HtmlMarkupFlags(int);

#endif /* __HTML_H__ */
