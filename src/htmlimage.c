static char const rcsid[] =
        "@(#) $Id: htmlimage.c,v 1.20 2005/03/23 01:36:54 danielk1977 Exp $";

/*
** Routines used for processing <IMG> markup
**
** This source code is released into the public domain by the author,
** D. Richard Hipp, on 2002 December 17.  Instead of a license, here
** is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
*/

#include <string.h>
#include <stdlib.h>
#include "html.h"

int tkhtmlexiting = 0;

#ifdef _TCLHTML_
HtmlImage *
HtmlGetImage(HtmlWidget * htmlPtr, HtmlElement * p)
{
    return 0;
}
#else

/*
** Find the alignment for an image
*/
int
HtmlGetImageAlignment(p)
    HtmlElement *p;
{
    char *z;
    int i;
    int result;

    static struct {
        char *zName;
        int iValue;
    } aligns[] = {
        {
        "bottom", IMAGE_ALIGN_Bottom}, {
        "baseline", IMAGE_ALIGN_Bottom}, {
        "middle", IMAGE_ALIGN_Middle}, {
        "top", IMAGE_ALIGN_Top}, {
        "absbottom", IMAGE_ALIGN_AbsBottom}, {
        "absmiddle", IMAGE_ALIGN_AbsMiddle}, {
        "texttop", IMAGE_ALIGN_TextTop}, {
        "left", IMAGE_ALIGN_Left}, {
    "right", IMAGE_ALIGN_Right},};

    z = HtmlMarkupArg(p, "align", 0);
    result = IMAGE_ALIGN_Bottom;
    if (z) {
        for (i = 0; i < sizeof(aligns) / sizeof(aligns[0]); i++) {
            if (stricmp(aligns[i].zName, z) == 0) {
                result = aligns[i].iValue;
                break;
            }
            else {
            }
        }
    }
    else {
    }
    return result;
}

/*
** This routine is called when an image changes.  If the size of the
** images changes, then we need to completely redo the layout.  If
** only the appearance changes, then this works like an expose event.
*/
static void
ImageChangeProc(clientData, x, y, w, h, newWidth, newHeight)
    ClientData clientData;             /* Pointer to an HtmlImage structure */
    int x;                             /* Left edge of region that changed */
    int y;                             /* Top edge of region that changed */
    int w;                             /* Width of region that changes.
                                        * Maybe 0 */
    int h;                             /* Height of region that changed.
                                        * Maybe 0 */
    int newWidth;                      /* New width of the image */
    int newHeight;                     /* New height of the image */
{
    HtmlImage *pImage;
    HtmlWidget *htmlPtr;
    HtmlElement *pElem;

    if (tkhtmlexiting)
        return;
    pImage = (HtmlImage *) clientData;
    htmlPtr = pImage->htmlPtr;
    if (pImage->w != newWidth || pImage->h != newHeight) {
        /*
         * We have to completely redo the layout after adjusting the size **
         * of the images 
         */
        for (pElem = pImage->pList; pElem; pElem = pElem->image.pNext) {
            pElem->image.w = newWidth;
            pElem->image.h = newHeight;
        }
        htmlPtr->flags |= RELAYOUT;
        pImage->w = newWidth;
        pImage->h = newHeight;
        HtmlRedrawEverything(htmlPtr);
    }
    else {
        for (pElem = pImage->pList; pElem; pElem = pElem->image.pNext) {
            pElem->image.redrawNeeded = 1;
        }
        htmlPtr->flags |= REDRAW_IMAGES;
        HtmlScheduleRedraw(htmlPtr);
    }
}

