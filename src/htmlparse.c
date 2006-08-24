/*
 *--------------------------------------------------------------------------
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
static char const rcsid[] =
        "@(#) $Id: htmlparse.c,v 1.78 2006/08/24 14:53:02 danielk1977 Exp $";

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include "html.h"

#define ISSPACE(x) isspace((unsigned char)(x))

/*
 *---------------------------------------------------------------------------
 *
 * AppendTextToken --
 *
 *      This is called by the Tokenize() function each time a text or
 *      whitespace token is parsed.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static void
AppendTextToken(pTree, pToken, iOffset)
    HtmlTree *pTree;
    HtmlToken *pToken;
    int iOffset;
{
    if( pTree->isIgnoreNewline ){
        assert(!pTree->pTextFirst);
        pTree->isIgnoreNewline = 0;
        if (pToken->type == Html_Space && pToken->x.newline) {
            HtmlFree(0, pToken);
            return;
        }
    }
    if (!pTree->pTextFirst) {
        assert(!pTree->pTextLast);
        pTree->pTextFirst = pToken;
        pTree->pTextLast = pToken;
        pTree->iTextOffset = iOffset;
    } else {
        assert(pTree->pTextLast);
        pTree->pTextLast->pNextToken = pToken;
        pTree->pTextLast = pToken;
    }
    pToken->pNextToken = 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * AppendToken --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
AppendToken(pTree, pToken, iOffset)
    HtmlTree *pTree;
    HtmlToken *pToken;
    int iOffset;
{
    int isEndToken = 0;

    if (pToken) {
        isEndToken = ((HtmlMarkupFlags(pToken->type)&HTMLTAG_END)?1:0);
    }

    if (pTree->pTextFirst) {
        HtmlToken *pTextFirst = pTree->pTextFirst;
        HtmlToken *pTextLast = pTree->pTextLast;

        /* Ignore any newline character that appears immediately before
         * an end tag.
         */
        if (
            isEndToken && 
            pTextLast->type == Html_Space && 
            pTextLast->x.newline
        ) {
            if( pTextFirst==pTextLast ){
                pTextFirst = 0;
            } else {
                HtmlToken *p = pTextFirst;
                while (p->pNextToken != pTextLast) p = p->pNextToken;
                p->pNextToken = 0;
            }
            HtmlFree(0, pTextLast);
        }

        pTree->pTextLast = 0;
        pTree->pTextFirst = 0;
        if( pTextFirst ){
            HtmlAddToken(pTree, pTextFirst, pTree->iTextOffset);
        }
        pTree->isIgnoreNewline = 0;
    }

    if (pToken) {
        pTree->isIgnoreNewline = isEndToken?0:1;
        pToken->pNextToken = 0;
        HtmlAddToken(pTree, pToken, iOffset);
    }
}

/*
 * The following elements have optional opening and closing tags:
 *
 *     <tbody>
 *     <html>
 *     <head>
 *     <body>
 *
 * These have optional end tags:
 *
 *     <dd>
 *     <dt>
 *     <li>
 *     <option>
 *     <p>
 *
 *     <colgroup>
 *     <td>
 *     <th>
 *     <tr>
 *     <thead>
 *     <tfoot>
 *
 * The following functions:
 *
 *     * HtmlFormContent
 *     * HtmlInlineContent
 *     * HtmlFlowContent
 *     * HtmlColgroupContent
 *     * HtmlTableSectionContent
 *     * HtmlTableRowContent
 *     * HtmlDlContent
 *     * HtmlUlContent
 *     * HtmlPcdataContent
 *
 * Are used to detect implicit close tags in HTML documents.  When a markup
 * tag encountered, one of the above functions is called with the parent
 * node and new markup tag as arguments. Three return values are possible:
 *
 *     TAG_CLOSE
 *     TAG_OK
 *     TAG_PARENT
 *
 * If TAG_CLOSE is returned, then the tag closes the tag that opened the
 * parent node. If TAG_OK is returned, then it does not. If TAG_PARENT is
 * returned, then the same call is made using the parent of pNode.
 */

