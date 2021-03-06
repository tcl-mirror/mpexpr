/*
 * Copyright (c) 1994 David I. Bell
 * Permission is granted to use, distribute, or modify this source,
 * provided that this copyright notice remains intact.
 *
 * Extended precision integral arithmetic non-primitive routines
 */

#include "zmath.h"

static ZVALUE primeprod;		/* product of primes under 100 */
ZVALUE _tenpowers_[2 * BASEB];		/* table of 10^2^n */

/*
 * Compute the factorial of a number.
 */
void
zfact(z, dest)
	ZVALUE z, *dest;
{
	long ptwo;		/* count of powers of two */
	long n;			/* current multiplication value */
	long m;			/* reduced multiplication value */
	long mul;		/* collected value to multiply by */
	ZVALUE res, temp;

	if (zisneg(z))
		math_error("Negative argument for factorial");
	if (zisbig(z))
		math_error("Very large factorial");
	n = (zistiny(z) ? z1tol(z) : z2tol(z));
	ptwo = 0;
	mul = 1;
	res = _one_;
	/*
	 * Multiply numbers together, but squeeze out all powers of two.
	 * We will put them back in at the end.  Also collect multiple
	 * numbers together until there is a risk of overflow.
	 */
	for (; n > 1; n--) {
		for (m = n; ((m & 0x1) == 0); m >>= 1)
			ptwo++;
		mul *= m;
		if (mul < BASE1/2)
			continue;
		zmuli(res, mul, &temp);
		zfree(res);
		res = temp;
		mul = 1;
	}
	/*
	 * Multiply by the remaining value, then scale result by
	 * the proper power of two.
	 */
	if (mul > 1) {
		zmuli(res, mul, &temp);
		zfree(res);
		res = temp;
	}
	zshift(res, ptwo, &temp);
	zfree(res);
	*dest = temp;
}


/*
 * Compute the product of the primes up to the specified number.
 */
void
zpfact(z, dest)
	ZVALUE z, *dest;
{
	long n;			/* limiting number to multiply by */
	long p;			/* current prime */
	long i;			/* test value */
	long mul;		/* collected value to multiply by */
	ZVALUE res, temp;

	if (zisneg(z))
		math_error("Negative argument for factorial");
	if (zisbig(z))
		math_error("Very large factorial");
	n = (zistiny(z) ? z1tol(z) : z2tol(z));
	/*
	 * Multiply by the primes in order, collecting multiple numbers
	 * together until there is a chance of overflow.
	 */
	mul = 1 + (n > 1);
	res = _one_;
	for (p = 3; p <= n; p += 2) {
		for (i = 3; (i * i) <= p; i += 2) {
			if ((p % i) == 0)
				goto next;
		}
		mul *= p;
		if (mul < BASE1/2)
			continue;
		zmuli(res, mul, &temp);
		zfree(res);
		res = temp;
		mul = 1;
next: ;
	}
	/*
	 * Multiply by the final value if any.
	 */
	if (mul > 1) {
		zmuli(res, mul, &temp);
		zfree(res);
		res = temp;
	}
	*dest = res;
}

/*
 * Compute the permutation function  M! / (M - N)!.
 */
void
zperm(z1, z2, res)
	ZVALUE z1, z2, *res;
{
	long count;
	ZVALUE cur, tmp, ans;

	if (zisneg(z1) || zisneg(z2))
		math_error("Negative argument for permutation");
	if (zrel(z1, z2) < 0)
		math_error("Second arg larger than first in permutation");
	if (zisbig(z2))
		math_error("Very large permutation");
	count = (zistiny(z2) ? z1tol(z2) : z2tol(z2));
	zcopy(z1, &ans);
	zsub(z1, _one_, &cur);
	while (--count > 0) {
		zmul(ans, cur, &tmp);
		zfree(ans);
		ans = tmp;
		zsub(cur, _one_, &tmp);
		zfree(cur);
		cur = tmp;
	}
	zfree(cur);
	*res = ans;
}


/*
 * Compute the combinatorial function  M! / ( N! * (M - N)! ).
 */
void
zcomb(z1, z2, res)
	ZVALUE z1, z2, *res;
{
	ZVALUE ans;
	ZVALUE mul, div, temp;
	FULL count, i;
	HALF dh[2];

	if (zisneg(z1) || zisneg(z2))
		math_error("Negative argument for combinatorial");
	zsub(z1, z2, &temp);
	if (zisneg(temp)) {
		zfree(temp);
		math_error("Second arg larger than first for combinatorial");
	}
	if (zisbig(z2) && zisbig(temp)) {
		zfree(temp);
		math_error("Very large combinatorial");
	}
	count = (zistiny(z2) ? z1tol(z2) : z2tol(z2));
	i = (zistiny(temp) ? z1tol(temp) : z2tol(temp));
	if (zisbig(z2) || (!zisbig(temp) && (i < count)))
		count = i;
	zfree(temp);
	mul = z1;
	div.sign = 0;
	div.v = dh;
	ans = _one_;
	for (i = 1; i <= count; i++) {
		dh[0] = i & BASE1;
		dh[1] = i / BASE;
		div.len = 1 + (dh[1] != 0);
		zmul(ans, mul, &temp);
		zfree(ans);
		zquo(temp, div, &ans);
		zfree(temp);
		zsub(mul, _one_, &temp);
		if (mul.v != z1.v)
			zfree(mul);
		mul = temp;
	}
	if (mul.v != z1.v)
		zfree(mul);
	*res = ans;
}


