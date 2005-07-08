static char const rcsid[] =
        "@(#) $Id: htmltcl.c,v 1.32 2005/07/08 09:22:33 danielk1977 Exp $";

/*
** The main routine for the HTML widget for Tcl/Tk
**
** This source code is released into the public domain by the author,
** D. Richard Hipp, on 2002 December 17.  Instead of a license, here
** is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
*/
#include <tk.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include "html.h"

#define SafeCheck(interp,str) if (Tcl_IsSafe(interp)) { \
    Tcl_AppendResult(interp, str, " invalid in safe interp", 0); \
    return TCL_ERROR; \
}

/*
 * Default values for some of the options declared in htmlOptionSpec.
 */
#define DEF_HTML_HEIGHT "600"
#define DEF_HTML_WIDTH "800"
#define DEF_HTML_XSCROLLINCREMENT "20"
#define DEF_HTML_YSCROLLINCREMENT "20"
#define DEF_HTML_DEFAULTSTYLE "html"

/*
 * Mask flags for options declared in htmlOptionSpec.
 */
#define OPTION_REQUIRE_UPDATE 0x00000001

static Tk_OptionSpec htmlOptionSpec[] = {
    {TK_OPTION_PIXELS, 
     "-height", "height", "Height", DEF_HTML_HEIGHT, 
        -1, Tk_Offset(HtmlOptions, height), 0, 0, 0},
    {TK_OPTION_PIXELS, 
     "-width", "width", "Width", DEF_HTML_WIDTH, 
        -1, Tk_Offset(HtmlOptions, width), 0, 0, 0},
    {TK_OPTION_PIXELS, "-yscrollincrement", "yScrollIncrement", 
        "ScrollIncrement", DEF_HTML_YSCROLLINCREMENT, -1, 
         Tk_Offset(HtmlOptions, yscrollincrement), 0, 0, 0},
    {TK_OPTION_PIXELS, "-xscrollincrement", "xScrollIncrement", 
        "ScrollIncrement", DEF_HTML_XSCROLLINCREMENT, -1, 
         Tk_Offset(HtmlOptions, xscrollincrement), 0, 0, 0},
    {TK_OPTION_STRING, "-xscrollcommand", "xScrollCommand", 
        "ScrollCommand", "",  
         Tk_Offset(HtmlOptions, xscrollcommand), -1, 0, 0, 0},
    {TK_OPTION_STRING, "-yscrollcommand", "yScrollCommand", 
        "ScrollCommand", "",  
         Tk_Offset(HtmlOptions, yscrollcommand), -1, 0, 0, 0},
    {TK_OPTION_STRING, 
        "-defaultstyle", "defaultStyle", "DefaultStyle", DEF_HTML_DEFAULTSTYLE,
         Tk_Offset(HtmlOptions, defaultstyle), -1, OPTION_REQUIRE_UPDATE, 
         0, 0 },
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
        (char *) NULL, 0, 0, 0, 0}
};

