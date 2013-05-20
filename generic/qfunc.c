/*
 * Copyright (c) 1994 David I. Bell
 * Permission is granted to use, distribute, or modify this source,
 * provided that this copyright notice remains intact.
 *
 * Extended precision rational arithmetic non-primitive functions
 */

#include "qmath.h"

/*
 * Return the inverse of one number modulo another.
 * That is, find x such that:
 *	Ax = 1 (mod B)
 * Returns zero if the numbers are not relatively prime (temporary hack).
 */
NUMBER *
qminv(q1, q2)
	NUMBER *q1, *q2;
{
	NUMBER *r;

	if (qisfrac(q1) || qisfrac(q2))
		math_error("Non-integers for minv");
	r = qalloc();
	if (zmodinv(q1->num, q2->num, &r->num)) {
		qfree(r);
		return qlink(&_qzero_);
	}
	return r;
}

/*
 * Return the power function of one number with another.
 * The power must be integral.
 *	q3 = qpowi(q1, q2);
 */
NUMBER *
qpowi(q1, q2)
	NUMBER *q1, *q2;
{
	register NUMBER *r;
	BOOL invert, sign;
	ZVALUE num, den, z2;

	if (qisfrac(q2))
		math_error("Raising number to fractional power");
	num = q1->num;
	den = q1->den;
	z2 = q2->num;
	sign = (num.sign && zisodd(z2));
	invert = z2.sign;
	num.sign = 0;
	z2.sign = 0;
	/*
	* Check for trivial cases first.
	*/
	if (ziszero(num) && !ziszero(z2)) {	/* zero raised to a power */
		if (invert)
			math_error("Zero raised to negative power");
		return qlink(&_qzero_);
	}
	if (zisunit(num) && zisunit(den)) {	/* 1 or -1 raised to a power */
		r = (sign ? q1 : &_qone_);
		r->links++;
		return r;
	}
	if (ziszero(z2))	/* raising to zeroth power */
		return qlink(&_qone_);
	if (zisunit(z2)) {	/* raising to power 1 or -1 */
		if (invert)
			return qinv(q1);
		return qlink(q1);
	}
	/*
	 * Not a trivial case.  Do the real work.
	 */
	r = qalloc();
	if (!zisunit(num))
		zpowi(num, z2, &r->num);
	if (!zisunit(den))
		zpowi(den, z2, &r->den);
	if (invert) {
		z2 = r->num;
		r->num = r->den;
		r->den = z2;
	}
	r->num.sign = sign;
	return r;
}


/*
 * Given the legs of a right triangle, compute its hypothenuse within
 * the specified error.  This is sqrt(a^2 + b^2).
 */
NUMBER *
qhypot(q1, q2, epsilon)
	NUMBER *q1, *q2, *epsilon;
{
	NUMBER *tmp1, *tmp2, *tmp3;

	if (qisneg(epsilon) || qiszero(epsilon))
		math_error("Bad epsilon value for hypot");
	if (qiszero(q1))
		return qabs(q2);
	if (qiszero(q2))
		return qabs(q1);
	tmp1 = qsquare(q1);
	tmp2 = qsquare(q2);
	tmp3 = qadd(tmp1, tmp2);
	qfree(tmp1);
	qfree(tmp2);
	tmp1 = qsqrt(tmp3, epsilon);
	qfree(tmp3);
	return tmp1;
}


/*
 * Given one leg of a right triangle with unit hypothenuse, calculate
 * the other leg within the specified error.  This is sqrt(1 - a^2).
 * If the wantneg flag is nonzero, then negative square root is returned.
 */
NUMBER *
qlegtoleg(q, epsilon, wantneg)
	NUMBER *q, *epsilon;
	BOOL wantneg;
{
	NUMBER *res, *qtmp1, *qtmp2;
	ZVALUE num;

	if (qisneg(epsilon) || qiszero(epsilon))
		math_error("Bad epsilon value for legtoleg");
	if (qisunit(q))
		return qlink(&_qzero_);
	if (qiszero(q)) {
		if (wantneg)
			return qlink(&_qnegone_);
		return qlink(&_qone_);
	}
	num = q->num;
	num.sign = 0;
	if (zrel(num, q->den) >= 0)
		math_error("Leg too large in legtoleg");
	qtmp1 = qsquare(q);
	qtmp2 = qsub(&_qone_, qtmp1);
	qfree(qtmp1);
	res = qsqrt(qtmp2, epsilon);
	qfree(qtmp2);
	if (wantneg) {
		qtmp1 = qneg(res);
		qfree(res);
		res = qtmp1;
	}
	return res;
}