/*
 * Perform a probabilistic primality test (algorithm P in Knuth).
 * Returns FALSE if definitely not prime, or TRUE if probably prime.
 * Count determines how many times to check for primality.
 * The chance of a non-prime passing this test is less than (1/4)^count.
 * For example, a count of 100 fails for only 1 in 10^60 numbers.
 */
BOOL
zprimetest(z, count)
	ZVALUE z;		/* number to test for primeness */
	long count;
{
	long ij, ik, ix;
	ZVALUE zm1, z1, z2, z3, ztmp;
	HALF val[2];

	z.sign = 0;
	if (ziseven(z))		/* if even, not prime if not 2 */
		return (zistwo(z) != 0);
	/*
	 * See if the number is small, and is either a small prime,
	 * or is divisible by a small prime.
	 */
	if (zistiny(z) && (*z.v <= (HALF)(101*101-1))) {
		ix = *z.v;
		for (ik = 3; (ik <= 97) && ((ik * ik) <= ix); ik += 2)
			if ((ix % ik) == 0)
				return FALSE;
		return TRUE;
	}
	/*
	 * See if the number is divisible by one of the primes 3, 5,
	 * 7, 11, or 13.  This is a very easy check.
	 */
	ij = zmodi(z, 15015L);
	if (!(ij % 3) || !(ij % 5) || !(ij % 7) || !(ij % 11) || !(ij % 13))
		return FALSE;
	/*
	 * Check the gcd of the number and the product of more of the first
	 * few odd primes.  We must build the prime product on the first call.
	 */
	ztmp.sign = 0;
	ztmp.len = 1;
	ztmp.v = val;
	if (primeprod.len == 0) {
		val[0] = 101;
		zpfact(ztmp, &primeprod);
	}
	zgcd(z, primeprod, &z1);
	if (!zisunit(z1)) {
		zfree(z1);
		return FALSE;
	}
	zfree(z1);
	/*
	 * Not divisible by a small prime, so onward with the real test.
	 * Make sure the count is limited by the number of odd numbers between
	 * three and the number being tested.
	 */
	ix = ((zistiny(z) ? z1tol(z) : z2tol(z) - 3) / 2);
	if (count > ix) count = ix;
	zsub(z, _one_, &zm1);
	ik = zlowbit(zm1);
	zshift(zm1, -ik, &z1);
	/*
	 * Loop over various "random" numbers, testing each one.
	 * These numbers are the odd numbers starting from three.
	 */
	for (ix = 0; ix < count; ix++) {
		val[0] = (ix * 2) + 3;
		ij = 0;
		zpowermod(ztmp, z1, z, &z3);
		for (;;) {
			if (zisone(z3)) {
				if (ij)	/* number is definitely not prime */
					goto notprime;
				break;
			}
			if (zcmp(z3, zm1) == 0)
				break;
			if (++ij >= ik)
				goto notprime;	/* number is definitely not prime */
			zsquare(z3, &z2);
			zfree(z3);
			zmod(z2, z, &z3);
			zfree(z2);
		}
		zfree(z3);
	}
	zfree(zm1);
	zfree(z1);
	return TRUE;	/* number might be prime */

notprime:
	zfree(z3);
	zfree(zm1);
	zfree(z1);
	return FALSE;
}

/*
 * Return the Fibonacci number F(n).
 * This is evaluated by recursively using the formulas:
 *	F(2N+1) = F(N+1)^2 + F(N)^2
 * and
 *	F(2N) = F(N+1)^2 - F(N-1)^2
 */
void
zfib(z, res)
	ZVALUE z, *res;
{
	unsigned long i;
	long n;
	int sign;
	ZVALUE fnm1, fn, fnp1;		/* consecutive fibonacci values */
	ZVALUE t1, t2, t3;

	if (zisbig(z))
		math_error("Very large Fibonacci number");
	n = (zistiny(z) ? z1tol(z) : z2tol(z));
	if (n == 0) {
		*res = _zero_;
		return;
	}
	sign = z.sign && ((n & 0x1) == 0);
	if (n <= 2) {
		*res = _one_;
		res->sign = (BOOL)sign;
		return;
	}
	i = TOPFULL;
	while ((i & n) == 0)
		i >>= 1L;
	i >>= 1L;
	fnm1 = _zero_;
	fn = _one_;
	fnp1 = _one_;
	while (i) {
		zsquare(fnm1, &t1);
		zsquare(fn, &t2);
		zsquare(fnp1, &t3);
		zfree(fnm1);
		zfree(fn);
		zfree(fnp1);
		zadd(t2, t3, &fnp1);
		zsub(t3, t1, &fn);
		zfree(t1);
		zfree(t2);
		zfree(t3);
		if (i & n) {
			fnm1 = fn;
			fn = fnp1;
			zadd(fnm1, fn, &fnp1);
		} else
			zsub(fnp1, fn, &fnm1);
		i >>= 1L;
	}
	zfree(fnm1);
	zfree(fnp1);
	*res = fn;
	res->sign = (BOOL)sign;
}


