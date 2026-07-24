/* *twin* prime number generator, returns maximum *twin* prime number in [0, 1000] */
// comment test 5000

#include "test/one.c"
#define CTR 16384

long primes()
{
	long array[CTR];
	long max = 0;
	long iter = 0;

	for(long i = 0; i < CTR; ++i) {
		array[i] = 0;
	}

	/* initial prime pass */
	for(long i = 2; i < CTR; i++) {
		iter = i * 2;
		while(iter < CTR) {
			array[iter] = 1;
			iter += i;
		}
	}

	/* sieve out non-twin primes */
	/* based on assumption all twin primes come in form
	 *  6k-1, 6k+1
	 * for all k in N */
	for(long i = 6; i < CTR - 1; i += 6) {
		long v = array[i - 1] | array[i + 1];
		array[i - 1] = v;
		array[i + 1] = v;
	}

	for(long i = 0; i < CTR - 1; ++i) {
		if(!array[i] && i > max) {
			max = i;
		}
	}

	print_num(max);
	return max;
}

int main()
{
	return primes();
}
