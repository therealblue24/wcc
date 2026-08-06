#include "test/one.c"

void div_full(unsigned long num, unsigned long den, unsigned long *div,
			  unsigned long *rem)
{
	unsigned long lnum = num;
	unsigned long lrem = 0;
	unsigned long ldiv = 0;
	unsigned long lden = den;

	for(int i = 0; i < 64; i++) {
		lrem = (lrem << 1) | (lnum >> 63);
		lnum = lnum << 1;
		ldiv = ldiv << 1;
		if(lrem >= lden) {
			lrem -= lden;
			ldiv |= 1;
		}
	}

	*div = ldiv;
	*rem = lrem;
	return;
}

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
	unsigned long num2 = num;
	unsigned long den2 = den;
	unsigned long rem = 0;
	unsigned long div = 0;
	unsigned long rem2 = 0;
	unsigned long div2 = 0;
	for(int i = 0; i < 64; i++) {
		div_round(&div, &rem, &num, den);
	}
	div_full(num2, den2, &div2, &rem2);

	print_num_unsignd(div2);
	print_num_unsignd(rem2);
	return div == 1u && rem == 44667u && div == div2 && rem == rem2;
}