/*
 * Compute the result of raising one number to the power of another
 * The second number is assumed to be non-negative.
 * It cannot be too large except for trivial cases.
 */
void
zpowi(z1, z2, res)
	ZVALUE z1, z2, *res;
{
	int sign;		/* final sign of number */
	unsigned long power;	/* power to raise to */
	unsigned long bit;	/* current bit value */
	long twos;		/* count of times 2 is in result */
	ZVALUE ans, temp;

	sign = (z1.sign && zisodd(z2));
	z1.sign = 0;
	z2.sign = 0;
	if (ziszero(z2) && !ziszero(z1)) {	/* number raised to power 0 */
		*res = _one_;
		return;
	}
	if (zisleone(z1)) {	/* 0, 1, or -1 raised to a power */
		ans = _one_;
		ans.sign = (BOOL)sign;
		if (*z1.v == 0)
			ans = _zero_;
		*res = ans;
		return;
	}
	if (zisbig(z2))
		math_error("Raising to very large power");
	power = (zistiny(z2) ? z1tol(z2) : z2tol(z2));
	if (zistwo(z1)) {	/* two raised to a power */
		zbitvalue((long) power, res);
		return;
	}
	/*
	 * See if this is a power of ten
	 */
	if (zistiny(z1) && (*z1.v == 10)) {
		ztenpow((long) power, res);
		res->sign = (BOOL)sign;
		return;
	}
	/*
	 * Handle low powers specially
	 */
	if (power <= 4) {
		switch ((int) power) {
			case 1:
				ans.len = z1.len;
				ans.v = alloc(ans.len);
				zcopyval(z1, ans);
				ans.sign = (BOOL)sign;
				*res = ans;
				return;
			case 2:
				zsquare(z1, res);
				return;
			case 3:
				zsquare(z1, &temp);
				zmul(z1, temp, res);
				zfree(temp);
				res->sign = (BOOL)sign;
				return;
			case 4:
				zsquare(z1, &temp);
				zsquare(temp, res);
				zfree(temp);
				return;
		}
	}
	/*
	 * Shift out all powers of twos so the multiplies are smaller.
	 * We will shift back the right amount when done.
	 */
	twos = 0;
	if (ziseven(z1)) {
		twos = zlowbit(z1);
		ans.v = alloc(z1.len);
		ans.len = z1.len;
		ans.sign = z1.sign;
		zcopyval(z1, ans);
		zshiftr(ans, twos);
		ztrim(&ans);
		z1 = ans;
		twos *= power;
	}
	/*
	 * Compute the power by squaring and multiplying.
	 * This uses the left to right method of power raising.
	 */
	bit = TOPFULL;
	while ((bit & power) == 0)
		bit >>= 1L;
	bit >>= 1L;
	zsquare(z1, &ans);
	if (bit & power) {
		zmul(ans, z1, &temp);
		zfree(ans);
		ans = temp;
	}
	bit >>= 1L;
	while (bit) {
		zsquare(ans, &temp);
		zfree(ans);
		ans = temp;
		if (bit & power) {
			zmul(ans, z1, &temp);
			zfree(ans);
			ans = temp;
		}
		bit >>= 1L;
	}
	/*
	 * Scale back up by proper power of two
	 */
	if (twos) {
		zshift(ans, twos, &temp);
		zfree(ans);
		ans = temp;
		zfree(z1);
	}
	ans.sign = (BOOL)sign;
	*res = ans;
}


/*
 * Compute ten to the specified power
 * This saves some work since the squares of ten are saved.
 */
void
ztenpow(power, res)
	long power;
	ZVALUE *res;
{
	long i;
	ZVALUE ans;
	ZVALUE temp;

	if (power <= 0) {
		*res = _one_;
		return;
	}
	ans = _one_;
	_tenpowers_[0] = _ten_;
	for (i = 0; power; i++) {
		if (_tenpowers_[i].len == 0)
			zsquare(_tenpowers_[i-1], &_tenpowers_[i]);
		if (power & 0x1) {
			zmul(ans, _tenpowers_[i], &temp);
			zfree(ans);
			ans = temp;
		}
		power /= 2;
	}
	*res = ans;
}