/*
 *---------------------------------------------------------------------------
 *
 * HtmlFormContent --
 *
 *     "Node content" callback for nodes generated by empty HTML tags. All
 *     tokens close this kind of node.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
HtmlFormContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag == Html_TR || tag == Html_TD || tag == Html_TH) {
        return TAG_CLOSE;
    }
    return TAG_PARENT;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlPcdataContent --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
HtmlPcdataContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag == Html_Space || tag == Html_Text) {
        return TAG_PARENT;
    }
    return TAG_CLOSE;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlDlContent --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
HtmlDlContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag==Html_DD || tag==Html_DT) return TAG_OK;
    if (tag == Html_Text || tag == Html_Space) return TAG_OK;
    return TAG_PARENT;
}


/*
 *---------------------------------------------------------------------------
 *
 * HtmlUlContent --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
HtmlUlContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag==Html_LI || tag==Html_EndLI) return TAG_OK;
    if (tag == Html_Text || tag == Html_Space) return TAG_OK;
    return TAG_PARENT;
}

static int 
HtmlHeadContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag==Html_BODY || tag==Html_FRAMESET) return TAG_CLOSE;
    return TAG_PARENT;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlInlineContent --
 *
 *     "Node content" callback for nodes that can only handle inline
 *     content. i.e. those generated by <p>. Return CLOSE if content is not
 *     inline, else PARENT.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
HtmlInlineContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    Html_u8 flags = HtmlMarkupFlags(tag);
    if (tag == Html_Text || tag == Html_Space) return TAG_OK;

    /* Quirks mode exception: <p> tags can contain <table> */
    if( 
        pTree->options.mode == HTML_MODE_QUIRKS && 
        HtmlNodeTagType(pNode) == Html_P && 
        tag == Html_TABLE 
    ){
        return TAG_OK;
    }

    if (!(flags&HTMLTAG_INLINE)) {
        return TAG_CLOSE;
    }
    return TAG_PARENT;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlAnchorContent --
 *
 *     "Node content" callback for anchor nodes (<a>).
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
HtmlAnchorContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    Html_u8 flags = HtmlMarkupFlags(tag);
    if (tag == Html_Text || tag == Html_Space) return TAG_OK;
    if (!(flags&HTMLTAG_INLINE) || tag == Html_A) {
        return TAG_CLOSE;
    }
    return TAG_PARENT;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlFlowContent --
 *
 *     The SGML specification says that some elements may only contain
 *     %flow items. %flow is either %block or %inline - i.e. only tags for
 *     which the HTMLTAG_INLINE or HTMLTAG_BLOCK flag is set.
 *
 *     We apply this rule to the following elements, which may only contain
 *     %flow and are also allowed implicit close tags - according to HTML
 *     4.01. This is a little scary, it's not clear right now how other
 *     rendering engines handle this.
 *
 *         * <li>
 *         * <td>
 *         * <th>
 *         * <dd>
 *         * <dt>
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#if 0
static int 
HtmlFlowContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    Html_u8 flags = HtmlMarkupFlags(tag);
    if (tag == Html_Text || tag == Html_Space) return TAG_OK;
    if (!(flags&(HTMLTAG_INLINE|HTMLTAG_BLOCK|HTMLTAG_END))) {
        return TAG_CLOSE;
    }
    return TAG_PARENT;
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * HtmlColgroupContent --
 *
 *     Todo! <colgroup> is not supported yet so it doesn't matter so
 *     much... But when we do support it make sure it can be implicitly
 *     closed here.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#if 0
static int 
HtmlColgroupContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    assert(0);
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTableContent --
 *
 *     No tags do an implicit close on <table>. But if there is a stray
 *     </tr> or </td> tag in the table somewhere, it cannot match a <tr> or
 *     <td> above the table node in the document hierachy.
 *
 *     This is specified nowhere I can find, but all the other rendering
 *     engines seem to do it. Unfortunately, this might not be the whole
 *     story...
 *
 *     Also, return TAG_OK for <tr>, <td> and <th> so that they do not
 *     close a like tag above the <table> node.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
HtmlTableContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag == Html_TABLE) return TAG_CLOSE;
    return TAG_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTableSectionContent --
 *
 *     Todo! This will be for managing implicit closes of <tbody>, <tfoot>
 *     and <thead>. But we don't support any of them yet so it isn't really
 *     a big deal.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#if 0
static int 
HtmlTableSectionContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag == Html_Text || tag == Html_Space) return TAG_OK;
    assert(0);
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTableRowContent --
 *
 *     According to the SGML definition of HTML, a <tr> node should contain
 *     nothing but <td> and <th> elements. So perhaps we should return
 *     TAG_CLOSE unless 'tag' is a <td> a <th> or some kind of closing tag.
 *
 *     For now though, just return TAG_CLOSE for another <tr> tag, and
 *     TAG_PARENT otherwise. Todo: Need to check how other browsers handle
 *     this.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
HtmlTableRowContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag == Html_TR) {
        return TAG_CLOSE;
    }
    if (
        tag == Html_FORM || 
        tag == Html_TD || 
        tag == Html_TH || 
        tag == Html_Space
    ) {
        return TAG_OK;
    }
    if (HtmlMarkupFlags(tag) & HTMLTAG_END) {
        return TAG_PARENT;
    }

    return TAG_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTableCellContent --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
HtmlTableCellContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag==Html_TH || tag==Html_TD || tag==Html_TR) return TAG_CLOSE;
    if (!(HtmlMarkupFlags(tag) & HTMLTAG_END)) return TAG_OK;
    return TAG_PARENT;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLiContent --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
HtmlLiContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag==Html_LI || tag==Html_DD || tag==Html_DT) return TAG_CLOSE;
    if (tag == Html_Text || tag == Html_Space) return TAG_OK;
    return TAG_PARENT;
}

/* htmltokens.c is generated from source file tokenlist.txt during the
 * build process. It contains the HtmlMarkupMap constant array, declared as:
 *
 * HtmlTokenMap HtmlMarkupMap[] = {...};
 */
#include "htmltokens.c"

static HtmlTokenMap *HtmlHashLookup(void *htmlPtr, CONST char *zType);

/******************* Begin HTML tokenizer code *******************/

/*
** The following variable becomes TRUE when the markup hash table
** (stored in HtmlMarkupMap[]) is initialized.
*/
static int isInit = 0;

/* The hash table for HTML markup names.
**
** If an HTML markup name hashes to H, then apMap[H] will point to
** a linked list of sgMap structure, one of which will describe the
** the particular markup (if it exists.)
*/
static HtmlTokenMap *apMap[HTML_MARKUP_HASH_SIZE];

/* Hash a markup name
**
** HTML markup is case insensitive, so this function will give the
** same hash regardless of the case of the markup name.
**
** The value returned is an integer between 0 and HTML_MARKUP_HASH_SIZE-1,
** inclusive.
*/
static int
HtmlHash(htmlPtr, zName)
    void *htmlPtr;
    const char *zName;
{
    int h = 0;
    char c;
    while ((c = *zName) != 0) {
        if (isupper(c)) {
            c = tolower(c);
        }
        h = h << 5 ^ h ^ c;
        zName++;
    }
    if (h < 0) {
        h = -h;
    }
    return h % HTML_MARKUP_HASH_SIZE;
}

#ifdef TEST

/* 
** Compute the longest and average collision chain length for the
** markup hash table
*/
static void
HtmlHashStats(void * htmlPtr)
{
    int i;
    int sum = 0;
    int max = 0;
    int cnt;
    int notempty = 0;
    struct sgMap *p;

    for (i = 0; i < HTML_MARKUP_COUNT; i++) {
        cnt = 0;
        p = apMap[i];
        if (p)
            notempty++;
        while (p) {
            cnt++;
            p = p->pCollide;
        }
        sum += cnt;
        if (cnt > max)
            max = cnt;

    }
    printf("longest chain=%d  avg=%g  slots=%d  empty=%d (%g%%)\n",
           max, (double) sum / (double) notempty, i, i - notempty,
           100.0 * (i - notempty) / (double) i);
}
#endif

