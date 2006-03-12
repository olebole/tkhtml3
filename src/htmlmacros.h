
#ifndef __HTMLMACROS_H__
#define __HTMLMACROS_H__

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeTagType --
 *
 *         int HtmlNodeTagType(HtmlNode *pNode);
 *
 *     Return the tag-type of the node, i.e. Html_P, Html_Text or
 *     Html_Space.
 *
 * Results:
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#define HtmlNodeTagType(pNode) \
(((pNode) && (pNode)->pToken) ? (pNode)->pToken->type : 0)

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeIsText --
 *
 *         int HtmlNodeIsText(HtmlNode *pNode);
 *
 *     Return non-zero if the node is a (possibly empty) text node, or zero
 *     otherwise.
 *
 * Results:
 *     Boolean.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#define HtmlNodeIsText(pNode) \
(HtmlNodeTagType(pNode) == Html_Text || (HtmlNodeTagType(pNode) == Html_Space))

#endif
