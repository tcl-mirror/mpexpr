/*
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
 *	This file contains the code to evaluate expressions for
 *	Tcl.
 *
 *	This implementation of floating-point support was modelled
 *	after an initial implementation by Bill Carpenter.
 *
 * 
 * Copyright (c) 1987-1994 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * (license.terms file from Tcl included here)

This software is copyrighted by the Regents of the University of
California, Sun Microsystems, Inc., and other parties.  The following
terms apply to all files associated with the software unless explicitly
disclaimed in individual files.

The authors hereby grant permission to use, copy, modify, distribute,
and license this software and its documentation for any purpose, provided
that existing copyright notices are retained in all copies and that this
notice is included verbatim in any distributions. No written agreement,
license, or royalty fee is required for any of the authorized uses.
Modifications to this software may be copyrighted by their authors
and need not follow the licensing terms described here, provided that
the new terms are clearly indicated on the first page of each file where
they apply.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
MODIFICATIONS.

GOVERNMENT USE: If you are acquiring this software on behalf of the
U.S. government, the Government shall have only "Restricted Rights"
in the software and related documentation as defined in the Federal 
Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
are acquiring the software on behalf of the Department of Defense, the
software shall be classified as "Commercial Computer Software" and the
Government shall have only "Restricted Rights" as defined in Clause
252.227-7013 (c) (1) of DFARs.  Notwithstanding the foregoing, the
authors grant the U.S. Government and others acting in its behalf
permission to use and distribute the software in accordance with the
terms specified in this license. 

 *
 */



#include <setjmp.h>
#include "mpexpr.h"


/*
 * The data structure below is used to describe an expression value,
 * which can be either an integer (the usual case), a double-precision
 * floating-point value, or a string.  A given number has only one
 * value at a time.
 */

#define STATIC_STRING_SPACE 150

typedef enum {MP_INT, MP_DOUBLE, MP_EITHER, MP_STRING, MP_UNDEF} Mp_ValueType;

typedef struct {
    ZVALUE intValue;		/* Integer value, if any. */
    NUMBER *doubleValue;	/* Floating-point value, if any. */
    ParseValue pv;		/* Used to hold a string value, if any. */
    char staticSpace[STATIC_STRING_SPACE];
				/* Storage for small strings;  large ones
				 * are malloc-ed. */
    Mp_ValueType type;		/* Type of value:  MP_INT, MP_DOUBLE,
				 * or MP_STRING. */
} Mp_Value;


/*
 * The data structure below describes the state of parsing an expression.
 * It's passed among the routines in this module.
 */

typedef struct {
    char *originalExpr;		/* The entire expression, as originally
				 * passed to Mp_ExprString et al. */
    char *expr;			/* Position to the next character to be
				 * scanned from the expression string. */
    int token;			/* Type of the last token to be parsed from
				 * expr.  See below for definitions.
				 * Corresponds to the characters just
				 * before expr. */
} ExprInfo;

/* make defines for easy stuff that is missing or different name */
 
#define zneg(z)         ((z).sign = (z).sign==0?1:0)
 
 
typedef int (Mp_MathProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Mp_Value *args, Mp_Value *resultPtr));
 

/*
 * The data structure below defines a math function (e.g. sin or hypot)
 * for use in Tcl expressions.
 */
 
#define MP_MAX_MATH_ARGS 5
typedef struct Mp_MathFunc {
    int numArgs;                /* Number of arguments for function. */
    Mp_ValueType argTypes[MP_MAX_MATH_ARGS];
                                /* Acceptable types for each argument. */
    Mp_MathProc *proc;         /* Procedure that implements this function. */
    ClientData clientData;      /* Additional argument to pass to the function
                                 * when invoking it. */
} Mp_MathFunc;


/*
 * The token types are defined below.  In addition, there is a table
 * associating a precedence with each operator.  The order of types
 * is important.  Consult the code before changing it.
 */

#define VALUE		0
#define OPEN_PAREN	1
#define CLOSE_PAREN	2
#define COMMA		3
#define END		4
#define UNKNOWN		5

/*
 * Binary operators:
 */

#define MULT		8
#define DIVIDE		9
#define MOD		10
#define PLUS		11
#define MINUS		12
#define LEFT_SHIFT	13
#define RIGHT_SHIFT	14
#define LESS		15
#define GREATER		16
#define LEQ		17
#define GEQ		18
#define EQUAL		19
#define NEQ		20
#define BIT_AND		21
#define BIT_XOR		22
#define BIT_OR		23
#define AND		24
#define OR		25
#define QUESTY		26
#define COLON		27

/*
 * Unary operators:
 */

#define	UNARY_MINUS	28
#define UNARY_PLUS	29
#define NOT		30
#define BIT_NOT		31

/*
 * Precedence table.  The values for non-operator token types are ignored.
 */

static int precTable[] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    12, 12, 12,				/* MULT, DIVIDE, MOD */
    11, 11,				/* PLUS, MINUS */
    10, 10,				/* LEFT_SHIFT, RIGHT_SHIFT */
    9, 9, 9, 9,				/* LESS, GREATER, LEQ, GEQ */
    8, 8,				/* EQUAL, NEQ */
    7,					/* BIT_AND */
    6,					/* BIT_XOR */
    5,					/* BIT_OR */
    4,					/* AND */
    3,					/* OR */
    2,					/* QUESTY */
    1,					/* COLON */
    13, 13, 13, 13			/* UNARY_MINUS, UNARY_PLUS, NOT,
					 * BIT_NOT */
};

/*
 * Mapping from operator numbers to strings;  used for error messages.
 */

static char *operatorStrings[] = {
    "VALUE", "(", ")", ",", "END", "UNKNOWN", "6", "7",
    "*", "/", "%", "+", "-", "<<", ">>", "<", ">", "<=",
    ">=", "==", "!=", "&", "^", "|", "&&", "||", "?", ":",
    "-", "+", "!", "~"
};


/*
 * The following global variable is use to signal matherr that Tcl
 * is responsible for the arithmetic, so errors can be handled in a
 * fashion appropriate for Tcl.  Zero means no Tcl math is in
 * progress;  non-zero means Tcl is doing math.
 */

int Mp_MathInProgress = 0;

/*
 * saved copy of current Tcl interp
 *
 */

static Tcl_Interp *mp_interp;


/*
 * precision and epsilon value for rounding and error allowance
 *
 */

long    mp_precision = MP_PRECISION_DEF;
NUMBER *mp_epsilon    = NULL;

/*
 * longjmp buffer and error string to pass errors back to interp
 *
 */

jmp_buf     mp_jump_buffer;
Tcl_DString mp_error_string;

int  MpnoEval = 0;

/*
 * Declarations for local procedures to this file:
 */

static int		ExprAbsFunc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, Mp_Value *args,
			    Mp_Value *resultPtr));
static int		ExprPiFunc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, Mp_Value *args,
			    Mp_Value *resultPtr));
static int		ExprBinaryFunc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, Mp_Value *args,
			    Mp_Value *resultPtr));
static int		ExprBinary2Func _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, Mp_Value *args,
			    Mp_Value *resultPtr));
static int		ExprBinaryZFunc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, Mp_Value *args,
			    Mp_Value *resultPtr));
static int		ExprDoubleFunc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, Mp_Value *args,
			    Mp_Value *resultPtr));
static void		ExprConvIntToDouble _ANSI_ARGS_((Mp_Value *valuePtr));
static void		ExprConvDoubleToInt _ANSI_ARGS_((Mp_Value *valuePtr));
static int		ExprGetValue _ANSI_ARGS_((Tcl_Interp *interp,
			    ExprInfo *infoPtr, int prec, Mp_Value *valuePtr));
static int		ExprIntFunc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, Mp_Value *args,
			    Mp_Value *resultPtr));
static int		ExprLex _ANSI_ARGS_((Tcl_Interp *interp,
			    ExprInfo *infoPtr, Mp_Value *valuePtr));
static int		ExprLooksLikeInt _ANSI_ARGS_((char *p));
static void		ExprMakeString _ANSI_ARGS_((Tcl_Interp *interp,
			    Mp_Value *valuePtr));
static int		ExprMathFunc _ANSI_ARGS_((Tcl_Interp *interp,
			    ExprInfo *infoPtr, Mp_Value *valuePtr));
static int		ExprParseString _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, Mp_Value *valuePtr));
static int		ExprRoundFunc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, Mp_Value *args,
			    Mp_Value *resultPtr));
static void		QZMathDeleteProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp));
static int		ExprTopLevel _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, Mp_Value *valuePtr));
static int		ExprUnaryFunc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, Mp_Value *args,
			    Mp_Value *resultPtr));
static int		ExprUnaryZFunc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, Mp_Value *args,
			    Mp_Value *resultPtr));
static void             Mp_CreateMathFunc _ANSI_ARGS_ ((Tcl_Interp *interp, 
			    char *name, int numArgs, Mp_ValueType *argTypes,
			    Mp_MathProc *proc, ClientData clientData));
static void             ExprFreeMathArgs _ANSI_ARGS_ ((Mp_Value *args));
/* EFP */
static int		ExprTertiaryZFunc _ANSI_ARGS_((ClientData clientData,
                                           Tcl_Interp *interp, Mp_Value *args,
                                           Mp_Value *resultPtr));


/*
 * Helper zmath &  qmath funcitons: (implemented near end of file)
 */

static void		Atoz       _ANSI_ARGS_ ((char *, ZVALUE *, char **));
static NUMBER *		Afractoq   _ANSI_ARGS_ ((char *, char **));
static NUMBER *		qceil      _ANSI_ARGS_ ((NUMBER *));
static NUMBER *		qfloor     _ANSI_ARGS_ ((NUMBER *));
static NUMBER *		qlog       _ANSI_ARGS_ ((NUMBER *));
static NUMBER *		qlog10     _ANSI_ARGS_ ((NUMBER *));
/*
static void		Qfree      _ANSI_ARGS_ ((NUMBER *));
*/
#define Qfree(q)  qfree(q); (q)=NULL
static void		Zlowfactor _ANSI_ARGS_ ((ZVALUE, ZVALUE, ZVALUE *));
static void		Zprimetest _ANSI_ARGS_ ((ZVALUE, ZVALUE, ZVALUE *));
static void		Zrelprime  _ANSI_ARGS_ ((ZVALUE, ZVALUE, ZVALUE *));


/*
 * Built-in math functions:
 */

#define MATH_TABLE_NAME "tclQZMathTable"

typedef struct {
    char *name;			/* Name of function. */
    int numArgs;		/* Number of arguments for function. */
    Mp_ValueType argTypes[MP_MAX_MATH_ARGS];
				/* Acceptable types for each argument. */
    Mp_MathProc *proc;		/* Procedure that implements this function. */
    ClientData clientData;	/* Additional argument to pass to the function
				 * when invoking it. */
} BuiltinFunc;