/*
 *---------------------------------------------------------------------------
 *
 * configureCommand --
 *
 *     Implementation of the standard Tk "configure" command.
 *
 *     <widget> configure -OPTION VALUE ?-OPTION VALUE? ...
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
configureCommand(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    int mask;
    int geometry_request = 0;

    if (!pTree->optionTable) {
        pTree->optionTable = Tk_CreateOptionTable(interp, htmlOptionSpec);
        Tk_InitOptions(interp, 
                (char *)&pTree->options, pTree->optionTable, pTree->tkwin);
        geometry_request = 1;
    }
    if (TCL_OK != Tk_SetOptions(
            interp, (char *)&pTree->options, pTree->optionTable,
            objc-2, &objv[2], pTree->tkwin, 0, &mask)
    ) {
        return TCL_ERROR;
    }

    /* The minimum values for width and height are 100 pixels */
    pTree->options.height = MAX(pTree->options.height, 100);
    pTree->options.width = MAX(pTree->options.width, 100);

    if (geometry_request) {
        int w = pTree->options.width;
        int h = pTree->options.height;
        Tk_GeometryRequest(pTree->tkwin, w, h);
    }

    /* If the OPTION_REQUIRE_UPDATE bit is set in the returned mask, then
     * one or more options that influence the layout have been modified.
     * Invoke the Tcl script "$widget update" to deal with this.
     */
    if (mask & OPTION_REQUIRE_UPDATE) {
        Tcl_Obj *pCmd = Tcl_DuplicateObj(objv[0]);
        Tcl_IncrRefCount(pCmd);
        Tcl_ListObjAppendElement(interp, pCmd, Tcl_NewStringObj("update", -1));
        Tcl_EvalObjEx(interp, pCmd, TCL_EVAL_DIRECT);
        Tcl_DecrRefCount(pCmd);
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * cgetCommand --
 *
 *     Standard Tk "cget" command for querying options.
 *
 *     <widget> cget -OPTION
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int cgetCommand(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    Tcl_Obj *pRet;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "-OPTION");
        return TCL_ERROR;
    }

    pRet = Tk_GetOptionValue(interp, (char *)&pTree->options, 
            pTree->optionTable, objv[2], pTree->tkwin);
    if( pRet ) {
        Tcl_SetObjResult(interp, pRet);
    } else {
        Tcl_AppendResult(
                interp, "unknown option \"", Tcl_GetString(objv[2]), "\"", 0);
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * commandCommand --
 *
 *     <widget> command SUB-COMMAND SCRIPT
 *
 *     This command is used to dynamically add commands to the widget.
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
commandCommand(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    Tcl_Obj *pCmd;
    Tcl_Obj *pScript;
    Tcl_HashEntry *pEntry;
    int newentry;
    HtmlTree *pTree = (HtmlTree *)clientData;

    if (objc != 4) {
        Tcl_WrongNumArgs(interp, 2, objv, "SUB-COMMAND SCRIPT");
        return TCL_ERROR;
    }

    pCmd = objv[2];
    pScript = objv[3];

    pEntry = Tcl_CreateHashEntry(&pTree->aCmd, Tcl_GetString(pCmd), &newentry);
    if (!newentry) {
        Tcl_Obj *pOld = (Tcl_Obj *)Tcl_GetHashValue(pEntry);
        Tcl_DecrRefCount(pOld);
    }
    Tcl_IncrRefCount(pScript);
    Tcl_SetHashValue(pEntry, pScript);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * varCommand --
 *
 *     Set or get the value of a variable from the widgets built-in
 *     dictionary. This is used by the widget logic programmed in Tcl to
 *     store state data.
 *
 *     $html var VAR-NAME ?Value?
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
varCommand(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    Tcl_HashEntry *pEntry; 
    HtmlTree *pTree = (HtmlTree *)clientData;
    char *zVar;

    if (objc != 3 && objc != 4) {
        Tcl_WrongNumArgs(interp, 2,objv, "VAR-NAME ?VALUE?");
        return TCL_ERROR;
    }

    zVar = Tcl_GetString(objv[2]);
    if (objc == 4) {
        int newentry;
        pEntry = Tcl_CreateHashEntry(&pTree->aVar, zVar, &newentry);
        if (!newentry) {
            Tcl_Obj *pOld = (Tcl_Obj *)Tcl_GetHashValue(pEntry);
            Tcl_DecrRefCount(pOld);
        }
        Tcl_IncrRefCount(objv[3]);
        Tcl_SetHashValue(pEntry, objv[3]);
    } else {
        pEntry = Tcl_FindHashEntry(&pTree->aVar, zVar);
        if (!pEntry) {
            Tcl_AppendResult(interp, "No such variable: ", zVar, 0);
            return TCL_ERROR;
        }
    }

    Tcl_SetObjResult(interp, (Tcl_Obj *)Tcl_GetHashValue(pEntry));
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * resetCmd --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int resetCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlTreeClear(pTree);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * parseCmd --
 *
 *     $widget internal parse HTML-TEXT
 * 
 *     Appends the given HTML text to the end of any HTML text that may have
 *     been inserted by prior calls to this command.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
parseCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    int i;
    char *arg1;
    HtmlTree *pTree = (HtmlTree *)clientData;

    /* Add the new text to the internal cache of the document. Also tokenize
     * it and add the new HtmlToken objects to the HtmlTree.pFirst/pLast 
     * linked list.
     */
    arg1 = Tcl_GetStringFromObj(objv[3], &i);
    HtmlTokenizerAppend(pTree, arg1, i);

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * parseFinalCmd --
 *
 *     $widget internal parsefinal
 * 
 *     This is called by the Tcl part of the widget when the entire
 *     document has been parsed (i.e. when the -final switch is passed to
 *     [$widget parse]). We need to set a flag so that no more text may be
 *     added to the document and execute any outstanding node-handler 
 *     callbacks.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
parseFinalCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    pTree->parseFinished = 1;
    HtmlFinishNodeHandlers(pTree);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * rootCmd --
 *
 *     $widget internal root
 *
 *     Returns the node command for the root node of the document. Or, if
 *     the tree has not been built, throws an error.
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
rootCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *CONST objv[];             /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    if (!pTree->pRoot) {
        Tcl_SetResult(interp, "", TCL_STATIC);
    } else {
        Tcl_Obj *pCmd = HtmlNodeCommand(interp, pTree->pRoot);
        Tcl_SetObjResult(interp, pCmd);
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * HtmlHandlerScriptCmd --
 *
 *     $widget handler script TAG SCRIPT
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
handlerScriptCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    Tcl_Obj *pTag;
    int tag;
    Tcl_Obj *pScript;
    Tcl_HashEntry *pEntry;
    int newentry;
    HtmlTree *pTree = (HtmlTree *)clientData;

    if (objc!=5) {
        Tcl_WrongNumArgs(interp, 3, objv, "TAG SCRIPT");
        return TCL_ERROR;
    }

    pTag = objv[3];
    pScript = objv[4];
    tag = HtmlNameToType(0, Tcl_GetString(pTag));

    if (tag==Html_Unknown) {
        Tcl_AppendResult(interp, "Unknown tag type: ", Tcl_GetString(pTag), 0);
        return TCL_ERROR;
    }

    pEntry = Tcl_CreateHashEntry(&pTree->aScriptHandler,(char *)tag,&newentry);
    if (!newentry) {
        Tcl_Obj *pOld = (Tcl_Obj *)Tcl_GetHashValue(pEntry);
        Tcl_DecrRefCount(pOld);
    }

    Tcl_IncrRefCount(pScript);
    Tcl_SetHashValue(pEntry, (ClientData)pScript);

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * handlerNodeCmd --
 *
 *     $widget handler node TAG SCRIPT
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
handlerNodeCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    Tcl_Obj *pTag;
    int tag;
    Tcl_Obj *pScript;
    Tcl_HashEntry *pEntry;
    int newentry;
    HtmlTree *pTree = (HtmlTree *)clientData;

    if (objc!=5) {
        Tcl_WrongNumArgs(interp, 3, objv, "TAG SCRIPT");
        return TCL_ERROR;
    }

    pTag = objv[3];
    pScript = objv[4];
    tag = HtmlNameToType(0, Tcl_GetString(pTag));

    if (tag==Html_Unknown) {
        Tcl_AppendResult(interp, "Unknown tag type: ", Tcl_GetString(pTag), 0);
        return TCL_ERROR;
    }

    pEntry = Tcl_CreateHashEntry(&pTree->aNodeHandler,(char *)tag,&newentry);
    if (!newentry) {
        Tcl_Obj *pOld = (Tcl_Obj *)Tcl_GetHashValue(pEntry);
        Tcl_DecrRefCount(pScript);
    }

    Tcl_IncrRefCount(pScript);
    Tcl_SetHashValue(pEntry, (ClientData)pScript);

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * styleParseCmd --
 * styleApplyCmd --
 * styleSyntaxErrsCmd --
 * layoutPrimitivesCmd --
 * layoutImageCmd --
 * layoutForceCmd --
 * layoutSizeCmd --
 * layoutNodeCmd --
 * widgetPaintCmd --
 * widgetScrollCmd --
 * widgetMapControlsCmd --
 * bboxCmd --
 *
 *     New versions of gcc don't allow pointers to non-local functions to
 *     be used as constant initializers (which we need to do in the
 *     aSubcommand[] array inside HtmlWidgetObjCommand(). So the following
 *     functions are wrappers around Tcl_ObjCmdProc functions implemented
 *     in other files.
 *
 * Results:
 *     Tcl result (i.e. TCL_OK, TCL_ERROR).
 *
 * Side effects:
 *     Whatever the called function does.
 *
 *---------------------------------------------------------------------------
 */
static int styleParseCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlStyleParse(clientData, interp, objc, objv);
}
static int 
styleApplyCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlStyleApply(clientData, interp, objc, objv);
}
static int 
styleSyntaxErrsCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlStyleSyntaxErrs(clientData, interp, objc, objv);
}
static int 
layoutPrimitivesCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlLayoutPrimitives(clientData, interp, objc, objv);
}
static int 
layoutImageCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlLayoutImage(clientData, interp, objc, objv);
}
static int 
layoutForceCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlLayoutForce(clientData, interp, objc, objv);
}
static int 
layoutSizeCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlLayoutSize(clientData, interp, objc, objv);
}
static int 
layoutNodeCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlLayoutNode(clientData, interp, objc, objv);
}
static int 
widgetPaintCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlWidgetPaint(clientData, interp, objc, objv);
}
static int 
widgetScrollCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlWidgetScroll(clientData, interp, objc, objv);
}
static int 
widgetMapControlsCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlWidgetMapControls(clientData, interp, objc, objv);
}
static int 
bboxCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlLayoutBbox(clientData, interp, objc, objv);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlWidgetObjCommand --
 *
 *     This is the C function invoked for a widget command.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int HtmlWidgetObjCommand(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    /* The following array defines all the widget commands.  This function
     * just parses the first one or two arguments and vectors control to
     * one of the command service routines defined in the following array.
     */
    static struct SC {
        char *zCmd1;                /* First-level subcommand.  Required */
        char *zCmd2;                /* Second-level subcommand.  May be NULL */
        Tcl_ObjCmdProc *xFuncObj;   /* Object cmd */
    } aSubcommand[] = {
        {
        "internal", "parse", parseCmd}, {
        "internal", "parsefinal", parseFinalCmd}, {
        "internal", "root",  rootCmd}, {
        "internal", "reset", resetCmd}, {
        "internal", "bbox", bboxCmd}, {

        "handler", "script", handlerScriptCmd}, {
        "handler", "node", handlerNodeCmd}, {

        "style", "parse", styleParseCmd}, {
        "style", "apply", styleApplyCmd}, {
        "style", "syntax_errs", styleSyntaxErrsCmd}, {

        "layout", "primitives", layoutPrimitivesCmd}, {
        "layout", "image",      layoutImageCmd}, {
        "layout", "force",      layoutForceCmd}, {
        "layout", "size",       layoutSizeCmd}, {
        "layout", "node",       layoutNodeCmd}, {

        "widget", "paint", widgetPaintCmd}, {
        "widget", "scroll", widgetScrollCmd}, {
        "widget", "mapcontrols", widgetMapControlsCmd}, {

        "var", 0, varCommand}, { 
        "command", 0, commandCommand}, {
        "configure", 0, configureCommand}, {
        "cget", 0, cgetCommand},
    };

    int i;
    CONST char *zArg1 = 0;
    CONST char *zArg2 = 0;
    Tcl_Obj *pError;
    Tcl_HashEntry *pEntry;
    HtmlTree *pTree = (HtmlTree *)clientData;

    int matchone = 0; /* True if the first argument matched something */
    int multiopt = 0; /* True if their were multiple options for second match */
    CONST char *zBad;

    if (objc>1) {
        zArg1 = Tcl_GetString(objv[1]);
    }
    if (objc>2) {
        zArg2 = Tcl_GetString(objv[2]);
    }

    /* See if this is a Tcl implemented command */
    pEntry = Tcl_FindHashEntry(&pTree->aCmd, zArg1);
    if (pEntry) {
        int rc;
        Tcl_Obj *pScript = (Tcl_Obj *)Tcl_GetHashValue(pEntry);
        Tcl_Obj *pList = Tcl_NewListObj(objc - 2, &objv[2]);

        pScript = Tcl_DuplicateObj(pScript);
        Tcl_IncrRefCount(pScript);
        Tcl_IncrRefCount(pList);

        Tcl_ListObjAppendList(interp, pScript, pList);
        rc = Tcl_EvalObjEx(interp, pScript, TCL_EVAL_DIRECT|TCL_EVAL_GLOBAL);

        Tcl_DecrRefCount(pList);
        Tcl_DecrRefCount(pScript);

        return rc;
    }

    /* Search the array of built-in commands */
    for (i=0; i<sizeof(aSubcommand)/sizeof(struct SC); i++) {
         struct SC *pCommand = &aSubcommand[i];
         if (zArg1 && 0==strcmp(zArg1, pCommand->zCmd1)) {
             matchone = 1;
             if (!pCommand->zCmd2 || 
                 (zArg2 && 0==strcmp(zArg2, pCommand->zCmd2))) 
             {
                 return pCommand->xFuncObj(clientData, interp, objc, objv);
             }
         }
    }

    /* Failed to find a matching sub-command. The remainder of this routine
     * is generating an error message. 
     */
    zBad = matchone?zArg2:zArg1;
    pError = Tcl_NewStringObj("bad option \"", -1);
    Tcl_IncrRefCount(pError);
    Tcl_AppendToObj(pError, zBad, -1);
    Tcl_AppendToObj(pError, "\" must be ", -1);
    zBad = 0;
    for (i=0; i<sizeof(aSubcommand)/sizeof(struct SC); i++) {
        struct SC *pCommand = &aSubcommand[i];
        CONST char *zAdd = 0;

        if (matchone) { 
            if (0==strcmp(pCommand->zCmd1, zArg1)) {
                zAdd = pCommand->zCmd2;
            }
        } else if (!zBad || strcmp(pCommand->zCmd1, zBad)) {
            zAdd = pCommand->zCmd1;
        }

        if (zAdd) {
            if (zBad) {
                Tcl_AppendToObj(pError, zBad, -1);
                Tcl_AppendToObj(pError, ", ", -1);
                multiopt = 1;
            }
            zBad = zAdd;
        }
    }
    if (zBad) {
        if (multiopt) {
            Tcl_AppendToObj(pError, "or ", -1);
        }
        Tcl_AppendToObj(pError, zBad, -1);
    }
    Tcl_SetObjResult(interp, pError);
    Tcl_DecrRefCount(pError);
    
    return TCL_ERROR;
}


