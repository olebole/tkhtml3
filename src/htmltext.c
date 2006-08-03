/*
 *--------------------------------------------------------------------------
 * Copyright (c) 2006 Dan Kennedy.
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
 *     * Neither the name of Eolas Technologies Inc. nor the names of its
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

#include <ctype.h>
#include <assert.h>
#include "html.h"

/****************** Begin Escape Sequence Translator *************/

/*
** The next section of code implements routines used to translate
** the '&' escape sequences of SGML to individual characters.
** Examples:
**
**         &amp;          &
**         &lt;           <
**         &gt;           >
**         &nbsp;         nonbreakable space
*/

/* Each escape sequence is recorded as an instance of the following
** structure
*/
struct sgEsc {
    char *zName;              /* The name of this escape sequence. ex: "amp" */
    char value[8];            /* The value for this sequence. ex: "&" */
    struct sgEsc *pNext;      /* Next sequence with the same hash on zName */
};

/* The following is a table of all escape sequences.  Add new sequences
** by adding entries to this table.
*/
static struct sgEsc esc_sequences[] = {
    {"nbsp", "\xC2\xA0", 0},            /* Unicode code-point 160 */
    {"iexcl", "\xC2\xA1", 0},           /* Unicode code-point 161 */
    {"cent", "\xC2\xA2", 0},            /* Unicode code-point 162 */
    {"pound", "\xC2\xA3", 0},           /* Unicode code-point 163 */
    {"curren", "\xC2\xA4", 0},          /* Unicode code-point 164 */
    {"yen", "\xC2\xA5", 0},             /* Unicode code-point 165 */
    {"brvbar", "\xC2\xA6", 0},          /* Unicode code-point 166 */
    {"sect", "\xC2\xA7", 0},            /* Unicode code-point 167 */
    {"uml", "\xC2\xA8", 0},             /* Unicode code-point 168 */
    {"copy", "\xC2\xA9", 0},            /* Unicode code-point 169 */
    {"ordf", "\xC2\xAA", 0},            /* Unicode code-point 170 */
    {"laquo", "\xC2\xAB", 0},           /* Unicode code-point 171 */
    {"not", "\xC2\xAC", 0},             /* Unicode code-point 172 */
    {"shy", "\xC2\xAD", 0},             /* Unicode code-point 173 */
    {"reg", "\xC2\xAE", 0},             /* Unicode code-point 174 */
    {"macr", "\xC2\xAF", 0},            /* Unicode code-point 175 */
    {"deg", "\xC2\xB0", 0},             /* Unicode code-point 176 */
    {"plusmn", "\xC2\xB1", 0},          /* Unicode code-point 177 */
    {"sup2", "\xC2\xB2", 0},            /* Unicode code-point 178 */
    {"sup3", "\xC2\xB3", 0},            /* Unicode code-point 179 */
    {"acute", "\xC2\xB4", 0},           /* Unicode code-point 180 */
    {"micro", "\xC2\xB5", 0},           /* Unicode code-point 181 */
    {"para", "\xC2\xB6", 0},            /* Unicode code-point 182 */
    {"middot", "\xC2\xB7", 0},          /* Unicode code-point 183 */
    {"cedil", "\xC2\xB8", 0},           /* Unicode code-point 184 */
    {"sup1", "\xC2\xB9", 0},            /* Unicode code-point 185 */
    {"ordm", "\xC2\xBA", 0},            /* Unicode code-point 186 */
    {"raquo", "\xC2\xBB", 0},           /* Unicode code-point 187 */
    {"frac14", "\xC2\xBC", 0},          /* Unicode code-point 188 */
    {"frac12", "\xC2\xBD", 0},          /* Unicode code-point 189 */
    {"frac34", "\xC2\xBE", 0},          /* Unicode code-point 190 */
    {"iquest", "\xC2\xBF", 0},          /* Unicode code-point 191 */
    {"Agrave", "\xC3\x80", 0},          /* Unicode code-point 192 */
    {"Aacute", "\xC3\x81", 0},          /* Unicode code-point 193 */
    {"Acirc", "\xC3\x82", 0},           /* Unicode code-point 194 */
    {"Atilde", "\xC3\x83", 0},          /* Unicode code-point 195 */
    {"Auml", "\xC3\x84", 0},            /* Unicode code-point 196 */
    {"Aring", "\xC3\x85", 0},           /* Unicode code-point 197 */
    {"AElig", "\xC3\x86", 0},           /* Unicode code-point 198 */
    {"Ccedil", "\xC3\x87", 0},          /* Unicode code-point 199 */
    {"Egrave", "\xC3\x88", 0},          /* Unicode code-point 200 */
    {"Eacute", "\xC3\x89", 0},          /* Unicode code-point 201 */
    {"Ecirc", "\xC3\x8A", 0},           /* Unicode code-point 202 */
    {"Euml", "\xC3\x8B", 0},            /* Unicode code-point 203 */
    {"Igrave", "\xC3\x8C", 0},          /* Unicode code-point 204 */
    {"Iacute", "\xC3\x8D", 0},          /* Unicode code-point 205 */
    {"Icirc", "\xC3\x8E", 0},           /* Unicode code-point 206 */
    {"Iuml", "\xC3\x8F", 0},            /* Unicode code-point 207 */
    {"ETH", "\xC3\x90", 0},             /* Unicode code-point 208 */
    {"Ntilde", "\xC3\x91", 0},          /* Unicode code-point 209 */
    {"Ograve", "\xC3\x92", 0},          /* Unicode code-point 210 */
    {"Oacute", "\xC3\x93", 0},          /* Unicode code-point 211 */
    {"Ocirc", "\xC3\x94", 0},           /* Unicode code-point 212 */
    {"Otilde", "\xC3\x95", 0},          /* Unicode code-point 213 */
    {"Ouml", "\xC3\x96", 0},            /* Unicode code-point 214 */
    {"times", "\xC3\x97", 0},           /* Unicode code-point 215 */
    {"Oslash", "\xC3\x98", 0},          /* Unicode code-point 216 */
    {"Ugrave", "\xC3\x99", 0},          /* Unicode code-point 217 */
    {"Uacute", "\xC3\x9A", 0},          /* Unicode code-point 218 */
    {"Ucirc", "\xC3\x9B", 0},           /* Unicode code-point 219 */
    {"Uuml", "\xC3\x9C", 0},            /* Unicode code-point 220 */
    {"Yacute", "\xC3\x9D", 0},          /* Unicode code-point 221 */
    {"THORN", "\xC3\x9E", 0},           /* Unicode code-point 222 */
    {"szlig", "\xC3\x9F", 0},           /* Unicode code-point 223 */
    {"agrave", "\xC3\xA0", 0},          /* Unicode code-point 224 */
    {"aacute", "\xC3\xA1", 0},          /* Unicode code-point 225 */
    {"acirc", "\xC3\xA2", 0},           /* Unicode code-point 226 */
    {"atilde", "\xC3\xA3", 0},          /* Unicode code-point 227 */
    {"auml", "\xC3\xA4", 0},            /* Unicode code-point 228 */
    {"aring", "\xC3\xA5", 0},           /* Unicode code-point 229 */
    {"aelig", "\xC3\xA6", 0},           /* Unicode code-point 230 */
    {"ccedil", "\xC3\xA7", 0},          /* Unicode code-point 231 */
    {"egrave", "\xC3\xA8", 0},          /* Unicode code-point 232 */
    {"eacute", "\xC3\xA9", 0},          /* Unicode code-point 233 */
    {"ecirc", "\xC3\xAA", 0},           /* Unicode code-point 234 */
    {"euml", "\xC3\xAB", 0},            /* Unicode code-point 235 */
    {"igrave", "\xC3\xAC", 0},          /* Unicode code-point 236 */
    {"iacute", "\xC3\xAD", 0},          /* Unicode code-point 237 */
    {"icirc", "\xC3\xAE", 0},           /* Unicode code-point 238 */
    {"iuml", "\xC3\xAF", 0},            /* Unicode code-point 239 */
    {"eth", "\xC3\xB0", 0},             /* Unicode code-point 240 */
    {"ntilde", "\xC3\xB1", 0},          /* Unicode code-point 241 */
    {"ograve", "\xC3\xB2", 0},          /* Unicode code-point 242 */
    {"oacute", "\xC3\xB3", 0},          /* Unicode code-point 243 */
    {"ocirc", "\xC3\xB4", 0},           /* Unicode code-point 244 */
    {"otilde", "\xC3\xB5", 0},          /* Unicode code-point 245 */
    {"ouml", "\xC3\xB6", 0},            /* Unicode code-point 246 */
    {"divide", "\xC3\xB7", 0},          /* Unicode code-point 247 */
    {"oslash", "\xC3\xB8", 0},          /* Unicode code-point 248 */
    {"ugrave", "\xC3\xB9", 0},          /* Unicode code-point 249 */
    {"uacute", "\xC3\xBA", 0},          /* Unicode code-point 250 */
    {"ucirc", "\xC3\xBB", 0},           /* Unicode code-point 251 */
    {"uuml", "\xC3\xBC", 0},            /* Unicode code-point 252 */
    {"yacute", "\xC3\xBD", 0},          /* Unicode code-point 253 */
    {"thorn", "\xC3\xBE", 0},           /* Unicode code-point 254 */
    {"yuml", "\xC3\xBF", 0},            /* Unicode code-point 255 */
    {"quot", "\x22", 0},                /* Unicode code-point 34 */
    {"amp", "\x26", 0},                 /* Unicode code-point 38 */
    {"lt", "\x3C", 0},                  /* Unicode code-point 60 */
    {"gt", "\x3E", 0},                  /* Unicode code-point 62 */
    {"OElig", "\xC5\x92", 0},           /* Unicode code-point 338 */
    {"oelig", "\xC5\x93", 0},           /* Unicode code-point 339 */
    {"Scaron", "\xC5\xA0", 0},          /* Unicode code-point 352 */
    {"scaron", "\xC5\xA1", 0},          /* Unicode code-point 353 */
    {"Yuml", "\xC5\xB8", 0},            /* Unicode code-point 376 */
    {"circ", "\xCB\x86", 0},            /* Unicode code-point 710 */
    {"tilde", "\xCB\x9C", 0},           /* Unicode code-point 732 */
    {"ensp", "\xE2\x80\x82", 0},        /* Unicode code-point 8194 */
    {"emsp", "\xE2\x80\x83", 0},        /* Unicode code-point 8195 */
    {"thinsp", "\xE2\x80\x89", 0},      /* Unicode code-point 8201 */
    {"zwnj", "\xE2\x80\x8C", 0},        /* Unicode code-point 8204 */
    {"zwj", "\xE2\x80\x8D", 0},         /* Unicode code-point 8205 */
    {"lrm", "\xE2\x80\x8E", 0},         /* Unicode code-point 8206 */
    {"rlm", "\xE2\x80\x8F", 0},         /* Unicode code-point 8207 */
    {"ndash", "\xE2\x80\x93", 0},       /* Unicode code-point 8211 */
    {"mdash", "\xE2\x80\x94", 0},       /* Unicode code-point 8212 */
    {"lsquo", "\xE2\x80\x98", 0},       /* Unicode code-point 8216 */
    {"rsquo", "\xE2\x80\x99", 0},       /* Unicode code-point 8217 */
    {"sbquo", "\xE2\x80\x9A", 0},       /* Unicode code-point 8218 */
    {"ldquo", "\xE2\x80\x9C", 0},       /* Unicode code-point 8220 */
    {"rdquo", "\xE2\x80\x9D", 0},       /* Unicode code-point 8221 */
    {"bdquo", "\xE2\x80\x9E", 0},       /* Unicode code-point 8222 */
    {"dagger", "\xE2\x80\xA0", 0},      /* Unicode code-point 8224 */
    {"Dagger", "\xE2\x80\xA1", 0},      /* Unicode code-point 8225 */
    {"permil", "\xE2\x80\xB0", 0},      /* Unicode code-point 8240 */
    {"lsaquo", "\xE2\x80\xB9", 0},      /* Unicode code-point 8249 */
    {"rsaquo", "\xE2\x80\xBA", 0},      /* Unicode code-point 8250 */
    {"euro", "\xE2\x82\xAC", 0},        /* Unicode code-point 8364 */
    {"fnof", "\xC6\x92", 0},            /* Unicode code-point 402 */
    {"Alpha", "\xCE\x91", 0},           /* Unicode code-point 913 */
    {"Beta", "\xCE\x92", 0},            /* Unicode code-point 914 */
    {"Gamma", "\xCE\x93", 0},           /* Unicode code-point 915 */
    {"Delta", "\xCE\x94", 0},           /* Unicode code-point 916 */
    {"Epsilon", "\xCE\x95", 0},         /* Unicode code-point 917 */
    {"Zeta", "\xCE\x96", 0},            /* Unicode code-point 918 */
    {"Eta", "\xCE\x97", 0},             /* Unicode code-point 919 */
    {"Theta", "\xCE\x98", 0},           /* Unicode code-point 920 */
    {"Iota", "\xCE\x99", 0},            /* Unicode code-point 921 */
    {"Kappa", "\xCE\x9A", 0},           /* Unicode code-point 922 */
    {"Lambda", "\xCE\x9B", 0},          /* Unicode code-point 923 */
    {"Mu", "\xCE\x9C", 0},              /* Unicode code-point 924 */
    {"Nu", "\xCE\x9D", 0},              /* Unicode code-point 925 */
    {"Xi", "\xCE\x9E", 0},              /* Unicode code-point 926 */
    {"Omicron", "\xCE\x9F", 0},         /* Unicode code-point 927 */
    {"Pi", "\xCE\xA0", 0},              /* Unicode code-point 928 */
    {"Rho", "\xCE\xA1", 0},             /* Unicode code-point 929 */
    {"Sigma", "\xCE\xA3", 0},           /* Unicode code-point 931 */
    {"Tau", "\xCE\xA4", 0},             /* Unicode code-point 932 */
    {"Upsilon", "\xCE\xA5", 0},         /* Unicode code-point 933 */
    {"Phi", "\xCE\xA6", 0},             /* Unicode code-point 934 */
    {"Chi", "\xCE\xA7", 0},             /* Unicode code-point 935 */
    {"Psi", "\xCE\xA8", 0},             /* Unicode code-point 936 */
    {"Omega", "\xCE\xA9", 0},           /* Unicode code-point 937 */
    {"alpha", "\xCE\xB1", 0},           /* Unicode code-point 945 */
    {"beta", "\xCE\xB2", 0},            /* Unicode code-point 946 */
    {"gamma", "\xCE\xB3", 0},           /* Unicode code-point 947 */
    {"delta", "\xCE\xB4", 0},           /* Unicode code-point 948 */
    {"epsilon", "\xCE\xB5", 0},         /* Unicode code-point 949 */
    {"zeta", "\xCE\xB6", 0},            /* Unicode code-point 950 */
    {"eta", "\xCE\xB7", 0},             /* Unicode code-point 951 */
    {"theta", "\xCE\xB8", 0},           /* Unicode code-point 952 */
    {"iota", "\xCE\xB9", 0},            /* Unicode code-point 953 */
    {"kappa", "\xCE\xBA", 0},           /* Unicode code-point 954 */
    {"lambda", "\xCE\xBB", 0},          /* Unicode code-point 955 */
    {"mu", "\xCE\xBC", 0},              /* Unicode code-point 956 */
    {"nu", "\xCE\xBD", 0},              /* Unicode code-point 957 */
    {"xi", "\xCE\xBE", 0},              /* Unicode code-point 958 */
    {"omicron", "\xCE\xBF", 0},         /* Unicode code-point 959 */
    {"pi", "\xCF\x80", 0},              /* Unicode code-point 960 */
    {"rho", "\xCF\x81", 0},             /* Unicode code-point 961 */
    {"sigmaf", "\xCF\x82", 0},          /* Unicode code-point 962 */
    {"sigma", "\xCF\x83", 0},           /* Unicode code-point 963 */
    {"tau", "\xCF\x84", 0},             /* Unicode code-point 964 */
    {"upsilon", "\xCF\x85", 0},         /* Unicode code-point 965 */
    {"phi", "\xCF\x86", 0},             /* Unicode code-point 966 */
    {"chi", "\xCF\x87", 0},             /* Unicode code-point 967 */
    {"psi", "\xCF\x88", 0},             /* Unicode code-point 968 */
    {"omega", "\xCF\x89", 0},           /* Unicode code-point 969 */
    {"thetasym", "\xCF\x91", 0},        /* Unicode code-point 977 */
    {"upsih", "\xCF\x92", 0},           /* Unicode code-point 978 */
    {"piv", "\xCF\x96", 0},             /* Unicode code-point 982 */
    {"bull", "\xE2\x80\xA2", 0},        /* Unicode code-point 8226 */
    {"hellip", "\xE2\x80\xA6", 0},      /* Unicode code-point 8230 */
    {"prime", "\xE2\x80\xB2", 0},       /* Unicode code-point 8242 */
    {"Prime", "\xE2\x80\xB3", 0},       /* Unicode code-point 8243 */
    {"oline", "\xE2\x80\xBE", 0},       /* Unicode code-point 8254 */
    {"frasl", "\xE2\x81\x84", 0},       /* Unicode code-point 8260 */
    {"weierp", "\xE2\x84\x98", 0},      /* Unicode code-point 8472 */
    {"image", "\xE2\x84\x91", 0},       /* Unicode code-point 8465 */
    {"real", "\xE2\x84\x9C", 0},        /* Unicode code-point 8476 */
    {"trade", "\xE2\x84\xA2", 0},       /* Unicode code-point 8482 */
    {"alefsym", "\xE2\x84\xB5", 0},     /* Unicode code-point 8501 */
    {"larr", "\xE2\x86\x90", 0},        /* Unicode code-point 8592 */
    {"uarr", "\xE2\x86\x91", 0},        /* Unicode code-point 8593 */
    {"rarr", "\xE2\x86\x92", 0},        /* Unicode code-point 8594 */
    {"darr", "\xE2\x86\x93", 0},        /* Unicode code-point 8595 */
    {"harr", "\xE2\x86\x94", 0},        /* Unicode code-point 8596 */
    {"crarr", "\xE2\x86\xB5", 0},       /* Unicode code-point 8629 */
    {"lArr", "\xE2\x87\x90", 0},        /* Unicode code-point 8656 */
    {"uArr", "\xE2\x87\x91", 0},        /* Unicode code-point 8657 */
    {"rArr", "\xE2\x87\x92", 0},        /* Unicode code-point 8658 */
    {"dArr", "\xE2\x87\x93", 0},        /* Unicode code-point 8659 */
    {"hArr", "\xE2\x87\x94", 0},        /* Unicode code-point 8660 */
    {"forall", "\xE2\x88\x80", 0},      /* Unicode code-point 8704 */
    {"part", "\xE2\x88\x82", 0},        /* Unicode code-point 8706 */
    {"exist", "\xE2\x88\x83", 0},       /* Unicode code-point 8707 */
    {"empty", "\xE2\x88\x85", 0},       /* Unicode code-point 8709 */
    {"nabla", "\xE2\x88\x87", 0},       /* Unicode code-point 8711 */
    {"isin", "\xE2\x88\x88", 0},        /* Unicode code-point 8712 */
    {"notin", "\xE2\x88\x89", 0},       /* Unicode code-point 8713 */
    {"ni", "\xE2\x88\x8B", 0},          /* Unicode code-point 8715 */
    {"prod", "\xE2\x88\x8F", 0},        /* Unicode code-point 8719 */
    {"sum", "\xE2\x88\x91", 0},         /* Unicode code-point 8721 */
    {"minus", "\xE2\x88\x92", 0},       /* Unicode code-point 8722 */
    {"lowast", "\xE2\x88\x97", 0},      /* Unicode code-point 8727 */
    {"radic", "\xE2\x88\x9A", 0},       /* Unicode code-point 8730 */
    {"prop", "\xE2\x88\x9D", 0},        /* Unicode code-point 8733 */
    {"infin", "\xE2\x88\x9E", 0},       /* Unicode code-point 8734 */
    {"ang", "\xE2\x88\xA0", 0},         /* Unicode code-point 8736 */
    {"and", "\xE2\x88\xA7", 0},         /* Unicode code-point 8743 */
    {"or", "\xE2\x88\xA8", 0},          /* Unicode code-point 8744 */
    {"cap", "\xE2\x88\xA9", 0},         /* Unicode code-point 8745 */
    {"cup", "\xE2\x88\xAA", 0},         /* Unicode code-point 8746 */
    {"int", "\xE2\x88\xAB", 0},         /* Unicode code-point 8747 */
    {"there4", "\xE2\x88\xB4", 0},      /* Unicode code-point 8756 */
    {"sim", "\xE2\x88\xBC", 0},         /* Unicode code-point 8764 */
    {"cong", "\xE2\x89\x85", 0},        /* Unicode code-point 8773 */
    {"asymp", "\xE2\x89\x88", 0},       /* Unicode code-point 8776 */
    {"ne", "\xE2\x89\xA0", 0},          /* Unicode code-point 8800 */
    {"equiv", "\xE2\x89\xA1", 0},       /* Unicode code-point 8801 */
    {"le", "\xE2\x89\xA4", 0},          /* Unicode code-point 8804 */
    {"ge", "\xE2\x89\xA5", 0},          /* Unicode code-point 8805 */
    {"sub", "\xE2\x8A\x82", 0},         /* Unicode code-point 8834 */
    {"sup", "\xE2\x8A\x83", 0},         /* Unicode code-point 8835 */
    {"nsub", "\xE2\x8A\x84", 0},        /* Unicode code-point 8836 */
    {"sube", "\xE2\x8A\x86", 0},        /* Unicode code-point 8838 */
    {"supe", "\xE2\x8A\x87", 0},        /* Unicode code-point 8839 */
    {"oplus", "\xE2\x8A\x95", 0},       /* Unicode code-point 8853 */
    {"otimes", "\xE2\x8A\x97", 0},      /* Unicode code-point 8855 */
    {"perp", "\xE2\x8A\xA5", 0},        /* Unicode code-point 8869 */
    {"sdot", "\xE2\x8B\x85", 0},        /* Unicode code-point 8901 */
    {"lceil", "\xE2\x8C\x88", 0},       /* Unicode code-point 8968 */
    {"rceil", "\xE2\x8C\x89", 0},       /* Unicode code-point 8969 */
    {"lfloor", "\xE2\x8C\x8A", 0},      /* Unicode code-point 8970 */
    {"rfloor", "\xE2\x8C\x8B", 0},      /* Unicode code-point 8971 */
    {"lang", "\xE2\x8C\xA9", 0},        /* Unicode code-point 9001 */
    {"rang", "\xE2\x8C\xAA", 0},        /* Unicode code-point 9002 */
    {"loz", "\xE2\x97\x8A", 0},         /* Unicode code-point 9674 */
    {"spades", "\xE2\x99\xA0", 0},      /* Unicode code-point 9824 */
    {"clubs", "\xE2\x99\xA3", 0},       /* Unicode code-point 9827 */
    {"hearts", "\xE2\x99\xA5", 0},      /* Unicode code-point 9829 */
    {"diams", "\xE2\x99\xA6", 0},       /* Unicode code-point 9830 */
};

