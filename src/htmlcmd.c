static char const rcsid[] =
        "@(#) $Id: htmlcmd.c,v 1.32 2005/03/26 11:54:30 danielk1977 Exp $";

/*
** Routines to implement the HTML widget commands
**
** This source code is released into the public domain by the author,
** D. Richard Hipp, on 2002 December 17.  Instead of a license, here
** is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
*/
#include <stdlib.h>
#include "html.h"
#include <X11/Xatom.h>
#include <string.h>
#include <assert.h>

/*
** WIDGET resolve ?URI ...?
**
** Call the TCL command specified by the -resolvercommand option
** to resolve the URL.
*/
int
HtmlResolveCmd(clientData, interp, argc, argv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int argc;                          /* Number of arguments */
    const char **argv;                 /* List of all arguments */
{
    return HtmlCallResolver((HtmlWidget *) clientData, argv + 2);
}

/*
** WIDGET cget CONFIG-OPTION
**
** Retrieve the value of a configuration option
*/
int
HtmlCgetObjCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    int rc;
    HtmlWidget *htmlPtr = (HtmlWidget *) clientData;
    Tk_ConfigSpec *cs = HtmlConfigSpec();

    if (htmlPtr->TclHtml)
        rc = TclConfigureWidgetObj(interp, htmlPtr, cs,
                                   objc - 2, objv + 2, (char *) htmlPtr, 0);
    else
        rc = Tk_ConfigureValue(interp, htmlPtr->tkwin, cs,
                               (char *) htmlPtr, Tcl_GetString(objv[2]), 0);
    return rc;
}

/*
** WIDGET clear
**
** Erase all HTML from this widget and clear the screen.  This is
** typically done before loading a new document.
*/
int
HtmlClearCmd(clientData, interp, argc, argv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int argc;                          /* Number of arguments */
    const char **argv;                 /* List of all arguments */
{
    HtmlWidget *htmlPtr = (HtmlWidget *) clientData;
    HtmlTreeFree(htmlPtr);
    HtmlClear(htmlPtr);
    htmlPtr->flags |= REDRAW_TEXT | VSCROLL | HSCROLL;
    HtmlScheduleRedraw(htmlPtr);
    return TCL_OK;
}

/*
** WIDGET configure ?OPTIONS?
**
** The standard Tk configure command.
*/
int
HtmlConfigCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlWidget *htmlPtr = (HtmlWidget *) clientData;
#ifdef _TCLHTML_
    if (objv == 2) {            /* ???? */
        return HtmlCgetObjCmd(htmlPtr, interp, objc, objv);
    }
    else if (argc == 3) {
        return HtmlCgetObjCmd(htmlPtr, interp, objc, objv);
    }
    else {
        return ConfigureHtmlWidget(interp, htmlPtr, objc - 2, objv + 2,
                                   TK_CONFIG_ARGV_ONLY, 0);
    }
#else
    if (objc == 2) {
        return Tk_ConfigureInfo(interp, htmlPtr->tkwin, HtmlConfigSpec(),
                                (char *) htmlPtr, (char *) NULL, 0);
    }
    else if (objc == 3) {
        return Tk_ConfigureInfo(interp, htmlPtr->tkwin, HtmlConfigSpec(),
                                (char *) htmlPtr, Tcl_GetString(objv[2]), 0);
    }
    else {
        return ConfigureHtmlWidgetObj(interp, htmlPtr, objc - 2, objv + 2,
                                      TK_CONFIG_ARGV_ONLY, 0);
    }
#endif
}

/* Return pElem with attr "name" == value */
HtmlElement *
HtmlAttrElem(htmlPtr, name, value)
    HtmlWidget *htmlPtr;
    char *name;
    const char *value;
{
    HtmlElement *p;
    char *z;
    for (p = htmlPtr->pFirst; p; p = p->pNext) {
        if (p->base.type != Html_A)
            continue;
        z = HtmlMarkupArg(p, name, 0);
        if (z && (!strcmp(z, value)))
            return p;
    }
    return 0;
}