/*
 * Calculate modular inverse suppressing unnecessary divisions.
 * This is based on the Euclidian algorithm for large numbers.
 * (Algorithm X from Knuth Vol 2, section 4.5.2. and exercise 17)
 * Returns TRUE if there is no solution because the numbers
 * are not relatively prime.
 */
BOOL
zmodinv(u, v, res)
	ZVALUE u, v;
	ZVALUE *res;
{
	FULL	q1, q2, ui3, vi3, uh, vh, A, B, C, D, T;
	ZVALUE	u2, u3, v2, v3, qz, tmp1, tmp2, tmp3;

	if (zisneg(u) || zisneg(v) || (zrel(u, v) >= 0))
		zmod(u, v, &v3);
	else
		zcopy(u, &v3);
	zcopy(v, &u3);
	u2 = _zero_;
	v2 = _one_;

	/*
	 * Loop here while the size of the numbers remain above
	 * the size of a FULL.  Throughout this loop u3 >= v3.
	 */
	while ((u3.len > 1) && !ziszero(v3)) {
		uh = (((FULL) u3.v[u3.len - 1]) << BASEB) + u3.v[u3.len - 2];
		vh = 0;
		if ((v3.len + 1) >= u3.len)
			vh = v3.v[v3.len - 1];
		if (v3.len == u3.len)
			vh = (vh << BASEB) + v3.v[v3.len - 2];
		A = 1;
		B = 0;
		C = 0;
		D = 1;

		/*
		 * Calculate successive quotients of the continued fraction
		 * expansion using only single precision arithmetic until
		 * greater precision is required.
		 */
		while ((vh + C) && (vh + D)) {
			q1 = (uh + A) / (vh + C);
			q2 = (uh + B) / (vh + D);
			if (q1 != q2)
				break;
			T = A - q1 * C;
			A = C;
			C = T;
			T = B - q1 * D;
			B = D;
			D = T;
			T = uh - q1 * vh;
			uh = vh;
			vh = T;
		}
	
		/*
		 * If B is zero, then we made no progress because
		 * the calculation requires a very large quotient.
		 * So we must do this step of the calculation in
		 * full precision
		 */
		if (B == 0) {
			zquo(u3, v3, &qz);
			zmul(qz, v2, &tmp1);
			zsub(u2, tmp1, &tmp2);
			zfree(tmp1);
			zfree(u2);
			u2 = v2;
			v2 = tmp2;
			zmul(qz, v3, &tmp1);
			zsub(u3, tmp1, &tmp2);
			zfree(tmp1);
			zfree(u3);
			u3 = v3;
			v3 = tmp2;
			zfree(qz);
			continue;
		}
		/*
		 * Apply the calculated A,B,C,D numbers to the current
		 * values to update them as if the full precision
		 * calculations had been carried out.
		 */
		zmuli(u2, (long) A, &tmp1);
		zmuli(v2, (long) B, &tmp2);
		zadd(tmp1, tmp2, &tmp3);
		zfree(tmp1);
		zfree(tmp2);
		zmuli(u2, (long) C, &tmp1);
		zmuli(v2, (long) D, &tmp2);
		zfree(u2);
		zfree(v2);
		u2 = tmp3;
		zadd(tmp1, tmp2, &v2);
		zfree(tmp1);
		zfree(tmp2);
		zmuli(u3, (long) A, &tmp1);
		zmuli(v3, (long) B, &tmp2);
		zadd(tmp1, tmp2, &tmp3);
		zfree(tmp1);
		zfree(tmp2);
		zmuli(u3, (long) C, &tmp1);
		zmuli(v3, (long) D, &tmp2);
		zfree(u3);
		zfree(v3);
		u3 = tmp3;
		zadd(tmp1, tmp2, &v3);
		zfree(tmp1);
		zfree(tmp2);
	}

	/*
	 * Here when the remaining numbers become single precision in size.
	 * Finish the procedure using single precision calculations.
	 */
	if (ziszero(v3) && !zisone(u3)) {
		zfree(u3);
		zfree(v3);
		zfree(u2);
		zfree(v2);
		return TRUE;
	}
	ui3 = (zistiny(u3) ? z1tol(u3) : z2tol(u3));
	vi3 = (zistiny(v3) ? z1tol(v3) : z2tol(v3));
	zfree(u3);
	zfree(v3);
	while (vi3) {
		q1 = ui3 / vi3;
		zmuli(v2, (long) q1, &tmp1);
		zsub(u2, tmp1, &tmp2);
		zfree(tmp1);
		zfree(u2);
		u2 = v2;
		v2 = tmp2;
		q2 = ui3 - q1 * vi3;
		ui3 = vi3;
		vi3 = q2;
	}
	zfree(v2);
	if (ui3 != 1) {
		zfree(u2);
		return TRUE;
	}
	if (zisneg(u2)) {
		zadd(v, u2, res);
		zfree(u2);
		return FALSE;
	}
	*res = u2;
	return FALSE;
}