static BuiltinFunc funcTable[] = {
    {"acos", 1, {MP_DOUBLE}, (Mp_MathProc *)ExprUnaryFunc, (ClientData) qacos},
    {"asin", 1, {MP_DOUBLE}, (Mp_MathProc *)ExprUnaryFunc, (ClientData) qasin},
    {"atan", 1, {MP_DOUBLE}, (Mp_MathProc *)ExprUnaryFunc, (ClientData) qatan},
    {"atan2", 2, {MP_DOUBLE, MP_DOUBLE}, (Mp_MathProc *)ExprBinaryFunc, (ClientData) qatan2},
    {"ceil", 1, {MP_DOUBLE}, (Mp_MathProc *)ExprUnaryFunc, (ClientData) qceil},
    {"cos", 1, {MP_DOUBLE}, (Mp_MathProc *)ExprUnaryFunc, (ClientData) qcos},
    {"cosh", 1, {MP_DOUBLE}, (Mp_MathProc *)ExprUnaryFunc, (ClientData) qcosh},
    {"exp", 1, {MP_DOUBLE}, (Mp_MathProc *)ExprUnaryFunc, (ClientData) qexp},
    {"floor", 1, {MP_DOUBLE}, (Mp_MathProc *)ExprUnaryFunc, (ClientData) qfloor},
    {"fmod", 2, {MP_DOUBLE, MP_DOUBLE}, (Mp_MathProc *)ExprBinaryFunc, (ClientData) qmod},
    {"hypot", 2, {MP_DOUBLE, MP_DOUBLE}, (Mp_MathProc *)ExprBinaryFunc, (ClientData) qhypot},
    {"log", 1, {MP_DOUBLE}, (Mp_MathProc *)ExprUnaryFunc, (ClientData) qlog},
    {"log10", 1, {MP_DOUBLE}, (Mp_MathProc *)ExprUnaryFunc, (ClientData) qlog10},
    {"pow", 2, {MP_DOUBLE, MP_DOUBLE}, (Mp_MathProc *)ExprBinaryFunc, (ClientData) qpower},
    {"sin", 1, {MP_DOUBLE}, (Mp_MathProc *)ExprUnaryFunc, (ClientData) qsin},
    {"sinh", 1, {MP_DOUBLE}, (Mp_MathProc *)ExprUnaryFunc, (ClientData) qsinh},
    {"sqrt", 1, {MP_DOUBLE}, (Mp_MathProc *)ExprUnaryFunc, (ClientData) qsqrt},
    {"tan", 1, {MP_DOUBLE}, (Mp_MathProc *)ExprUnaryFunc, (ClientData) qtan},
    {"tanh", 1, {MP_DOUBLE}, (Mp_MathProc *)ExprUnaryFunc, (ClientData) qtanh},
    {"abs", 1, {MP_EITHER}, (Mp_MathProc *)ExprAbsFunc, 0},
    {"double", 1, {MP_EITHER}, (Mp_MathProc *)ExprDoubleFunc, 0},
    {"int", 1, {MP_EITHER}, (Mp_MathProc *)ExprIntFunc, 0},
    {"round", 1, {MP_EITHER}, (Mp_MathProc *)ExprRoundFunc, 0},

    /* extra math functions in calc library */

    {"root", 2, {MP_DOUBLE, MP_DOUBLE}, (Mp_MathProc *)ExprBinaryFunc, (ClientData) qroot},
    {"frem", 2, {MP_DOUBLE, MP_DOUBLE}, (Mp_MathProc *)ExprBinaryFunc, (ClientData) qfacrem},

    {"minv", 2, {MP_DOUBLE, MP_DOUBLE}, (Mp_MathProc *)ExprBinary2Func, (ClientData) qminv},
    {"gcd", 2, {MP_DOUBLE, MP_DOUBLE}, (Mp_MathProc *)ExprBinary2Func, (ClientData) qgcd},
    {"lcm", 2, {MP_DOUBLE, MP_DOUBLE}, (Mp_MathProc *)ExprBinary2Func, (ClientData) qlcm},
    {"max", 2, {MP_DOUBLE, MP_DOUBLE}, (Mp_MathProc *)ExprBinary2Func, (ClientData) qmax},
    {"min", 2, {MP_DOUBLE, MP_DOUBLE}, (Mp_MathProc *)ExprBinary2Func, (ClientData) qmin},

    {"pi", 0, {MP_EITHER}, (Mp_MathProc *)ExprPiFunc, 0},

    {"fib", 1, {MP_INT}, (Mp_MathProc *)ExprUnaryZFunc, (ClientData) zfib},
    {"fact", 1, {MP_INT}, (Mp_MathProc *)ExprUnaryZFunc, (ClientData) zfact},
    {"pfact", 1, {MP_INT}, (Mp_MathProc *)ExprUnaryZFunc, (ClientData) zpfact},

    {"lfactor", 2, {MP_INT, MP_INT}, (Mp_MathProc *)ExprBinaryZFunc, (ClientData) Zlowfactor},
    {"iroot", 2, {MP_INT, MP_INT}, (Mp_MathProc *)ExprBinaryZFunc, (ClientData) zroot},
    {"gcdrem", 2, {MP_INT, MP_INT}, (Mp_MathProc *)ExprBinaryZFunc, (ClientData) zgcdrem},
    {"perm", 2, {MP_INT, MP_INT}, (Mp_MathProc *)ExprBinaryZFunc, (ClientData) zperm},
    {"comb", 2, {MP_INT, MP_INT}, (Mp_MathProc *)ExprBinaryZFunc, (ClientData) zcomb},
    {"prime", 2, {MP_INT, MP_INT}, (Mp_MathProc *)ExprBinaryZFunc, (ClientData) Zprimetest},
    {"relprime", 2, {MP_INT, MP_INT}, (Mp_MathProc *)ExprBinaryZFunc, (ClientData) Zrelprime},
    /* EFP */
    {"pmod", 3, {MP_INT,MP_INT,MP_INT}, (Mp_MathProc *)ExprTertiaryZFunc, (ClientData) zpowermod},
    {0},
};

/*
 *----------------------------------------------------------------------
 *
 * math_error --
 *
 * called from qmath or zmath when there is a math error
 * grab the error message, stuff into interp result, and longjmp out of here
 *
 *----------------------------------------------------------------------
 */
 
        /* VARARGS2 */
void
math_error TCL_VARARGS_DEF(char *, arg1)
 
{
    va_list argList;
    char *string;
 
 
    string = TCL_VARARGS_START(char *,arg1,argList);
    Tcl_DStringAppend(&mp_error_string, string, -1);
    va_end(argList);
 
    longjmp(mp_jump_buffer,1);
}