/*
 *---------------------------------------------------------------------------
 *
 * deleteWidget --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void deleteWidget(clientData)
    ClientData clientData;
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlTreeClear(pTree);
}

/*
 *---------------------------------------------------------------------------
 *
 * newWidget --
 *
 *     Create a new Html widget command:
 *
 *         html PATH ?<options>?
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int newWidget(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlTree *pTree;
    CONST char *zCmd;
    int rc;

    if (objc<2) {
        Tcl_WrongNumArgs(interp, 1, objv, "WINDOW-PATH ?OPTIONS?");
        return TCL_ERROR;
    }
    
    zCmd = Tcl_GetString(objv[1]);
    pTree = (HtmlTree *)ckalloc(sizeof(HtmlTree));
    memset(pTree, 0, sizeof(HtmlTree));

    /* Create the Tk window.
     *
     * The outer window has the class "Html". The inner window is of class
     * "HtmlClip". Some of the logic for the widget is implemented via
     * bindings to these two window classes in file 'tkhtml.tcl'.
     */
    pTree->win = Tk_MainWindow(interp);
    pTree->tkwin = Tk_CreateWindowFromPath(interp, pTree->win, zCmd, NULL); 
    if (!pTree->tkwin) {
        goto error_out;
    }
    Tk_SetClass(pTree->tkwin, "Html");

    pTree->interp = interp;
    Tcl_InitHashTable(&pTree->aScriptHandler, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&pTree->aNodeHandler, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&pTree->aImage, TCL_STRING_KEYS);
    Tcl_InitHashTable(&pTree->aFontCache, TCL_STRING_KEYS);
    Tcl_InitHashTable(&pTree->aColor, TCL_STRING_KEYS);
    Tcl_InitHashTable(&pTree->aCmd, TCL_STRING_KEYS);
    Tcl_InitHashTable(&pTree->aVar, TCL_STRING_KEYS);
    Tcl_CreateObjCommand(interp,zCmd,HtmlWidgetObjCommand,pTree,deleteWidget);

    rc = configureCommand(pTree, interp, objc, objv);

    /* Return the name of the widget just created. */
    Tcl_SetObjResult(interp, objv[1]);
    return TCL_OK;

    /* Exception handler. Jump here if an error occurs during
     * initialisation. An error message should already have been written
     * into the result of the interpreter.
     */
