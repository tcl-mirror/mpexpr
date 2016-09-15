/*
 * Copyright (c) 1994 David I. Bell
 * Permission is granted to use, distribute, or modify this source,
 * provided that this copyright notice remains intact.
 *
 * Scanf and printf routines for arbitrary precision integers.
 */

#include "mpexpr.h"

#define	OUTBUFSIZE	200		/* realloc size for output buffers */

#define	PUTCHAR(ch)		math_chr(ch)
#define	PUTSTR(str)		math_str(str)
#define	PRINTF1(fmt, a1)	math_fmt(fmt, a1)
#define	PRINTF2(fmt, a1, a2)	math_fmt(fmt, a1, a2)

/*
 * Output state that has been saved when diversions are done.
 */

static Tcl_ThreadDataKey outKey = NULL;
typedef struct Out {
    struct Out *next;	/* List of pending nested buffers */
    char *buf;		/* output string buffer */
    long size;		/* current size of buffer */
    long used;		/* space used in buffer */
} Out;

static Out *
GetOut()
{
	Out **outListPtr = Tcl_GetThreadData(&outKey, sizeof(Out *));
	Out *out = *outListPtr;

	if (out == NULL) {
		math_divertio();
		out = *outListPtr;
	}
	return out;
}

/*
 * Routine to output a character either to a FILE
 * handle or into a string.
 */
void
math_chr(ch)
	int ch;
{
	char	*cp;
	Out *out = GetOut();

	if (out->used >= out->size) {
		cp = (char *)ckrealloc(out->buf, out->size + OUTBUFSIZE + 1);
		if (cp == NULL)
			math_error("Cannot realloc output string");
		out->buf = cp;
		out->size += OUTBUFSIZE;
	}
	out->buf[out->used++] = (char)ch;
}


/*
 * Routine to output a null-terminated string either
 * to a FILE handle or into a string.
 */
void
math_str(str)
	CONST char	*str;
{
	Out *out = GetOut();
	char	*cp;
	int	len;

	len = strlen(str);
	if ((out->used + len) > out->size) {
		cp = (char *)ckrealloc(out->buf, out->size + len + OUTBUFSIZE + 1);
		if (cp == NULL)
			math_error("Cannot realloc output string");
		out->buf = cp;
		out->size += (len + OUTBUFSIZE);
	}
	memcpy(out->buf + out->used, str, len);
	out->used += len;
}


/*
 * Output a null-terminated string either to a FILE handle or into a string,
 * padded with spaces as needed so as to fit within the specified width.
 * If width is positive, the spaces are added at the front of the string.
 * If width is negative, the spaces are added at the end of the string.
 * The complete string is always output, even if this overflows the width.
 * No characters within the string are handled specially.
 */
void
math_fill(str, width)
	char *str;
	long width;
{
	if (width > 0) {
		width -= strlen(str);
		while (width-- > 0)
			PUTCHAR(' ');
		PUTSTR(str);
	} else {
		width += strlen(str);
		PUTSTR(str);
		while (width++ < 0)
			PUTCHAR(' ');
	}
}


/*
 * Routine to output a printf-style formatted string either
 * to a FILE handle or into a string.
 */

/*VARARGS2*/
void
math_fmt TCL_VARARGS_DEF(char *, arg1)

{
	va_list ap;
	char buf[200];
	char *fmt;

	fmt = TCL_VARARGS_START(char *, arg1, ap);
	
	vsprintf(buf, fmt, ap);
	va_end(ap);
	math_str(buf);
}

/*
 * Divert further output so that it is saved into a string that will be
 * returned later when the diversion is completed.  The current state of
 * output is remembered for later restoration.  Diversions can be nested.
 * Output diversion is only intended for saving output to "stdout".
 */
void
math_divertio()
{
	Out **outListPtr = Tcl_GetThreadData(&outKey, sizeof(Out *));
	Out *new = (Out *) ckalloc(sizeof(Out));

	if (new == NULL)
		math_error("No memory for diverting output");
	new->next = *outListPtr;
	new->buf = (char *) ckalloc(OUTBUFSIZE + 1);
	if (new->buf == NULL)
		math_error("Cannot allocate divert string");
	new->size = OUTBUFSIZE;
	new->used = 0;
	*outListPtr = new;
}


/*
 * Undivert output and return the saved output as a string.  This also
 * restores the output state to what it was before the diversion began.
 * The string needs freeing by the caller when it is no longer needed.
 */
char *
math_getdivertedio()
{
	Out **outListPtr = Tcl_GetThreadData(&outKey, sizeof(Out *));
	Out *out = *outListPtr;
	char *cp;

	if (out == NULL)
		math_error("No diverted state to restore");
	cp = out->buf;
	cp[out->used] = '\0';
	*outListPtr = out->next;
	ckfree((char *)out);
	return cp;
}