/*
 *--------------------------------------------------------------
 *
 * ExprParseString --
 *
 *	Given a string (such as one coming from command or variable
 *	substitution), make a Value based on the string.  The value
 *	will be a floating-point or integer, if possible, or else it
 *	will just be a copy of the string.
 *
 * Results:
 *	TCL_OK is returned under normal circumstances, and TCL_ERROR
 *	is returned if a floating-point overflow or underflow occurred
 *	while reading in a number.  The value at *valuePtr is modified
 *	to hold a number, if possible.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
ExprParseString(interp, string, valuePtr)
    Tcl_Interp *interp;		/* Where to store error message. */
    char *string;		/* String to turn into value. */
    Mp_Value *valuePtr;		/* Where to store value information. 
				 * Caller must have initialized pv field. */
{
    char *term, *p, *start;

    if (*string != 0) {
	if (ExprLooksLikeInt(string)) {
	    valuePtr->type = MP_INT;
    
	    for (p = string; isspace(UCHAR(*p)); p++) {
		/* Empty loop body. */
	    }
	    if (*p == '-') {
		start = p+1;
		zfree(valuePtr->intValue);
		Atoz(start, &valuePtr->intValue, &term);
		zneg(valuePtr->intValue);
	    } else if (*p == '+') {
		start = p+1;
		zfree(valuePtr->intValue);
		Atoz(start, &valuePtr->intValue, &term);
	    } else {
		start = p;
		zfree(valuePtr->intValue);
		Atoz(start, &valuePtr->intValue, &term);
	    }
            if (*term == 0) {
	        valuePtr->type = MP_INT;
 	        return TCL_OK;
            }
	} else {
	    if (valuePtr->doubleValue != NULL ) {
	        Qfree(valuePtr->doubleValue);
	        valuePtr->doubleValue = NULL;
	    }
	    valuePtr->doubleValue = Atoq(string, &term);
	    if ((term != string) && (*term == 0)) {
	        valuePtr->type = MP_DOUBLE;
 	        return TCL_OK;
	    }
	}
    }

    /*
     * Not a valid number.  Save a string value (but don't do anything
     * if it's already the value).
     */

    valuePtr->type = MP_STRING;
    if (string != valuePtr->pv.buffer) {
	int length, shortfall;

	length = strlen(string);
	valuePtr->pv.next = valuePtr->pv.buffer;
	shortfall = length - (valuePtr->pv.end - valuePtr->pv.buffer);
	if (shortfall > 0) {
	    (*valuePtr->pv.expandProc)(&valuePtr->pv, shortfall);
	}
	strcpy(valuePtr->pv.buffer, string);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ExprLex --
 *
 *	Lexical analyzer for expression parser:  parses a single value,
 *	operator, or other syntactic element from an expression string.
 *
 * Results:
 *	TCL_OK is returned unless an error occurred while doing lexical
 *	analysis or executing an embedded command.  In that case a
 *	standard Tcl error is returned, using interp->result to hold
 *	an error message.  In the event of a successful return, the token
 *	and field in infoPtr is updated to refer to the next symbol in
 *	the expression string, and the expr field is advanced past that
 *	token;  if the token is a value, then the value is stored at
 *	valuePtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ExprLex(interp, infoPtr, valuePtr)
    Tcl_Interp *interp;			/* Interpreter to use for error
					 * reporting. */
    register ExprInfo *infoPtr;		/* Describes the state of the parse. */
    register Mp_Value *valuePtr;	/* Where to store value, if that is
					 * what's parsed from string.  Caller
					 * must have initialized pv field
					 * correctly. */
{
    register char *p;
    char *var, *term;
    int result;

    p = infoPtr->expr;
    while (isspace(UCHAR(*p))) {
	p++;
    }
    if (*p == 0) {
	infoPtr->token = END;
	infoPtr->expr = p;
	return TCL_OK;
    }

    /*
     * First try to parse the token as an integer or floating-point number.
     * Don't want to check for a number if the first character is "+"
     * or "-".  If we do, we might treat a binary operator as unary by
     * mistake, which will eventually cause a syntax error.
     */

    if ((*p != '+')  && (*p != '-')) {
	if (ExprLooksLikeInt(p)) {
	    zfree(valuePtr->intValue);
	    Atoz(p, &valuePtr->intValue, &term);
	    infoPtr->token = VALUE;
	    infoPtr->expr = term;
	    valuePtr->type = MP_INT;
	    return TCL_OK;
	} else {
	    Qfree(valuePtr->doubleValue);
	    valuePtr->doubleValue = Atoq(p, &term);
	    if (term != p) {
	        infoPtr->token = VALUE;
	        infoPtr->expr = term;
	        valuePtr->type = MP_DOUBLE;
	        return TCL_OK;
	    }
	}
    }

    infoPtr->expr = p+1;
    switch (*p) {
	case '$':
	    /*
	     * Variable.  Fetch its value, then see if it makes sense
	     * as an integer or floating-point number.
	     */
	    infoPtr->token = VALUE;
	    var = Mp_ParseVar(interp, p, &infoPtr->expr);
	    if (var == NULL) {
		return TCL_ERROR;
	    }
	    Tcl_ResetResult(interp);
	    if (MpnoEval) {
		valuePtr->type = MP_INT;
		zfree(valuePtr->intValue);
		valuePtr->intValue = _zero_;
		return TCL_OK;
	    }
	    return ExprParseString(interp, var, valuePtr);

	case '[':
	    infoPtr->token = VALUE;
	    result= MpParseNestedCmd(interp, p+1, 1, &term, &valuePtr->pv);
	    infoPtr->expr = term;
	    if (result != TCL_OK) {
		valuePtr->type = MP_INT;
		zfree(valuePtr->intValue);
		valuePtr->intValue = _zero_;
		return result;
	    }
	    if (MpnoEval) {
		valuePtr->type = MP_INT;
		zfree(valuePtr->intValue);
		valuePtr->intValue = _zero_;
		Tcl_ResetResult(interp);
		return TCL_OK;
	    }
	    result = ExprParseString(interp, valuePtr->pv.buffer, valuePtr);
	    if (result != TCL_OK) {
		return result;
	    }
	    Tcl_ResetResult(interp);
	    return TCL_OK;

	case '"':
	    infoPtr->token = VALUE;
	    result = MpParseQuotes(interp, infoPtr->expr, '"', 1,
		        &infoPtr->expr, &valuePtr->pv);
	    if (result != TCL_OK) {
		return result;
	    }
	    Tcl_ResetResult(interp);
	    return ExprParseString(interp, valuePtr->pv.buffer, valuePtr);

	case '{':
	    infoPtr->token = VALUE;
	    result = MpParseBraces(interp, infoPtr->expr, &infoPtr->expr,
		    &valuePtr->pv);
	    if (result != TCL_OK) {
		return result;
	    }
	    Tcl_ResetResult(interp);
	    return ExprParseString(interp, valuePtr->pv.buffer, valuePtr);

	case '(':
	    infoPtr->token = OPEN_PAREN;
	    return TCL_OK;

	case ')':
	    infoPtr->token = CLOSE_PAREN;
	    return TCL_OK;

	case ',':
	    infoPtr->token = COMMA;
	    return TCL_OK;

	case '*':
	    infoPtr->token = MULT;
	    return TCL_OK;

	case '/':
	    infoPtr->token = DIVIDE;
	    return TCL_OK;

	case '%':
	    infoPtr->token = MOD;
	    return TCL_OK;

	case '+':
	    infoPtr->token = PLUS;
	    return TCL_OK;

	case '-':
	    infoPtr->token = MINUS;
	    return TCL_OK;

	case '?':
	    infoPtr->token = QUESTY;
	    return TCL_OK;

	case ':':
	    infoPtr->token = COLON;
	    return TCL_OK;

	case '<':
	    switch (p[1]) {
		case '<':
		    infoPtr->expr = p+2;
		    infoPtr->token = LEFT_SHIFT;
		    break;
		case '=':
		    infoPtr->expr = p+2;
		    infoPtr->token = LEQ;
		    break;
		default:
		    infoPtr->token = LESS;
		    break;
	    }
	    return TCL_OK;

	case '>':
	    switch (p[1]) {
		case '>':
		    infoPtr->expr = p+2;
		    infoPtr->token = RIGHT_SHIFT;
		    break;
		case '=':
		    infoPtr->expr = p+2;
		    infoPtr->token = GEQ;
		    break;
		default:
		    infoPtr->token = GREATER;
		    break;
	    }
	    return TCL_OK;

	case '=':
	    if (p[1] == '=') {
		infoPtr->expr = p+2;
		infoPtr->token = EQUAL;
	    } else {
		infoPtr->token = UNKNOWN;
	    }
	    return TCL_OK;

	case '!':
	    if (p[1] == '=') {
		infoPtr->expr = p+2;
		infoPtr->token = NEQ;
	    } else {
		infoPtr->token = NOT;
	    }
	    return TCL_OK;

	case '&':
	    if (p[1] == '&') {
		infoPtr->expr = p+2;
		infoPtr->token = AND;
	    } else {
		infoPtr->token = BIT_AND;
	    }
	    return TCL_OK;

	case '^':
	    infoPtr->token = BIT_XOR;
	    return TCL_OK;

	case '|':
	    if (p[1] == '|') {
		infoPtr->expr = p+2;
		infoPtr->token = OR;
	    } else {
		infoPtr->token = BIT_OR;
	    }
	    return TCL_OK;

	case '~':
	    infoPtr->token = BIT_NOT;
	    return TCL_OK;

	default:
	    if (isalpha(UCHAR(*p))) {
		infoPtr->expr = p;
		return ExprMathFunc(interp, infoPtr, valuePtr);
	    }
	    infoPtr->expr = p+1;
	    infoPtr->token = UNKNOWN;
	    return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ExprConvIntToDouble -
 *
 *	Convert a value from int to double
 *
 */

static void
ExprConvIntToDouble(valuePtr)
    Mp_Value *valuePtr;			
{
    Qfree(valuePtr->doubleValue); 
    valuePtr->doubleValue = qalloc();
    zcopy(valuePtr->intValue, &(valuePtr->doubleValue->num));
    valuePtr->type = MP_DOUBLE;
}

/*
 *----------------------------------------------------------------------
 *
 * ExprConvDoubleToInt -
 *
 *	Convert a value from double to int
 *
 */

static void
ExprConvDoubleToInt(valuePtr)
    Mp_Value *valuePtr;			
{
    NUMBER *q;
    int neg;

    neg = qisneg(valuePtr->doubleValue);
    q = qint(valuePtr->doubleValue);
    zfree(valuePtr->intValue); 
    zcopy(q->num, &valuePtr->intValue);
    Qfree(q);
    if (neg && !zisneg(valuePtr->intValue)) {  
      zneg(valuePtr->intValue);
    }
    valuePtr->type = MP_INT;
}

/*
 *----------------------------------------------------------------------
 *
 * ExprGetValue --
 *
 *	Parse a "value" from the remainder of the expression in infoPtr.
 *
 * Results:
 *	Normally TCL_OK is returned.  The value of the expression is
 *	returned in *valuePtr.  If an error occurred, then interp->result
 *	contains an error message and TCL_ERROR is returned.
 *	InfoPtr->token will be left pointing to the token AFTER the
 *	expression, and infoPtr->expr will point to the character just
 *	after the terminating token.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ExprGetValue(interp, infoPtr, prec, valuePtr)
    Tcl_Interp *interp;			/* Interpreter to use for error
					 * reporting. */
    register ExprInfo *infoPtr;		/* Describes the state of the parse
					 * just before the value (i.e. ExprLex
					 * will be called to get first token
					 * of value). */
    int prec;				/* Treat any un-parenthesized operator
					 * with precedence <= this as the end
					 * of the expression. */
    Mp_Value *valuePtr;			/* Where to store the value of the
					 * expression.   Caller must have
					 * initialized pv field. */
{
    Mp_Value value2;			/* Second operand for current
					 * operator.  */
    int operator;			/* Current operator (either unary
					 * or binary). */
    int badType;			/* Type of offending argument;  used
					 * for error messages. */
    int gotOp;				/* Non-zero means already lexed the
					 * operator (while picking up value
					 * for unary operator).  Don't lex
					 * again. */
    int result;

    ZVALUE  z_tmp;
    ZVALUE  z_div;
    ZVALUE  z_quot;
    ZVALUE  z_rem;
    long    l_shift;
    char   *math_io;

    NUMBER  *q_tmp;

    value2.intValue    = _zero_;
    value2.doubleValue = qlink(&_qzero_);
    value2.type        = MP_UNDEF;

    /*
     * There are two phases to this procedure.  First, pick off an initial
     * value.  Then, parse (binary operator, value) pairs until done.
     */

    gotOp = 0;
    value2.pv.buffer = value2.pv.next = value2.staticSpace;
    value2.pv.end = value2.pv.buffer + STATIC_STRING_SPACE - 1;
    value2.pv.expandProc = MpExpandParseValue;
    value2.pv.clientData = (ClientData) NULL;
    result = ExprLex(interp, infoPtr, valuePtr);
    if (result != TCL_OK) {
	goto done;
    }
    if (infoPtr->token == OPEN_PAREN) {

	/*
	 * Parenthesized sub-expression.
	 */

	result = ExprGetValue(interp, infoPtr, -1, valuePtr);
	if (result != TCL_OK) {
	    goto done;
	}
	if (infoPtr->token != CLOSE_PAREN) {
	    Tcl_AppendResult(interp, "unmatched parentheses in expression \"",
		    infoPtr->originalExpr, "\"", (char *) NULL);
	    result = TCL_ERROR;
	    goto done;
	}
    } else {
	if (infoPtr->token == MINUS) {
	    infoPtr->token = UNARY_MINUS;
	}
	if (infoPtr->token == PLUS) {
	    infoPtr->token = UNARY_PLUS;
	}
	if (infoPtr->token >= UNARY_MINUS) {

	    /*
	     * Process unary operators.
	     */

	    operator = infoPtr->token;
	    result = ExprGetValue(interp, infoPtr, precTable[infoPtr->token],
		    valuePtr);
	    if (result != TCL_OK) {
		goto done;
	    }
	    if (!MpnoEval) {
		switch (operator) {
		    case UNARY_MINUS:
			if (valuePtr->type == MP_INT) {
			    zneg(valuePtr->intValue);
			} else if (valuePtr->type == MP_DOUBLE){
			    q_tmp = qneg(valuePtr->doubleValue);
			    Qfree(valuePtr->doubleValue);
			    valuePtr->doubleValue = q_tmp;
			} else {
			    badType = valuePtr->type;
			    goto illegalType;
			} 
			break;
		    case UNARY_PLUS:
			if ((valuePtr->type != MP_INT)
				&& (valuePtr->type != MP_DOUBLE)) {
			    badType = valuePtr->type;
			    goto illegalType;
			} 
			break;
		    case NOT:
			if (valuePtr->type == MP_INT) {
			    if (ziszero(valuePtr->intValue)) {
			        zfree(valuePtr->intValue);
				valuePtr->intValue = _one_;
			    } else {
			        zfree(valuePtr->intValue);
				valuePtr->intValue = _zero_;
			    }
			} else if (valuePtr->type == MP_DOUBLE) {
			    zfree(valuePtr->intValue);
			    if (qiszero(valuePtr->doubleValue)) {
				valuePtr->intValue = _one_;
			    } else {
				valuePtr->intValue = _zero_;
			    }
			    valuePtr->type = MP_INT;
			} else {
			    badType = valuePtr->type;
			    goto illegalType;
			}
			break;
		    case BIT_NOT:
			if (valuePtr->type == MP_INT) {
			    zneg(valuePtr->intValue);
			    zsub(valuePtr->intValue, _one_, &z_tmp);
			    zfree(valuePtr->intValue);
			    zcopy(z_tmp, &valuePtr->intValue);
			    zfree(z_tmp);
			} else {
			    badType  = valuePtr->type;
			    goto illegalType;
			}
			break;
		}
	    }
	    gotOp = 1;
	} else if (infoPtr->token != VALUE) {
	    goto syntaxError;
	}
    }

    /*
     * Got the first operand.  Now fetch (operator, operand) pairs.
     */

    if (!gotOp) {
	result = ExprLex(interp, infoPtr, &value2);
	if (result != TCL_OK) {
	    goto done;
	}
    }
    while (1) {
	operator = infoPtr->token;
	value2.pv.next = value2.pv.buffer;
	if ((operator < MULT) || (operator >= UNARY_MINUS)) {
	    if ((operator == END) || (operator == CLOSE_PAREN)
		    || (operator == COMMA)) {
		result = TCL_OK;
		goto done;
	    } else {
		goto syntaxError;
	    }
	}
	if (precTable[operator] <= prec) {
	    result = TCL_OK;
	    goto done;
	}

	/*
	 * If we're doing an AND or OR and the first operand already
	 * determines the result, don't execute anything in the
	 * second operand:  just parse.  Same style for ?: pairs.
	 */

	if ((operator == AND) || (operator == OR) || (operator == QUESTY)) {
	    if (valuePtr->type == MP_DOUBLE) {
		zfree(valuePtr->intValue);
		valuePtr->intValue = qiszero(valuePtr->doubleValue) ? 
							_zero_ : _one_;
		valuePtr->type = MP_INT;
	    } else if (valuePtr->type == MP_STRING) {
		if (!MpnoEval) {
		    badType = MP_STRING;
		    goto illegalType;
		}

		/*
		 * Must set valuePtr->intValue to avoid referencing
		 * uninitialized memory in the "if" below;  the actual
		 * value doesn't matter, since it will be ignored.
		 */

		zfree(valuePtr->intValue);
		valuePtr->intValue = _zero_;
	    }
	    if ( ((operator == AND) && ziszero(valuePtr->intValue))
		    || ((operator == OR) && !(ziszero(valuePtr->intValue)))) {
		MpnoEval++;
		result = ExprGetValue(interp, infoPtr, precTable[operator],
			&value2);
		MpnoEval--;
		if (result != TCL_OK) {
		    goto done;
		}
		if (operator == OR) {
		    zfree(valuePtr->intValue);
		    valuePtr->intValue = _one_;
		}
		continue;
	    } else if (operator == QUESTY) {
		/*
		 * Special note:  ?: operators must associate right to
		 * left.  To make this happen, use a precedence one lower
		 * than QUESTY when calling ExprGetValue recursively.
		 */

		if (! ziszero(valuePtr->intValue)) {
		    valuePtr->pv.next = valuePtr->pv.buffer;
		    result = ExprGetValue(interp, infoPtr,
			    precTable[QUESTY] - 1, valuePtr);
		    if (result != TCL_OK) {
			goto done;
		    }
		    if (infoPtr->token != COLON) {
			goto syntaxError;
		    }
		    value2.pv.next = value2.pv.buffer;
		    MpnoEval++;
		    result = ExprGetValue(interp, infoPtr,
			    precTable[QUESTY] - 1, &value2);
		    MpnoEval--;
		} else {
		    MpnoEval++;
		    result = ExprGetValue(interp, infoPtr,
			    precTable[QUESTY] - 1, &value2);
		    MpnoEval--;
		    if (result != TCL_OK) {
			goto done;
		    }
		    if (infoPtr->token != COLON) {
			goto syntaxError;
		    }
		    valuePtr->pv.next = valuePtr->pv.buffer;
		    result = ExprGetValue(interp, infoPtr,
			    precTable[QUESTY] - 1, valuePtr);
		    if (result != TCL_OK) {
			goto done;
		    }
		}
		continue;
	    } else {
		result = ExprGetValue(interp, infoPtr, precTable[operator],
			&value2);
	    }
	} else {
	    result = ExprGetValue(interp, infoPtr, precTable[operator],
		    &value2);
	}
	if (result != TCL_OK) {
	    goto done;
	}
	if ((infoPtr->token < MULT) && (infoPtr->token != VALUE)
		&& (infoPtr->token != END) && (infoPtr->token != COMMA)
		&& (infoPtr->token != CLOSE_PAREN)) {
	    goto syntaxError;
	}

	if (MpnoEval) {
	    continue;
	}

	/*
	 * At this point we've got two values and an operator.  Check
	 * to make sure that the particular data types are appropriate
	 * for the particular operator, and perform type conversion
	 * if necessary.
	 */


	switch (operator) {

	    /*
	     * For the operators below, no strings are allowed and
	     * ints get converted to floats if necessary.
	     */

	    case MULT: case DIVIDE: case PLUS: case MINUS:
		if ((valuePtr->type == MP_STRING)
			|| (value2.type == MP_STRING)) {
		    badType = MP_STRING;
		    goto illegalType;
		}
		if (valuePtr->type == MP_DOUBLE) {
		    if (value2.type == MP_INT) {
			ExprConvIntToDouble(&value2);
		    }
		} else if (value2.type == MP_DOUBLE) {
		    if (valuePtr->type == MP_INT) {
			ExprConvIntToDouble(valuePtr);
		    }
		}
		break;

	    /*
	     * For the operators below, only integers are allowed.
	     */

	    case MOD: case LEFT_SHIFT: case RIGHT_SHIFT:
	    case BIT_AND: case BIT_XOR: case BIT_OR:
		 if (valuePtr->type != MP_INT) {
		     badType = valuePtr->type;
		     goto illegalType;
		 } else if (value2.type != MP_INT) {
		     badType = value2.type;
		     goto illegalType;
		 }
		 break;

	    /*
	     * For the operators below, any type is allowed but the
	     * two operands must have the same type.  Convert integers
	     * to floats and either to strings, if necessary.
	     */

	    case LESS: case GREATER: case LEQ: case GEQ:
	    case EQUAL: case NEQ:
		if (valuePtr->type == MP_STRING) {
		    if (value2.type != MP_STRING) {
			ExprMakeString(interp, &value2);
		    }
		} else if (value2.type == MP_STRING) {
		    if (valuePtr->type != MP_STRING) {
			ExprMakeString(interp, valuePtr);
		    }
		} else if (valuePtr->type == MP_DOUBLE) {
		    if (value2.type == MP_INT) {
			ExprConvIntToDouble(&value2);
		    }
		} else if (value2.type == MP_DOUBLE) {
		    if (valuePtr->type == MP_INT) {
		        ExprConvIntToDouble(valuePtr); 
		    }
		}
		break;

	    /*
	     * For the operators below, no strings are allowed, but
	     * no int->double conversions are performed.
	     */

	    case AND: case OR:
		if (valuePtr->type == MP_STRING) {
		    badType = valuePtr->type;
		    goto illegalType;
		}
		if (value2.type == MP_STRING) {
		    badType = value2.type;
		    goto illegalType;
		}
		break;

	    /*
	     * For the operators below, type and conversions are
	     * irrelevant:  they're handled elsewhere.
	     */

	    case QUESTY: case COLON:
		break;

	    /*
	     * Any other operator is an error.
	     */

	    default:
		interp->result = "unknown operator in expression";
		result = TCL_ERROR;
		goto done;
	}


	/*
	 * Carry out the function of the specified operator.
	 */

	switch (operator) {
	    case MULT:
		if (valuePtr->type == MP_INT) {
		    zmul(valuePtr->intValue, value2.intValue, &z_tmp);
		    zfree(valuePtr->intValue);
		    zcopy(z_tmp, &valuePtr->intValue);
		    zfree(z_tmp);
		} else {
		    q_tmp = qmul(valuePtr->doubleValue, value2.doubleValue);
		    Qfree(valuePtr->doubleValue);
		    valuePtr->doubleValue = q_tmp;
		}
		break;
	    case DIVIDE:
	    case MOD:
		if (valuePtr->type == MP_INT) {
		    int negative1, negative2;

		    if (ziszero(value2.intValue)) {
			divideByZero:
			interp->result = "divide by zero";
			Tcl_SetErrorCode(interp, "ARITH", "DIVZERO",
				interp->result, (char *) NULL);
			result = TCL_ERROR;
			goto done;
		    }

		    /* follow Tcl conventions: remainder is always a smaller
		     * absolute value and same sign as divisor 
		     */

		    if (ziszero(valuePtr->intValue)) {
  			zfree(valuePtr->intValue);
  			zcopy(_zero_, &valuePtr->intValue);
			break;
		    }

		    negative1 = 0;
		    if (zisneg(valuePtr->intValue)) {
		        zneg(valuePtr->intValue);
			negative1 = 1;
		    } 

		    zcopy(value2.intValue, &z_div);
		    negative2 = 0;
		    if (zisneg(z_div)) {
			zneg(z_div);
			negative2 = 1;
		    }
		    zdiv(valuePtr->intValue, z_div, &z_quot, &z_rem);
		    if (operator == DIVIDE) {
		        if ((negative1 != negative2) && !ziszero(z_rem) ) {
                            if (!zisneg(z_quot)) {
			        zneg(z_quot);
			    }
			    zsub(z_quot, _one_, &z_tmp);
			    zfree(z_quot);
			    zcopy(z_tmp, &z_quot);
			    zfree(z_tmp);
			} else {
		            if ((negative1 != negative2)) {
                                if (!zisneg(z_quot)) {
			            zneg(z_quot);
			        }
			    }
			}
		        zfree(valuePtr->intValue);
		        zcopy(z_quot, &valuePtr->intValue);
		    } else {
			if ((negative1 != negative2) && !ziszero(z_rem)) {
                            if (ziszero(z_rem)) {
                                zneg(z_rem);
                            }
			    zsub(z_div, z_rem, &z_tmp);
			    zfree(z_rem);
			    zcopy(z_tmp, &z_rem);
			    zfree(z_tmp);
		        }
			if (negative2 && !ziszero(z_rem)) {
			    zneg(z_rem);
			}
		        zfree(valuePtr->intValue);
		        zcopy(z_rem,  &valuePtr->intValue);
		    }
		    zfree(z_quot);
		    zfree(z_rem);
		    zfree(z_div);
		} else {
		    if (qiszero(value2.doubleValue)) {
			goto divideByZero;
		    }
		    q_tmp = qdiv(valuePtr->doubleValue, value2.doubleValue);
		    Qfree(valuePtr->doubleValue);
		    valuePtr->doubleValue = q_tmp;
		}
		break;
	    case PLUS:
		if (valuePtr->type == MP_INT) {
		    zadd(valuePtr->intValue, value2.intValue, &z_tmp);
		    zfree(valuePtr->intValue);
		    zcopy(z_tmp, &valuePtr->intValue);
		    zfree(z_tmp);
		} else {
		    q_tmp = qadd(valuePtr->doubleValue, value2.doubleValue);
		    Qfree(valuePtr->doubleValue);
		    valuePtr->doubleValue = q_tmp;
		}
		break;
	    case MINUS:
		if (valuePtr->type == MP_INT) {
		    zsub(valuePtr->intValue, value2.intValue, &z_tmp);
		    zfree(valuePtr->intValue);
		    zcopy(z_tmp, &valuePtr->intValue);
		    zfree(z_tmp);
		} else {
		    q_tmp = qsub(valuePtr->doubleValue, value2.doubleValue);
		    Qfree(valuePtr->doubleValue);
		    valuePtr->doubleValue = q_tmp;
		}
		break;
	    case LEFT_SHIFT:
    		math_divertio();
		zprintval(value2.intValue, 0L, 0L);
		math_io = math_getdivertedio();
		math_cleardiversions();
		l_shift = atol(math_io);
		ckfree(math_io);
		zshift(valuePtr->intValue, l_shift, &z_tmp); 
		zfree(valuePtr->intValue);
		zcopy(z_tmp, &valuePtr->intValue);
		zfree(z_tmp);
		break;
	    case RIGHT_SHIFT:
    		math_divertio();
		zprintval(value2.intValue, 0L, 0L);
		math_io = math_getdivertedio();
		math_cleardiversions();
		l_shift = atol(math_io);
		ckfree(math_io);
		zshift(valuePtr->intValue, (-l_shift), &z_tmp); 
		zfree(valuePtr->intValue);
		zcopy(z_tmp, &valuePtr->intValue);
		zfree(z_tmp);
		break;
	    case LESS:
		if (valuePtr->type == MP_INT) {
		    if (zrel(valuePtr->intValue,value2.intValue) == -1) {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _one_;
		    } else {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _zero_;
		    }
		} else if (valuePtr->type == MP_DOUBLE) {
		    if (qrel(valuePtr->doubleValue, value2.doubleValue) == -1) {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _one_;
		    } else {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _zero_;
		    }
		} else {
		    if (strcmp(valuePtr->pv.buffer, value2.pv.buffer) < 0) {
		        zfree(valuePtr->intValue);
		        valuePtr->intValue = _one_;
		    } else {
		        zfree(valuePtr->intValue);
		        valuePtr->intValue = _zero_;
		    }
		}
		valuePtr->type = MP_INT;
		break;
	    case GREATER:
		if (valuePtr->type == MP_INT) {
		    if (zrel(valuePtr->intValue,value2.intValue) == 1) {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _one_;
		    } else {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _zero_;
		    }
		} else if (valuePtr->type == MP_DOUBLE) {
		    if (qrel(valuePtr->doubleValue, value2.doubleValue) == 1) {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _one_;
		    } else {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _zero_;
		    }
		} else {
		    if (strcmp(valuePtr->pv.buffer, value2.pv.buffer) > 0) {
		        zfree(valuePtr->intValue);
		        valuePtr->intValue = _one_;
		    } else {
		        zfree(valuePtr->intValue);
		        valuePtr->intValue = _zero_;
		    }
		}
		valuePtr->type = MP_INT;
		break;
	    case LEQ:
		if (valuePtr->type == MP_INT) {
		    if (zrel(valuePtr->intValue,value2.intValue) == -1 ||
			zcmp(valuePtr->intValue,value2.intValue) ==  0) {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _one_;
		    } else {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _zero_;
		    }
		} else if (valuePtr->type == MP_DOUBLE) {
		    if (qrel(valuePtr->doubleValue, value2.doubleValue) == -1 ||
		        qcmp(valuePtr->doubleValue, value2.doubleValue) ==  0) {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _one_;
		    } else {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _zero_;
		    }
		} else {
		    if (strcmp(valuePtr->pv.buffer, value2.pv.buffer) <= 0) {
		        zfree(valuePtr->intValue);
		        valuePtr->intValue = _one_;
		    } else {
		        zfree(valuePtr->intValue);
		        valuePtr->intValue = _zero_;
		    }
		}
		valuePtr->type = MP_INT;
		break;
	    case GEQ:
		if (valuePtr->type == MP_INT) {
		    if (zrel(valuePtr->intValue,value2.intValue) ==  1 ||
			zcmp(valuePtr->intValue,value2.intValue) ==  0) {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _one_;
		    } else {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _zero_;
		    }
		} else if (valuePtr->type == MP_DOUBLE) {
		    if (qrel(valuePtr->doubleValue, value2.doubleValue) == 1 ||
		        qcmp(valuePtr->doubleValue, value2.doubleValue) ==  0) {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _one_;
		    } else {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _zero_;
		    }
		} else {
		    if (strcmp(valuePtr->pv.buffer, value2.pv.buffer) >= 0) {
		        zfree(valuePtr->intValue);
		        valuePtr->intValue = _one_;
		    } else {
		        zfree(valuePtr->intValue);
		        valuePtr->intValue = _zero_;
		    }
		}
		valuePtr->type = MP_INT;
		break;
	    case EQUAL:
		if (valuePtr->type == MP_INT) {
		    if (zcmp(valuePtr->intValue,value2.intValue) ==  0) {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _one_;
		    } else {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _zero_;
		    }
		} else if (valuePtr->type == MP_DOUBLE) {
		    if (qcmp(valuePtr->doubleValue, value2.doubleValue) ==  0) {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _one_;
		    } else {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _zero_;
		    }
		} else {
		    if (strcmp(valuePtr->pv.buffer, value2.pv.buffer) == 0) {
		        zfree(valuePtr->intValue);
		        valuePtr->intValue = _one_;
		    } else {
		        zfree(valuePtr->intValue);
		        valuePtr->intValue = _zero_;
		    }
		}
		valuePtr->type = MP_INT;
		break;
	    case NEQ:
		if (valuePtr->type == MP_INT) {
		    if (zcmp(valuePtr->intValue,value2.intValue) !=  0) {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _one_;
		    } else {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _zero_;
		    }
		} else if (valuePtr->type == MP_DOUBLE) {
		    if (qcmp(valuePtr->doubleValue, value2.doubleValue) !=  0) {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _one_;
		    } else {
		        zfree(valuePtr->intValue);
			valuePtr->intValue = _zero_;
		    }
		} else {
		    if (strcmp(valuePtr->pv.buffer, value2.pv.buffer) != 0) {
		        zfree(valuePtr->intValue);
		        valuePtr->intValue = _one_;
		    } else {
		        zfree(valuePtr->intValue);
		        valuePtr->intValue = _zero_;
		    }
		}
		valuePtr->type = MP_INT;
		break;
	    case BIT_AND:
		zand(valuePtr->intValue, value2.intValue, &z_tmp);
		zfree(valuePtr->intValue);
		zcopy(z_tmp, &valuePtr->intValue);
		zfree(z_tmp);
		break;
	    case BIT_XOR:
		zxor(valuePtr->intValue, value2.intValue, &z_tmp);
		zfree(valuePtr->intValue);
		zcopy(z_tmp, &valuePtr->intValue);
		zfree(z_tmp);
		break;
	    case BIT_OR:
		zor(valuePtr->intValue, value2.intValue, &z_tmp);
		zfree(valuePtr->intValue);
		zcopy(z_tmp, &valuePtr->intValue);
		zfree(z_tmp);
		break;

	    /*
	     * For AND and OR, we know that the first value has already
	     * been converted to an integer.  Thus we need only consider
	     * the possibility of int vs. double for the second value.
	     */

	    case AND:
		if (value2.type == MP_DOUBLE) {
		    if (!qiszero(value2.doubleValue)) {
		        zfree(value2.intValue);
			value2.intValue = _one_;
		    } else {
		        zfree(value2.intValue);
			value2.intValue = _zero_;
		    }
		    zand(valuePtr->intValue, value2.intValue, &z_tmp);
		    zfree(valuePtr->intValue);
		    zcopy(z_tmp, &valuePtr->intValue);
		    zfree(z_tmp);
		    value2.type = MP_INT;
		}
		if (ziszero(valuePtr->intValue) || ziszero(value2.intValue)) {
		    zfree(valuePtr->intValue);
		    valuePtr->intValue = _zero_;
		} else {
		    zfree(valuePtr->intValue);
		    valuePtr->intValue = _one_;
		}
		break;
	    case OR:
		if (value2.type == MP_DOUBLE) {
		    if (!qiszero(value2.doubleValue)) {
		        zfree(value2.intValue);
			value2.intValue = _one_;
		    } else {
		        zfree(value2.intValue);
			value2.intValue = _zero_;
		    }
		    value2.type = MP_INT;
		}
		if (ziszero(valuePtr->intValue) && ziszero(value2.intValue)) {
		    zfree(valuePtr->intValue);
		    valuePtr->intValue = _zero_;
		} else {
		    zfree(valuePtr->intValue);
		    valuePtr->intValue = _one_;
		}
		break;

	    case COLON:
		interp->result = "can't have : operator without ? first";
		result = TCL_ERROR;
		goto done;
	}
    }

    done:
    if (value2.pv.buffer != value2.staticSpace) {
	ckfree(value2.pv.buffer);
    }
    /* avoid return -0 */
    if (valuePtr->type == MP_INT) {
	if (ziszero(valuePtr->intValue) && zisneg(valuePtr->intValue)) {
	    zneg(valuePtr->intValue);
	}
    }
    zfree(value2.intValue);
    Qfree(value2.doubleValue);
    return result;

    syntaxError:
    Tcl_AppendResult(interp, "syntax error in expression \"",
	    infoPtr->originalExpr, "\"", (char *) NULL);
    result = TCL_ERROR;
    goto done;

    illegalType:
    Tcl_AppendResult(interp, "can't use ", (badType == MP_DOUBLE) ?
	    "floating-point value" : "non-numeric string",
	    " as operand of \"", operatorStrings[operator], "\"",
	    (char *) NULL);
    result = TCL_ERROR;
    goto done;
}

/*
 *--------------------------------------------------------------
 *
 * ExprMakeString --
 *
 *	Convert a value from int or double representation to
 *	a string.
 *
 * Results:
 *	The information at *valuePtr gets converted to string
 *	format, if it wasn't that way already.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
ExprMakeString(interp, valuePtr)
    Tcl_Interp *interp;			/* Interpreter to use for precision
					 * information. */
    register Mp_Value *valuePtr;	/* Value to be converted. */
{
    int shortfall;
    int total;
    long precision;
    char *math_io;
    NUMBER *q_rounded;

    mp_interp = interp;

    if (valuePtr->type == MP_INT) {
        shortfall = zdigits(valuePtr->intValue) - 
		   (valuePtr->pv.end - valuePtr->pv.buffer);
	total = zdigits(valuePtr->intValue) + 
		   (valuePtr->pv.end - valuePtr->pv.buffer);
    } else {
	q_rounded = qround(valuePtr->doubleValue, mp_precision);
	Qfree(valuePtr->doubleValue);
	valuePtr->doubleValue = q_rounded;
	precision  = qplaces(valuePtr->doubleValue);
        if (precision < 0) {
	    precision = mp_precision;
	} else {
	    precision  = (precision > mp_precision) ? mp_precision : 
		 (precision == 0) ? 1 : precision;
	}
	 
        shortfall = (qdigits(valuePtr->doubleValue) + precision) - 
		   (valuePtr->pv.end - valuePtr->pv.buffer);
	total = (qdigits(valuePtr->doubleValue) + precision) + 
		   (valuePtr->pv.end - valuePtr->pv.buffer);
    }
    total += 4;
    shortfall += 4;
    if (shortfall > 0) {
	(*valuePtr->pv.expandProc)(&valuePtr->pv, shortfall);
    }
    math_divertio();
    if (valuePtr->type == MP_INT) {
	Zprintval(valuePtr->intValue, 0L, 0L);
	math_io = math_getdivertedio();
	math_cleardiversions();
	strncpy(valuePtr->pv.buffer, math_io, total);
    } else if (valuePtr->type == MP_DOUBLE) {
	Qprintff(valuePtr->doubleValue, 0L, precision); 
	math_io = math_getdivertedio();
	math_cleardiversions();
	strncpy(valuePtr->pv.buffer, math_io, total);
    }
    ckfree(math_io);
    valuePtr->type = MP_STRING;
}

/*
 * QZMathDeleteProc -- 
 *
 * Delete the QZMathTable and free mp_epsilon if set
 *
 */

static void
QZMathDeleteProc(clientData, interp)
    ClientData  clientData;
    Tcl_Interp *interp;
{
    Tcl_HashTable *ZQMathTablePtr;


    ZQMathTablePtr = (Tcl_HashTable *) clientData;
    Tcl_DeleteHashTable(ZQMathTablePtr);
    ckfree((char *) ZQMathTablePtr);

    math_cleardiversions();
}


/*
 *--------------------------------------------------------------
 *
 * ExprTopLevel --
 *
 *	This procedure provides top-level functionality shared by
 *	procedures like Mp_ExprInt, Mp_ExprDouble, etc.
 *
 * Results:
 *	The result is a standard Tcl return value.  If an error
 *	occurs then an error message is left in interp->result.
 *	The value of the expression is returned in *valuePtr, in
 *	whatever form it ends up in (could be string or integer
 *	or double).  Caller may need to convert result.  Caller
 *	is also responsible for freeing string memory in *valuePtr,
 *	if any was allocated.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
ExprTopLevel(interp, string, valuePtr)
    Tcl_Interp *interp;			/* Context in which to evaluate the
					 * expression. */
    char *string;			/* Expression to evaluate. */
    Mp_Value *valuePtr;			/* Where to store result.  Should
					 * not be initialized by caller. */
{
    ExprInfo info;
    int result;
    Tcl_HashTable *ZQMathTablePtr;

    /*
     * Create the math functions the first time an expression is
     * evaluated.
     */

    ZQMathTablePtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, 
			MATH_TABLE_NAME, NULL);
    if (ZQMathTablePtr == NULL) {

	BuiltinFunc *funcPtr;

	ZQMathTablePtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable)); 
	Tcl_InitHashTable(ZQMathTablePtr, TCL_STRING_KEYS);
	(void) Tcl_SetAssocData(interp, MATH_TABLE_NAME, 
		  QZMathDeleteProc, (ClientData) ZQMathTablePtr);

	for (funcPtr = funcTable; funcPtr->name != NULL;
		funcPtr++) {
	    Mp_CreateMathFunc(interp, funcPtr->name, funcPtr->numArgs,
		    funcPtr->argTypes, funcPtr->proc, funcPtr->clientData);
	}
    }

    info.originalExpr = string;
    info.expr = string;
    valuePtr->pv.buffer = valuePtr->pv.next = valuePtr->staticSpace;
    valuePtr->pv.end = valuePtr->pv.buffer + STATIC_STRING_SPACE - 1;
    valuePtr->pv.expandProc = MpExpandParseValue;
    valuePtr->pv.clientData = (ClientData) NULL;

    result = ExprGetValue(interp, &info, -1, valuePtr);

    if (result != TCL_OK) {
	return result;
    }
    if (info.token != END) {
	Tcl_AppendResult(interp, "syntax error in expression \"",
		string, "\"", (char *) NULL);
	return TCL_ERROR;
    }

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Mp_ExprString --
 *
 *	Evaluate an expression and return its value in string form.
 *
 * Results:
 *	A standard Tcl result.  If the result is TCL_OK, then the
 *	interpreter's result is set to the string value of the
 *	expression.  If the result is TCL_OK, then interp->result
 *	contains an error message.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Mp_ExprString(interp, string)
    Tcl_Interp *interp;			/* Context in which to evaluate the
					 * expression. */
    char *string;			/* Expression to evaluate. */
{
    Mp_Value value;
    int result;
    long precision;
    char *math_io;
    NUMBER *q_rounded;
    static int inMpExpr = 0;

    value.intValue    = _zero_;
    value.doubleValue = qlink(&_qzero_);
    value.type        = MP_UNDEF;

    mp_interp = interp;
    Tcl_DStringInit(&mp_error_string);

    inMpExpr++;
    if (inMpExpr == 1) {
	if (setjmp(mp_jump_buffer) == 1) {
    	    inMpExpr = 0;
            zfree(value.intValue);
            Qfree(value.doubleValue);
	    Tcl_ResetResult(interp);
	    Tcl_DStringResult(interp, &mp_error_string);
	    return TCL_ERROR;
	}
    }

    result = ExprTopLevel(interp, string, &value);

    inMpExpr--;
    if (result == TCL_OK) {
	if (value.type == MP_INT) {
    	    math_divertio();
	    Zprintval(value.intValue, 0L, 0L);
	    math_io = math_getdivertedio();
	    math_cleardiversions();
	    Tcl_SetResult(interp, math_io, TCL_VOLATILE);
	    ckfree(math_io);
	} else if (value.type == MP_DOUBLE) {
	    q_rounded = qround(value.doubleValue, mp_precision);
	    Qfree(value.doubleValue);
	    value.doubleValue = q_rounded;
	    precision  = qplaces(value.doubleValue);
            if (precision < 0) {
	        precision = mp_precision;
	    } else {
	        precision  = (precision > mp_precision) ? mp_precision : 
		     (precision == 0) ? 1 : precision;
	    }
            math_divertio();
	    Qprintff(value.doubleValue, 0L, precision); 
	    math_io = math_getdivertedio();
	    math_cleardiversions();
	    Tcl_SetResult(interp, math_io, TCL_VOLATILE);
	    ckfree(math_io);
	} else {
	    if (value.pv.buffer != value.staticSpace) {
		interp->result = value.pv.buffer;
		interp->freeProc = TCL_DYNAMIC;
		value.pv.buffer = value.staticSpace;
	    } else {
		Tcl_SetResult(interp, value.pv.buffer, TCL_VOLATILE);
	    }
	}
    }
    if (value.pv.buffer != value.staticSpace) {
	ckfree(value.pv.buffer);
    }

    zfree(value.intValue);
    Qfree(value.doubleValue);
    Tcl_DStringFree(&mp_error_string);

    math_cleardiversions();

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Mp_CreateMathFunc --
 *
 *	Creates a new math function for expressions in a given
 *	interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The function defined by "name" is created;  if such a function
 *	already existed then its definition is overriden.
 *
 *----------------------------------------------------------------------
 */

void
Mp_CreateMathFunc(interp, name, numArgs, argTypes, proc, clientData)
    Tcl_Interp *interp;			/* Interpreter in which function is
					 * to be available. */
    char *name;				/* Name of function (e.g. "sin"). */
    int numArgs;			/* Nnumber of arguments required by
					 * function. */
    Mp_ValueType *argTypes;		/* Array of types acceptable for
					 * each argument. */
    Mp_MathProc *proc;			/* Procedure that implements the
					 * math function. */
    ClientData clientData;		/* Additional value to pass to the
					 * function. */
{
    Tcl_HashEntry *hPtr;
    Tcl_HashTable *ZQMathTablePtr;
    Mp_MathFunc *mathFuncPtr;
    int new, i;

    ZQMathTablePtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, 
			MATH_TABLE_NAME, NULL);
    hPtr = Tcl_CreateHashEntry(ZQMathTablePtr, name, &new);

    if (new) {
	Tcl_SetHashValue(hPtr, ckalloc(sizeof(Mp_MathFunc)));
    }
    mathFuncPtr = (Mp_MathFunc *) Tcl_GetHashValue(hPtr);
    if (numArgs > MP_MAX_MATH_ARGS) {
	numArgs = MP_MAX_MATH_ARGS;
    }
    mathFuncPtr->numArgs = numArgs;
    for (i = 0; i < numArgs; i++) {
	mathFuncPtr->argTypes[i] = argTypes[i];
    }
    mathFuncPtr->proc = proc;
    mathFuncPtr->clientData = clientData;
}

/*
 *----------------------------------------------------------------------
 *
 * ExprFreeMathArgs
 *
 */

static void
ExprFreeMathArgs (args)
    Mp_Value *args;
{
    int i;
    for (i = 0; i < MP_MAX_MATH_ARGS; i++) {
	zfree(args[i].intValue);
	Qfree(args[i].doubleValue);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ExprMathFunc --
 *
 *	This procedure is invoked to parse a math function from an
 *	expression string, carry out the function, and return the
 *	value computed.
 *
 * Results:
 *	TCL_OK is returned if all went well and the function's value
 *	was computed successfully.  If an error occurred, TCL_ERROR
 *	is returned and an error message is left in interp->result.
 *	After a successful return infoPtr has been updated to refer
 *	to the character just after the function call, the token is
 *	set to VALUE, and the value is stored in valuePtr.
 *
 * Side effects:
 *	Embedded commands could have arbitrary side-effects.
 *
 *----------------------------------------------------------------------
 */

static int
ExprMathFunc(interp, infoPtr, valuePtr)
    Tcl_Interp *interp;			/* Interpreter to use for error
					 * reporting. */
    register ExprInfo *infoPtr;		/* Describes the state of the parse.
					 * infoPtr->expr must point to the
					 * first character of the function's
					 * name. */
    register Mp_Value *valuePtr;	/* Where to store value, if that is
					 * what's parsed from string.  Caller
					 * must have initialized pv field
					 * correctly. */
{
    Mp_MathFunc *mathFuncPtr;		/* Info about math function. */
    Mp_Value args[MP_MAX_MATH_ARGS];	/* Arguments for function call. */
    Mp_Value funcResult;		/* Result of function call. */
    Tcl_HashEntry *hPtr;
    Tcl_HashTable *ZQMathTablePtr;
    char *p, *funcName, savedChar;
    int i, result;

    funcResult.intValue    = _zero_;
    funcResult.doubleValue = qlink(&_qzero_);
    funcResult.type        = MP_UNDEF;

    for (i = 0; i < MP_MAX_MATH_ARGS; i++) {
	args[i].type = MP_UNDEF;
	args[i].intValue = _zero_;
	args[i].doubleValue = qlink(&_qzero_);
    }

    ZQMathTablePtr = (Tcl_HashTable *) Tcl_GetAssocData(interp, 
			MATH_TABLE_NAME, NULL);
    /*
     * Find the end of the math function's name and lookup the MathFunc
     * record for the function.
     */

    p = funcName = infoPtr->expr;
    while (isalnum(UCHAR(*p)) || (*p == '_')) {
	p++;
    }
    infoPtr->expr = p;
    result = ExprLex(interp, infoPtr, valuePtr);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    if (infoPtr->token != OPEN_PAREN) {
	goto syntaxError;
    }
    savedChar = *p;
    *p = 0;
    hPtr = Tcl_FindHashEntry(ZQMathTablePtr, funcName);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "unknown math function \"", funcName,
		"\"", (char *) NULL);
	*p = savedChar;
	return TCL_ERROR;
    }
    *p = savedChar;
    mathFuncPtr = (Mp_MathFunc *) Tcl_GetHashValue(hPtr);

    /*
     * Scan off the arguments for the function, if there are any.
     */

    if (mathFuncPtr->numArgs == 0) {
	result = ExprLex(interp, infoPtr, valuePtr);
	if ((result != TCL_OK) || (infoPtr->token != CLOSE_PAREN)) {
	    goto syntaxError;
	}
    } else {
	for (i = 0; ; i++) {
	    valuePtr->pv.next = valuePtr->pv.buffer;
	    result = ExprGetValue(interp, infoPtr, -1, valuePtr);
	    if (result != TCL_OK) {
		ExprFreeMathArgs(args);
        	zfree(funcResult.intValue);
        	Qfree(funcResult.doubleValue);
		return result;
	    }
	    if (valuePtr->type == MP_STRING) {
		ExprFreeMathArgs(args);
        	zfree(funcResult.intValue);
        	Qfree(funcResult.doubleValue);
		interp->result =
			"argument to math function didn't have numeric value";
		return TCL_ERROR;
	    }
    
	    /*
	     * Copy the value to the argument record, converting it if
	     * necessary.
	     */
    
	    if (valuePtr->type == MP_INT) {
		if (mathFuncPtr->argTypes[i] == MP_DOUBLE) {
		    args[i].type = MP_DOUBLE;
		    zcopy(valuePtr->intValue, &(args[i].intValue));
		    args[i].doubleValue = qlink(&_qzero_);
		    ExprConvIntToDouble(&args[i]);
		} else {
		    args[i].type = MP_INT;
		    zcopy(valuePtr->intValue, &(args[i].intValue));
		    args[i].doubleValue = qlink(&_qzero_);
		}
	    } else { 
		if (mathFuncPtr->argTypes[i] == MP_INT) {
		    args[i].type = MP_INT;
		    args[i].doubleValue = qcopy(valuePtr->doubleValue);
		    args[i].intValue = _zero_;
		    ExprConvDoubleToInt(&args[i]);
		} else {
		    args[i].type = MP_DOUBLE;
		    args[i].intValue = _zero_;
		    args[i].doubleValue = qcopy(valuePtr->doubleValue);
		}
	    }
    
	    /*
	     * Check for a comma separator between arguments or a close-paren
	     * to end the argument list.
	     */
    
	    if (i == (mathFuncPtr->numArgs-1)) {
		if (infoPtr->token == CLOSE_PAREN) {
		    break;
		}
		if (infoPtr->token == COMMA) {
		    interp->result = "too many arguments for math function";
		    ExprFreeMathArgs(args);
        	    zfree(funcResult.intValue);
        	    Qfree(funcResult.doubleValue);
		    return TCL_ERROR;
		} else {
		    goto syntaxError;
		}
	    }
	    if (infoPtr->token != COMMA) {
		if (infoPtr->token == CLOSE_PAREN) {
		    interp->result = "too few arguments for math function";
		    ExprFreeMathArgs(args);
        	    zfree(funcResult.intValue);
        	    Qfree(funcResult.doubleValue);
		    return TCL_ERROR;
		} else {
		    goto syntaxError;
		}
	    }
	}
    }
    if (MpnoEval) {
	ExprFreeMathArgs(args);
        zfree(funcResult.intValue);
        Qfree(funcResult.doubleValue);
	valuePtr->type = MP_INT;
	zfree(valuePtr->intValue);
	valuePtr->intValue = _zero_;
	infoPtr->token = VALUE;
	return TCL_OK;
    }

    /*
     * Invoke the function and copy its result back into valuePtr.
     */

    Mp_MathInProgress++;
    result = (*mathFuncPtr->proc)(mathFuncPtr->clientData, interp, args,
	    &funcResult);
    Mp_MathInProgress--;
    
    ExprFreeMathArgs(args);

    if (result != TCL_OK) {
        zfree(funcResult.intValue);
        Qfree(funcResult.doubleValue);
	return result;
    }
    if (funcResult.type == MP_INT) {
	valuePtr->type = MP_INT;
	zfree(valuePtr->intValue);
	zcopy(funcResult.intValue, &valuePtr->intValue);
    } else {
	valuePtr->type = MP_DOUBLE;
	Qfree(valuePtr->doubleValue);
	valuePtr->doubleValue = qcopy(funcResult.doubleValue);
    }
    infoPtr->token = VALUE;
    zfree(funcResult.intValue);
    Qfree(funcResult.doubleValue);
    return TCL_OK;

    syntaxError:
    zfree(funcResult.intValue);
    Qfree(funcResult.doubleValue);
    Tcl_AppendResult(interp, "syntax error in expression \"",
	    infoPtr->originalExpr, "\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Math Functions --
 *
 *	This page contains the procedures that implement all of the
 *	built-in math functions for expressions.
 *
 * Results:
 *	Each procedure returns TCL_OK if it succeeds and places result
 *	information at *resultPtr.  If it fails it returns TCL_ERROR
 *	and leaves an error message in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ExprUnaryFunc(clientData, interp, args, resultPtr)
    ClientData clientData;		/* Contains address of procedure that
					 * takes one double argument and
					 * returns a double result. */
    Tcl_Interp *interp;
    Mp_Value *args;
    Mp_Value *resultPtr;
{
    NUMBER *(*func) _ANSI_ARGS_((NUMBER *, NUMBER *)) = 
		  (NUMBER *(*) _ANSI_ARGS_((NUMBER *,NUMBER *))) clientData;
    NUMBER *q_tmp;

    resultPtr->type = MP_DOUBLE;
    q_tmp = (*func)(args[0].doubleValue, mp_epsilon);
    Qfree(resultPtr->doubleValue);
    resultPtr->doubleValue = q_tmp;
    return TCL_OK;
}

static int
ExprUnaryZFunc(clientData, interp, args, resultPtr)
    ClientData clientData;		/* Contains address of procedure that
					 * takes one int argument and
					 * returns a int result. */
    Tcl_Interp *interp;
    Mp_Value *args;
    Mp_Value *resultPtr;
{
    void (*func) _ANSI_ARGS_((ZVALUE, ZVALUE *)) = 
		  (void(*) _ANSI_ARGS_((ZVALUE,ZVALUE *))) clientData;

    resultPtr->type = MP_INT;
    zfree(resultPtr->intValue);
    (void) (*func)(args[0].intValue, &resultPtr->intValue);
    return TCL_OK;
}

static int
ExprBinaryFunc(clientData, interp, args, resultPtr)
    ClientData clientData;		/* Contains address of procedure that
					 * takes two double arguments and
					 * returns a double result. */
    Tcl_Interp *interp;
    Mp_Value *args;
    Mp_Value *resultPtr;
{
    NUMBER *(*func) _ANSI_ARGS_((NUMBER *, NUMBER *, NUMBER *))
	= (NUMBER *(*)_ANSI_ARGS_((NUMBER *, NUMBER *, NUMBER *))) clientData;

    NUMBER *q_tmp;

    resultPtr->type = MP_DOUBLE;
    q_tmp = (*func)(args[0].doubleValue, args[1].doubleValue, mp_epsilon);
    Qfree(resultPtr->doubleValue);
    resultPtr->doubleValue = q_tmp;
    return TCL_OK;
}

static int
ExprBinary2Func(clientData, interp, args, resultPtr)
    ClientData clientData;		/* Contains address of procedure that
					 * takes two double arguments and
					 * returns a double result. */
    Tcl_Interp *interp;
    Mp_Value *args;
    Mp_Value *resultPtr;
{
    NUMBER *(*func) _ANSI_ARGS_((NUMBER *, NUMBER *))
	= (NUMBER *(*)_ANSI_ARGS_((NUMBER *, NUMBER *))) clientData;

    NUMBER *q_tmp;

    resultPtr->type = MP_DOUBLE;
    q_tmp = (*func)(args[0].doubleValue, args[1].doubleValue);
    Qfree(resultPtr->doubleValue);
    resultPtr->doubleValue = q_tmp;
    return TCL_OK;
}

static int
ExprBinaryZFunc(clientData, interp, args, resultPtr)
    ClientData clientData;		/* Contains address of procedure that
					 * takes two int arguments and
					 * returns a int result. */
    Tcl_Interp *interp;
    Mp_Value *args;
    Mp_Value *resultPtr;
{
    void (*func) _ANSI_ARGS_((ZVALUE, ZVALUE, ZVALUE *))
	= (void (*)_ANSI_ARGS_((ZVALUE, ZVALUE, ZVALUE *))) clientData;


    resultPtr->type = MP_INT;
    zfree(resultPtr->intValue);
    (void) (*func)(args[0].intValue, args[1].intValue, &resultPtr->intValue);
    return TCL_OK;
}


/* EFP */
static int
ExprTertiaryZFunc(clientData, interp, args, resultPtr)
ClientData clientData;		/* Contains address of procedure that
                                 * takes two int arguments and
                                 * returns a int result. */
Tcl_Interp *interp;
Mp_Value *args;
Mp_Value *resultPtr;
{
    void (*func) _ANSI_ARGS_((ZVALUE, ZVALUE, ZVALUE, ZVALUE *))
    = (void (*)_ANSI_ARGS_((ZVALUE, ZVALUE, ZVALUE, ZVALUE *))) clientData;


    resultPtr->type = MP_INT;
    zfree(resultPtr->intValue);
    (void) (*func)(args[0].intValue, args[1].intValue, args[2].intValue, &resultPtr->intValue);
    return TCL_OK;
}


	/* ARGSUSED */
static int
ExprAbsFunc(clientData, interp, args, resultPtr)
    ClientData clientData;
    Tcl_Interp *interp;
    Mp_Value *args;
    Mp_Value *resultPtr;
{
    resultPtr->type = MP_DOUBLE;
    if (args[0].type == MP_DOUBLE) {
	resultPtr->type = MP_DOUBLE;
	Qfree(resultPtr->doubleValue);
	if (qisneg(args[0].doubleValue)) {
	    resultPtr->doubleValue = qneg(args[0].doubleValue);
	} else {
	    resultPtr->doubleValue = qcopy(args[0].doubleValue);
	}
    } else {
	resultPtr->type = MP_INT;
	zfree(resultPtr->intValue);
	zcopy(args[0].intValue, &resultPtr->intValue);
	if (zisneg(args[0].intValue)) {
	    zneg(resultPtr->intValue);
	}
    }
    return TCL_OK;
}

	/* ARGSUSED */
	/* ARGSUSED */
static int
ExprPiFunc(clientData, interp, args, resultPtr)
    ClientData clientData;
    Tcl_Interp *interp;
    Mp_Value *args;
    Mp_Value *resultPtr;
{
    resultPtr->type = MP_DOUBLE;
    Qfree(resultPtr->doubleValue);
    resultPtr->doubleValue = qpi(mp_epsilon);
    return TCL_OK;
}

	/* ARGSUSED */
static int
ExprDoubleFunc(clientData, interp, args, resultPtr)
    ClientData clientData;
    Tcl_Interp *interp;
    Mp_Value *args;
    Mp_Value *resultPtr;
{
    resultPtr->type = MP_DOUBLE;
    if (args[0].type == MP_DOUBLE) {
	Qfree(resultPtr->doubleValue);
	resultPtr->doubleValue = qcopy(args[0].doubleValue);
    } else {
        zfree(resultPtr->intValue);
	zcopy(args[0].intValue, &resultPtr->intValue);
	ExprConvIntToDouble(resultPtr);
    }
    return TCL_OK;
}

	/* ARGSUSED */
static int
ExprIntFunc(clientData, interp, args, resultPtr)
    ClientData clientData;
    Tcl_Interp *interp;
    Mp_Value *args;
    Mp_Value *resultPtr;
{
    int neg;

    resultPtr->type = MP_INT;
    if (args[0].type == MP_INT) {
	zfree(resultPtr->intValue);
	zcopy(args[0].intValue, &resultPtr->intValue );
    } else {
	Qfree(resultPtr->doubleValue);
	neg = qisneg(args[0].doubleValue);
	resultPtr->doubleValue = qcopy(args[0].doubleValue);
	ExprConvDoubleToInt(resultPtr);
	if (neg && !zisneg(resultPtr->intValue)) {
	    zneg(resultPtr->intValue);
	}
    }
    return TCL_OK;
}

	/* ARGSUSED */
static int
ExprRoundFunc(clientData, interp, args, resultPtr)
    ClientData clientData;
    Tcl_Interp *interp;
    Mp_Value *args;
    Mp_Value *resultPtr;
{
    NUMBER *q_tmp;

    resultPtr->type = MP_INT;
    if (args[0].type == MP_INT) {
	zfree(resultPtr->intValue);
	zcopy(args[0].intValue, &resultPtr->intValue );
    } else {
	Qfree(resultPtr->doubleValue);
	q_tmp = qround(args[0].doubleValue, 0L);
	resultPtr->doubleValue = qcopy(q_tmp);
	Qfree(q_tmp);
	ExprConvDoubleToInt(resultPtr);
    }
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * ExprLooksLikeInt --
 *
 *	This procedure decides whether the leading characters of a
 *	string look like an integer or something else (such as a
 *	floating-point number or string).
 *
 * Results:
 *	The return value is 1 if the leading characters of p look
 *	like a valid Tcl integer.  If they look like a floating-point
 *	number (e.g. "e01" or "2.4"), or if they don't look like a
 *	number at all, then 0 is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ExprLooksLikeInt(p)
    char *p;			/* Pointer to string. */
{
    while (isspace(UCHAR(*p))) {
	p++;
    }
    if ((*p == '+') || (*p == '-')) {
	p++;
    }
    if (!isdigit(UCHAR(*p))) {
	return 0;
    }
    p++;
    while (isdigit(UCHAR(*p))) {
	p++;
    }
    if ((*p != '.') && (*p != 'e') && (*p != 'E')) {
	return 1;
    }
    return 0;
}


static NUMBER *
qceil (q)
    NUMBER *q;
{
    NUMBER *res;
    NUMBER *q2;
    int neg;

    if (qisint(q)) {
      return qlink(q);
    }
    neg = qisneg(q);
    if (qispos(q)) {
      q2 = qadd(q, &_qone_);
    } else {
      q2 = qcopy(q);
    }
    res = qint(q2);
    if (neg && !qisneg(res)) {
      Qfree(q2);
      q2 = qneg(res);
      Qfree(res);
      return q2;
    }
    Qfree(q2);
    return res;
}


static NUMBER *
qfloor (q)
    NUMBER *q;
{
    NUMBER *res;
    NUMBER *q2;
    int neg;

    if (qisint(q)) {
      return qlink(q);
    }
    neg = qisneg(q);
    if (qisneg(q)) {
      q2 = qsub(q, &_qone_);
    } else {
      q2 = qcopy(q);
    }
    res = qint(q2);
    if (neg && !qisneg(res)) {
      Qfree(q2);
      q2 = qneg(res);
      Qfree(res);
      return q2;
    }
    Qfree(q2);
    return res;
}


static NUMBER *
qlog (q)
    NUMBER *q;
{
    NUMBER *q_res;

    q_res = qln(q, mp_epsilon);
    return q_res;
}

static NUMBER *
qlog10 (q)
    NUMBER *q;
{
    NUMBER *q2, *q3, *q4, *q5, *q_res;

    q2 = itoq(10);
    q3 = qln(q,mp_epsilon); 
    q4 = qln(q2,mp_epsilon);
    q5 = qdiv(q3,q4);
    q_res = qround(q5, mp_precision);
    Qfree(q2);
    Qfree(q3);
    Qfree(q4);
    Qfree(q5);
    return q_res;
}


/* reimplement atoz to return terminating char pointer */

static void
Atoz (s, res, term)
    register char *s;
    ZVALUE *res;
    char **term;
{
    ZVALUE z, ztmp, digit;
    HALF digval;
    BOOL minus;
    long shift;

    minus = FALSE;
    shift = 0;
    if (*s == '+')
	    s++;
    else if (*s == '-') {
	    minus = TRUE;
	    s++;
    }
    /* eat any whitespace */
    while (*s == ' ' || *s == '\t') {
       s++;
    }
    if (*s == '0') {                /* possibly hex, octal, or binary */
	    s++;
	    if ((*s >= '0') && (*s <= '9')) {
		    shift = 3;
	    } else if ((*s == 'x') || (*s == 'X')) {
		    shift = 4;
		    s++;
	    } else if ((*s == 'b') || (*s == 'B')) {
		    shift = 1;
		    s++;
	    }
    }
    digit.v = &digval;
    digit.len = 1;
    digit.sign = 0;
    z = _zero_;
    while (*s) {
	    /* tp: check digit */
	    switch (shift) {

	      case 0:
		  if (*s < '0' || *s > '9') {
		    goto badnum;
		  }
		  break;

	      case 1:
		  if (*s < '0' || *s > '1') {
		    goto badnum;
		  }
		  break;

	      case 3:
		  if (*s < '0' || *s > '7') {
		    goto badnum;
		  }
		  break;

	      case 4:
		  if (!( (*s >= '0' && *s <= '9') || 
		         (*s >= 'a' && *s <= 'f') ||
		         (*s >= 'A' && *s <= 'F')  ) ) {
		    goto badnum;
		  }
		  break;

	      default:
		  break;
	    }

	    digval = *s;
	    if ((digval >= '0') && (digval <= '9'))
		    digval -= '0';
	    else if ((digval >= 'a') && (digval <= 'f') && shift)
		    digval -= ('a' - 10);
	    else if ((digval >= 'A') && (digval <= 'F') && shift)
		    digval -= ('A' - 10);
	    else
		    break;
	    if (shift)
		    zshift(z, shift, &ztmp);
	    else
		    zmuli(z, 10L, &ztmp);
	    zfree(z);
	    zadd(ztmp, digit, &z);
	    zfree(ztmp);
	    s++;
    }

  badnum:

    ztrim(&z);
    if (minus && !ziszero(z))
	    z.sign = 1;
    *res = z;
    *term = s;
    return;
}






/* reimplement atoq to return terminating char pointer */
NUMBER *
Atoq (s, term)
    register char *s;
    char **term;
{
    register NUMBER *q;
    register char *t;
    ZVALUE div, newnum, newden, tmp;
    long decimals, exp;
    BOOL hex, negexp;
    char *tmp_term = NULL;
    long valid_q;
    char *org;

    decimals = 0;
    exp = 0;
    negexp = FALSE;
    hex = FALSE;
    org = s;
    t = s;
    
    /* eliminate any leading zeros and/or hex & binary characters */
    while (*t) {
        if (*t == '0' || *t == 'x' || *t == 'X' || *t == 'b' || *t == 'B' ) {
	    t++;
	} else {
	    break;
	}
    }
    s = t;

    valid_q = qparse(s, 0);
    *term = s + (valid_q <= 0 ? 0 : valid_q);
    if (valid_q <= 0) {
        q = qlink(&_qzero_);
	*term = org;
	return q;
    }
    q = qalloc();

    if ((*t == '+') || (*t == '-'))
	    t++;
    if ((*t == '0') && ((t[1] == 'x') || (t[1] == 'X'))) {
	    hex = TRUE;
	    t += 2;
    }
    while (((*t >= '0') && (*t <= '9')) || (hex &&
	    (((*t >= 'a') && (*t <= 'f')) || ((*t >= 'A') && (*t <= 'F')))))                        t++;
    if ((*t == '.') || (*t == 'e') || (*t == 'E')) {
	    if (*t == '.') {
		    t++;
		    while ((*t >= '0') && (*t <= '9')) {
			    t++;
			    decimals++;
		    }
	    }
	    /*
	     * Parse exponent if any
	     */
	    if ((*t == 'e') || (*t == 'E')) {
		    t++;
		    if (*t == '+')
			    t++;
		    else if (*t == '-') {
			    negexp = TRUE;
			    t++;
		    }
		    while ((*t >= '0') && (*t <= '9')) {
			    exp = (exp * 10) + *t++ - '0';
			    if (exp > 1000000)
				    math_error("Exponent too large");
		    }

	    }
	    tmp_term = t;
	    *term = t;
	    ztenpow(decimals, &q->den);
    }
    atoz(s, &q->num);
    if (qiszero(q)) {
	    Qfree(q);
	    return qlink(&_qzero_);
    }
    /*
     * Apply the exponential if any
     */
    if (exp) {
	    ztenpow(exp, &tmp);
	    if (negexp) {
		    zmul(q->den, tmp, &newden);
		    zfree(q->den);
		    q->den = newden;
	    } else {
		    zmul(q->num, tmp, &newnum);
		    zfree(q->num);
		    q->num = newnum;
	    }
	    zfree(tmp);
    }
    /*
     * Reduce the fraction to lowest terms
     */
    if (zisunit(q->num) || zisunit(q->den))
	    return q;
    zgcd(q->num, q->den, &div);
    if (zisunit(div)) {
            zfree(div);
	    return q;
    }
    zquo(q->num, div, &newnum);
    zfree(q->num);
    zquo(q->den, div, &newden);
    zfree(q->den);
    zfree(div);
    q->num = newnum;
    q->den = newden;
    return q;
}



/* reimplement atoq to return terminating char pointer */
static NUMBER *
Afractoq (s, term)
    register char *s;
    char **term;
{
    register NUMBER *q;
    register char *t;
    ZVALUE div, newnum, newden, tmp;
    long decimals, exp;
    BOOL hex, negexp;
    char *tmp_term = NULL;

    q = qalloc();
    decimals = 0;
    exp = 0;
    negexp = FALSE;
    hex = FALSE;
    t = s;
    if ((*t == '+') || (*t == '-'))
	    t++;
    if ((*t == '0') && ((t[1] == 'x') || (t[1] == 'X'))) {
	    hex = TRUE;
	    t += 2;
    }
    while (((*t >= '0') && (*t <= '9')) || (hex &&
	    (((*t >= 'a') && (*t <= 'f')) || ((*t >= 'A') && (*t <= 'F')))))                        t++;
    if (*t == '/') {
	    t++;
	    atoz(t, &q->den);
	    *term = tmp_term;
    } else if ((*t == '.') || (*t == 'e') || (*t == 'E')) {
	    if (*t == '.') {
		    t++;
		    while ((*t >= '0') && (*t <= '9')) {
			    t++;
			    decimals++;
		    }
	    }
	    /*
	     * Parse exponent if any
	     */
	    if ((*t == 'e') || (*t == 'E')) {
		    t++;
		    if (*t == '+')
			    t++;
		    else if (*t == '-') {
			    negexp = TRUE;
			    t++;
		    }
		    while ((*t >= '0') && (*t <= '9')) {
			    exp = (exp * 10) + *t++ - '0';
			    if (exp > 1000000)
				    math_error("Exponent too large");
		    }

	    }
	    tmp_term = t;
	    *term = t;
	    ztenpow(decimals, &q->den);
    }
    atoz(s, &q->num);
    if (qiszero(q)) {
	    Qfree(q);
	    return qlink(&_qzero_);
    }
    /*
     * Apply the exponential if any
     */
    if (exp) {
	    ztenpow(exp, &tmp);
	    if (negexp) {
		    zmul(q->den, tmp, &newden);
		    zfree(q->den);
		    q->den = newden;
	    } else {
		    zmul(q->num, tmp, &newnum);
		    zfree(q->num);
		    q->num = newnum;
	    }
	    zfree(tmp);
    }
    /*
     * Reduce the fraction to lowest terms
     */
    if (zisunit(q->num) || zisunit(q->den))
	    return q;
    zgcd(q->num, q->den, &div);
    if (zisunit(div))
	    return q;
    zquo(q->num, div, &newnum);
    zfree(q->num);
    zquo(q->den, div, &newden);
    zfree(q->den);
    q->num = newnum;
    q->den = newden;
    return q;
}

/* Qfree  - safe interface to qfree */
/*
static void
Qfree(q)
    NUMBER *q;
{
    if (q == NULL) {
	return;
    }
    qfree(q);
}
*/

static void
Zlowfactor(z1, z2, res) 
    ZVALUE z1;
    ZVALUE z2;
    ZVALUE *res;
{
    long   count;
    long   lowest;

    count = ztoi(z2);
    lowest = zlowfactor(z1, count);
    itoz(lowest, res);
}


static void
Zprimetest(z1, z2, res) 
    ZVALUE z1;
    ZVALUE z2;
    ZVALUE *res;
{   
    long   count;

    count = ztoi(z2);
    if ( zprimetest(z1, count) ) {
	zcopy(_one_, res);
    } else {
	zcopy(_zero_, res);
    }
}



static void
Zrelprime(z1, z2, res) 
    ZVALUE z1;
    ZVALUE z2;
    ZVALUE *res;
{

    if ( zrelprime(z1, z2) ) {
	zcopy(_one_, res);
    } else {
	zcopy(_zero_, res);
    }
}