/*
 * Compute the square root of a number to within the specified error.
 * If the number is an exact square, the exact result is returned.
 *	q3 = qsqrt(q1, q2);
 */
NUMBER *
qsqrt(q1, epsilon)
	register NUMBER *q1, *epsilon;
{
	long bits, bits2;	/* number of bits of precision */
	int exact;
	NUMBER *r;
	ZVALUE t1, t2;

	if (qisneg(q1))
		math_error("Square root of negative number");
	if (qisneg(epsilon) || qiszero(epsilon))
		math_error("Bad epsilon value for sqrt");
	if (qiszero(q1))
		return qlink(&_qzero_);
	if (qisunit(q1))
		return qlink(&_qone_);
	if (qiszero(epsilon) && qisint(q1) && zistiny(q1->num) && (*q1->num.v <= 3))
		return qlink(&_qone_);
	bits = zhighbit(epsilon->den) - zhighbit(epsilon->num) + 1;
	if (bits < 0)
		bits = 0;
	bits2 = zhighbit(q1->num) - zhighbit(q1->den) + 1;
	if (bits2 > 0)
		bits += bits2;
	r = qalloc();
	zshift(q1->num, bits * 2, &t2);
	zmul(q1->den, t2, &t1);
	zfree(t2);
	exact = zsqrt(t1, &t2);
	zfree(t1);
	if (exact) {
		zshift(q1->den, bits, &t1);
		zreduce(t2, t1, &r->num, &r->den);
	} else {
		zquo(t2, q1->den, &t1);
		zfree(t2);
		zbitvalue(bits, &t2);
		zreduce(t1, t2, &r->num, &r->den);
	}
	zfree(t1);
	zfree(t2);
	if (qiszero(r)) {
		qfree(r);
		r = qlink(&_qzero_);
	}
	return r;
}

/*
 * Return the number of digits in a number, ignoring the sign.
 * For fractions, this is the number of digits of its greatest integer.
 * Examples: qdigits(3456) = 4, qdigits(-23.45) = 2, qdigits(.0120) = 1.
 */
long
qdigits(q)
	NUMBER *q;		/* number to count digits of */
{
	long n;			/* number of digits */
	ZVALUE temp;		/* temporary value */

	if (qisint(q))
		return zdigits(q->num);
	zquo(q->num, q->den, &temp);
	n = zdigits(temp);
	zfree(temp);
	return n;
}

/*
 * Round a number to the specified number of decimal places.
 * Zero decimal places means round to the nearest integer.
 * Example: qround(2/3, 3) = .667
 */
NUMBER *
qround(q, places)
	NUMBER *q;		/* number to be rounded */
	long places;		/* number of decimal places to round to */
{
	NUMBER *r;
	ZVALUE tenpow, roundval, tmp1, tmp2, rem;

	if (places < 0)
		math_error("Negative places for qround");
	if (qisint(q))
		return qlink(q);
	/*
	 * Calculate one half of the denominator, ignoring fractional results.
	 * This is the value we will add in order to cause rounding.
	 */
	zshift(q->den, -1L, &roundval);
	roundval.sign = q->num.sign;
	/*
	 * Ok, now do the actual work to produce the result.
	 */
	r = qalloc();
	ztenpow(places, &tenpow);
	zmul(q->num, tenpow, &tmp2);
	zadd(tmp2, roundval, &tmp1);
	zfree(tmp2);
	zfree(roundval);
	zdiv(tmp1, q->den, &tmp2, &rem);
	zfree(tmp1);
	if (ziszero(rem) && zisodd(tmp2) && ziseven(q->den)) {
		zsub(tmp2, _one_, &tmp1);
		zfree(tmp2);
		tmp2 = tmp1;
	}
	zfree(rem);
	if (ziszero(tmp2)) {
		zfree(tmp2);
		return qlink(&_qzero_);
	}
	/*
	 * Now reduce the result to the lowest common denominator.
	 */
	zgcd(tmp2, tenpow, &tmp1);
	if (zisunit(tmp1)) {
		zfree(tmp1);
		r->num = tmp2;
		r->den = tenpow;
		return r;
	}
	zquo(tmp2, tmp1, &r->num);
	zquo(tenpow, tmp1, &r->den);
	zfree(tmp1);
	zfree(tmp2);
	zfree(tenpow);
	return r;
}

/*
 * Round a number to the specified number of binary places.
 * Zero binary places means round to the nearest integer.
 */