/* The size of the handler hash table.  For best results this should
** be a prime number which is about the same size as the number of
** escape sequences known to the system. */
#define ESC_HASH_SIZE (sizeof(esc_sequences)/sizeof(esc_sequences[0])+7)

/* The hash table 
**
** If the name of an escape sequences hashes to the value H, then
** apEscHash[H] will point to a linked list of Esc structures, one of
** which will be the Esc structure for that escape sequence.
*/
static struct sgEsc *apEscHash[ESC_HASH_SIZE];

/* Hash a escape sequence name.  The value returned is an integer
** between 0 and ESC_HASH_SIZE-1, inclusive.
*/
static int
EscHash(zName)
    const char *zName;
{
    int h = 0;                         /* The hash value to be returned */
    char c;                            /* The next character in the name
                                        * being hashed */

    while ((c = *zName) != 0) {
        h = h << 5 ^ h ^ c;
        zName++;
    }
    if (h < 0) {
        h = -h;
    }
    else {
    }
    return h % ESC_HASH_SIZE;
}

#ifdef TEST

/* 
** Compute the longest and average collision chain length for the
** escape sequence hash table
*/
static void
EscHashStats(void)
{
    int i;
    int sum = 0;
    int max = 0;
    int cnt;
    int notempty = 0;
    struct sgEsc *p;

    for (i = 0; i < sizeof(esc_sequences) / sizeof(esc_sequences[0]); i++) {
        cnt = 0;
        p = apEscHash[i];
        if (p)
            notempty++;
        while (p) {
            cnt++;
            p = p->pNext;
        }
        sum += cnt;
        if (cnt > max)
            max = cnt;
    }
    printf("Longest chain=%d  avg=%g  slots=%d  empty=%d (%g%%)\n",
           max, (double) sum / (double) notempty, i, i - notempty,
           100.0 * (i - notempty) / (double) i);
}
#endif

