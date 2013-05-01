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
static Tcl_CmdDeleteProc ExprDelete;
static Tcl_CmdDeleteProc FormatDelete;

static void DestroyMeData(Mp_Data *mdPtr);
static void UpdateEpsilon(Mp_Data *mdPtr);

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
    Mp_Data *mdPtr;
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

    mdPtr = (Mp_Data *) ckalloc(sizeof(Mp_Data));
    mdPtr->interp = interp;
    mdPtr->precVarName = MP_PRECISION_VAR;
    mdPtr->precision = 0;
    mdPtr->epsilon = NULL;
    mdPtr->exprCmd = Tcl_CreateCommand (interp, "mpexpr", ExprCmd,
	    (ClientData) mdPtr, ExprDelete);
    mdPtr->funcTable = NULL;
    mdPtr->fmtCmd = Tcl_CreateCommand (interp, "mpformat", FormatCmd,
	    (ClientData) mdPtr, FormatDelete);

    /* set up trace on mp_precision */
    Tcl_TraceVar(interp, mdPtr->precVarName,
            TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS|TCL_TRACE_READS,
            PrecTrace, (ClientData) mdPtr);

    /* Trigger the trace to initialize */
    (void) Tcl_UnsetVar(interp, mdPtr->precVarName, TCL_GLOBAL_ONLY);

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
ExprCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Client Data. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    CONST84 char **argv;		/* Argument strings. */
{
    Tcl_DString buffer;
    int i, result;
    Mp_Data *mdPtr = (Mp_Data *) clientData;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" arg ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }

    if (argc == 2) {
        result = Mp_ExprString(interp, argv[1], mdPtr);
	return result;
    }
    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer, argv[1], -1);
    for (i = 2; i < argc; i++) {
	Tcl_DStringAppend(&buffer, " ", 1);
	Tcl_DStringAppend(&buffer, argv[i], -1);
    }
    result = Mp_ExprString(interp, buffer.string, mdPtr);
    Tcl_DStringFree(&buffer);
    return result;

}

static void
ExprDelete(clientData)
    ClientData clientData;
{
    Mp_Data *mdPtr = (Mp_Data *)clientData;

    if (mdPtr->funcTable) {
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;

	for (hPtr = Tcl_FirstHashEntry(mdPtr->funcTable, &search); hPtr;
		hPtr = Tcl_NextHashEntry(&search)) {
	    ckfree((char *) Tcl_GetHashValue(hPtr));
	}
	Tcl_DeleteHashTable(mdPtr->funcTable);
	ckfree((char *)mdPtr->funcTable);
	mdPtr->funcTable = NULL;
    }
    mdPtr->exprCmd = NULL;
    if (mdPtr->fmtCmd == NULL) {
	DestroyMeData(mdPtr);
    }
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

static void
FormatDelete(clientData)
    ClientData clientData;
{
    Mp_Data *mdPtr = (Mp_Data *)clientData;

    mdPtr->fmtCmd = NULL;
    if (mdPtr->exprCmd == NULL) {
	DestroyMeData(mdPtr);
    }
}

static void
DestroyMeData(mdPtr)
    Mp_Data *mdPtr;
{
    Tcl_UntraceVar(mdPtr->interp, mdPtr->precVarName,
            TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS|TCL_TRACE_READS,
            PrecTrace, (ClientData) mdPtr);
    ckfree((char *)mdPtr);
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
    char mp_buf[6];
    long prec;
    char *result = NULL;

    Mp_Data *mdPtr = (Mp_Data *)clientData;

    if (flags & TCL_TRACE_UNSETS) {
	/*
	 * If the variable is unset, then recreate the trace and restore
	 * the default precision value, unless the interp is dying.
	 */
	if (Tcl_InterpDeleted(interp)) {
	    return result;
	}
	Tcl_TraceVar(interp, mdPtr->precVarName, TCL_GLOBAL_ONLY|
		TCL_TRACE_WRITES|TCL_TRACE_UNSETS|TCL_TRACE_READS,
		PrecTrace, clientData);
	mdPtr->precision = MP_PRECISION_DEF;
	UpdateEpsilon(mdPtr);
    } else if (flags & TCL_TRACE_WRITES) {
	CONST84 char *sVal;
	int iVal;

	result = "improper precision value";

	sVal = Tcl_GetVar(interp, mdPtr->precVarName, TCL_GLOBAL_ONLY);
	if (sVal) {
	    /* Avoid Octal problem */
	    while (*sVal == '0' && sVal[1] != '\0') {
		sVal++;
	    }
	    if ((TCL_OK == Tcl_GetInt(interp, sVal, &iVal))
		    && (iVal >= 0) && (iVal <= MP_PRECISION_MAX)) {

		mdPtr->precision = iVal;
		UpdateEpsilon(mdPtr);
		result = NULL;
	    }
	}
    }

    sprintf(mp_buf, "%ld", mdPtr->precision);
    Tcl_SetVar(interp, mdPtr->precVarName, mp_buf, TCL_GLOBAL_ONLY);
    return result;
}

static void
UpdateEpsilon(mdPtr)
    Mp_Data *mdPtr;
{
    if (mdPtr->epsilon) {
	qfree(mdPtr->epsilon);
    }
    mdPtr->epsilon = qalloc();
    ztenpow(mdPtr->precision, &(mdPtr->epsilon->den));
}