void
HtmlAddImages(htmlPtr, p, pImage, str, append)
    HtmlWidget *htmlPtr;
    HtmlElement *p;
    HtmlImage *pImage;
    char *str;
    int append;
{
    int argc, code, doupdate = 0;
    CONST char **argv;
    HtmlElement *pElem;

    if (!str[0]) {
#ifdef DEBUG

/*    fprintf(stderr,"OOPS null string\n");  */
#endif
        return;
    }
    code = Tcl_SplitList(htmlPtr->interp, str, &argc, &argv);
    if ((code != TCL_OK || argc == 1) && (!append)) {
        if (pImage->image)
            Tk_FreeImage(pImage->image);
        pImage->image = Tk_GetImage(htmlPtr->interp, htmlPtr->clipwin,
                                    str, ImageChangeProc, pImage);
        doupdate = 1;
    }
    else {
        int i, m = argc;
        struct HtmlImageAnim *pi, *pl = 0;
        if (append) {
            pImage->num += argc;
            pl = pImage->anims;
            while (pl && pl->next)
                pl = pl->next;
            doupdate = 1;
            pImage->cur++;
        }
        else {
            pImage->cur = 0;
            pImage->num = argc;
        }
        if (!pImage->image) {
            pImage->image = Tk_GetImage(htmlPtr->interp, htmlPtr->clipwin,
                                        argv[0], ImageChangeProc, pImage);
            if (!pImage->image) {
                return;
            }
            m--;
        }
        for (i = 0; i < m; i++) {
            pi = (struct HtmlImageAnim *)
                    HtmlAlloc(sizeof(struct HtmlImageAnim));
            pi->next = 0;
            pi->image = Tk_GetImage(htmlPtr->interp, htmlPtr->clipwin,
                                    argv[i], ImageChangeProc, pImage);
            if (pl)
                pl->next = pi;
            else
                pImage->anims = pi;
            pl = pi;
        }
    }
    if (doupdate) {
        for (pElem = pImage->pList; pElem; pElem = pElem->image.pNext)
            pElem->image.redrawNeeded = 1;
        htmlPtr->flags |= REDRAW_IMAGES;
        HtmlScheduleRedraw(htmlPtr);
    }
    HtmlFree((char *) argv);
}

/* Return the height/width converting percent (%) if required */
char *
HtmlPctWidth(h, p, opt, ret)
    HtmlWidget *h;
    HtmlElement *p;
    char *opt;
    char *ret;
{
    int n, m, w;
    HtmlElement *tp = p;
    char *tz, *z = HtmlMarkupArg(p, opt, "");
    if (!strchr(z, '%'))
        return z;
    if (!sscanf(z, "%d", &n))
        return z;
    if (n <= 0 || n > 100)
        return z;
    if (opt[0] == 'h')
        w = h->height * 100;
    else
        w = h->width * 100;
    if (!h->inTd) {
        sprintf(ret, "%d", w / n);
    }
    else {
        while (tp && tp->base.type != Html_TD)
            tp = tp->base.pPrev;
        if (!tp)
            return z;
        tz = HtmlMarkupArg(tp, opt, 0);
        if (tz && (!strchr(tz, '%')) && sscanf(tz, "%d", &m)) {
            sprintf(ret, "%d", (m * 100) / n);
            return ret;
        }
        tp = tp->cell.pTable;
        if (!tp)
            return z;
        tz = HtmlMarkupArg(tp, opt, 0);
        if (tz && (!strchr(tz, '%')) && sscanf(tz, "%d", &m)) {
            sprintf(ret, "%d", (m * 100) / n);
            return ret;
        }
        else
            return z;
    }
    return ret;
}