/*
 * Clear all diversions and set output back to the original destination.
 * This is called when resetting the global state of the program.
 */
void
math_cleardiversions()
{
	Out **outListPtr = Tcl_GetThreadData(&outKey, sizeof(Out *));

	while (*outListPtr) 
		ckfree(math_getdivertedio());
}

/*
 * Print an integer value as a hex number.
 * Width is the number of columns to print the number in, including the
 * sign if required.  If zero, no extra output is done.  If positive,
 * leading spaces are typed if necessary. If negative, trailing spaces are
 * typed if necessary.  The special characters 0x appear to indicate the
 * number is hex.
 */
/*ARGSUSED*/
void
zprintx(z, width)
	ZVALUE z;
	long width;
{
	register HALF *hp;	/* current word to print */
	int len;		/* number of halfwords to type */
	char *str;

	if (width) {
		math_divertio();
		zprintx(z, 0L);
		str = math_getdivertedio();
		math_fill(str, width);
		ckfree(str);
		return;
	}
	len = z.len - 1;
	if (zisneg(z))
		PUTCHAR('-');
	if ((len == 0) && (*z.v <= (FULL) 9)) {
		len = '0' + *z.v;
		PUTCHAR(len);
		return;
	}
	hp = z.v + len;
	PRINTF1("0x%x", (FULL) *hp--);
	while (--len >= 0)
		PRINTF2("%0*x", BASEB/4, (FULL) *hp--);
}


/*
 * Print an integer value as a binary number.
 * The special characters 0b appear to indicate the number is binary.
 */
/*ARGSUSED*/
void
zprintb(z, width)
	ZVALUE z;
	long width;
{
	register HALF *hp;	/* current word to print */
	int len;		/* number of halfwords to type */
	HALF val;		/* current value */
	HALF mask;		/* current mask */
	int didprint;		/* nonzero if printed some digits */
	int ch;			/* current char */
	char *str;

	if (width) {
		math_divertio();
		zprintb(z, 0L);
		str = math_getdivertedio();
		math_fill(str, width);
		ckfree(str);
		return;
	}
	len = z.len - 1;
	if (zisneg(z))
		PUTCHAR('-');
	if ((len == 0) && (*z.v <= (FULL) 1)) {
		len = '0' + *z.v;
		PUTCHAR(len);
		return;
	}
	hp = z.v + len;
	didprint = 0;
	PUTSTR("0b");
	while (len-- >= 0) {
		val = *hp--;
		mask = (1 << (BASEB - 1));
		while (mask) {
			ch = '0' + ((mask & val) != 0);
			if (didprint || (ch != '0')) {
				PUTCHAR(ch);
				didprint = 1;
			}
			mask >>= 1;
		}
	}
}


/*
 * Print an integer value as an octal number.
 * The number begins with a leading 0 to indicate that it is octal.
 */
/*ARGSUSED*/
void
zprinto(z, width)
	ZVALUE z;
	long width;
{
	register HALF *hp;	/* current word to print */
	int len;		/* number of halfwords to type */
	FULL num1='0', num2='0'; /* numbers to type */
	int rem;		/* remainder number of halfwords */
	char *str;
	HALF mask;

	if (width) {
		math_divertio();
		zprinto(z, 0L);
		str = math_getdivertedio();
		math_fill(str, width);
		ckfree(str);
		return;
	}
	if (zisneg(z))
		PUTCHAR('-');
	len = z.len;
	if ((len == 1) && (*z.v <= (FULL) 7)) {
		num1 = '0' + *z.v;
		PUTCHAR(num1);
		return;
	}
	hp = z.v + len - 1;
	mask = (HALF)(BASE1 >> (BASEB/2));
	rem = len % 3;
	switch (rem) {	/* handle odd amounts first */
		case 0:
			num1 = (((FULL) hp[0]) << (BASEB/2)) + (((FULL) hp[-1]) >> (BASEB/2));
			num2 = (((FULL) (hp[-1] & mask)) << BASEB) + ((FULL) hp[-2]);
			rem = 3;
			break;
		case 1:
			num1 = 0;
			num2 = (FULL) hp[0];
			break;
		case 2:
			num1 = (((FULL) hp[0]) >> (BASEB/2));
			num2 = (((FULL) (hp[0] & mask)) << BASEB) + ((FULL) hp[-1]);
			break;
	}
	if (num1) {
		PRINTF1("0%"FULLFMT"o", num1);
		PRINTF2("%0*"FULLFMT"o", BASEB/2, num2);
	} else {
		PRINTF1("0%"FULLFMT"o", num2);
	}
	len -= rem;
	hp -= rem;
	while (len > 0) {	/* finish in groups of 3 halfwords */
		num1 = (((FULL) hp[0]) << (BASEB/2)) + (((FULL) hp[-1]) >> (BASEB/2));
		num2 = (((FULL) (hp[-1] & mask)) << BASEB) + ((FULL) hp[-2]);
		PRINTF2("%0*"FULLFMT"o", BASEB/2, num1);
		PRINTF2("%0*"FULLFMT"o", BASEB/2, num2);
		hp -= 3;
		len -= 3;
	}
}


