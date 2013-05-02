/*
 * Copyright (c) 1994 David I. Bell
 * Permission is granted to use, distribute, or modify this source,
 * provided that this copyright notice remains intact.
 *
 * Data structure declarations for extended precision integer arithmetic.
 * The assumption made is that a long is 32 bits and shorts are 16 bits,
 * and longs must be addressible on word boundaries.
 */

#ifndef	ZMATH_H
#define	ZMATH_H

#include <stdio.h>

#include "longbits.h"

#include <tcl.h>

#ifdef NO_STDLIB_H
#include "../compat/stdlib.h"
#else
#include <stdlib.h>
#endif
#ifdef NO_STRING_H
#include "../compat/string.h"
#else
#include <string.h>
#endif
#if defined(__STDC__) || defined(HAS_STDARG)
#include <stdarg.h>
#else
#include <varargs.h>
#endif


#ifndef BYTE_ORDER
#include "calcendian.h"
#endif


#ifndef ALLOCTEST
# if defined(CALC_MALLOC)
#  define freeh(p) (((void *)p == (void *)_zeroval_) ||			\
		    ((void *)p == (void *)_oneval_) || 			\
		    ((void *)p == (void *)_twoval_) || 			\
		    ((void *)p == (void *)_tenval_) || 			\
		    ckfree((void *)p))
# else
#  define freeh(p) { if (((void *)p != (void *)_zeroval_) &&		\
			 ((void *)p != (void *)_oneval_) &&		\
			 ((void *)p != (void *)_twoval_) &&		\
			 ((void *)p != (void *)_tenval_)		\
			) ckfree((void *)p); }
# endif
#endif


typedef	int FLAG;			/* small value (e.g. comparison) */
typedef int BOOL;			/* TRUE or FALSE value */
typedef unsigned long HASH;		/* hash value */


#if !defined(TRUE)
#define	TRUE	((BOOL) 1)			/* booleans */
#endif
#if !defined(FALSE)
#define	FALSE	((BOOL) 0)
#endif


/*
 * NOTE: FULL must be twice the storage size of a HALF
 *	 LEN storage size must be <= FULL storage size
 */

#if LONG_BITS == 64			/* for 64-bit machines */
typedef unsigned int HALF;		/* unit of number storage */
typedef int SHALF;			/* signed HALF */
typedef unsigned long FULL;		/* double unit of number storage */
typedef long LEN;			/* unit of length storage */

#define BASE	((FULL) 4294967296)	/* base for calculations (2^32) */
#define BASE1	((FULL) (BASE - 1))	/* one less than base */
#define BASEB	32			/* number of bits in base */
#define	BASEDIG	10			/* number of digits in base */
#define	MAXHALF	((FULL) 0x7fffffff)	/* largest positive half value */
#define	MAXFULL	((FULL) 0x7fffffffffffffff) /* largest positive full value */
#define	TOPHALF	((FULL) 0x80000000)	/* highest bit in half value */
#define	TOPFULL	((FULL) 0x8000000000000000)	/* highest bit in full value */
#define MAXLEN	((LEN)	0x7fffffffffffffff)	/* longest value allowed */
#define FULLFMT "l"                     /* printf length specifier for FULL */

#else					/* for 32-bit machines */
typedef unsigned short HALF;		/* unit of number storage */
typedef short SHALF;			/* signed HALF */
typedef unsigned long FULL;		/* double unit of number storage */
typedef long LEN;			/* unit of length storage */

#define BASE	((FULL) 65536)		/* base for calculations (2^16) */
#define BASE1	((FULL) (BASE - 1))	/* one less than base */
#define BASEB	16			/* number of bits in base */
#define	BASEDIG	5			/* number of digits in base */
#define	MAXHALF	((FULL) 0x7fff)		/* largest positive half value */
#define	MAXFULL	((FULL) 0x7fffffff)	/* largest positive full value */
#define	TOPHALF	((FULL) 0x8000)		/* highest bit in half value */
#define	TOPFULL	((FULL) 0x80000000)	/* highest bit in full value */
#define MAXLEN	((LEN)	0x7fffffff)	/* longest value allowed */
#define FULLFMT "l"                     /* printf length specifier for FULL */
#endif

#define	MAXREDC	5			/* number of entries in REDC cache */
#define	SQ_ALG2	20			/* size for alternative squaring */
#define	MUL_ALG2 20			/* size for alternative multiply */
#define	POW_ALG2 40			/* size for using REDC for powers */
#define	REDC_ALG2 50			/* size for using alternative REDC */


