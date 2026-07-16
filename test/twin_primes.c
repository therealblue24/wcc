/* *twin* prime number generator, returns maximum *twin* prime number in [0, 1000] */
// comment test 5000

#include "test/one.c"
int primes()
{
	int array[1000];
	int max = 0;
	int iter = 0;

	for(int i = 0; i < 1000; ++i) {
		array[i] = 0;
	}

	/* initial prime pass */
	for(int i = 2; i < 1000; i++) {
		iter = i * 2;
		while(iter < 1000) {
			array[iter] = 1;
			iter += i;
		}
	}

	/* sieve out non-twin primes */
	/* based on assumption all twin primes come in form
	 *  6k-1, 6k+1
	 * for all k in N */
	for(int i = 6; i < 999; i += 6) {
		int v = array[i - 1] | array[i + 1];
		array[i - 1] = v;
		array[i + 1] = v;
	}

	for(int i = 0; i < 1000; ++i) {
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
