#include "test/one.c"

void div_round(unsigned long *div, unsigned long *rem, unsigned long *num,
			   unsigned long den)
{
	unsigned long ldiv = *div;
	unsigned long lrem = *rem;
	unsigned long lnum = *num;
	lrem = (lrem << 1) | (lnum >> 63);
	lnum = lnum << 1;

	/* emit one, else zero */
	if(lrem >= den) {
		lrem -= den;
		ldiv = (ldiv << 1) | 1;
	} else {
		ldiv = (ldiv << 1);
	}

	*div = ldiv;
	*rem = lrem;
	*num = lnum;
	return;
}

int main()
{
	unsigned long num = 134801;
	unsigned long den = 90134;
	unsigned long rem = 0;
	unsigned long div = 0;
	for(int i = 0; i < 64; i++) {
		div_round(&div, &rem, &num, den);
	}

	return div == 1u && rem == 44667u;
}