/* Initialize the escape sequence hash table
*/
static void
EscInit()
{
    int i;                             /* For looping thru the list of escape 
                                        * sequences */
    int h;                             /* The hash on a sequence */

    for (i = 0; i < sizeof(esc_sequences) / sizeof(esc_sequences[i]); i++) {

/* #ifdef TCL_UTF_MAX */
#if 0
        {
            int c = esc_sequences[i].value[0];
            Tcl_UniCharToUtf(c, esc_sequences[i].value);
        }
#endif
        h = EscHash(esc_sequences[i].zName);
        esc_sequences[i].pNext = apEscHash[h];
        apEscHash[h] = &esc_sequences[i];
    }
#ifdef TEST
    EscHashStats();
#endif
}

/*
** This table translates the non-standard microsoft characters between
** 0x80 and 0x9f into plain ASCII so that the characters will be visible
** on Unix systems.  Care is taken to translate the characters
** into values less than 0x80, to avoid UTF-8 problems.
*/
#ifndef __WIN32__
static char acMsChar[] = {
    /*
     * 0x80 
     */ 'C',
    /*
     * 0x81 
     */ ' ',
    /*
     * 0x82 
     */ ',',
    /*
     * 0x83 
     */ 'f',
    /*
     * 0x84 
     */ '"',
    /*
     * 0x85 
     */ '.',
    /*
     * 0x86 
     */ '*',
    /*
     * 0x87 
     */ '*',
    /*
     * 0x88 
     */ '^',
    /*
     * 0x89 
     */ '%',
    /*
     * 0x8a 
     */ 'S',
    /*
     * 0x8b 
     */ '<',
    /*
     * 0x8c 
     */ 'O',
    /*
     * 0x8d 
     */ ' ',
    /*
     * 0x8e 
     */ 'Z',
    /*
     * 0x8f 
     */ ' ',
    /*
     * 0x90 
     */ ' ',
    /*
     * 0x91 
     */ '\'',
    /*
     * 0x92 
     */ '\'',
    /*
     * 0x93 
     */ '"',
    /*
     * 0x94 
     */ '"',
    /*
     * 0x95 
     */ '*',
    /*
     * 0x96 
     */ '-',
    /*
     * 0x97 
     */ '-',
    /*
     * 0x98 
     */ '~',
    /*
     * 0x99 
     */ '@',
    /*
     * 0x9a 
     */ 's',
    /*
     * 0x9b 
     */ '>',
    /*
     * 0x9c 
     */ 'o',
    /*
     * 0x9d 
     */ ' ',
    /*
     * 0x9e 
     */ 'z',
    /*
     * 0x9f 
     */ 'Y',
};
#endif

