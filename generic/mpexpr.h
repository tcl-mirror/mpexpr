/*
 *
 * mpexpr.h 
 * mpexpr - multiple precision expresion evaluator
 *
 *          hacked up from Tcl 7.6 tclExpr.c file
 *          and some from 'calc'.
 *
 * Copyright 1998 Tom Poindexter
 *
 * copyright notices from borrowed code:
 *
 *---------------------------------------------------------------------------
 * Copyright (c) 1994 David I. Bell
 * Permission is granted to use, distribute, or modify this source,
 * provided that this copyright notice remains intact.
 *
 *---------------------------------------------------------------------------
 *
 *      This file contains the code to evaluate expressions for
 *      Tcl.
 *
 *      This implementation of floating-point support was modelled
 *      after an initial implementation by Bill Carpenter.
 *
 *
 * Copyright (c) 1987-1994 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 *
 */


#ifndef _MPEXPRH
#define _MPEXPRH

#include <ctype.h>

#include <tcl.h>

#undef TCL_STORAGE_CLASS
#ifdef BUILD_mpexpr
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# ifdef USE_TCL_STUBS
#  define TCL_STORAGE_CLASS
# else
#  define TCL_STORAGE_CLASS DLLIMPORT
# endif
#endif

#if TCL_MAJOR_VERSION < 8
# define Tcl_GetStringResult(interp) ((interp)->result)
#endif

#if (TCL_MAJOR_VERSION < 8) || ((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION == 0))
typedef void * Tcl_ThreadDataKey;

/* Leaks size bytes per process -- do not care */
#define Tcl_GetThreadData(keyPtr, size) \
    (*(keyPtr) == NULL) ? *(keyPtr) = ckalloc(size) : *(keyPtr)
#endif

#ifndef CONST
# define CONST
#endif

#ifndef CONST84
# define CONST84
#endif

#include "qmath.h"

#define MPEXPR_VERSION   "1.0"

#define MP_PRECISION_DEF      17
#define MP_PRECISION_DEF_STR "17"
#define MP_PRECISION_VAR      "mp_precision"
#define MP_PRECISION_MAX      10000

EXTERN long    mp_precision;
EXTERN NUMBER *mp_epsilon;
 
EXTERN int		Mp_ExprString _ANSI_ARGS_((Tcl_Interp *interp,
			    CONST char *string));
EXTERN int              Mp_FormatString _ANSI_ARGS_((Tcl_Interp *interp,
			    int argc, CONST84 char **argv));

/* hacked tclParse routines that don't rely on Tcl internals */

typedef struct ParseValue {
    char *buffer;               /* Address of first character in
                                 * output buffer. */
    char *next;                 /* Place to store next character in
                                 * output buffer. */
    char *end;                  /* Address of the last usable character
                                 * in the buffer. */
    void (*expandProc) _ANSI_ARGS_((struct ParseValue *pvPtr, int needed));
                                /* Procedure to call when space runs out;
                                 * it will make more space. */
    ClientData clientData;      /* Arbitrary information for use of
                                 * expandProc. */
    int noEval;			/* Whether parsing should include evaluation */
} ParseValue;

EXTERN int              MpParseBraces _ANSI_ARGS_((Tcl_Interp *interp,
                            CONST char *string, CONST char **termPtr, ParseValue *pvPtr));
EXTERN void             MpExpandParseValue _ANSI_ARGS_((ParseValue *pvPtr,
                            int needed));
EXTERN int              MpParseQuotes _ANSI_ARGS_((Tcl_Interp *interp,
                            CONST char *string, int termChar, int flags,
                            CONST char **termPtr, ParseValue *pvPtr));
EXTERN int              MpParseNestedCmd _ANSI_ARGS_((Tcl_Interp *interp,
                            CONST char *string, int flags, CONST char **termPtr,
                            ParseValue *pvPtr));
EXTERN CONST char *   Mp_ParseVar _ANSI_ARGS_((Tcl_Interp *interp,
                            CONST char *string, CONST char **termPtr, int noEval));

EXTERN char *           MpPrecTraceProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, CONST84 char *name1, CONST84 char *name2,
		 	    int flags));


EXTERN NUMBER *		Atoq _ANSI_ARGS_((CONST char *, CONST char **));
#define Qfree(q)  qfree(q); (q)=NULL


/* include declares from tclInt.h, so we won't have to find Tcl source dir */

#define UCHAR(c) ((unsigned char) (c))

/* mpexpr  tcl command procs */

EXTERN Tcl_CmdProc      Mp_ExprCmd;
EXTERN Tcl_CmdProc      Mp_FormatCmd;


#endif 