/*
** Given an <IMG> markup, find or create an appropriate HtmlImage
** structure and return a pointer to that structure.  NULL might
** be returned.
**
** This routine may invoke a callback procedure which could delete
** the HTML widget.  Use HtmlLock() if necessary to preserve the
** widget structure.
*/
HtmlImage *
HtmlGetImage(htmlPtr, p)
    HtmlWidget *htmlPtr;
    HtmlElement *p;
{
    char zId[30];
    char *zWidth;
    char *zHeight;
    char *zSrc;
    char *zImageName;
    HtmlImage *pImage;
    int result;
    Tcl_DString cmd;
    int lenSrc, lenW, lenH;            /* Lengths of various strings */

    if (p->base.type != Html_IMG) {
        CANT_HAPPEN;
        return 0;
    }
    if (htmlPtr->zGetImage == 0 || htmlPtr->zGetImage[0] == 0) {
        return 0;
    }
    zSrc = HtmlMarkupArg(p, "src", 0);
    if (zSrc == 0) {
        return 0;
    }
    HtmlLock(htmlPtr);
    zSrc = HtmlResolveUri(htmlPtr, zSrc);
    if (HtmlUnlock(htmlPtr) || zSrc == 0) {
        if (zSrc)
            HtmlFree(zSrc);
        return 0;
    }
    zWidth = HtmlMarkupArg(p, "width", "");
    zHeight = HtmlMarkupArg(p, "height", "");
    for (pImage = htmlPtr->imageList; pImage; pImage = pImage->pNext) {
        if (strcmp(pImage->zUrl, zSrc) == 0
            && strcmp(pImage->zWidth, zWidth) == 0
            && strcmp(pImage->zHeight, zHeight) == 0) {
            HtmlFree(zSrc);
            return pImage;
        }
    }
    Tcl_DStringInit(&cmd);
    Tcl_DStringAppend(&cmd, htmlPtr->zGetImage, -1);
    Tcl_DStringAppendElement(&cmd, zSrc);
    Tcl_DStringAppendElement(&cmd, HtmlPctWidth(htmlPtr, p, "width", zId));
    Tcl_DStringAppendElement(&cmd, HtmlPctWidth(htmlPtr, p, "height", zId));
    Tcl_DStringStartSublist(&cmd);
    HtmlAppendArglist(&cmd, p);
    Tcl_DStringEndSublist(&cmd);
    sprintf(zId, "%d", HtmlTokenNumber(p));
    Tcl_DStringAppendElement(&cmd, zId);
    HtmlLock(htmlPtr);
    htmlPtr->inParse++;
    result = Tcl_GlobalEval(htmlPtr->interp, Tcl_DStringValue(&cmd));
    htmlPtr->inParse--;
    Tcl_DStringFree(&cmd);
    if (HtmlUnlock(htmlPtr)) {
        HtmlFree(zSrc);
        return 0;
    }
    zImageName = htmlPtr->interp->result;
    lenSrc = strlen(zSrc);
    lenW = strlen(zWidth);
    lenH = strlen(zHeight);
    pImage = HtmlAlloc(sizeof(HtmlImage) + lenSrc + lenW + lenH + 3);
    memset(pImage, 0, sizeof(HtmlImage));
    pImage->htmlPtr = htmlPtr;
    pImage->zUrl = (char *) &pImage[1];
    strcpy(pImage->zUrl, zSrc);
    HtmlFree(zSrc);
    pImage->zWidth = &pImage->zUrl[lenSrc + 1];
    strcpy(pImage->zWidth, zWidth);
    pImage->zHeight = &pImage->zWidth[lenW + 1];
    strcpy(pImage->zHeight, zHeight);
    pImage->w = 0;
    pImage->h = 0;
    if (result == TCL_OK) {
        HtmlAddImages(htmlPtr, p, pImage, htmlPtr->interp->result, 0);
    }
    else {
        Tcl_AddErrorInfo(htmlPtr->interp,
                         "\n    (\"-imagecommand\" command executed by html widget)");
        Tcl_BackgroundError(htmlPtr->interp);
        pImage->image = 0;
    }
    if (pImage->image == 0) {
        HtmlFree((char *) pImage);
        return 0;
    }
    pImage->pNext = htmlPtr->imageList;
    htmlPtr->imageList = pImage;
    Tcl_ResetResult(htmlPtr->interp);
    return pImage;
}

#endif /* _TCLHTML_ */
