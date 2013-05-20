/*
 * Copyright (c) 1993 David I. Bell
 * Permission is granted to use, distribute, or modify this source,
 * provided that this copyright notice remains intact.
 *
 * Modular arithmetic routines for normal numbers, and also using
 * the faster REDC algorithm.
 */

#include "qmath.h"

/*
 * Return the remainder when one number is divided by another.
 * The second argument cannot be negative.  The result is normalized
 * to lie in the range 0 to q2, and works for fractional values as:
 *	qmod(q1, q2) = q1 - (int(q1 / q2) * q2).
 */
NUMBER *
qmod(q1, q2)
	register NUMBER *q1, *q2;
{
	ZVALUE res;
	NUMBER *q, *tmp;

	if (qisneg(q2) || qiszero(q2))
		math_error("Non-positive modulus");
	if (qisint(q1) && qisint(q2)) {		/* easy case */
		zmod(q1->num, q2->num, &res);
		if (ziszero(res)) {
			zfree(res);
			return qlink(&_qzero_);
		}
		if (zisone(res)) {
			zfree(res);
			return qlink(&_qone_);
		}
		q = qalloc();
		q->num = res;
		return q;
	}
	q = qquo(q1, q2);
	tmp = qmul(q, q2);
	qfree(q);
	q = qsub(q1, tmp);
	qfree(tmp);
	if (qisneg(q)) {
		tmp = qadd(q2, q);
		qfree(q);
		q = tmp;
	}
	return q;
}

/* END CODE */
