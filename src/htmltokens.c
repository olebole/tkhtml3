/* Automatically generated from token.list.  Do not edit */
#include <tk.h>
#include "htmltokens.h"
#if INTERFACE
struct HtmlTokenMap {
  char *zName;                /* Name of a markup */
  Html_16 type;               /* Markup type code */
  Html_16 extra;              /* Extra space needed above HtmlBaseElement */
  HtmlTokenMap *pCollide;     /* Hash table collision chain */
};
#define Html_Text    1
#define Html_Space   2
#define Html_Unknown 3
#define Html_Block   4
#define HtmlIsMarkup(X) ((X)->base.type>Html_Block)
#define Html_A               5
#define Html_EndA            6
#define Html_ADDRESS         7
#define Html_EndADDRESS      8
#define Html_APPLET          9
#define Html_EndAPPLET       10
#define Html_AREA            11
#define Html_B               12
#define Html_EndB            13
#define Html_BASE            14
#define Html_BASEFONT        15
#define Html_EndBASEFONT     16
#define Html_BGSOUND         17
#define Html_BIG             18
#define Html_EndBIG          19
#define Html_BLOCKQUOTE      20
#define Html_EndBLOCKQUOTE   21
#define Html_BODY            22
#define Html_EndBODY         23
#define Html_BR              24
#define Html_CAPTION         25
#define Html_EndCAPTION      26
#define Html_CENTER          27
#define Html_EndCENTER       28
#define Html_CITE            29
#define Html_EndCITE         30
#define Html_CODE            31
#define Html_EndCODE         32
#define Html_COMMENT         33
#define Html_EndCOMMENT      34
#define Html_DD              35
#define Html_EndDD           36
#define Html_DFN             37
#define Html_EndDFN          38
#define Html_DIR             39
#define Html_EndDIR          40
#define Html_DIV             41
#define Html_EndDIV          42
#define Html_DL              43
#define Html_EndDL           44
#define Html_DT              45
#define Html_EndDT           46
#define Html_EM              47
#define Html_EndEM           48
#define Html_FONT            49
#define Html_EndFONT         50
#define Html_FORM            51
#define Html_EndFORM         52
#define Html_FRAME           53
#define Html_EndFRAME        54
#define Html_FRAMESET        55
#define Html_EndFRAMESET     56
#define Html_H1              57
#define Html_EndH1           58
#define Html_H2              59
#define Html_EndH2           60
#define Html_H3              61
#define Html_EndH3           62
#define Html_H4              63
#define Html_EndH4           64
#define Html_H5              65
#define Html_EndH5           66
#define Html_H6              67
#define Html_EndH6           68
#define Html_HR              69
#define Html_HTML            70
#define Html_EndHTML         71
#define Html_I               72
#define Html_EndI            73
#define Html_IMG             74
#define Html_INPUT           75
#define Html_ISINDEX         76
#define Html_KBD             77
#define Html_EndKBD          78
#define Html_LI              79
#define Html_EndLI           80
#define Html_LINK            81
#define Html_LISTING         82
#define Html_EndLISTING      83
#define Html_MAP             84
#define Html_EndMAP          85
#define Html_MARQUEE         86
#define Html_EndMARQUEE      87
#define Html_MENU            88
#define Html_EndMENU         89
#define Html_META            90
#define Html_NEXTID          91
#define Html_NOBR            92
#define Html_EndNOBR         93
#define Html_NOFRAME         94
#define Html_EndNOFRAME      95
#define Html_OL              96
#define Html_EndOL           97
#define Html_P               98
#define Html_EndP            99
#define Html_PARAM           100
#define Html_EndPARAM        101
#define Html_PLAINTEXT       102
#define Html_PRE             103
#define Html_EndPRE          104
#define Html_S               105
#define Html_EndS            106
#define Html_SAMP            107
#define Html_EndSAMP         108
#define Html_SELECT          109
#define Html_EndSELECT       110
#define Html_SMALL           111
#define Html_EndSMALL        112
#define Html_STRIKE          113
#define Html_EndSTRIKE       114
#define Html_STRONG          115
#define Html_EndSTRONG       116
#define Html_SUB             117
#define Html_EndSUB          118
#define Html_SUP             119
#define Html_EndSUP          120
#define Html_TABLE           121
#define Html_EndTABLE        122
#define Html_TD              123
#define Html_EndTD           124
#define Html_TEXTAREA        125
#define Html_EndTEXTAREA     126
#define Html_TH              127
#define Html_EndTH           128
#define Html_TITLE           129
#define Html_EndTITLE        130
#define Html_TR              131
#define Html_EndTR           132
#define Html_TT              133
#define Html_EndTT           134
#define Html_UL              135
#define Html_EndUL           136
#define Html_VAR             137
#define Html_EndVAR          138
#define Html_WBR             139
#define Html_XMP             140
#define Html_EndXMP          141
#define Html_TypeCount       141
#define HTML_MARKUP_HASH_SIZE 153
#define HTML_MARKUP_COUNT 137
#endif /* INTERFACE */
HtmlTokenMap HtmlMarkupMap[] = {
  { "a",            Html_A,                   sizeof(HtmlAnchor),            },
  { "/a",           Html_EndA,                sizeof(HtmlRef),               },
  { "address",      Html_ADDRESS,             0,                             },
  { "/address",     Html_EndADDRESS,          0,                             },
  { "applet",       Html_APPLET,              sizeof(HtmlInput),             },
  { "/applet",      Html_EndAPPLET,           0,                             },
  { "area",         Html_AREA,                0,                             },
  { "b",            Html_B,                   0,                             },
  { "/b",           Html_EndB,                0,                             },
  { "base",         Html_BASE,                0,                             },
  { "basefont",     Html_BASEFONT,            0,                             },
  { "/basefont",    Html_EndBASEFONT,         0,                             },
  { "bgsound",      Html_BGSOUND,             0,                             },
  { "big",          Html_BIG,                 0,                             },
  { "/big",         Html_EndBIG,              0,                             },
  { "blockquote",   Html_BLOCKQUOTE,          0,                             },
  { "/blockquote",  Html_EndBLOCKQUOTE,       0,                             },
  { "body",         Html_BODY,                0,                             },
  { "/body",        Html_EndBODY,             0,                             },
  { "br",           Html_BR,                  0,                             },
  { "caption",      Html_CAPTION,             0,                             },
  { "/caption",     Html_EndCAPTION,          0,                             },
  { "center",       Html_CENTER,              0,                             },
  { "/center",      Html_EndCENTER,           0,                             },
  { "cite",         Html_CITE,                0,                             },
  { "/cite",        Html_EndCITE,             0,                             },
  { "code",         Html_CODE,                0,                             },
  { "/code",        Html_EndCODE,             0,                             },
  { "comment",      Html_COMMENT,             0,                             },
  { "/comment",     Html_EndCOMMENT,          0,                             },
  { "dd",           Html_DD,                  sizeof(HtmlRef),               },
  { "/dd",          Html_EndDD,               0,                             },
  { "dfn",          Html_DFN,                 0,                             },
  { "/dfn",         Html_EndDFN,              0,                             },
  { "dir",          Html_DIR,                 sizeof(HtmlListStart),         },
  { "/dir",         Html_EndDIR,              sizeof(HtmlRef),               },
  { "div",          Html_DIV,                 0,                             },
  { "/div",         Html_EndDIV,              0,                             },
  { "dl",           Html_DL,                  sizeof(HtmlListStart),         },
  { "/dl",          Html_EndDL,               sizeof(HtmlRef),               },
  { "dt",           Html_DT,                  sizeof(HtmlRef),               },
  { "/dt",          Html_EndDT,               0,                             },
  { "em",           Html_EM,                  0,                             },
  { "/em",          Html_EndEM,               0,                             },
  { "font",         Html_FONT,                0,                             },
  { "/font",        Html_EndFONT,             0,                             },
  { "form",         Html_FORM,                sizeof(HtmlForm),              },
  { "/form",        Html_EndFORM,             sizeof(HtmlRef),               },
  { "frame",        Html_FRAME,               0,                             },
  { "/frame",       Html_EndFRAME,            0,                             },
  { "frameset",     Html_FRAMESET,            0,                             },
  { "/frameset",    Html_EndFRAMESET,         0,                             },
  { "h1",           Html_H1,                  0,                             },
  { "/h1",          Html_EndH1,               0,                             },
  { "h2",           Html_H2,                  0,                             },
  { "/h2",          Html_EndH2,               0,                             },
  { "h3",           Html_H3,                  0,                             },
  { "/h3",          Html_EndH3,               0,                             },
  { "h4",           Html_H4,                  0,                             },
  { "/h4",          Html_EndH4,               0,                             },
  { "h5",           Html_H5,                  0,                             },
  { "/h5",          Html_EndH5,               0,                             },
  { "h6",           Html_H6,                  0,                             },
  { "/h6",          Html_EndH6,               0,                             },
  { "hr",           Html_HR,                  sizeof(HtmlHr),                },
  { "html",         Html_HTML,                0,                             },
  { "/html",        Html_EndHTML,             0,                             },
  { "i",            Html_I,                   0,                             },
  { "/i",           Html_EndI,                0,                             },
  { "img",          Html_IMG,                 sizeof(HtmlImageMarkup),       },
  { "input",        Html_INPUT,               sizeof(HtmlInput),             },
  { "isindex",      Html_ISINDEX,             0,                             },
  { "kbd",          Html_KBD,                 0,                             },
  { "/kbd",         Html_EndKBD,              0,                             },
  { "li",           Html_LI,                  sizeof(HtmlLi),                },
  { "/li",          Html_EndLI,               0,                             },
  { "link",         Html_LINK,                0,                             },
  { "listing",      Html_LISTING,             0,                             },
  { "/listing",     Html_EndLISTING,          0,                             },
  { "map",          Html_MAP,                 0,                             },
  { "/map",         Html_EndMAP,              0,                             },
  { "marquee",      Html_MARQUEE,             0,                             },
  { "/marquee",     Html_EndMARQUEE,          0,                             },
  { "menu",         Html_MENU,                sizeof(HtmlListStart),         },
  { "/menu",        Html_EndMENU,             sizeof(HtmlRef),               },
  { "meta",         Html_META,                0,                             },
  { "nextid",       Html_NEXTID,              0,                             },
  { "nobr",         Html_NOBR,                0,                             },
  { "/nobr",        Html_EndNOBR,             0,                             },
  { "noframe",      Html_NOFRAME,             0,                             },
  { "/noframe",     Html_EndNOFRAME,          0,                             },
  { "ol",           Html_OL,                  sizeof(HtmlListStart),         },
  { "/ol",          Html_EndOL,               sizeof(HtmlRef),               },
  { "p",            Html_P,                   0,                             },
  { "/p",           Html_EndP,                0,                             },
  { "param",        Html_PARAM,               0,                             },
  { "/param",       Html_EndPARAM,            0,                             },
  { "plaintext",    Html_PLAINTEXT,           0,                             },
  { "pre",          Html_PRE,                 0,                             },
  { "/pre",         Html_EndPRE,              0,                             },
  { "s",            Html_S,                   0,                             },
  { "/s",           Html_EndS,                0,                             },
  { "samp",         Html_SAMP,                0,                             },
  { "/samp",        Html_EndSAMP,             0,                             },
  { "select",       Html_SELECT,              sizeof(HtmlInput),             },
  { "/select",      Html_EndSELECT,           sizeof(HtmlRef),               },
  { "small",        Html_SMALL,               0,                             },
  { "/small",       Html_EndSMALL,            0,                             },
  { "strike",       Html_STRIKE,              0,                             },
  { "/strike",      Html_EndSTRIKE,           0,                             },
  { "strong",       Html_STRONG,              0,                             },
  { "/strong",      Html_EndSTRONG,           0,                             },
  { "sub",          Html_SUB,                 0,                             },
  { "/sub",         Html_EndSUB,              0,                             },
  { "sup",          Html_SUP,                 0,                             },
  { "/sup",         Html_EndSUP,              0,                             },
  { "table",        Html_TABLE,               sizeof(HtmlTable),             },
  { "/table",       Html_EndTABLE,            sizeof(HtmlRef),               },
  { "td",           Html_TD,                  sizeof(HtmlCell),              },
  { "/td",          Html_EndTD,               sizeof(HtmlRef),               },
  { "textarea",     Html_TEXTAREA,            sizeof(HtmlInput),             },
  { "/textarea",    Html_EndTEXTAREA,         sizeof(HtmlRef),               },
  { "th",           Html_TH,                  sizeof(HtmlCell),              },
  { "/th",          Html_EndTH,               sizeof(HtmlRef),               },
  { "title",        Html_TITLE,               0,                             },
  { "/title",       Html_EndTITLE,            0,                             },
  { "tr",           Html_TR,                  sizeof(HtmlRef),               },
  { "/tr",          Html_EndTR,               sizeof(HtmlRef),               },
  { "tt",           Html_TT,                  0,                             },
  { "/tt",          Html_EndTT,               0,                             },
  { "ul",           Html_UL,                  sizeof(HtmlListStart),         },
  { "/ul",          Html_EndUL,               sizeof(HtmlRef),               },
  { "var",          Html_VAR,                 0,                             },
  { "/var",         Html_EndVAR,              0,                             },
  { "wbr",          Html_WBR,                 0,                             },
  { "xmp",          Html_XMP,                 0,                             },
  { "/xmp",         Html_EndXMP,              0,                             },
};