/* Initialize the escape sequence hash table
*/
static void
HtmlHashInit(htmlPtr, start)
    void *htmlPtr;
    int start;
{
    int i;                             /* For looping thru the list of markup 
                                        * names */
    int h;                             /* The hash on a markup name */

    for (i = start; i < HTML_MARKUP_COUNT; i++) {
        h = HtmlHash(htmlPtr, HtmlMarkupMap[i].zName);
        HtmlMarkupMap[i].pCollide = apMap[h];
        apMap[h] = &HtmlMarkupMap[i];
    }
#ifdef TEST
    HtmlHashStats(htmlPtr);
#endif
}


/*
 *---------------------------------------------------------------------------
 *
 * NextColumn --
 *
 *     Compute the new column index following the given character.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
NextColumn(iCol, c)
    int iCol;
    char c;
{
    switch (c) {
        case '\n':
            return 0;
        case '\t':
            return (iCol | 7) + 1;
        case '\0':
            return iCol;
        default:
            return iCol + 1;
    }
    /*
     * NOT REACHED 
     */
}

/*
** Convert a string to all lower-case letters.
*/
void
ToLower(z)
    char *z;
{
    while (*z) {
        if (isupper(*z))
            *z = tolower(*z);
        z++;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * getScriptHandler --
 *
 *     If there is a script handler for tag type 'tag', return the Tcl_Obj*
 *     containing the script. Otherwise return NULL.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static Tcl_Obj *
getScriptHandler(pTree, tag)
    HtmlTree *pTree;
    int tag;
{
    Tcl_HashEntry *pEntry;
    pEntry = Tcl_FindHashEntry(&pTree->aScriptHandler, (char *)tag);
    if (pEntry) {
        return (Tcl_Obj *)Tcl_GetHashValue(pEntry);
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tokenize --
 *
 *     Process as much of the input HTML as possible. This results in 
 *     zero or more calls to the following functions:
 *
 *         AppendTextToken()
 *         AppendToken()
 *
 * Results:
 *     Return the number of bytes actually processed.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
Tokenize(pTree, isFinal)
    HtmlTree *pTree;             /* The HTML widget doing the parsing */
    int isFinal;
{
    char *z;                     /* The input HTML text */
    int c;                       /* The next character of input */
    int n;                       /* Number of bytes processed so far */
    int iCol;                    /* Local copy of HtmlTree.iCol */
    int i, j;                    /* Loop counters */
    int nByte;                   /* Space allocated for a single HtmlElement */
    int selfClose;               /* True for content free elements. Ex: <br/> */
    int argc;                    /* The number of arguments on a markup */
    HtmlTokenMap *pMap;          /* For searching the markup name hash table */
    char *zBuf;                  /* For handing out buffer space */
# define mxARG 200               /* Max parameters in a single markup */
    char *argv[mxARG];           /* Pointers to each markup argument. */
    int arglen[mxARG];           /* Length of each markup argument */

    int nStartScript = 0;
    Tcl_Obj *pScript = 0;
    HtmlToken *pScriptToken = 0;
    int rc;

    iCol = pTree->iCol;
    n = pTree->nParsed;
    z = Tcl_GetString(pTree->pDocument);

    while ((c = z[n]) != 0) {

        /* TODO: What is the significance of -64 and -128? BOM or something? */
        if ((signed char) c == -64 && (signed char) (z[n + 1]) == -128) {
            n += 2;
            continue;
        }

	/* If pScript is not NULL, then we are parsing a node that tkhtml
	 * treats as a "script". Essentially this means we will pass the
	 * entire text of the node to some user callback for processing and
	 * take no further action. So we just search through the text until
	 * we encounter </script>, </noscript> or whatever closing tag
	 * matches the tag that opened the script node.
         */
        if (pScript) {
            int nEnd, sqcnt;
            char zEnd[64];
            char *zScript;
            int nScript;

            Tcl_Obj *pEval;
            Tcl_Obj *pAttr;   /* List containing attributes of pScriptToken */
            int jj;

            /* Figure out the string we are looking for as a end tag */
            sprintf(zEnd, "</%s>", HtmlMarkupName(pScriptToken->type));
            nEnd = strlen(zEnd);
          
            /* Skip through the input until we find such a string. We
             * respect strings quoted with " and ', so long as they do not
             * include new-lines.
             */
            zScript = &z[n];
            sqcnt = 0;
            for (i = n; z[i]; i++) {
                if (z[i] == '\'' || z[i] == '"')
                    sqcnt++;    /* Skip if odd # quotes */
                else if (z[i] == '\n')
                    sqcnt = 0;
                if (strnicmp(&z[i], zEnd, nEnd)==0 && (sqcnt%2)==0) {
                    nScript = i - n;
                    break;
                }
            }

            if (z[i] == 0) {
                n = nStartScript;
                HtmlFree(0, pScriptToken);
                goto incomplete;
            }

            /* Create the attributes list */
            pAttr = Tcl_NewObj();
            Tcl_IncrRefCount(pAttr);
            for (jj = 2; jj < pScriptToken->count; jj++) {
                Tcl_Obj *pArg = Tcl_NewStringObj(pScriptToken->x.zArgs[jj], -1);
                Tcl_ListObjAppendElement(0, pAttr, pArg);
            }

            /* Execute the script */
            pEval = Tcl_DuplicateObj(pScript);
            Tcl_IncrRefCount(pEval);
            Tcl_ListObjAppendElement(0, pEval, pAttr);
            Tcl_DecrRefCount(pAttr);
            Tcl_ListObjAppendElement(0,pEval,Tcl_NewStringObj(zScript,nScript));
            rc = Tcl_EvalObjEx(pTree->interp, pEval, TCL_EVAL_GLOBAL);
            Tcl_DecrRefCount(pEval);
            n += (nScript+nEnd);
 
            /* If the script executed successfully, append the output to
             * the document text (it will be the next thing tokenized).
             */
            if (rc==TCL_OK) {
                Tcl_Obj *pResult;
                Tcl_Obj *pTail;
                Tcl_Obj *pHead;

                pTail = Tcl_NewStringObj(&z[n], -1);
                pResult = Tcl_GetObjResult(pTree->interp);
                pHead = Tcl_NewStringObj(z, n);
                Tcl_IncrRefCount(pTail);
                Tcl_IncrRefCount(pResult);
                Tcl_IncrRefCount(pHead);

                Tcl_AppendObjToObj(pHead, pResult);
                Tcl_AppendObjToObj(pHead, pTail);
                
                Tcl_DecrRefCount(pTail);
                Tcl_DecrRefCount(pResult);
                Tcl_DecrRefCount(pTree->pDocument);
                pTree->pDocument = pHead;
                z = Tcl_GetString(pHead);
                assert(!Tcl_IsShared(pTree->pDocument));
            } 
            Tcl_ResetResult(pTree->interp);

            pScript = 0;
            HtmlFree(0, (char *)pScriptToken);
            pScriptToken = 0;
        }

        /* A text (or whitespace) node */
        else if (c != '<' && c != 0) {
            for (i = 0; (c = z[n + i]) != 0 && c != '<'; i++);
            if (c || isFinal) {
                int j;
                char *z2;

                /* Make a temporary copy of the text and translate any
                 * embedded html escape characters (i.e. "&nbsp;")
                 */
                z2 = (char *)HtmlAlloc("temp", i + 1);
                memcpy(z2, &z[n], i);
                z2[i] = '\0';
                HtmlTranslateEscapes(z2);

                j = 0;
                while (z2[j]) {
                    char c = z2[j];

                    if (ISSPACE(c)) {
                        HtmlToken *pSpace;
                        int nBytes = sizeof(HtmlToken);
                        pSpace = (HtmlToken *)HtmlClearAlloc(0, nBytes);
                        pSpace->type = Html_Space;
                        if (c == '\n' || c == '\r') {
                            pSpace->x.newline = 1;
                            pSpace->count = 1;
                            j++;
                            iCol = 0;
                        } else {
                            int iColStart = iCol;
                            while (c && ISSPACE(c) && c != '\n' && c != '\r') {
                                iCol = NextColumn(iCol, c);
                                c = (unsigned char)z2[++j];
                            }
                            pSpace->count = iCol - iColStart;
                        }
                        AppendTextToken(pTree, pSpace, n);
                    } else {
                        int nBytes;
                        int iStart = j;
                        HtmlToken *pText;
                        while (c && !ISSPACE(c)) {
                            c = (unsigned char)z2[++j];
                        }
                        nBytes = 1 + j + sizeof(HtmlToken);

                        pText = (HtmlToken *)HtmlAlloc(0, nBytes);
                        pText->type = Html_Text;
                        pText->x.zText = (char *)&pText[1];
                        memcpy(pText->x.zText, &z2[iStart], j - iStart);
                        pText->x.zText[j - iStart] = '\0';
                        pText->count = j - iStart;
                        AppendTextToken(pTree, pText, n);
                        iCol += j - iStart;
                    }
                }
                HtmlFree("temp", z2);
                n += i;
            } else {
                goto incomplete;
            }
        }

        /*
         * An HTML comment. Just skip it. DK: This should be combined
         * with the script case above to reduce the amount of code.
         */
        else if (strncmp(&z[n], "<!--", 4) == 0) {
            for (i = 4; z[n + i]; i++) {
                if (z[n + i] == '-' && strncmp(&z[n + i], "-->", 3) == 0) {
                    break;
                }
            }
            if (z[n + i] == 0) {
                goto incomplete;
            }
            for (j = 0; j < i + 3; j++) {
                iCol = NextColumn(iCol, z[n + j]);
            }
            n += i + 3;
        }

        /* A markup tag (i.e "<p>" or <p color="red"> or </p>). We parse 
         * this into a vector of strings stored in the argv[] array. The
         * length of each string is stored in the corresponding element
         * of arglen[]. Variable argc stores the length of both arrays.
         *
         * The first element of the vector is the markup tag name (i.e. "p" 
         * or "/p"). Each attribute consumes two elements of the vector, 
         * the attribute name and the value.
         */
        else {
            /* At this point, &z[n] points to the "<" character that opens
             * a markup tag. Variable 'i' is used to record the current
             * position, relative to &z[n], while parsing the tags name
             * and attributes. The pointer to the tag name, argv[0], is 
             * therefore &z[n+1].
             */
            nStartScript = n;
            argc = 1;
            argv[0] = &z[n + 1];
            assert( c=='<' );

            /* Increment i until &z[n+i] is the first byte past the
             * end of the tag name. Then set arglen[0] to the length of
             * argv[0].
             */
            i = 0;
            do {
                i++;
                c = z[n + i];
            } while( c!=0 && !ISSPACE(c) && c!='>' && (i<2 || c!='/') );
            arglen[0] = i - 1;
            i--;

            /* Now prepare to parse the markup attributes. Advance i until
             * &z[n+i] points to the first character of the first attribute,
             * the closing '>' character, the closing "/>" string
	     * of a self-closing tag, or the end of the document. If the end of
	     * the document is reached, bail out via the 'incomplete' 
	     * exception handler.
             */
            while (ISSPACE(z[n + i])) {
                i++;
            }
            if (z[n + i] == 0) {
                goto incomplete;
            }

            /* This loop runs until &z[n+i] points to '>', "/>" or the
             * end of the document. The argv[] array is completely filled
             * by the time the loop exits.
             */
            while (
                (c = z[n+i]) != 0 &&          /* End of document */
                (c != '>') &&                 /* '>'             */
                1
#if 0
                (c != '/' || z[n+i+1] != '>') /* "/>"            */
#endif
            ){
                if (argc > mxARG - 3) {
                    argc = mxARG - 3;
                }

                /* Set the next element of the argv[] array to point at
                 * the attribute name. Then figure out the length of the
                 * attribute name by searching for one of ">", "=", "/>", 
                 * white-space or the end of the document.
                 */
                argv[argc] = &z[n+i];
                j = 0;
                while ((c = z[n + i + j]) != 0 && !ISSPACE(c) && c != '>'
                       && c != '=' && 
                       1
#if 0
(c != '/' || z[n + i + j + 1] != '>')
#endif
                ) {
                    j++;
                }
                arglen[argc] = j;

                if (c == 0) {
                    goto incomplete;
                }
                i += j;

                while (ISSPACE(c)) {
                    i++;
                    c = z[n + i];
                }
                if (c == 0) {
                    goto incomplete;
                }
                argc++;
                if (c != '=') {
                    argv[argc] = "";
                    arglen[argc] = 0;
                    argc++;
                    continue;
                }
                i++;
                c = z[n + i];
                while (ISSPACE(c)) {
                    i++;
                    c = z[n + i];
                }
                if (c == 0) {
                    goto incomplete;
                }
                if (c == '\'' || c == '"') {
                    int cQuote = c;
                    i++;
                    argv[argc] = &z[n + i];
                    for (j = 0; (c = z[n + i + j]) != 0 && c != cQuote; j++) {
                    }
                    if (c == 0) {
                        goto incomplete;
                    }
                    arglen[argc] = j;
                    i += j + 1;
                }
                else {
                    argv[argc] = &z[n + i];
                    for (j = 0;
                         (c = z[n + i + j]) != 0 && !ISSPACE(c) && c != '>';
                         j++) {
                    }
                    if (c == 0) {
                        goto incomplete;
                    }
                    arglen[argc] = j;
                    i += j;
                }
                argc++;
                while (ISSPACE(z[n + i])) {
                    i++;
                }
            }
            if( c==0 ){
                goto incomplete;
            }

            /* If this was a self-closing tag, set selfClose to 1 and 
             * increment i so that &z[n+i] points to the '>' character.
             */
            if (c == '/') {
                i++;
                c = z[n + i];
                selfClose = 1;
            } else {
                selfClose = 0;
            }
            assert( c!=0 );

            for (j = 0; j < i + 1; j++) {
                iCol = NextColumn(iCol, z[n + j]);
            }
            n += i + 1;

            /* Look up the markup name in the hash table. If it is an unknown
             * tag, just ignore it by jumping to the next iteration of
             * the while() loop. The data in argv[] is discarded in this case.
             *
             * DK: We jump through hoops to pass a NULL-terminated string to 
             * HtmlHashLookup(). It would be easy enough to fix 
             * HtmlHashLookup() to understand a length argument.
             */
            if (!isInit) {
                HtmlHashInit(0, 0);
                isInit = 1;
            }
            c = argv[0][arglen[0]];
            argv[0][arglen[0]] = 0;
            pMap = HtmlHashLookup(0, argv[0]);
            argv[0][arglen[0]] = c;
            if (pMap == 0) {
                continue;
            }

          makeMarkupEntry: {
            /* If we get here, we need to allocate a structure to store
             * the markup element. 
             */
            HtmlToken *pMarkup;
            nByte = sizeof(HtmlToken);
            if (argc > 1) {
                nByte += sizeof(char *) * (argc + 1);
                for (j = 1; j < argc; j++) {
                    nByte += arglen[j] + 1;
                }
            }
            pMarkup = (HtmlToken *)HtmlAlloc(0, nByte);
            pMarkup->type = pMap->type;
            pMarkup->count = argc - 1;
            pMarkup->x.zArgs = 0;

            /* If the tag had attributes, then copy all the attribute names
             * and values into the space just allocated. Translate escapes
	     * on the way. The idea is that calling HtmlFree() on pToken frees
	     * the space used by the attributes as well as the HtmlToken.
             */
            if (argc > 1) {
                pMarkup->x.zArgs = (char **)&pMarkup[1];
                zBuf = (char *)&pMarkup->x.zArgs[argc + 1];
                for (j=1; j < argc; j++) {
                    pMarkup->x.zArgs[j-1] = zBuf;
                    zBuf += arglen[j]+1;

                    strncpy(pMarkup->x.zArgs[j-1], argv[j], arglen[j]);
                    pMarkup->x.zArgs[j - 1][arglen[j]] = 0;
                    HtmlTranslateEscapes(pMarkup->x.zArgs[j - 1]);
                    if ((j&1) == 1) {
                        ToLower(pMarkup->x.zArgs[j-1]);
                    }
                }
                pMarkup->x.zArgs[argc - 1] = 0;
            }

            pScript = getScriptHandler(pTree, pMarkup->type);
            if (!pScript) {
                /* No special handler for this markup. Just append it to the 
                 * list of all tokens. 
                 */
                assert(nStartScript >= 0);
                AppendToken(pTree, pMarkup, nStartScript);
            } else {
                pScriptToken = pMarkup;
            }
          }

            /* If this is self-closing markup (ex: <br/> or <img/>) then
             * synthesize a closing token. 
             */
            if (selfClose && argv[0][0] != '/'
                && strcmp(&pMap[1].zName[1], pMap->zName) == 0) {
                selfClose = 0;
                pMap++;
                argc = 1;
                goto makeMarkupEntry;
            }
        }
    }

  incomplete:
    pTree->iCol = iCol;
    return n;
}

/************************** End HTML Tokenizer Code ***************************/

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTokenizerAppend --
 *
 *     Append text to the tokenizer engine.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     This routine (actually the Tokenize() subroutine that is called
 *     by this routine) may invoke a callback procedure which could delete
 *     the HTML widget. 
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlTokenizerAppend(pTree, zText, nText, isFinal)
    HtmlTree *pTree;
    const char *zText;
    int nText;
    int isFinal;
{
    /* TODO: Add a flag to prevent recursive calls to this routine. */
    const char *z = zText;
    int n = nText;
    /* Tcl_DString utf8; */

    if (!pTree->pDocument) {
        pTree->pDocument = Tcl_NewObj();
        Tcl_IncrRefCount(pTree->pDocument);
        assert(!Tcl_IsShared(pTree->pDocument));
    }

    assert(!Tcl_IsShared(pTree->pDocument));
    Tcl_AppendToObj(pTree->pDocument, z, n);

    pTree->nParsed = Tokenize(pTree, isFinal);
    if (isFinal) {
        AppendToken(pTree, 0, -1);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlHashLookup --
 *
 *     Look up an HTML tag name in the hash-table.
 *
 * Results: 
 *     Return the corresponding HtmlTokenMap if the tag name is recognized,
 *     or NULL otherwise.
 *
 * Side effects:
 *     May initialise the hash table from the autogenerated array
 *     in htmltokens.c (generated from tokenlist.txt).
 *
 *---------------------------------------------------------------------------
 */
static HtmlTokenMap * 
HtmlHashLookup(htmlPtr, zType)
    void *htmlPtr;
    const char *zType;          /* Null terminated tag name. eg. "br" */
{
    HtmlTokenMap *pMap;         /* For searching the markup name hash table */
    int h;                      /* The hash on zType */
    char buf[256];
    if (!isInit) {
        HtmlHashInit(htmlPtr, 0);
        isInit = 1;
    }
    h = HtmlHash(htmlPtr, zType);
    for (pMap = apMap[h]; pMap; pMap = pMap->pCollide) {
        if (stricmp(pMap->zName, zType) == 0) {
            return pMap;
        }
    }
    strncpy(buf, zType, 255);
    buf[255] = 0;

    return NULL;
}

/*
** Convert a markup name into a type integer
*/
int
HtmlNameToType(htmlPtr, zType)
    void *htmlPtr;
    char *zType;
{
    HtmlTokenMap *pMap = HtmlHashLookup(htmlPtr, zType);
    return pMap ? pMap->type : Html_Unknown;
}

/*
** Convert a type into a symbolic name
*/
const char *
HtmlTypeToName(htmlPtr, eTag)
    void *htmlPtr;
    int eTag;
{
    if (eTag >= Html_A && eTag < Html_TypeCount) {
        HtmlTokenMap *pMap = apMap[eTag - Html_A];
        return pMap->zName;
    }
    else {
        return "???";
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlMarkupArg --
 *
 *     Lookup an argument in the given markup with the name given.
 *     Return a pointer to its value, or the given default
 *     value if it doesn't appear.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
char * HtmlMarkupArg(pToken, zTag, zDefault)
    HtmlToken *pToken;
    const char *zTag;
    char *zDefault;
{
    int i;
    if (pToken->type==Html_Space || pToken->type==Html_Text) {
        return 0;
    }
    for (i = 0; i < pToken->count; i += 2) {
        if (strcmp(pToken->x.zArgs[i], zTag) == 0) {
            return pToken->x.zArgs[i + 1];
        }
    }
    return zDefault;
}