typedef union {
	FULL	ivalue;
	struct {
		HALF Svalue1;
		HALF Svalue2;
	} sis;
} SIUNION;



#if !defined(LITTLE_ENDIAN)
#define LITTLE_ENDIAN	1234	/* Least Significant Byte first */
#endif
#if !defined(BIG_ENDIAN)
#define BIG_ENDIAN	4321	/* Most Significant Byte first */
#endif
/* PDP_ENDIAN - LSB in word, MSW in long is not supported */

#if BYTE_ORDER == LITTLE_ENDIAN
# define silow	sis.Svalue1	/* low order half of full value */
# define sihigh	sis.Svalue2	/* high order half of full value */
#else
# if BYTE_ORDER == BIG_ENDIAN
#  define silow	sis.Svalue2	/* low order half of full value */
#  define sihigh sis.Svalue1	/* high order half of full value */
# else
   :@</*/>@:    BYTE_ORDER must be BIG_ENDIAN or LITTLE_ENDIAN    :@</*/>@:
# endif
#endif


typedef struct {
	HALF	*v;		/* pointer to array of values */
	LEN	len;		/* number of values in array */
	BOOL	sign;		/* sign, nonzero is negative */
} ZVALUE;



/*
 * Function prototypes for integer math routines.
 */
#if defined(__STDC__)
#define MATH_PROTO(a) a
#else
#define MATH_PROTO(a) ()
#endif

extern HALF * alloc MATH_PROTO((LEN len));	/* NEED **/
#ifdef	ALLOCTEST
extern void freeh MATH_PROTO((HALF *));		/* NEED **/
#endif


/*
 * Input, output, and conversion routines.
 */
extern void zcopy MATH_PROTO((ZVALUE z, ZVALUE *res));/* NEED **/
extern void itoz MATH_PROTO((long i, ZVALUE *res));/* NEED **/
extern void atoz MATH_PROTO((CONST char *s, ZVALUE *res));/* NEED **/
extern long ztoi MATH_PROTO((ZVALUE z));/* NEED **/
extern void zprintval MATH_PROTO((ZVALUE z, long decimals, long width));/* NEED **/
extern void Zprintval MATH_PROTO((ZVALUE z, long decimals, long width));/* NEED **/
extern void zprintx MATH_PROTO((ZVALUE z, long width));/* NEED **/
extern void zprintb MATH_PROTO((ZVALUE z, long width));/* NEED **/
extern void zprinto MATH_PROTO((ZVALUE z, long width));/* NEED **/


/*
 * Basic numeric routines.
 */
extern void zmuli MATH_PROTO((ZVALUE z, long n, ZVALUE *res));/* NEED **/
extern long zdivi MATH_PROTO((ZVALUE z, long n, ZVALUE *res));/* NEED **/
extern long zmodi MATH_PROTO((ZVALUE z, long n));/* NEED **/
extern void zadd MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *res));/* NEED **/
extern void zsub MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *res));/* NEED **/
extern void zmul MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *res));/* NEED **/
extern void zdiv MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *res, ZVALUE *rem));/* NEED **/
extern void zquo MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *res));/* NEED **/
extern void zmod MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *rem));/* NEED **/
#if 0
extern BOOL zdivides MATH_PROTO((ZVALUE z1, ZVALUE z2));
#endif
extern void zor MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *res));/* NEED **/
extern void zand MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *res));/* NEED **/
extern void zxor MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *res));/* NEED **/
extern void zshift MATH_PROTO((ZVALUE z, long n, ZVALUE *res));/* NEED **/
extern void zsquare MATH_PROTO((ZVALUE z, ZVALUE *res));/* NEED **/
extern long zlowbit MATH_PROTO((ZVALUE z));/* NEED **/
extern long zhighbit MATH_PROTO((ZVALUE z));/* NEED **/
extern void zbitvalue MATH_PROTO((long n, ZVALUE *res));/* NEED **/
#if 0
extern BOOL zisset MATH_PROTO((ZVALUE z, long n));
#endif
extern BOOL zisonebit MATH_PROTO((ZVALUE z));/* NEED **/
extern BOOL zisallbits MATH_PROTO((ZVALUE z));/* NEED **/
extern FLAG ztest MATH_PROTO((ZVALUE z));/* NEED **/
extern FLAG zrel MATH_PROTO((ZVALUE z1, ZVALUE z2));/* NEED **/
extern BOOL zcmp MATH_PROTO((ZVALUE z1, ZVALUE z2));/* NEED **/


