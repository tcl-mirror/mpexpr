/*
 * Copyright (c) 1994 David I. Bell
 * Permission is granted to use, distribute, or modify this source,
 * provided that this copyright notice remains intact.
 *
 * Scanf and printf routines for arbitrary precision rational numbers
 */

#include "mpexpr.h"

#define	PUTCHAR(ch)		math_chr(ch)
#define	PUTSTR(str)		math_str(str)
#define	PRINTF1(fmt, a1)	math_fmt(fmt, a1)
#define	PRINTF2(fmt, a1, a2)	math_fmt(fmt, a1, a2)

static CONST int tilde_ok = TRUE;	/* FALSE => don't print '~' for rounded value */

static Tcl_ThreadDataKey scaleKey = NULL;
typedef struct {
    long factor;
    ZVALUE number;
} Scale;

/*
 * Print a number in floating point representation.
 * Example:  193.784
 */
void
qprintff(q, width, precision)
	NUMBER *q;
	long width;
	long precision;
{
	ZVALUE z, z1;
	Scale *scale = Tcl_GetThreadData(&scaleKey, sizeof(Scale));

	if (precision != scale->factor) {
		if (scale->number.v)
			zfree(scale->number);
		ztenpow(precision, &(scale->number));
		scale->factor = precision;
	}
	if (scale->number.v)
		zmul(q->num, scale->number, &z);
	else
		z = q->num;
	if (qisfrac(q)) {
		zquo(z, q->den, &z1);
		if (z.v != q->num.v)
			zfree(z);
		z = z1;
	}
	if (qisneg(q) && ziszero(z))
		PUTCHAR('-');
	zprintval(z, precision, width);
	if (z.v != q->num.v)
		zfree(z);
}

/*
 * Print a number in floating point representation.
 * Example:  193.784
 */
void
Qprintff(q, width, precision)
	NUMBER *q;
	long width;
	long precision;
{
	ZVALUE z, z1;
	Scale *scale = Tcl_GetThreadData(&scaleKey, sizeof(Scale));

	if (precision != scale->factor) {
		if (scale->number.v)
			zfree(scale->number);
		ztenpow(precision, &(scale->number));
		scale->factor = precision;
	}
	if (scale->number.v)
		zmul(q->num, scale->number, &z);
	else
		z = q->num;
	if (qisfrac(q)) {
		zquo(z, q->den, &z1);
		if (z.v != q->num.v)
			zfree(z);
		z = z1;
	}
	if (qisneg(q) && ziszero(z))
		PUTCHAR('-');
	Zprintval(z, precision, width);
	if (z.v != q->num.v)
		zfree(z);
}

/*
 * Print a number in exponential notation.
 * Same as qprintfe, except the last digit is rounded instead of truncated.
 * The rounding is that given by qround, i.e., it always rounds away from
 * zero if the truncated value is exactly 0.5 relative to a unit in the
 * last printed digit.
 * Example: 4.1856e34
 */
/*ARGSUSED*/
void
qprintfe_round(q, width, precision)
	register NUMBER *q;
	long width;
	long precision;
{
	long exponent;
	NUMBER q2;
	ZVALUE num, den, tenpow, tmp;

	NUMBER* q2_round;

	if (qiszero(q)) {
		PUTSTR("0.0");
		return;
	}
	num = q->num;
	den = q->den;
	num.sign = 0;
	exponent = zdigits(num) - zdigits(den);
	if (exponent > 0) {
		ztenpow(exponent, &tenpow);
		zmul(den, tenpow, &tmp);
		zfree(tenpow);
		den = tmp;
	}
	if (exponent < 0) {
		ztenpow(-exponent, &tenpow);
		zmul(num, tenpow, &tmp);
		zfree(tenpow);
		num = tmp;
	}
	if (zrel(num, den) < 0) {
		zmuli(num, 10L, &tmp);
		if (num.v != q->num.v)
			zfree(num);
		num = tmp;
		exponent--;
	}
	q2.num = num;
	q2.den = den;
	q2.num.sign = q->num.sign;
	q2.links = 1;  /* Needed for qround call */
	q2_round = qround(&q2,precision);
	if (qreli(q2_round,10)>=0 || qreli(q2_round,-10)<=0) {
		NUMBER* tmp = qdivi(q2_round,(long)10);
		qfree(q2_round);
		q2_round = tmp;
		exponent++;
	}
	qprintff(q2_round, 0L, precision);
	qfree(q2_round);
	if (exponent)
		PRINTF1("e%ld", exponent);
	if (num.v != q->num.v)
		zfree(num);
	if (den.v != q->den.v)
		zfree(den);
}

