
#ifndef __HTMLMACROS_H__
#define __HTMLMACROS_H__

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeTagType --
 *
 *     Return the tag-type of the node, i.e. Html_P, Html_Text or
 *     Html_Space.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#define HtmlNodeTagType(pNode) \
(((pNode) && (pNode)->pToken) ? (pNode)->pToken->type : 0)

#define HtmlNodeIsText(pNode) \
(HtmlNodeTagType(pNode) == Html_Text || (HtmlNodeTagType(pNode) == Html_Space))

#endif