/*
 * More complicated numeric functions.
 */
extern long iigcd MATH_PROTO((long i1, long i2));/* NEED **/
extern void zgcd MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *res));/* NEED **/
extern void zlcm MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *res));/* NEED **/
extern void zreduce MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *z1res, ZVALUE *z2res));/* NEED **/
extern void zfact MATH_PROTO((ZVALUE z, ZVALUE *dest));/* NEED **/
extern void zpfact MATH_PROTO((ZVALUE z, ZVALUE *dest));/* NEED **/
#if 0
extern void zlcmfact MATH_PROTO((ZVALUE z, ZVALUE *dest));
#endif
extern void zperm MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *res));/* NEED **/
extern void zcomb MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *res));/* NEED **/
extern BOOL zprimetest MATH_PROTO((ZVALUE z, long count));/* NEED **/
#if 0
extern FLAG zjacobi MATH_PROTO((ZVALUE z1, ZVALUE z2));
#endif
extern void zfib MATH_PROTO((ZVALUE z, ZVALUE *res));/* NEED **/
extern void zpowi MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *res));/* NEED **/
extern void ztenpow MATH_PROTO((long power, ZVALUE *res));/* NEED **/
extern void zpowermod MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE z3, ZVALUE *res));/* NEED **/
extern BOOL zmodinv MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *res));/* NEED **/
extern BOOL zrelprime MATH_PROTO((ZVALUE z1, ZVALUE z2));/* NEED **/
#if 0
extern long zlog MATH_PROTO((ZVALUE z1, ZVALUE z2));
#endif
extern long zlog10 MATH_PROTO((ZVALUE z));/* NEED **/
#if 0
extern long zdivcount MATH_PROTO((ZVALUE z1, ZVALUE z2));
#endif
extern long zfacrem MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *rem));/* NEED **/
extern void zgcdrem MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *res));/* NEED **/
extern long zlowfactor MATH_PROTO((ZVALUE z, long count));/* NEED **/
extern long zdigits MATH_PROTO((ZVALUE z1));/* NEED **/
#if 0
extern FLAG zdigit MATH_PROTO((ZVALUE z1, long n));
#endif
extern BOOL zsqrt MATH_PROTO((ZVALUE z1, ZVALUE *dest));/* NEED **/
extern void zroot MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *dest));/* NEED **/
#if 0
extern BOOL zissquare MATH_PROTO((ZVALUE z));
extern HASH zhash MATH_PROTO((ZVALUE z));
#endif

#if 0
extern void zapprox MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *res1, ZVALUE *res2));
#endif


#if 0
extern void zmulmod MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE z3, ZVALUE *res));
extern void zsquaremod MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *res));
extern void zsubmod MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE z3, ZVALUE *res));
#endif
#if 0
extern void zminmod MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE *res));
extern BOOL zcmpmod MATH_PROTO((ZVALUE z1, ZVALUE z2, ZVALUE z3));
#endif


/*
 * These functions are for internal use only.
 */
extern void ztrim MATH_PROTO((ZVALUE *z));/* NEED **/
extern void zshiftr MATH_PROTO((ZVALUE z, long n));/* NEED **/
extern void zshiftl MATH_PROTO((ZVALUE z, long n));/* NEED **/
extern HALF *zalloctemp MATH_PROTO((LEN len));/* NEED **/
extern void initmasks MATH_PROTO((void));/* NEED **/


/*
 * Modulo arithmetic definitions.
 * Structure holding state of REDC initialization.
 * Multiple instances of this structure can be used allowing
 * calculations with more than one modulus at the same time.
 * Len of zero means the structure is not initialized.
 */
typedef	struct {
	LEN len;		/* number of words in binary modulus */
	ZVALUE mod;		/* modulus REDC is computing with */
	ZVALUE inv;		/* inverse of modulus in binary modulus */
	ZVALUE one;		/* REDC format for the number 1 */
} REDC;

extern REDC *zredcalloc MATH_PROTO((ZVALUE z1));/* NEED **/
extern void zredcfree MATH_PROTO((REDC *rp));/* NEED **/
extern void zredcencode MATH_PROTO((REDC *rp, ZVALUE z1, ZVALUE *res));/* NEED **/
extern void zredcdecode MATH_PROTO((REDC *rp, ZVALUE z1, ZVALUE *res));/* NEED **/
extern void zredcmul MATH_PROTO((REDC *rp, ZVALUE z1, ZVALUE z2, ZVALUE *res));/* NEED **/
extern void zredcsquare MATH_PROTO((REDC *rp, ZVALUE z1, ZVALUE *res));/* NEED **/
extern void zredcpower MATH_PROTO((REDC *rp, ZVALUE z1, ZVALUE z2, ZVALUE *res));/* NEED **/