/*
** WIDGET names
**
** Returns a list of names associated with <a name=...> tags.
*/
int
HtmlNamesCmd(clientData, interp, argc, argv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int argc;                          /* Number of arguments */
    const char **argv;                 /* List of all arguments */
{
    HtmlElement *p;
    char *z;
    HtmlWidget *htmlPtr = (HtmlWidget *) clientData;
    for (p = htmlPtr->pFirst; p; p = p->pNext) {
        if (p->base.type != Html_A)
            continue;
        z = HtmlMarkupArg(p, "name", 0);
        if (z) {
            Tcl_AppendElement(interp, z);
        }
        else {
            z = HtmlMarkupArg(p, "id", 0);
            if (z) {
                Tcl_AppendElement(interp, z);
            }
        }
    }
    return TCL_OK;
}

int
HtmlAdvanceLayout(htmlPtr)
    HtmlWidget *htmlPtr;               /* The HTML widget */
{
    if (htmlPtr->LOendPtr) {
        if (htmlPtr->LOendPtr->pNext) {
            htmlPtr->formStart = htmlPtr->LOformStart;
            HtmlAddStyle(htmlPtr, htmlPtr->LOendPtr->pNext);
            HtmlSizer(htmlPtr);
        }
    }
    else if (htmlPtr->pFirst) {
        htmlPtr->paraAlignment = ALIGN_None;
        htmlPtr->rowAlignment = ALIGN_None;
        htmlPtr->anchorFlags = 0;
        htmlPtr->inDt = 0;
        htmlPtr->anchorStart = 0;
        htmlPtr->formStart = 0;
        htmlPtr->LOformStart = 0;
        htmlPtr->innerList = 0;
        htmlPtr->nInput = 0;
        HtmlAddStyle(htmlPtr, htmlPtr->pFirst);
        HtmlSizer(htmlPtr);
    }
    htmlPtr->LOendPtr = htmlPtr->pLast;
    htmlPtr->LOformStart = htmlPtr->formStart;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlParseCmd --
 *
 *     $widget parse HTML-TEXT
 * 
 *     Appends the given HTML text to the end of any HTML text that may have
 *     been inserted by prior calls to this command.  Then it runs the
 *     tokenizer, parser and layout engine as far as possible with the text
 *     that is available.  The display is updated appropriately.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     User callbacks might be invoked. A redraw is scheduled for the next
 *     idle time.
 *
 *---------------------------------------------------------------------------
 */
int HtmlParseCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    int i;
    char *arg1, *arg2;
    HtmlIndex iStart;
    HtmlElement *savePtr;
    HtmlWidget *htmlPtr = (HtmlWidget *) clientData;
    assert( !HtmlIsDead(htmlPtr) );

    iStart.p = 0;
    iStart.i = 0;
    htmlPtr->LOendPtr = htmlPtr->pLast;
    HtmlLock(htmlPtr);

    /*
     * Currently this proc accepts two arguments: -insert and -ypos.
     */
    for (i = 3; i < (objc - 1); i += 2) {
        arg1 = Tcl_GetString(objv[i]);
        arg2 = Tcl_GetString(objv[i + 1]);
        if ((!strcmp(arg1, "-insert")) && htmlPtr->LOendPtr) {
            if (HtmlGetIndex(htmlPtr, arg2, &iStart.p, &iStart.i) != 0) {
                Tcl_AppendResult(interp, "malformed index: \"", arg2, "\"", NULL);
                return TCL_ERROR;
            }
            if (iStart.p) {
                savePtr = iStart.p->pNext;
                htmlPtr->pLast = iStart.p;
                iStart.p->pNext = 0;
            }
        }
        else if ((!strcmp(arg1, "-ypos")) && arg2[0]) {
            htmlPtr->zGoto = (char *) strdup(arg2);
        }
    }

    /* Add the new text to the internal cache of the document. Also tokenize
     * it and add the new HtmlElement objects to the HtmlWidget.pFirst/pLast 
     * linked list.
     */
    arg1 = Tcl_GetStringFromObj(objv[2], &i);
    HtmlTokenizerAppend(htmlPtr, arg1, i);
    if (HtmlIsDead(htmlPtr)) {
        return TCL_OK;
    }

    /* Call HtmlAddStlye to add 'style' to the elements just added to
     * the list (the entire list if it was initially empty).
     */
    if (htmlPtr->LOendPtr) {
        htmlPtr->formStart = htmlPtr->LOformStart;
        if (iStart.p && savePtr) {
            HtmlAddStyle(htmlPtr, htmlPtr->LOendPtr);
            htmlPtr->pLast->pNext = savePtr;
            savePtr->base.pPrev = htmlPtr->pLast;
            htmlPtr->pLast = htmlPtr->LOendPtr;
            htmlPtr->flags |= (REDRAW_TEXT | RELAYOUT);
            HtmlScheduleRedraw(htmlPtr);
        }
        else if (htmlPtr->LOendPtr->pNext) {
            HtmlAddStyle(htmlPtr, htmlPtr->LOendPtr->pNext);
        }
    } else if (htmlPtr->pFirst) {
        htmlPtr->paraAlignment = ALIGN_None;
        htmlPtr->rowAlignment = ALIGN_None;
        htmlPtr->anchorFlags = 0;
        htmlPtr->inDt = 0;
        htmlPtr->anchorStart = 0;
        htmlPtr->formStart = 0;
        htmlPtr->innerList = 0;
        htmlPtr->nInput = 0;
        HtmlAddStyle(htmlPtr, htmlPtr->pFirst);
    }

    /* Schedule a redraw (idle callback to HtmlRedrawCallback) - or just run
     * the layout engine if there is no Tk window associated with this html
     * object (i.e. -tclhtml was passed when it was constructed).
     */
    if (!HtmlUnlock(htmlPtr)) {
        htmlPtr->flags |= EXTEND_LAYOUT;
        HtmlScheduleRedraw(htmlPtr);
    }
    if (htmlPtr->TclHtml){
        HtmlLayout(htmlPtr);
    }

    return TCL_OK;
}

int
HtmlLayoutCmd(clientData, interp, argc, argv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int argc;                          /* Number of arguments */
    const char **argv;                 /* List of all arguments */
{
    HtmlLayout((HtmlWidget *) clientData);
    return TCL_OK;
}

#ifndef _TCLHTML_

/*
** WIDGET href X Y
**
** Returns the URL on the hyperlink that is beneath the position X,Y.
** Returns {} if there is no hyperlink beneath X,Y.
*/
int
HtmlHrefCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlWidget *htmlPtr = (HtmlWidget *) clientData;
    int x, y;
    char *z, *target = 0;

    if (Tcl_GetIntFromObj(interp, objv[2], &x) != TCL_OK
        || Tcl_GetIntFromObj(interp, objv[3], &y) != TCL_OK) {
        return TCL_ERROR;
    }
    z = HtmlGetHref(htmlPtr, x + htmlPtr->xOffset, y + htmlPtr->yOffset,
                    &target);
    if (z) {
        HtmlLock(htmlPtr);
        z = HtmlResolveUri(htmlPtr, z);
        if (z && !HtmlUnlock(htmlPtr)) {
            Tcl_DString cmd;
            Tcl_DStringInit(&cmd);
            Tcl_DStringStartSublist(&cmd);
            Tcl_DStringAppendElement(&cmd, z);
            Tcl_DStringEndSublist(&cmd);
            if (target) {
                Tcl_DStringStartSublist(&cmd);
                Tcl_DStringAppendElement(&cmd, target);
                Tcl_DStringEndSublist(&cmd);
            }
            Tcl_DStringResult(interp, &cmd);
        }
        if (z)
            HtmlFree(z);
    }
    return TCL_OK;
}

/*
** WIDGET xview ?SCROLL-OPTIONS...?
**
** Implements horizontal scrolling in the usual Tk way.
*/
int
HtmlXviewCmd(clientData, interp, argc, argv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int argc;                          /* Number of arguments */
    const char **argv;                 /* List of all arguments */
{
    HtmlWidget *htmlPtr = (HtmlWidget *) clientData;
    if (argc == 2) {
        HtmlComputeHorizontalPosition(htmlPtr, interp->result);
    }
    else {
        int count;
        double fraction;
        int maxX = htmlPtr->maxX;
        int w = HtmlUsableWidth(htmlPtr);
        int offset = htmlPtr->xOffset;
        int type = Tk_GetScrollInfo(interp, argc, argv, &fraction, &count);
        switch (type) {
            case TK_SCROLL_ERROR:
                return TCL_ERROR;
            case TK_SCROLL_MOVETO:
                offset = fraction * maxX;
                break;
            case TK_SCROLL_PAGES:
                offset += (count * w * 9) / 10;
                break;
            case TK_SCROLL_UNITS:
                offset += (count * w) / 10;
                break;
        }
        if (offset + w > maxX) {
            offset = maxX - w;
        }
        else {
        }
        if (offset < 0) {
            offset = 0;
        }
        else {
        }
        HtmlHorizontalScroll(htmlPtr, offset);
        htmlPtr->flags |= ANIMATE_IMAGES;
    }
    return TCL_OK;
}

/*
** WIDGET yview ?SCROLL-OPTIONS...?
**
** Implements vertical scrolling in the usual Tk way, but with one
** enhancement.  If the argument is a single word, the widget looks
** for a <a name=...> tag with that word as the "name" and scrolls
** to the position of that tag.
*/
int
HtmlYviewCmd(clientData, interp, argc, argv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int argc;                          /* Number of arguments */
    const char **argv;                 /* List of all arguments */
{
    HtmlWidget *htmlPtr = (HtmlWidget *) clientData;
    if (argc == 2) {
        HtmlComputeVerticalPosition(htmlPtr, interp->result);
    }
    else if (argc == 3) {
        HtmlElement *p = HtmlAttrElem(htmlPtr, "name", argv[2]);
        if (p && p->anchor.y == 0) {
            HtmlLock(htmlPtr);
            HtmlLayout(htmlPtr);
            HtmlUnlock(htmlPtr);
        }
        if (p)
            HtmlVerticalScroll(htmlPtr, p->anchor.y);
    }
    else {
        int count;
        double fraction;
        int maxY = htmlPtr->maxY;
        int h = HtmlUsableHeight(htmlPtr);
        int offset = htmlPtr->yOffset;
        int type =
                Tk_GetScrollInfo(interp, argc, (const char **) argv, &fraction,
                                 &count);
        switch (type) {
            case TK_SCROLL_ERROR:
                return TCL_ERROR;
            case TK_SCROLL_MOVETO:
                offset = fraction * maxY;
                break;
            case TK_SCROLL_PAGES:
                offset += (count * h * 9) / 10;
                break;
            case TK_SCROLL_UNITS:
                offset += (count * h) / 10;
                break;
        }
        if (offset + h > maxY) {
            offset = maxY - h;
        }
        else {
        }
        if (offset < 0) {
            offset = 0;
        }
        else {
        }
        HtmlVerticalScroll(htmlPtr, offset);
        htmlPtr->flags |= ANIMATE_IMAGES;
    }
    return TCL_OK;
}

/* The pSelStartBlock and pSelEndBlock values have been changed.
** This routine's job is to loop over all HtmlBlocks and either
** set or clear the HTML_Selected bits in the .base.flags field
** as appropriate.  For every HtmlBlock where the bit changes,
** mark that block for redrawing.
*/
static void
UpdateSelection(htmlPtr)
    HtmlWidget *htmlPtr;
{
    int selected = 0;
    HtmlIndex tempIndex;
    HtmlBlock *pTempBlock;
    int temp;
    HtmlBlock *p;

    for (p = htmlPtr->firstBlock; p; p = p->pNext) {
        if (p == htmlPtr->pSelStartBlock) {
            selected = 1;
            HtmlRedrawBlock(htmlPtr, p);
        }
        else if (!selected && p == htmlPtr->pSelEndBlock) {
            selected = 1;
            tempIndex = htmlPtr->selBegin;
            htmlPtr->selBegin = htmlPtr->selEnd;
            htmlPtr->selEnd = tempIndex;
            pTempBlock = htmlPtr->pSelStartBlock;
            htmlPtr->pSelStartBlock = htmlPtr->pSelEndBlock;
            htmlPtr->pSelEndBlock = pTempBlock;
            temp = htmlPtr->selStartIndex;
            htmlPtr->selStartIndex = htmlPtr->selEndIndex;
            htmlPtr->selEndIndex = temp;
            HtmlRedrawBlock(htmlPtr, p);
        }
        else {
        }
        if (p->base.flags & HTML_Selected) {
            if (!selected) {
                p->base.flags &= ~HTML_Selected;
                HtmlRedrawBlock(htmlPtr, p);
            }
            else {
            }
        }
        else {
            if (selected) {
                p->base.flags |= HTML_Selected;
                HtmlRedrawBlock(htmlPtr, p);
            }
            else {
            }
        }
        if (p == htmlPtr->pSelEndBlock) {
            selected = 0;
            HtmlRedrawBlock(htmlPtr, p);
        }
        else {
        }
    }
}

/* Given the selection end-points in htmlPtr->selBegin
** and htmlPtr->selEnd, recompute pSelBeginBlock and
** pSelEndBlock, then call UpdateSelection to update the
** display.
**
** This routine should be called whenever the selection
** changes or whenever the set of HtmlBlock structures
** change.
*/
void
HtmlUpdateSelection(htmlPtr, forceUpdate)
    HtmlWidget *htmlPtr;
    int forceUpdate;
{
    HtmlBlock *pBlock;
    int index;
    int needUpdate = forceUpdate;
    int temp;

    if (htmlPtr->selEnd.p == 0) {
        htmlPtr->selBegin.p = 0;
    }
    else {
    }
    HtmlIndexToBlockIndex(htmlPtr, htmlPtr->selBegin, &pBlock, &index);
    if (needUpdate || pBlock != htmlPtr->pSelStartBlock) {
        needUpdate = 1;
        HtmlRedrawBlock(htmlPtr, htmlPtr->pSelStartBlock);
        htmlPtr->pSelStartBlock = pBlock;
        htmlPtr->selStartIndex = index;
    }
    else if (index != htmlPtr->selStartIndex) {
        HtmlRedrawBlock(htmlPtr, pBlock);
        htmlPtr->selStartIndex = index;
    }
    else {
    }
    if (htmlPtr->selBegin.p == 0) {
        htmlPtr->selEnd.p = 0;
    }
    else {
    }
    HtmlIndexToBlockIndex(htmlPtr, htmlPtr->selEnd, &pBlock, &index);
    if (needUpdate || pBlock != htmlPtr->pSelEndBlock) {
        needUpdate = 1;
        HtmlRedrawBlock(htmlPtr, htmlPtr->pSelEndBlock);
        htmlPtr->pSelEndBlock = pBlock;
        htmlPtr->selEndIndex = index;
    }
    else if (index != htmlPtr->selEndIndex) {
        HtmlRedrawBlock(htmlPtr, pBlock);
        htmlPtr->selEndIndex = index;
    }
    else {
    }
    if (htmlPtr->pSelStartBlock
        && htmlPtr->pSelStartBlock == htmlPtr->pSelEndBlock
        && htmlPtr->selStartIndex > htmlPtr->selEndIndex) {
        temp = htmlPtr->selStartIndex;
        htmlPtr->selStartIndex = htmlPtr->selEndIndex;
        htmlPtr->selEndIndex = temp;
    }
    else {
    }
    if (needUpdate) {
        htmlPtr->flags |= ANIMATE_IMAGES;
        UpdateSelection(htmlPtr);
    }
    else {
    }
}

void
HtmlLostSelection(clientData)
    ClientData clientData;             /* Information about table widget. */
{
    HtmlWidget *htmlPtr = (HtmlWidget *) clientData;
    CONST char *argv[3];
    argv[2] = "";
    if (htmlPtr->exportSelection) {
        HtmlSelectionClearCmd(htmlPtr, 0, 3, argv);
    }
}

/*
** WIDGET selection set INDEX INDEX
*/
int
HtmlSelectionSetCmd(clientData, interp, argc, argv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int argc;                          /* Number of arguments */
    const char **argv;                 /* List of all arguments */
{
    HtmlWidget *htmlPtr = (HtmlWidget *) clientData;
    HtmlIndex selBegin, selEnd;
    int bi, ei;

    HtmlLock(htmlPtr);
    if (HtmlGetIndex(htmlPtr, argv[3], &selBegin.p, &selBegin.i)) {
        if (!HtmlUnlock(htmlPtr)) {
            Tcl_AppendResult(interp, "malformed index: \"", argv[3], "\"", NULL);
        }
        return TCL_ERROR;
    }
    if (HtmlIsDead(htmlPtr))
        return TCL_OK;
    if (HtmlGetIndex(htmlPtr, argv[4], &selEnd.p, &selEnd.i)) {
        if (!HtmlUnlock(htmlPtr)) {
            Tcl_AppendResult(interp, "malformed index: \"", argv[4], "\"", NULL);
        }
        return TCL_ERROR;
    }
    if (HtmlUnlock(htmlPtr))
        return TCL_OK;
    bi = HtmlTokenNumber(selBegin.p);
    ei = HtmlTokenNumber(selEnd.p);
    if (!(selBegin.p && selEnd.p))
        return TCL_OK;
    if (bi < ei || (bi == ei && selBegin.i <= selEnd.i)) {
        htmlPtr->selBegin = selBegin;
        htmlPtr->selEnd = selEnd;
    }
    else {
        htmlPtr->selBegin = selEnd;
        htmlPtr->selEnd = selBegin;
    }
    HtmlUpdateSelection(htmlPtr, 0);
    if (htmlPtr->exportSelection) {
        Tk_OwnSelection(htmlPtr->tkwin, XA_PRIMARY, HtmlLostSelection,
                        (ClientData) htmlPtr);
    }

    return TCL_OK;
}

/*
** WIDGET selection clear
*/
int
HtmlSelectionClearCmd(clientData, interp, argc, argv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int argc;                          /* Number of arguments */
    const char **argv;                 /* List of all arguments */
{
    HtmlWidget *htmlPtr = (HtmlWidget *) clientData;
    htmlPtr->pSelStartBlock = 0;
    htmlPtr->pSelEndBlock = 0;
    htmlPtr->selBegin.p = 0;
    htmlPtr->selEnd.p = 0;
    UpdateSelection(htmlPtr);
    return TCL_OK;
}

#endif /* _TCLHTML_ */

#

/*
** Recompute the position of the insertion cursor based on the
** position in htmlPtr->ins.
*/
void
HtmlUpdateInsert(htmlPtr)
    HtmlWidget *htmlPtr;
{
#ifndef _TCLHTML_
    if (htmlPtr->TclHtml)
        return;
    HtmlIndexToBlockIndex(htmlPtr, htmlPtr->ins,
                          &htmlPtr->pInsBlock, &htmlPtr->insIndex);
    HtmlRedrawBlock(htmlPtr, htmlPtr->pInsBlock);
    if (htmlPtr->insTimer == 0) {
        htmlPtr->insStatus = 0;
        HtmlFlashCursor(htmlPtr);
    }
    else {
    }
#endif /* _TCLHTML_ */
}

/*
** WIDGET token handler TAG ?SCRIPT?
*/
int
HtmlTokenHandlerCmd(clientData, interp, argc, argv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int argc;                          /* Number of arguments */
    const char **argv;                 /* List of all arguments */
{
    HtmlWidget *htmlPtr = (HtmlWidget *) clientData;
    int type = HtmlNameToType(htmlPtr, argv[3]);
    if (type == Html_Unknown) {
        Tcl_AppendResult(interp, "unknown tag: \"", argv[3], "\"", NULL);
        return TCL_ERROR;
    }
    if (argc == 4) {
        if (htmlPtr->zHandler[type] != 0) {
            interp->result = htmlPtr->zHandler[type];
        }
    }
    else {
        if (htmlPtr->zHandler[type] != 0) {
            HtmlFree(htmlPtr->zHandler[type]);
        }
        htmlPtr->zHandler[type] = HtmlAlloc(strlen(argv[4]) + 1);
        if (htmlPtr->zHandler[type]) {
            strcpy(htmlPtr->zHandler[type], argv[4]);
        }
    }
    return TCL_OK;
}

/*
** WIDGET index INDEX	
*/
int
HtmlIndexCmd(clientData, interp, argc, argv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int argc;                          /* Number of arguments */
    const char **argv;                 /* List of all arguments */
{
    HtmlElement *p;
    int i;
    HtmlWidget *htmlPtr = (HtmlWidget *) clientData;

    HtmlLock(htmlPtr);
    if (HtmlGetIndex(htmlPtr, argv[2], &p, &i) != 0) {
        if (!HtmlUnlock(htmlPtr)) {
            Tcl_AppendResult(interp, "malformed index: \"", argv[2], "\"", NULL);
        }
        return TCL_ERROR;
    }
    if (!HtmlUnlock(htmlPtr) && p) {
        sprintf(interp->result, "%d.%d", HtmlTokenNumber(p), i);
    }
    else {
    }
    return TCL_OK;
}

/*
** WIDGET insert INDEX
*/
int
HtmlInsertCmd(clientData, interp, argc, argv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int argc;                          /* Number of arguments */
    const char **argv;                 /* List of all arguments */
{
    HtmlIndex ins;
    HtmlWidget *htmlPtr = (HtmlWidget *) clientData;
    if (argv[2][0] == 0) {
        HtmlRedrawBlock(htmlPtr, htmlPtr->pInsBlock);
        htmlPtr->insStatus = 0;
        htmlPtr->pInsBlock = 0;
        htmlPtr->ins.p = 0;
    }
    else {
        HtmlLock(htmlPtr);
        if (HtmlGetIndex(htmlPtr, argv[2], &ins.p, &ins.i)) {
            if (!HtmlUnlock(htmlPtr)) {
                Tcl_AppendResult(interp, "malformed index: \"", argv[1], "\"",
                                 0);
            }
            return TCL_ERROR;
        }
        if (HtmlUnlock(htmlPtr))
            return TCL_OK;
        HtmlRedrawBlock(htmlPtr, htmlPtr->pInsBlock);
        htmlPtr->ins = ins;
        HtmlUpdateInsert(htmlPtr);
    }
    return TCL_OK;
}

/*
** WIDGET debug dump START END
*/
int
HtmlDebugDumpCmd(clientData, interp, argc, argv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int argc;                          /* Number of arguments */
    const char **argv;                 /* List of all arguments */
{
    HtmlWidget *htmlPtr = (HtmlWidget *) clientData;
    HtmlElement *pStart, *pEnd;
    int i;

    if (HtmlGetIndex(htmlPtr, argv[3], &pStart, &i) != 0) {
        Tcl_AppendResult(interp, "malformed index: \"", argv[3], "\"", NULL);
        return TCL_ERROR;
    }
    if (HtmlGetIndex(htmlPtr, argv[4], &pEnd, &i) != 0) {
        Tcl_AppendResult(interp, "malformed index: \"", argv[4], "\"", NULL);
        return TCL_ERROR;
    }
    if (pStart) {
        HtmlPrintList(htmlPtr, pStart, pEnd ? pEnd->base.pNext : 0);
    }
    return TCL_OK;
}

/*
** WIDGET debug testpt FILENAME
*/
int
HtmlDebugTestPtCmd(clientData, interp, argc, argv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int argc;                          /* Number of arguments */
    const char **argv;                 /* List of all arguments */
{
    HtmlWidget *htmlPtr = (HtmlWidget *) clientData;
    HtmlTestPointDump(argv[3]);
    return TCL_OK;
}