/*
 * Binary gcd algorithm
 * This algorithm taken from Knuth
 */
void
zgcd(z1, z2, res)
	ZVALUE z1, z2, *res;
{
	ZVALUE u, v, t;
	register long j, k, olen, mask;
	register HALF h;
	HALF *oldv1, *oldv2;

	z1.sign = 0;
	z2.sign = 0;
	if (ziszero(z1)) {
		zcopy(z2, res);
		return;
	}
	if (ziszero(z2)) {
		zcopy(z1, res);
		return;
	}
	if (zisunit(z1) || zisunit(z2)) {
		*res = _one_;
		return;
	}
	/*
	 * First see if one number is very much larger than the other.
	 * If so, then divide as necessary to get the numbers near each other.
	 */
	oldv1 = z1.v;
	oldv2 = z2.v;
	if (z1.len < z2.len) {
		t = z1;
		z1 = z2;
		z2 = t;
	}
	while ((z1.len > (z2.len + 5)) && !ziszero(z2)) {
		zmod(z1, z2, &t);
		if ((z1.v != oldv1) && (z1.v != oldv2))
			zfree(z1);
		z1 = z2;
		z2 = t;
	}
	/*
	 * Ok, now do the binary method proper
	 */
	u.len = z1.len;
	v.len = z2.len;
	u.sign = 0;
	v.sign = 0;
	if (!ztest(z1)) {
		v.v = alloc(v.len);
		zcopyval(z2, v);
		*res = v;
		goto done;
	}
	if (!ztest(z2)) {
		u.v = alloc(u.len);
		zcopyval(z1, u);
		*res = u;
		goto done;
	}
	u.v = alloc(u.len);
	v.v = alloc(v.len);
	zcopyval(z1, u);
	zcopyval(z2, v);
	k = 0;
	while (u.v[k] == 0 && v.v[k] == 0)
		++k;
	mask = 01;
	h = u.v[k] | v.v[k];
	k *= BASEB;
	while (!(h & mask)) {
		mask <<= 1;
		k++;
	}
	zshiftr(u, k);
	zshiftr(v, k);
	ztrim(&u);
	ztrim(&v);
	if (zisodd(u)) {
		t.v = alloc(v.len);
		t.len = v.len;
		zcopyval(v, t);
		t.sign = !v.sign;
	} else {
		t.v = alloc(u.len);
		t.len = u.len;
		zcopyval(u, t);
		t.sign = u.sign;
	}
	while (ztest(t)) {
		j = 0;
		while (!t.v[j])
			++j;
		mask = 01;
		h = t.v[j];
		j *= BASEB;
		while (!(h & mask)) {
			mask <<= 1;
			j++;
		}
		zshiftr(t, j);
		ztrim(&t);
		if (ztest(t) > 0) {
			zfree(u);
			u = t;
		} else {
			zfree(v);
			v = t;
			v.sign = !t.sign;
		}
		zsub(u, v, &t);
	}
	zfree(t);
	zfree(v);
	if (k) {
		olen = u.len;
		u.len += k / BASEB + 1;
		u.v = (HALF *) ckrealloc((char *)u.v, u.len * sizeof(HALF));
		if (u.v == NULL) {
		    math_error("Not enough memory to expand number");
		}
		while (olen != u.len)
			u.v[olen++] = 0;
		zshiftl(u, k);
	}
	ztrim(&u);
	*res = u;

done:
	if ((z1.v != oldv1) && (z1.v != oldv2))
		zfree(z1);
	if ((z2.v != oldv1) && (z2.v != oldv2))
		zfree(z2);
}


/*
 * Compute the lcm of two integers (least common multiple).
 * This is done using the formula:  gcd(a,b) * lcm(a,b) = a * b.
 */
void
zlcm(z1, z2, res)
	ZVALUE z1, z2, *res;
{
	ZVALUE temp1, temp2;

	zgcd(z1, z2, &temp1);
	zquo(z1, temp1, &temp2);
	zfree(temp1);
	zmul(temp2, z2, res);
	zfree(temp2);
}


/*
 * Return whether or not two numbers are relatively prime to each other.
 */
