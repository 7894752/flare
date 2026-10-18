/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Biased x87 float-to-integer rounding.
===========================================================================
*/

/* Biased x87 float-to-integer rounding. Requires round-to-nearest-even mode. */
#ifndef FLARE_Q_ROUND_H
#define FLARE_Q_ROUND_H

/*
=============
RoundToInt
=============
*/
__inline int RoundToInt( float value ) {
	double bias = 9.31322574615478515625e-10;
	int i;
	__asm {
		fld value
		fadd bias
		fistp i
	}
	return i;
}

/*
=============
FloorToInt
=============
*/
__inline int FloorToInt( float value ) {
	double bias = 0.49999999906867743;
	int i;
	__asm {
		fld value
		fsub bias
		fistp i
	}
	return i;
}

/*
=============
CeilToInt
=============
*/
__inline int CeilToInt( float value ) {
	double bias = 0.49999999906867743;
	int i;
	__asm {
		fld value
		fadd bias
		fistp i
	}
	return i;
}

#endif
