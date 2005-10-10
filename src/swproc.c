
#include <tcl.h>
#include "swproc.h"

/*
 *---------------------------------------------------------------------------
 *
 * SwprocRt --
 *
 *     This function is used to interpret the arguments passed to a Tcl
 *     command. The assumption is that Tcl commands take three types of
 *     arguments:
 *
 *         * Regular arguments, the type interpeted automatically by
 *           [proc]. When using this function, regular arguments may not
 *           have default values.
 *
 *         * Switches that take arguments.
 *
 *         * Switches that do not require an argument (called "options").
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
SwprocRt(interp, objc, objv, aConf, apObj)
    Tcl_Interp *interp;               /* Tcl interpreter */
    int objc;
    Tcl_Obj *CONST objv[];
    SwprocConf *aConf;
    Tcl_Obj **apObj;
{
    SwprocConf *pConf;
    int ii;
    int jj;
    int argsatend = 0;
    int argcnt = 0;       /* Number of compulsory arguments */
    int firstarg;         /* Index of first compulsory arg in aConf */
    int lastswitch;       /* Index of element after last switch or option */
    char const *zSwitch;

    /* Set all the entries in apObj[] to 0. This makes cleaning up in the
     * case of an error easier. Also, check whether the compulsory
     * arguments (if any) are at the start or end of the array. Set argcnt
     * to the number of compulsory args.
     */
    argsatend = (aConf[0].eType == SWPROC_ARG) ? 0 : 1;
    for (jj = 0; aConf[jj].eType != SWPROC_END; jj++) {
        apObj[jj] = 0;
        if (aConf[jj].eType == SWPROC_ARG) {
            argcnt++;
        }
    }

    /* Set values of compulsory arguments. Also set all switches and
     * options to their default values. 
     */
    firstarg = argsatend ? (objc - argcnt) : 0;
    ii = firstarg;
    for (jj = 0; aConf[jj].eType != SWPROC_END; jj++) {
        pConf = &aConf[jj];
        if (pConf->eType == SWPROC_ARG) {
            if (ii < objc && ii >= 0) {
                apObj[jj] = objv[ii];
                ii++;
            } else {
                goto error_insufficient_args;
            }
        } else {
            apObj[jj] = Tcl_NewStringObj(pConf->zDefault, -1);
        }
        Tcl_IncrRefCount(apObj[jj]);
    }

    /* Now set values for any options or switches passed */
    lastswitch = (argsatend ? firstarg : objc);
    for (ii = (argsatend ? 0 : argcnt); ii < lastswitch ;ii++) {
        zSwitch = Tcl_GetString(objv[ii]);
        if (zSwitch[0] != '-') {
            goto error_no_such_option;
        }

        for (jj = 0; aConf[jj].eType != SWPROC_END; jj++) {
            pConf = &aConf[jj];
            if (pConf->eType == SWPROC_OPT || pConf->eType == SWPROC_SWITCH) {
                if (0 == strcmp(pConf->zSwitch, &zSwitch[1])) {
                   if (pConf->eType == SWPROC_SWITCH) {
                       Tcl_DecrRefCount(apObj[jj]);
                       apObj[jj] = Tcl_NewStringObj(pConf->zTrue, -1);
                       Tcl_IncrRefCount(apObj[jj]);
                   } else if (ii+1 < lastswitch) {
                       Tcl_DecrRefCount(apObj[jj]);
                       ii++;
                       apObj[jj] = objv[ii];
                       Tcl_IncrRefCount(apObj[jj]);
                   } else {
                       goto error_option_requires_arg;
                   }
                   break;
                }
            }
        }
        if (aConf[jj].eType == SWPROC_END) {
            goto error_no_such_option;
        }
    }

    return TCL_OK;

error_insufficient_args:
    Tcl_AppendResult(interp, "Insufficient args", 0);
    goto error_out;

error_no_such_option:
    Tcl_AppendResult(interp, "No such option: ", zSwitch, 0);
    goto error_out;

error_option_requires_arg:
    Tcl_AppendResult(interp, "Option \"", zSwitch, "\"requires an argument", 0);
    goto error_out;

error_out:
    /* Any error condition eventually jumps here. Discard any accumulated
     * object references and return TCL_ERROR.
     */
    for (jj = 0; aConf[jj].eType != SWPROC_END; jj++) {
        if (apObj[jj]) {
            Tcl_DecrRefCount(apObj[jj]);
            apObj[jj] = 0;
        }
    }
    return TCL_ERROR;
}