error_out:
    if (pTree->tkwin) {
        Tk_DestroyWindow(pTree->tkwin);
    }
    if (pTree) {
        ckfree((char *)pTree);
    }
    return TCL_ERROR;
}

#if INTERFACE
#define DLL_EXPORT
#endif
#if defined(USE_TCL_STUBS) && defined(__WIN32__)
# undef DLL_EXPORT
# define DLL_EXPORT __declspec(dllexport)
#endif

#ifndef DLL_EXPORT
#define DLL_EXPORT
#endif

/*
 *---------------------------------------------------------------------------
 *
 * Tkhtml_SafeInit --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
DLL_EXPORT int Tkhtml_SafeInit(interp)
    Tcl_Interp *interp;
{
    return Tkhtml_Init(interp);
}

/*
 *---------------------------------------------------------------------------
 *
 * Tkhtml_Init --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
DLL_EXPORT int Tkhtml_Init(interp)
    Tcl_Interp *interp;
{
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.3", 0) == 0) {
        return TCL_ERROR;
    }
    if (Tk_InitStubs(interp, "8.3", 0) == 0) {
        return TCL_ERROR;
    }
#endif

    Tcl_CreateObjCommand(interp, "::tk::htmlinternal", newWidget, 0, 0);
    return TCL_OK;
}
