/*
 * mp_iface.c --
 *
 * provide interface and math error handling for mpexpr 
 *
 * Copyright 1998 Tom Poindexter.
 *
 * portions  from Tcl 7.6 source for Tcl_ExprCmd
 *
 * Copyright (c) 1987-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 */


#include "mpexpr.h"


#ifdef __WIN32__
#if defined(__WIN32__)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   undef WIN32_LEAN_AND_MEAN

#   if defined(_MSC_VER)
#       define EXPORT(a,b) a b
#       define DllEntryPoint DllMain
#   else
#       if defined(__BORLANDC__)
#           define EXPORT(a,b) a _export b
#       else
#           define EXPORT(a,b) a b
#       endif
#   endif
#else
#   define EXPORT(a,b) a b
#endif

EXTERN EXPORT(int,Mpexpr_Init) _ANSI_ARGS_((Tcl_Interp *interp));

BOOL APIENTRY
DllEntryPoint(hInst, reason, reserved)
HINSTANCE hInst;            /* Library instance handle. */
DWORD reason;               /* Reason this function is being called. */
LPVOID reserved;            /* Not used. */
{
    return TRUE;
}

#endif

static Tcl_CmdProc ExprCmd;
static Tcl_CmdProc FormatCmd;
static Tcl_VarTraceProc PrecTrace;

/*
 *----------------------------------------------------------------------
 *
 * Mpexpr_Init -
 *
 *    add the mpexpr, mpformat commands and mp_precision variable
 */


int 
Mpexpr_Init (interp)
    Tcl_Interp *interp;
{
    char mp_buf[256];
    static initialized = 0;
    TCL_DECLARE_MUTEX(mpMutex)

    if (!initialized) {
	Tcl_MutexLock(&mpMutex);
	if (!initialized) {
	    initmasks();		/* initialize math shift bits */
	    initialized = 1;
	}
	Tcl_MutexUnlock(&mpMutex);
    }

    /* set mp_precision tcl var */
    (void) Tcl_SetVar(interp, MP_PRECISION_VAR, MP_PRECISION_DEF_STR, 
			TCL_GLOBAL_ONLY);

    /* set mp_precision and mp_epsilon */
    mp_precision = MP_PRECISION_DEF;

    sprintf(mp_buf,"1e-%ld",mp_precision); 
    mp_epsilon = atoqnum(mp_buf);

    /* set up trace on mp_precision */
    Tcl_TraceVar2(interp, MP_PRECISION_VAR, (char *) NULL,
            TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
            PrecTrace, (ClientData) NULL);

    Tcl_CreateCommand (interp, "mpexpr",   ExprCmd, (ClientData)NULL,
		  (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand (interp, "mpformat", FormatCmd, (ClientData)NULL,
		  (Tcl_CmdDeleteProc *) NULL);

    if (Tcl_PkgProvide(interp, "Mpexpr", MPEXPR_VERSION) != TCL_OK) {
	return TCL_ERROR;
    }

    return TCL_OK;
}



/*
 *----------------------------------------------------------------------
 *
 * ExprCmd --
 *
 * interface to Mp_ExprString
 *
 *----------------------------------------------------------------------
 */

	
static int
ExprCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    CONST84 char **argv;		/* Argument strings. */
{
    Tcl_DString buffer;
    int i, result;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" arg ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }

    if (argc == 2) {
        result = Mp_ExprString(interp, argv[1]);
	return result;
    }
    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer, argv[1], -1);
    for (i = 2; i < argc; i++) {
	Tcl_DStringAppend(&buffer, " ", 1);
	Tcl_DStringAppend(&buffer, argv[i], -1);
    }
    result = Mp_ExprString(interp, buffer.string);
    Tcl_DStringFree(&buffer);
    return result;

}


/*
 *----------------------------------------------------------------------
 *
 * FormatCmd --
 *
 * interface to Qformat
 *
 *----------------------------------------------------------------------
 */

	
static int
FormatCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    CONST84 char **argv;		/* Argument strings. */
{

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" arg ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }

    return (Mp_FormatString(interp, argc, argv));

}

/*
 *----------------------------------------------------------------------
 *
 * PrecTrace --
 *
 *      This procedure is invoked whenever the variable "mp_precision"
 *      is written.
 *
 * Results:
 *      Returns NULL if all went well, or an error message if the
 *      new value for the variable doesn't make sense.
 *
 * Side effects:
 *      If the new value doesn't make sense then this procedure
 *      undoes the effect of the variable modification.  Otherwise
 *      it modifies the 'mp_precision' value, and the
 *      'mp_epsilon' q value.
 *
 *----------------------------------------------------------------------
 */

        /* ARGSUSED */

static char *
PrecTrace(clientData, interp, name1, name2, flags)
    ClientData clientData;      /* Not used. */
    Tcl_Interp *interp;         /* Interpreter containing variable. */
    CONST84 char *name1;        /* Name of variable. */
    CONST84 char *name2;        /* Second part of variable name. */
    int flags;                  /* Information about what happened. */
{
    CONST char *value;
    char *end;
    char mp_buf[256];
    long prec;

    /*
     * If the variable is unset, then recreate the trace and restore
     * the default value of the format string.
     */
    if (flags & TCL_TRACE_UNSETS) {
        if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
            Tcl_TraceVar2(interp, name1, name2,
                    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
                    PrecTrace, clientData);
        }

        mp_precision = MP_PRECISION_DEF;
        if (mp_epsilon != NULL) {
            qfree(mp_epsilon);
        }
        sprintf(mp_buf,"1e-%ld",mp_precision);
        mp_epsilon = atoqnum(mp_buf);
        return (char *) NULL;
    }

    value = Tcl_GetVar2(interp, name1, name2, flags & TCL_GLOBAL_ONLY);
    if (value == NULL) {
        value = "";
    }
    prec = strtoul(value, &end, 10);
    if ((prec < 0) || (prec > MP_PRECISION_MAX) ||
            (end == value) || (*end != 0)) {

        sprintf(mp_buf, "%ld", mp_precision);
        Tcl_SetVar2(interp, name1, name2, mp_buf, flags & TCL_GLOBAL_ONLY);
        return "improper value for mp_precision";
    }

    mp_precision = prec;
    if (mp_epsilon != NULL) {
        Qfree(mp_epsilon);
    }
    sprintf(mp_buf,"1e-%ld",mp_precision);
    mp_epsilon = atoqnum(mp_buf);

    return (char *) NULL;
}