/*
 * macro expansions to speed this thing up
 */
#define ziseven(z)	(!(*(z).v & 01))
#define zisodd(z)	(*(z).v & 01)
#define ziszero(z)	((*(z).v == 0) && ((z).len == 1))
#define zisneg(z)	((z).sign)
#define zispos(z)	(((z).sign == 0) && (*(z).v || ((z).len > 1)))
#define zisunit(z)	((*(z).v == 1) && ((z).len == 1))
#define zisone(z)	((*(z).v == 1) && ((z).len == 1) && !(z).sign)
#define zisnegone(z)	((*(z).v == 1) && ((z).len == 1) && (z).sign)
#define zistwo(z)	((*(z).v == 2) && ((z).len == 1) && !(z).sign)
#define zisleone(z)	((*(z).v <= 1) && ((z).len == 1))
#define zistiny(z)	((z).len == 1)
#define zissmall(z)	(((z).len < 2) || (((z).len == 2) && (((SHALF)(z).v[1]) >= 0)))
#define zisbig(z)	(((z).len > 2) || (((z).len == 2) && (((SHALF)(z).v[1]) < 0)))

#define z1tol(z)	((long)((z).v[0]))
#define z2tol(z)	(((long)((z).v[0])) + \
				(((long)((z).v[1] & MAXHALF)) << BASEB))
#define	zclearval(z)	memset((z).v, 0, (z).len * sizeof(HALF))
#define	zcopyval(z1,z2)	memcpy((z2).v, (z1).v, (z1).len * sizeof(HALF))
#define zquicktrim(z)	{if (((z).len > 1) && ((z).v[(z).len-1] == 0)) \
				(z).len--;}
#define	zfree(z)	freeh((z).v)


/*
 * Output modes for numeric displays.
 */
#define MODE_DEFAULT	0
#define MODE_FRAC	1
#define MODE_INT	2
#define MODE_REAL	3
#define MODE_EXP	4
#define MODE_HEX	5
#define MODE_OCTAL	6
#define MODE_BINARY	7
#define MODE_MAX	7

#define MODE_INITIAL	MODE_REAL


/*
 * Output routines for either FILE handles or strings.
 */
extern void math_chr MATH_PROTO((int ch));/* NEED **/
extern void math_str MATH_PROTO((CONST char *str));/* NEED **/
extern void math_fill MATH_PROTO((char *str, long width));/* NEED **/
#if 0
extern void math_flush MATH_PROTO((void));
#endif
extern void math_divertio MATH_PROTO((void));/* NEED **/
extern void math_cleardiversions MATH_PROTO((void));/* NEED **/
#if 0
extern void math_setfp MATH_PROTO((FILE *fp));
#endif
extern char *math_getdivertedio MATH_PROTO((void));/* NEED **/
#if 0
extern long math_setdigits MATH_PROTO((long digits));
#endif


#ifdef VARARGS
extern void math_fmt();/* NEED **/
#else
extern void math_fmt MATH_PROTO((char *, ...));/* NEED **/
#endif


/*
 * The error routine.
 */
#ifdef VARARGS
extern void math_error();
#else
extern void math_error MATH_PROTO((char *, ...));
#endif


/*
 * constants used often by the arithmetic routines
 */
extern HALF _zeroval_[], _oneval_[], _twoval_[], _tenval_[];
extern ZVALUE _zero_, _one_, _ten_;

/* Note the shared use of a global _tenpowers_ array by all threads creates
   the possibility of overlapping initialization of it by mulitple threads.
   As the initialization is constructed, this does not appear to be able to
   compute incorrect table entries.  It merely consumes greater effort and
   leaks more memory than is necessary.

   An attempt was made to make a single initialization pass by one master
   thread on startup, but that's not a good idea because the on-demand
   initializations currently in place make sure we only init the entries
   in the table we actually need to perform a requested calculation.
   Since the cost to add another entry to the table increases (exponentially?)
   as the table fills, the ability to avoid filling the entries we do not
   need is a practical necessity.
*/
extern ZVALUE _tenpowers_[2 * BASEB];	/* table of 10^2^n */
extern HALF *bitmask;		/* bit rotation, norm 0 */

#endif

/* END CODE */
