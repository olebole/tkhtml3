
/*
 * htmltagdb.c ---
 *
 *     This file implements the interface used by other modules to the
 *     HtmlMarkupMap array. Right now this is partially here, and partially
 *     in htmlparse.c. But the idea is that it should all be here soon.
 *
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
static const char rcsid[] = "$Id: htmltagdb.c,v 1.10 2006/07/14 13:37:56 danielk1977 Exp $";

#include "html.h"
#include <assert.h>
#include <string.h>

/*
 * Public interface to code in this file:
 *
 *     HtmlMarkupName()
 *     HtmlMarkupFlags()
 *     HtmlMarkup()
 */

extern HtmlTokenMap HtmlMarkupMap[];

static int 
textContent(pTree, pNode, tag)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int tag;
{
    if (tag == Html_Space || tag == Html_Text) {
        return TAG_OK;
    }
    return TAG_CLOSE;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlMarkup --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
HtmlTokenMap *
HtmlMarkup(markup)
    int markup;
{
    if (markup == Html_Text || markup == Html_Space) {
        static HtmlTokenMap textmapentry = {
            "text",
            Html_Text,
            HTMLTAG_INLINE,
            textContent,
            0
        };
        return &textmapentry;
    } else {
        int i = markup-Html_A;
        assert(i>=0 && i<HTML_MARKUP_COUNT);
        return &HtmlMarkupMap[i];
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlMarkupFlags --
 *
 * Results:
 *     Return the 'flags' value associated with Html markup tag 'markup'.
 *     The flags value is a bitmask comprised of the HTMLTAG_xxx symbols
 *     defined in html.h.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
Html_u8 
HtmlMarkupFlags(markup)
    int markup;
{
    int i = markup-Html_A;
    if (i>=0 && i<HTML_MARKUP_COUNT){
        return HtmlMarkupMap[i].flags;
    }

    /* Regular text behaves as an inline element. */
    if( markup==Html_Text || markup==Html_Space ){
        return HTMLTAG_INLINE;
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlMarkupName --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
CONST char *
HtmlMarkupName(markup)
    int markup;
{
    int i = markup-Html_A;
    if (i>=0 && i<HTML_MARKUP_COUNT){
        return HtmlMarkupMap[i].zName;
    }

    if( markup==Html_Text || markup==Html_Space ){
        return "";
    }

    return "unknown";
}