BOOL
zrelprime(z1, z2)
	ZVALUE z1, z2;			/* numbers to be tested */
{
	FULL rem1, rem2;		/* remainders */
	ZVALUE rem;
	BOOL result;

	z1.sign = 0;
	z2.sign = 0;
	if (ziseven(z1) && ziseven(z2))	/* false if both even */
		return FALSE;
	if (zisunit(z1) || zisunit(z2))	/* true if either is a unit */
		return TRUE;
	if (ziszero(z1) || ziszero(z2))	/* false if either is zero */
		return FALSE;
	if (zistwo(z1) || zistwo(z2))	/* true if either is two */
		return TRUE;
	/*
	 * Try reducing each number by the product of the first few odd primes
	 * to see if any of them are a common factor.
	 */
	rem1 = zmodi(z1, 3L * 5 * 7 * 11 * 13);
	rem2 = zmodi(z2, 3L * 5 * 7 * 11 * 13);
	if (((rem1 % 3) == 0) && ((rem2 % 3) == 0))
		return FALSE;
	if (((rem1 % 5) == 0) && ((rem2 % 5) == 0))
		return FALSE;
	if (((rem1 % 7) == 0) && ((rem2 % 7) == 0))
		return FALSE;
	if (((rem1 % 11) == 0) && ((rem2 % 11) == 0))
		return FALSE;
	if (((rem1 % 13) == 0) && ((rem2 % 13) == 0))
		return FALSE;
	/*
	 * Try a new batch of primes now
	 */
	rem1 = zmodi(z1, 17L * 19 * 23);
	rem2 = zmodi(z2, 17L * 19 * 23);
	if (((rem1 % 17) == 0) && ((rem2 % 17) == 0))
		return FALSE;
	if (((rem1 % 19) == 0) && ((rem2 % 19) == 0))
		return FALSE;
	if (((rem1 % 23) == 0) && ((rem2 % 23) == 0))
		return FALSE;
	/*
	 * Yuk, we must actually compute the gcd to know the answer
	 */
	zgcd(z1, z2, &rem);
	result = zisunit(rem);
	zfree(rem);
	return result;
}

/*
 * Return the integral log base 10 of a number.
 */
long
zlog10(z)
	ZVALUE z;
{
	register ZVALUE *zp;		/* current square */
	long power;			/* current power */
	long worth;			/* worth of current square */
	ZVALUE val;			/* current value of power */
	ZVALUE temp;			/* temporary */

	if (!zispos(z))
		math_error("Non-positive number for log10");
	/*
	 * Loop by squaring the base each time, and see whether or
	 * not each successive square is still smaller than the number.
	 */
	worth = 1;
	zp = &_tenpowers_[0];
	*zp = _ten_;
	while (((zp->len * 2) - 1) <= z.len) {	/* while square not too large */
		if (zp[1].len == 0)
			zsquare(*zp, zp + 1);
		zp++;
		worth *= 2;
	}
	/*
	 * Now back down the squares, and multiply them together to see
	 * exactly how many times the base can be raised by.
	 */
	val = _one_;
	power = 0;
	for (; zp >= _tenpowers_; zp--, worth /= 2) {
		if ((val.len + zp->len - 1) <= z.len) {
			zmul(val, *zp, &temp);
			if (zrel(z, temp) >= 0) {
				zfree(val);
				val = temp;
				power += worth;
			} else
				zfree(temp);
		}
	}
	zfree(val);  /* tp: fix leak */
	return power;
}

/*
 * Remove all occurances of the specified factor from a number.
 * Also returns the number of factors removed as a function return value.
 * Example:  zfacrem(540, 3, &x) returns 3 and sets x to 20.
 */
long
zfacrem(z1, z2, rem)
	ZVALUE z1, z2, *rem;
{
	register ZVALUE *zp;		/* current square */
	long count;			/* total count of divisions */
	long worth;			/* worth of current square */
	ZVALUE temp1, temp2, temp3;	/* temporaries */
	ZVALUE squares[32];		/* table of squares of factor */

	z1.sign = 0;
	z2.sign = 0;
	/*
	 * Make sure factor isn't 0 or 1.
	 */
	if (zisleone(z2))
		math_error("Bad argument for facrem");
	/*
	 * Reject trivial cases.
	 */
	if ((z1.len < z2.len) || (zisodd(z1) && ziseven(z2)) ||
		((z1.len == z2.len) && (z1.v[z1.len-1] < z2.v[z2.len-1]))) {
		rem->v = alloc(z1.len);
		rem->len = z1.len;
		rem->sign = 0;
		zcopyval(z1, *rem);
		return 0;
	}
	/*
	 * Handle any power of two special.
	 */
	if (zisonebit(z2)) {
		count = zlowbit(z1) / zlowbit(z2);
		rem->v = alloc(z1.len);
		rem->len = z1.len;
		rem->sign = 0;
		zcopyval(z1, *rem);
		zshiftr(*rem, count);
		ztrim(rem);
		return count;
	}
	/*
	 * See if the factor goes in even once.
	 */
	zdiv(z1, z2, &temp1, &temp2);
	if (!ziszero(temp2)) {
		zfree(temp1);
		zfree(temp2);
		rem->v = alloc(z1.len);
		rem->len = z1.len;
		rem->sign = 0;
		zcopyval(z1, *rem);
		return 0;
	}
	zfree(temp2);
	z1 = temp1;
	/*
	 * Now loop by squaring the factor each time, and see whether
	 * or not each successive square will still divide the number.
	 */
	count = 1;
	worth = 1;
	zp = &squares[0];
	*zp = z2;
	while (((zp->len * 2) - 1) <= z1.len) {	/* while square not too large */
		zsquare(*zp, &temp1);
		zdiv(z1, temp1, &temp2, &temp3);
		if (!ziszero(temp3)) {
			zfree(temp1);
			zfree(temp2);
			zfree(temp3);
			break;
		}
		zfree(temp3);
		zfree(z1);
		z1 = temp2;
		*++zp = temp1;
		worth *= 2;
		count += worth;
	}
	/*
	 * Now back down the list of squares, and see if the lower powers
	 * will divide any more times.
	 */
	for (; zp >= squares; zp--, worth /= 2) {
		if (zp->len <= z1.len) {
			zdiv(z1, *zp, &temp1, &temp2);
			if (ziszero(temp2)) {
				temp3 = z1;
				z1 = temp1;
				temp1 = temp3;
				count += worth;
			}
			zfree(temp1);
			zfree(temp2);
		}
		if (zp != squares)
			zfree(*zp);
	}
	*rem = z1;
	return count;
}