/* Translate escape sequences in the string "z".  "z" is overwritten
** with the translated sequence.
**
** Unrecognized escape sequences are unaltered.
**
** Example:
**
**      input =    "AT&amp;T &gt MCI"
**      output =   "AT&T > MCI"
*/
void
HtmlTranslateEscapes(z)
    char *z;
{
    int from;                          /* Read characters from this position
                                        * in z[] */
    int to;                            /* Write characters into this position 
                                        * in z[] */
    int h;                             /* A hash on the escape sequence */
    struct sgEsc *p;                   /* For looping down the escape
                                        * sequence collision chain */
    static int isInit = 0;             /* True after initialization */

    from = to = 0;
    if (!isInit) {
        EscInit();
        isInit = 1;
    }
    while (z[from]) {
        if (z[from] == '&') {
            if (z[from + 1] == '#') {
                int i = from + 2;
                int v = 0;
                while (isdigit(z[i])) {
                    v = v * 10 + z[i] - '0';
                    i++;
                }
                if (z[i] == ';') {
                    i++;
                }

                /*
                 * On Unix systems, translate the non-standard microsoft **
                 * characters in the range of 0x80 to 0x9f into something **
                 * we can see. 
                 */
#ifndef __WIN32__
                if (v >= 0x80 && v < 0xa0) {
                    v = acMsChar[v & 0x1f];
                }
#endif
                /*
                 * Put the character in the output stream in place of ** the
                 * "&#000;".  How we do this depends on whether or ** not we
                 * are using UTF-8. 
                 */
#ifdef TCL_UTF_MAX
                {
                    int j, n;
                    char value[8];
                    n = Tcl_UniCharToUtf(v, value);
                    for (j = 0; j < n; j++) {
                        z[to++] = value[j];
                    }
                }
#else
                z[to++] = v;
#endif
                from = i;
            }
            else {
                int i = from + 1;
                int c;
                while (z[i] && isalnum(z[i])) {
                    i++;
                }
                c = z[i];
                z[i] = 0;
                h = EscHash(&z[from + 1]);
                p = apEscHash[h];
                while (p && strcmp(p->zName, &z[from + 1]) != 0) {
                    p = p->pNext;
                }
                z[i] = c;
                if (p) {
                    int j;
                    for (j = 0; p->value[j]; j++) {
                        z[to++] = p->value[j];
                    }
                    from = i;
                    if (c == ';') {
                        from++;
                    }
                }
                else {
                    z[to++] = z[from++];
                }
            }

            /*
             * On UNIX systems, look for the non-standard microsoft
             * characters ** between 0x80 and 0x9f and translate them into
             * printable ASCII ** codes.  Separate algorithms are required to 
             * do this for plain ** ascii and for utf-8. 
             */
#ifndef __WIN32__
#ifdef TCL_UTF_MAX
        }
        else if ((z[from] & 0x80) != 0) {
            Tcl_UniChar c;
            int n;
            n = Tcl_UtfToUniChar(&z[from], &c);
            if (c >= 0x80 && c < 0xa0) {
                z[to++] = acMsChar[c & 0x1f];
                from += n;
            }
            else {
                while (n--)
                    z[to++] = z[from++];
            }
#else /* if !defined(TCL_UTF_MAX) */
        }
        else if (((unsigned char) z[from]) >= 0x80
                 && ((unsigned char) z[from]) < 0xa0) {
            z[to++] = acMsChar[z[from++] & 0x1f];
#endif /* TCL_UTF_MAX */
#endif /* __WIN32__ */
        }
        else {
            z[to++] = z[from++];
        }
    }
    z[to] = 0;
}