/*
 * Print a decimal integer to the terminal.
 * This works by dividing the number by 10^2^N for some N, and
 * then doing this recursively on the quotient and remainder.
 * Decimals supplies number of decimal places to print, with a decimal
 * point at the right location, with zero meaning no decimal point.
 * Width is the number of columns to print the number in, including the
 * decimal point and sign if required.  If zero, no extra output is done.
 * If positive, leading spaces are typed if necessary. If negative, trailing
 * spaces are typed if necessary.  As examples of the effects of these values,
 * (345,0,0) = "345", (345,2,0) = "3.45", (345,5,8) = "  .00345".
 */
void
zprintval(z, decimals, width)
	ZVALUE z;		/* number to be printed */
	long decimals;		/* number of decimal places */
	long width;		/* number of columns to print in */
{
	int depth;		/* maximum depth */
	int n;			/* current index into array */
	int i;			/* number to print */
	long leadspaces;	/* number of leading spaces to print */
	long putpoint;		/* digits until print decimal point */
	long digits;		/* number of digits of raw number */
	BOOL output;		/* TRUE if have output something */
	BOOL neg;		/* TRUE if negative */
	ZVALUE quo, rem;	/* quotient and remainder */
	ZVALUE leftnums[32];	/* left parts of the number */
	ZVALUE rightnums[32];	/* right parts of the number */

	if (decimals < 0)
		decimals = 0;
	if (width < 0)
		width = 0;
	neg = (z.sign != 0);

	leadspaces = width - neg - (decimals > 0);
	z.sign = 0;
	/*
	 * Find the 2^N power of ten which is greater than or equal
	 * to the number, calculating it the first time if necessary.
	 */
	_tenpowers_[0] = _ten_;
	depth = 0;
	while ((_tenpowers_[depth].len < z.len) || (zrel(_tenpowers_[depth], z) <= 0)) {
		depth++;
		if (_tenpowers_[depth].len == 0)
			zsquare(_tenpowers_[depth-1], &_tenpowers_[depth]);
	}
	/*
	 * Divide by smaller 2^N powers of ten until the parts are small
	 * enough to output.  This algorithm walks through a binary tree
	 * where each node is a piece of the number to print, and such that
	 * we visit left nodes first.  We do the needed recursion in line.
	 */
	digits = 1;
	output = FALSE;
	n = 0;
	putpoint = 0;
	rightnums[0].len = 0;
	leftnums[0] = z;
	for (;;) {
		while (n < depth) {
			i = depth - n - 1;
			zdiv(leftnums[n], _tenpowers_[i], &quo, &rem);
			if (!ziszero(quo))
				digits += (1L << i);
			n++;
			leftnums[n] = quo;
			rightnums[n] = rem;
		}
		i = leftnums[n].v[0];
		if (output || i || (n == 0)) {
			if (!output) {
				output = TRUE;
				if (decimals > digits)
					leadspaces -= decimals;
				else
					leadspaces -= digits;
				while (--leadspaces >= 0)
					PUTCHAR(' ');
				if (neg)
					PUTCHAR('-');
				if (decimals) {
					putpoint = (digits - decimals);
					if (putpoint <= 0) {
						PUTCHAR('.');
						while (++putpoint <= 0)
							PUTCHAR('0');
						putpoint = 0;
					}
				}
			}
			i += '0';
			PUTCHAR(i);
			if (--putpoint == 0)
				PUTCHAR('.');
		}
		while (rightnums[n].len == 0) {
			if (n <= 0)
				return;
			if (leftnums[n].len)
				zfree(leftnums[n]);
			n--;
		}
		zfree(leftnums[n]);
		leftnums[n] = rightnums[n];
		rightnums[n].len = 0;
	}
}

