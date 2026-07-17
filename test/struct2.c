#include "test/one.c"
int main()
{
	struct foo {
		int a;
	};
	struct bar {
		char x[5];
	};

	struct foo foo;
	struct bar bar;

	foo.a = 1;
	for(int i = 0; i < 5; i++) {
		bar.x[i] = i + 1;
	}

	int sum = 0;
	sum += foo.a;
	for(int i = 0; i < 5; i++) {
		sum += bar.x[i];
	}

	return sum; /* should be 16 */
}
