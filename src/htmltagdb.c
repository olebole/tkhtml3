
/*
 * htmltagdb.c ---
 *
 *     This file implements the interface used by other modules to the
 *     HtmlMarkupMap array. Right now this is partially here, and partially
 *     in htmlparse.c. But the idea is that it should all be here soon.
 *
 * TODO: Copyright.
 */
static char rcsid[] = "@(#) $Id:";

#include "html.h"
#include <assert.h>
#include <string.h>

extern HtmlTokenMap HtmlMarkupMap[];

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
    int i = markup-Html_A;
    assert(i>=0 && i<HTML_MARKUP_COUNT);
    return &HtmlMarkupMap[i];
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
        return "text";
    }

    return "unknown";
}
