/*
 * mpformat.c
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
 */

#include "mpexpr.h"


EXTERN void			Qprintf _ANSI_ARGS_((int argc, char **argv));

int
Mp_FormatString(interp, argc, argv)
    Tcl_Interp *interp;
    int         argc;
    char      **argv;
{
    char *string;

    argc--;  		/* skip past command name */
    argv = &argv[1];

    math_divertio();
    Qprintf(argc, argv);
    string = math_getdivertedio();
    Tcl_SetResult(interp, string, TCL_VOLATILE);
    math_cleardiversions();
    ckfree(string);
    return TCL_OK;
}

#define PUTSTR(str)    math_str(str)
#define PUTCHAR(ch)    math_chr(ch)


/*
 * wrapper for qprintf
 */

void
Qprintf (argc, argv)
  int    argc;
  char **argv;
	
{
    
    char   *fmt = argv[0];
    NUMBER *q, *q2;
    int ch, sign;
    long width, precision;
    int i = 1;
    char *term;
   
    argc--;  /* fmt string was first */

    while ((ch = *fmt++) != '\0') {
    	if (ch == '\\') {
       	    ch = *fmt++;
            switch (ch) {
       	        case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
            	case 't': ch = '\t'; break;
            	case 'f': ch = '\f'; break;
    	    	case 'v': ch = '\v'; break;
    	    	case 'b': ch = '\b'; break;
    	    	case 0:
                    return;
            }
            PUTCHAR(ch);
            continue;
        }
    	if (ch != '%') {
    	    PUTCHAR(ch);
    	    continue;
    	}
        ch = *fmt++;
    	width = 0; 
	precision = 8; 
	sign = 1;

percent:	;
    	switch (ch) {
    	    case 'd':
                if (argc > 0) {
		    q = Atoq(argv[i],&term); i++;  argc--;
		} else { 
		    q = qlink(&_qzero_);
		}
                qprintfd(q, width);
		qfree(q);
    	    	break;
    	    case 'f':
                if (argc > 0) {
		    q = Atoq(argv[i],&term); i++;  argc--;
		    q2 = qround(q,precision);
		} else { 
		    q = qlink(&_qzero_);
		    q2 = qlink(&_qzero_);
		}
    	    	Qprintff(q2, width, precision);
		qfree(q);
		qfree(q2);
    	    	break;
    	    case 'e':
                if (argc > 0) {
		    q = Atoq(argv[i],&term); i++;  argc--;
		} else { 
		    q = qlink(&_qzero_);
		}
    	    	qprintfe(q, width, precision);
		qfree(q);
    	    	break;
    	    case 'r':
    	    case 'R':
                if (argc > 0) {
		    q = Atoq(argv[i],&term); i++;  argc--;
		} else { 
		    q = qlink(&_qzero_);
		}
    	        qprintfr(q, width, (BOOL) (ch == 'R'));
		qfree(q);
    	    	break;
    	    case 'N':
                if (argc > 0) {
		    q = Atoq(argv[i],&term); i++;  argc--;
		} else { 
		    q = qlink(&_qzero_);
		}
            	zprintval(q->num, 0L, width);
		qfree(q);
    	    	break;
    	    case 'D':
                if (argc > 0) {
		    q = Atoq(argv[i],&term); i++;  argc--;
		} else { 
		    q = qlink(&_qzero_);
		}
    	    	zprintval(q->den, 0L, width);
		qfree(q);
    	    	break;
    	    case 'o':
                if (argc > 0) {
		    q = Atoq(argv[i],&term); i++;  argc--;
		} else { 
		    q = qlink(&_qzero_);
		}
    	    	qprintfo(q, width);
		qfree(q);
    	    	break;
    	    case 'x':
                if (argc > 0) {
		    q = Atoq(argv[i],&term); i++;  argc--;
		} else { 
		    q = qlink(&_qzero_);
		}
                qprintfx(q, width);
		qfree(q);
    	    	break;
    	    case 'b':
                if (argc > 0) {
		    q = Atoq(argv[i],&term); i++;  argc--;
		} else { 
		    q = qlink(&_qzero_);
		}
    	    	qprintfb(q, width);
		qfree(q);
    	    	break;
	    case 's':
		if (argc > 0) {
		    PUTSTR(argv[i]);
		    argc--; i++;
		}
		break;
	    case 'c':
		if (argc > 0) {
		    PUTCHAR(*argv[i]);
		    argc--; i++;
		}
		break;
            case 0:
    	    	return;
	    case '%':
		PUTCHAR('%');
		break;
    	    case '-':
            	sign = -1;
    	    	ch = *fmt++;
    	    default:
		if (('0' <= ch && ch <= '9') || ch == '.' || ch == '*') {
		    if (ch == '*') {
                        if (argc > 0) {
		            width = atoi(argv[i]); i++;  argc--;
		        } else { 
		            width = 0;
		        }
		    	width = sign * width;
		        ch = *fmt++;
		    } else if (ch != '.') {
			if (ch < '0' || ch > '9') {
			    goto percent;
			}
		    	width = ch - '0';
		    	while ('0' <= (ch = *fmt++) && ch <= '9') {
			        width = width * 10 + ch - '0';
			}
			width *= sign;
		    }
		    if (ch == '.') {
		        if ((ch = *fmt++) == '*') {
                            if (argc > 0) {
		                precision = atoi(argv[i]); i++;  argc--;
		            } else { 
		                precision = 8;
		            }
		 	    ch = *fmt++;
		        } else {
			    if (ch < '0' || ch > '9') {
			        goto percent;
			    }
			    precision = ch - '0';
			    while ('0' <= (ch = *fmt++) && ch <= '9') {
			    	 precision = precision * 10 + ch - '0';
			    }
			}
	            }
    	            goto percent;
	        }
        }
    }
}