/*
 * Print a number in rational representation.
 * Example: 397/37
 */
void
qprintfr(q, width, force)
	NUMBER *q;
	long width;
	BOOL force;
{
	zprintval(q->num, 0L, width);
	if (force || qisfrac(q)) {
		PUTCHAR('/');
		zprintval(q->den, 0L, width);
	}
}


/*
 * Print a number as an integer (truncating fractional part).
 * Example: 958421
 */
void
qprintfd(q, width)
	NUMBER *q;
	long width;
{
	ZVALUE z;

	if (qisfrac(q)) {
		zquo(q->num, q->den, &z);
		zprintval(z, 0L, width);
		zfree(z);
	} else
		zprintval(q->num, 0L, width);
}


/*
 * Print a number in hex.
 * This prints the numerator and denominator in hex.
 */
void
qprintfx(q, width)
	NUMBER *q;
	long width;
{
	zprintx(q->num, width);
	if (qisfrac(q)) {
		PUTCHAR('/');
		zprintx(q->den, 0L);
	}
}


/*
 * Print a number in binary.
 * This prints the numerator and denominator in binary.
 */
void
qprintfb(q, width)
	NUMBER *q;
	long width;
{
	zprintb(q->num, width);
	if (qisfrac(q)) {
		PUTCHAR('/');
		zprintb(q->den, 0L);
	}
}


/*
 * Print a number in octal.
 * This prints the numerator and denominator in octal.
 */
void
qprintfo(q, width)
	NUMBER *q;
	long width;
{
	zprinto(q->num, width);
	if (qisfrac(q)) {
		PUTCHAR('/');
		zprinto(q->den, 0L);
	}
}

/*
 * Parse a number in any of the various legal forms, and return the count
 * of characters that are part of a legal number.  Numbers can be either a
 * decimal integer, possibly two decimal integers separated with a slash, a
 * floating point or exponential number, a hex number beginning with "0x",
 * a binary number beginning with "0b", or an octal number beginning with "0".
 * The flags argument modifies the end of number testing for ease in handling
 * fractions or complex numbers.  Minus one is returned if the number format
 * is definitely illegal.
 */
long
qparse(cp, flags)
	register CONST char *cp;
	int flags;
{
        CONST char *oldcp;

	oldcp = cp;
	if ((*cp == '+') || (*cp == '-'))
		cp++;
	if ((*cp == '+') || (*cp == '-'))
		return -1;
	if ((*cp == '0') && ((cp[1] == 'x') || (cp[1] == 'X'))) {	/* hex */
		cp += 2;
		while (((*cp >= '0') && (*cp <= '9')) ||
			((*cp >= 'a') && (*cp <= 'f')) ||
			((*cp >= 'A') && (*cp <= 'F')))
				cp++;
		goto done;
	}
	if ((*cp == '0') && ((cp[1] == 'b') || (cp[1] == 'B'))) {	/* binary */
		cp += 2;
		while ((*cp == '0') || (*cp == '1'))
			cp++;
		goto done;
	}
	if ((*cp == '0') && (cp[1] >= '0') && (cp[1] <= '9')) { /* octal */
		while ((*cp >= '0') && (*cp <= '7')) {
			if (*cp >= '8') return -1;  /* tp: signal bad octal */
			cp++;
		}
		goto done;
	}
	/*
	 * Number is decimal, but can still be a fraction or real or exponential.
	 */
	while ((*cp >= '0') && (*cp <= '9'))
		cp++;
	if (*cp == '/' && flags & QPF_SLASH) {	/* fraction */
		cp++;
		while ((*cp >= '0') && (*cp <= '9'))
			cp++;
		goto done;
	}
	if (*cp == '.') {	/* floating point */
		cp++;
		while ((*cp >= '0') && (*cp <= '9'))
			cp++;
	}
	if ((*cp == 'e') || (*cp == 'E')) {	/* exponential */
		cp++;
		if ((*cp == '+') || (*cp == '-'))
			cp++;
		if ((*cp == '+') || (*cp == '-'))
			return -1;
		while ((*cp >= '0') && (*cp <= '9'))
			cp++;
	}

done:
	if (((*cp == 'i') || (*cp == 'I')) && (flags & QPF_IMAG))
		cp++;
	if ((*cp == '.') || ((*cp == '/') && (flags & QPF_SLASH)) ||
		((*cp >= '0') && (*cp <= '9')) ||
		((*cp >= 'a') && (*cp <= 'z')) ||
		((*cp >= 'A') && (*cp <= 'Z')))
			return -1;
	return (cp - oldcp);
}

/* END CODE */