/*
 * Print a decimal integer to the terminal.
 * This works by dividing the number by 10^2^N for some N, and
 * then doing this recursively on the quotient and remainder.
 * Decimals supplies number of decimal places to print, with a decimal
 * point at the right location, with zero meaning no decimal point.
 * Width is the number of columns to print the number in, including the
 * decimal point and sign if required.  If zero, no extra output is done.
 * If positive, leading spaces are typed if necessary. If negative, trailing
 * spaces are typed if necessary.  As examples of the effects of these values,
 * (345,0,0) = "345", (345,2,0) = "3.45", (345,5,8) = "  .00345".
 */
void
Zprintval(z, decimals, width)
	ZVALUE z;		/* number to be printed */
	long decimals;		/* number of decimal places */
	long width;		/* number of columns to print in */
{
	int depth;		/* maximum depth */
	int n;			/* current index into array */
	int i;			/* number to print */
	long leadspaces;	/* number of leading spaces to print */
	long putpoint;		/* digits until print decimal point */
	long digits;		/* number of digits of raw number */
	BOOL output;		/* TRUE if have output something */
	BOOL neg;		/* TRUE if negative */
	ZVALUE quo, rem;	/* quotient and remainder */
	ZVALUE leftnums[32];	/* left parts of the number */
	ZVALUE rightnums[32];	/* right parts of the number */

	if (decimals < 0)
		decimals = 0;
	if (width < 0)
		width = 0;
	neg = (z.sign != 0);

	leadspaces = width - neg - (decimals > 0);
	z.sign = 0;
	/*
	 * Find the 2^N power of ten which is greater than or equal
	 * to the number, calculating it the first time if necessary.
	 */
	_tenpowers_[0] = _ten_;
	depth = 0;
	while ((_tenpowers_[depth].len < z.len) || (zrel(_tenpowers_[depth], z) <= 0)) {
		depth++;
		if (_tenpowers_[depth].len == 0)
			zsquare(_tenpowers_[depth-1], &_tenpowers_[depth]);
	}
	/*
	 * Divide by smaller 2^N powers of ten until the parts are small
	 * enough to output.  This algorithm walks through a binary tree
	 * where each node is a piece of the number to print, and such that
	 * we visit left nodes first.  We do the needed recursion in line.
	 */
	digits = 1;
	output = FALSE;
	n = 0;
	putpoint = 0;
	rightnums[0].len = 0;
	leftnums[0] = z;
	for (;;) {
		while (n < depth) {
			i = depth - n - 1;
			zdiv(leftnums[n], _tenpowers_[i], &quo, &rem);
			if (!ziszero(quo))
				digits += (1L << i);
			n++;
			leftnums[n] = quo;
			rightnums[n] = rem;
		}
		i = leftnums[n].v[0];
		if (output || i || (n == 0)) {
			if (!output) {
				output = TRUE;
				if (decimals > digits)
					leadspaces -= decimals;
				else
					leadspaces -= digits;
				while (--leadspaces >= 0)
					PUTCHAR(' ');
				if (neg)
					PUTCHAR('-');
				if (decimals) {
					putpoint = (digits - decimals);
					if (putpoint <= 0) {
						PUTCHAR('0');
						PUTCHAR('.');
						while (++putpoint <= 0)
							PUTCHAR('0');
						putpoint = 0;
					}
				}
			}
			i += '0';
			PUTCHAR(i);
			if (--putpoint == 0)
				PUTCHAR('.');
		}
		while (rightnums[n].len == 0) {
			if (n <= 0)
				return;
			if (leftnums[n].len)
				zfree(leftnums[n]);
			n--;
		}
		zfree(leftnums[n]);
		leftnums[n] = rightnums[n];
		rightnums[n].len = 0;
	}
}


/*
 * Read an integer value in decimal, hex, octal, or binary.
 * Hex numbers are indicated by a leading "0x", binary with a leading "0b",
 * and octal by a leading "0".  Periods are skipped over, but any other
 * extraneous character stops the scan.
 */
void
atoz(s, res)
	register CONST char *s;
	ZVALUE *res;
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
	if (*s == '0') {		/* possibly hex, octal, or binary */
		s++;
		if ((*s >= '0') && (*s <= '7')) {
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
		digval = *s++;
		if ((digval >= '0') && (digval <= '9'))
			digval -= '0';
		else if ((digval >= 'a') && (digval <= 'f') && shift)
			digval -= ('a' - 10);
		else if ((digval >= 'A') && (digval <= 'F') && shift)
			digval -= ('A' - 10);
		else if (digval == '.')
			continue;
		else
			break;
		if (shift)
			zshift(z, shift, &ztmp);
		else
			zmuli(z, 10L, &ztmp);
		zfree(z);
		zadd(ztmp, digit, &z);
		zfree(ztmp);
	}
	ztrim(&z);
	if (minus && !ziszero(z))
		z.sign = 1;
	*res = z;
}

/* END CODE */