/*
 * Keep dividing a number by the gcd of it with another number until the
 * result is relatively prime to the second number.
 */
void
zgcdrem(z1, z2, res)
	ZVALUE z1, z2, *res;
{
	ZVALUE tmp1, tmp2;

	/*
	 * Begin by taking the gcd for the first time.
	 * If the number is already relatively prime, then we are done.
	 */
	zgcd(z1, z2, &tmp1);
	if (zisunit(tmp1) || ziszero(tmp1)) {
		res->len = z1.len;
		res->v = alloc(z1.len);
		res->sign = z1.sign;
		zcopyval(z1, *res);
		return;
	}
	zquo(z1, tmp1, &tmp2);
	z1 = tmp2;
	z2 = tmp1;
	/*
	 * Now keep alternately taking the gcd and removing factors until
	 * the gcd becomes one.
	 */
	while (!zisunit(z2)) {
		(void) zfacrem(z1, z2, &tmp1);
		zfree(z1);
		z1 = tmp1;
		zgcd(z1, z2, &tmp1);
		zfree(z2);
		z2 = tmp1;
	}
	zfree(z2);   /* tp: fix leak */
	*res = z1;
}


/*
 * Find the lowest prime factor of a number if one can be found.
 * Search is conducted for the first count primes.
 * Returns 1 if no factor was found.
 */
long
zlowfactor(z, count)
	ZVALUE z;
	long count;
{
	FULL p, d;
	ZVALUE div, tmp;
	HALF divval[2];

	if ((--count < 0) || ziszero(z))
		return 1;
	if (ziseven(z))
		return 2;
	div.sign = 0;
	div.v = divval;
	for (p = 3; (count > 0); p += 2) {
		for (d = 3; (d * d) <= p; d += 2)
			if ((p % d) == 0)
				goto next;
		divval[0] = (p & BASE1);
		divval[1] = (p / BASE);
		div.len = 1 + (p >= BASE);
		zmod(z, div, &tmp);
		if (ziszero(tmp)) {
			zfree(tmp);   /* tp: fix leak */
			return p;
		}
		zfree(tmp);
		count--;
next:;
	}
	return 1;
}


/*
 * Return the number of digits (base 10) in a number, ignoring the sign.
 */
long
zdigits(z1)
	ZVALUE z1;
{
	long count;
	FULL val;

	z1.sign = 0;
	if (zistiny(z1)) {	/* do small numbers ourself */
		count = 1;
		val = 10;
		while ((FULL)(*z1.v) >= val) {
			count++;
			val *= 10;
		}
		return count;
	}
	return (zlog10(z1) + 1);
}

/*
 * Find the square root of a number.  This is the greatest integer whose
 * square is less than or equal to the number. Returns TRUE if the
 * square root is exact.
 */
