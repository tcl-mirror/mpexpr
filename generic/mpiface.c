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
#       define EXPORT(a,b) __declspec(dllexport) a b
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
	
    initmasks();		/* initialize math shift bits */

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
            MpPrecTraceProc, (ClientData) NULL);

    Tcl_CreateCommand (interp, "mpexpr",   Mp_ExprCmd, (ClientData)NULL,
		  (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand (interp, "mpformat", Mp_FormatCmd, (ClientData)NULL,
		  (Tcl_CmdDeleteProc *) NULL);

    if (Tcl_PkgProvide(interp, "Mpexpr", MPEXPR_VERSION) != TCL_OK) {
	return TCL_ERROR;
    }

    return TCL_OK;
}



/*
 *----------------------------------------------------------------------
 *
 * Mp_ExprCmd --
 *
 * interface to Mp_ExprString
 *
 *----------------------------------------------------------------------
 */

	
int
Mp_ExprCmd(dummy, interp, argc, argv)
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
 * Mp_FormatCmd --
 *
 * interface to Qformat
 *
 *----------------------------------------------------------------------
 */

	
int
Mp_FormatCmd(dummy, interp, argc, argv)
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