NUMBER *
qbround(q, places)
	NUMBER *q;		/* number to be rounded */
	long places;		/* number of binary places to round to */
{
	long twopow;
	NUMBER *r;
	ZVALUE roundval, tmp1, tmp2;

	if (places < 0)
		math_error("Negative places for qbround");
	if (qisint(q))
		return qlink(q);
	r = qalloc();
	/*
	 * Calculate one half of the denominator, ignoring fractional results.
	 * This is the value we will add in order to cause rounding.
	 */
	zshift(q->den, -1L, &roundval);
	roundval.sign = q->num.sign;
	/*
	 * Ok, produce the result.
	 */
	zshift(q->num, places, &tmp1);
	zadd(tmp1, roundval, &tmp2);
	zfree(roundval);
	zfree(tmp1);
	zquo(tmp2, q->den, &tmp1);
	zfree(tmp2);
	if (ziszero(tmp1)) {
		qfree(r);
		zfree(tmp1);
		return qlink(&_qzero_);
	}
	/*
	 * Now reduce the result to the lowest common denominator.
	 */
	if (zisodd(tmp1)) {
		r->num = tmp1;
		zbitvalue(places, &r->den);
		return r;
	}
	twopow = zlowbit(tmp1);
	if (twopow > places)
		twopow = places;
	places -= twopow;
	zshift(tmp1, -twopow, &r->num);
	zfree(tmp1);
	zbitvalue(places, &r->den);
	return r;
}

/*
 * Compute the gcd (greatest common divisor) of two numbers.
 *	q3 = qgcd(q1, q2);
 */
NUMBER *
qgcd(q1, q2)
	register NUMBER *q1, *q2;
{
	ZVALUE z;
	NUMBER *q;

	if (q1 == q2)
		return qabs(q1);
	if (qisfrac(q1) || qisfrac(q2)) {
		q = qalloc();
		zgcd(q1->num, q2->num, &q->num);
		zlcm(q1->den, q2->den, &q->den);
		return q;
	}
	if (qiszero(q1))
		return qabs(q2);
	if (qiszero(q2))
		return qabs(q1);
	if (qisunit(q1) || qisunit(q2))
		return qlink(&_qone_);
	zgcd(q1->num, q2->num, &z);
	if (zisunit(z)) {
		zfree(z);
		return qlink(&_qone_);
	}
	q = qalloc();
	q->num = z;
	return q;
}


/*
 * Compute the lcm (least common multiple) of two numbers.
 *	q3 = qlcm(q1, q2);
 */
NUMBER *
qlcm(q1, q2)
	register NUMBER *q1, *q2;
{
	NUMBER *q;

	if (qiszero(q1) || qiszero(q2))
		return qlink(&_qzero_);
	if (q1 == q2)
		return qabs(q1);
	if (qisunit(q1))
		return qabs(q2);
	if (qisunit(q2))
		return qabs(q1);
	q = qalloc();
	zlcm(q1->num, q2->num, &q->num);
	if (qisfrac(q1) || qisfrac(q2))
		zgcd(q1->den, q2->den, &q->den);
	return q;
}


/*
 * Remove all occurances of the specified factor from a number.
 * Returned number is always positive.
 */
NUMBER *
qfacrem(q1, q2)
	NUMBER *q1, *q2;
{
	long count;
	ZVALUE tmp;
	NUMBER *r;

	if (qisfrac(q1) || qisfrac(q2))
		math_error("Non-integers for factor removal");
	count = zfacrem(q1->num, q2->num, &tmp);
	if (zisunit(tmp)) {
		zfree(tmp);
		return qlink(&_qone_);
	}
	if (count == 0) {
		zfree(tmp);
		return qlink(q1);
	}
	r = qalloc();
	r->num = tmp;
	return r;
}

/*
 * Return the number of places after the decimal point needed to exactly
 * represent the specified number as a real number.  Integers return zero,
 * and non-terminating decimals return minus one.  Examples:
 *	qplaces(1/7)=-1, qplaces(3/10)= 1, qplaces(1/8)=3, qplaces(4)=0.
 */
long
qplaces(q)
	NUMBER *q;
{
	long twopow, fivepow;
	HALF fiveval[2];
	ZVALUE five;
	ZVALUE tmp;

	if (qisint(q))	/* no decimal places if number is integer */
		return 0;
	/*
	 * The number of decimal places of a fraction in lowest terms is finite
	 * if an only if the denominator is of the form 2^A * 5^B, and then the
	 * number of decimal places is equal to MAX(A, B).
	 */
	five.sign = 0;
	five.len = 1;
	five.v = fiveval;
	fiveval[0] = 5;
	fivepow = zfacrem(q->den, five, &tmp);
	if (!zisonebit(tmp)) {
		zfree(tmp);
		return -1;
	}
	twopow = zlowbit(tmp);
	zfree(tmp);
	if (twopow < fivepow)
		twopow = fivepow;
	return twopow;
}

/* END CODE */