BOOL
zsqrt(z1, dest)
	ZVALUE z1, *dest;
{
	ZVALUE ztry, quo, rem, old, temp;
	FULL iquo, val;
	long i,j;

	if (z1.sign)
		math_error("Square root of negative number");
	if (ziszero(z1)) {
		*dest = _zero_;
		return TRUE;
	}
	if ((*z1.v < 4) && zistiny(z1)) {
		*dest = _one_;
		return (*z1.v == 1);
	}
	/*
	 * Pick the square root of the leading one or two digits as a first guess.
	 */
	val = z1.v[z1.len-1];
	if ((z1.len & 0x1) == 0)
		val = (val * BASE) + z1.v[z1.len-2];

	/*
	 * Find the largest power of 2 that when squared
	 * is <= val > 0.  Avoid multiply overflow by doing 
	 * a careful check at the BASE boundary.
	 */
	j = 1L<<(BASEB+BASEB-2);
	if (val > j) {
		iquo = BASE;
	} else {
		i = BASEB-1;
		while (j > val) {
			--i;
			j >>= 2;
		}
		iquo = bitmask[i];
	}

	for (i = 8; i > 0; i--)
		iquo = (iquo + (val / iquo)) / 2;
	if (iquo > BASE1)
		iquo = BASE1;
	/*
	 * Allocate the numbers to use for the main loop.
	 * The size and high bits of the final result are correctly set here.
	 * Notice that the remainder of the test value is rubbish, but this
	 * is unimportant.
	 */
	ztry.sign = 0;
	ztry.len = (z1.len + 1) / 2;
	ztry.v = alloc(ztry.len);
	zclearval(ztry);
	ztry.v[ztry.len-1] = (HALF)iquo;
	old.sign = 0;
	old.v = alloc(ztry.len);
	old.len = 1;
	zclearval(old);
	/*
	 * Main divide and average loop
	 */
	for (;;) {
		zdiv(z1, ztry, &quo, &rem);
		i = zrel(ztry, quo);
		if ((i == 0) && ziszero(rem)) {	/* exact square root */
			zfree(rem);
			zfree(quo);
			zfree(old);
			*dest = ztry;
			return TRUE;
		}
		zfree(rem);
		if (i <= 0) {
			/*
			* Current ztry is less than or equal to the square root since
			* it is less than the quotient.  If the quotient is equal to
			* the ztry, we are all done.  Also, if the ztry is equal to the
			* old ztry value, we are done since no improvement occurred.
			* If not, save the improved value and loop some more.
			*/
			if ((i == 0) || (zcmp(old, ztry) == 0)) {
				zfree(quo);
				zfree(old);
				*dest = ztry;
				return FALSE;
			}
			old.len = ztry.len;
			zcopyval(ztry, old);
		}
		/* average current ztry and quotent for the new ztry */
		zadd(ztry, quo, &temp);
		zfree(quo);
		zfree(ztry);
		ztry = temp;
		zshiftr(ztry, 1L);
		zquicktrim(ztry);
	}
}


/*
 * Take an arbitrary root of a number (to the greatest integer).
 * This uses the following iteration to get the Kth root of N:
 *	x = ((K-1) * x + N / x^(K-1)) / K
 */
void
zroot(z1, z2, dest)
	ZVALUE z1, z2, *dest;
{
	ZVALUE ztry, quo, old, temp, temp2;
	ZVALUE k1;			/* holds k - 1 */
	int sign;
	long i, k, highbit;
	SIUNION sival;

	sign = z1.sign;
	if (sign && ziseven(z2))
		math_error("Even root of negative number");
	if (ziszero(z2) || zisneg(z2))
		math_error("Non-positive root");
	if (ziszero(z1)) {	/* root of zero */
		*dest = _zero_;
		return;
	}
	if (zisunit(z2)) {	/* first root */
		zcopy(z1, dest);
		return;
	}
	if (zisbig(z2)) {	/* humongous root */
		*dest = _one_;
		dest->sign = (HALF)sign;
		return;
	}
	k = (zistiny(z2) ? z1tol(z2) : z2tol(z2));
	highbit = zhighbit(z1);
	if (highbit < k) {	/* too high a root */
		*dest = _one_;
		dest->sign = (HALF)sign;
		return;
	}
	sival.ivalue = k - 1;
	k1.v = &sival.silow;
	k1.len = 1 + (sival.sihigh != 0);
	k1.sign = 0;
	z1.sign = 0;
	/*
	 * Allocate the numbers to use for the main loop.
	 * The size and high bits of the final result are correctly set here.
	 * Notice that the remainder of the test value is rubbish, but this
	 * is unimportant.
	 */
	highbit = (highbit + k - 1) / k;
	ztry.len = (highbit / BASEB) + 1;
	ztry.v = alloc(ztry.len);
	zclearval(ztry);
	ztry.v[ztry.len-1] = ((HALF)1 << (highbit % BASEB));
	ztry.sign = 0;
	old.v = alloc(ztry.len);
	old.len = 1;
	zclearval(old);
	old.sign = 0;
	/*
	 * Main divide and average loop
	 */
	for (;;) {
		zpowi(ztry, k1, &temp);
		zquo(z1, temp, &quo);
		zfree(temp);
		i = zrel(ztry, quo);
		if (i <= 0) {
			/*
			 * Current ztry is less than or equal to the root since it is
			 * less than the quotient. If the quotient is equal to the ztry,
			 * we are all done.  Also, if the ztry is equal to the old value,
			 * we are done since no improvement occurred.
			 * If not, save the improved value and loop some more.
			 */
			if ((i == 0) || (zcmp(old, ztry) == 0)) {
				zfree(quo);
				zfree(old);
				ztry.sign = (HALF)sign;
				zquicktrim(ztry);
				*dest = ztry;
				return;
			}
			old.len = ztry.len;
			zcopyval(ztry, old);
		}
		/* average current ztry and quotent for the new ztry */
		zmul(ztry, k1, &temp);
		zfree(ztry);
		zadd(quo, temp, &temp2);
		zfree(temp);
		zfree(quo);
		zquo(temp2, z2, &ztry);
		zfree(temp2);
	}
}

/* END CODE */
