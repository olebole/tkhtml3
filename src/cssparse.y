
%name tkhtmlCssParser
%extra_argument {CssParse *pParse}
%include {
#include "cssInt.h"
#include <string.h>
}

/* Token prefix 'CT' stands for Css Token. */
%token_prefix CT_
%token_type {CssToken}

/* Need this value for a trick in the tokenizer used to parse CT_FUNCTION. */
%nonassoc RRP. 

%syntax_error {
    pParse->pStyle->nSyntaxErr++;
}

/* Style sheet consists of a header followed by a body. */
stylesheet ::= ss_header ss_body.

/* Optional whitespace. */
ws ::= .
ws ::= SPACE ws.

/*********************************************************************
** Style sheet header. Contains @charset and @import directives. Both
** of these are ignored for now.
*/
ss_header ::= ws charset_opt imports_opt.

charset_opt ::= CHARSET_SYM ws STRING ws SEMICOLON ws.
charset_opt ::= .

imports_opt ::= IMPORT_SYM ws STRING medium_list_opt SEMICOLON.
imports_opt ::= .

medium_list_opt ::= medium_list.
medium_list_opt ::= .

/*********************************************************************
** Style sheet body. A list of style sheet body items.
*/
/*
ss_body ::= .
ss_body ::= ss_body_item ws ss_body.
*/

ss_body ::= ss_body_item.
ss_body ::= ss_body ws ss_body_item.

ss_body_item ::= media.
ss_body_item ::= ruleset.
ss_body_item ::= font_face. 

/*********************************************************************
** @media {...} block.
*/
media ::= MEDIA_SYM ws medium_list LP ws ruleset_list RP.

medium_list ::= IDENT ws.
medium_list ::= IDENT ws COMMA ws medium_list.

ruleset_list ::= ruleset ws.
ruleset_list ::= ruleset ws ruleset_list.

/*********************************************************************
** @page {...} block. 
*/
page ::= PAGE_SYM ws pseudo_opt LP ws declaration_list RP.

pseudo_opt ::= COLON IDENT ws.
pseudo_opt ::= .

/*********************************************************************
** @font_face {...} block.
*/
font_face ::= FONT_SYM LP declaration_list RP.

/*********************************************************************
** Style sheet rules. e.g. "<selector> { <properties> }"
*/
ruleset ::= selector_list LP ws declaration_list semicolon_opt RP. {
    tkhtmlCssRule(pParse, 1);
}
ruleset ::= page.

selector_list ::= selector.
selector_list ::= selector_list comma ws selector.

comma ::= COMMA. {
    HtmlCssSelectorComma(pParse);
}

declaration_list ::= declaration.
declaration_list ::= declaration_list SEMICOLON ws declaration.

semicolon_opt ::= SEMICOLON ws.
semicolon_opt ::= .

declaration ::= IDENT(X) ws COLON ws expr(E). {
    tkhtmlCssDeclaration(pParse, &X, &E);
}

/*********************************************************************
** Selector syntax. This is in a section of it's own because it's
** complicated.
*/
selector ::= simple_selector ws.
selector ::= simple_selector combinator(X) selector. 

%type combinator {int}
combinator ::= ws PLUS ws. {
    tkhtmlCssSelector(pParse, CSS_SELECTORCHAIN_ADJACENT, 0, 0);
}
combinator ::= ws GT ws. {
    tkhtmlCssSelector(pParse, CSS_SELECTORCHAIN_CHILD, 0, 0);
}
combinator ::= SPACE ws. {
    tkhtmlCssSelector(pParse, CSS_SELECTORCHAIN_DESCENDANT, 0, 0);
}

simple_selector ::= IDENT(X) simple_selector_tail_opt. {
    tkhtmlCssSelector(pParse, CSS_SELECTOR_TYPE, 0, &X);
}
simple_selector ::= STAR simple_selector_tail_opt. {
    tkhtmlCssSelector(pParse, CSS_SELECTOR_UNIVERSAL, 0, 0);
}
simple_selector ::= simple_selector_tail.

simple_selector_tail_opt ::= simple_selector_tail.
simple_selector_tail_opt ::= .

simple_selector_tail ::= HASH IDENT(X) simple_selector_tail_opt. {
    CssToken id = {"id", 2};
    tkhtmlCssSelector(pParse, CSS_SELECTOR_ATTRVALUE, &id, &X);
}
simple_selector_tail ::= DOT IDENT(X) simple_selector_tail_opt. {
    CssToken cls = {"class", 5};
    tkhtmlCssSelector(pParse, CSS_SELECTOR_ATTRVALUE, &cls, &X);
}
simple_selector_tail ::= LSP IDENT(X) RSP simple_selector_tail_opt. {
    tkhtmlCssSelector(pParse, CSS_SELECTOR_ATTR, &X, 0);
}
simple_selector_tail ::= 
    LSP IDENT(X) EQUALS STRING(Y) RSP simple_selector_tail_opt. {
    tkhtmlCssSelector(pParse, CSS_SELECTOR_ATTRVALUE, &X, &Y);
}
simple_selector_tail ::= 
    LSP IDENT(X) TILDE EQUALS STRING(Y) RSP simple_selector_tail_opt. {
    tkhtmlCssSelector(pParse, CSS_SELECTOR_ATTRLISTVALUE, &X, &Y);
}
simple_selector_tail ::= 
    LSP IDENT(X) PIPE EQUALS STRING(Y) RSP simple_selector_tail_opt. {
    tkhtmlCssSelector(pParse, CSS_SELECTOR_ATTRHYPHEN, &X, &Y);
}

/* Todo: Deal with pseudo selectors. This rule makes the parser ignore them. */
simple_selector_tail ::= COLON IDENT(X) simple_selector_tail_opt. {
    tkhtmlCssSelector(pParse, tkhtmlCssPseudo(&X), 0, 0);
}

/*********************************************************************
** Expression syntax. This is very simple, it may need to be extended
** so that the structure of the expression is preserved. At present,
** all stylesheet expressions are stored as strings.
*/
expr(A) ::= term(X) ws.               { A = X; }
expr(A) ::= term(X) operator expr(Y). { A.z = X.z; A.n = (Y.z+Y.n - X.z); }

operator ::= ws COMMA ws.
operator ::= ws SLASH ws.
operator ::= SPACE ws.

term(A) ::= IDENT(X). { A = X; }
term(A) ::= DOT(X) IDENT(Y). { A.z = X.z; A.n = (Y.z+Y.n - X.z); }
term(A) ::= IDENT(X) DOT IDENT(Y). { A.z = X.z; A.n = (Y.z+Y.n - X.z); }
term(A) ::= STRING(X). { A = X; }
term(A) ::= FUNCTION(X). { A = X; }
term(A) ::= HASH(X) IDENT(Y). { A.z = X.z; A.n = (Y.z+Y.n - X.z); }
term(A) ::= PLUS(X) IDENT(Y). { A.z = X.z; A.n = (Y.z+Y.n - X.z); }
term(A) ::= IMPORTANT_SYM(X). { A.z = X.z; A.n = 0; }

