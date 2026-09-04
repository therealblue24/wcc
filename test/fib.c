#include "test/one.c"

int fib(int n)
{
	int ln = n;
	int a = 1;
	int b = 1;
	while(ln--) {
		a += b;
		a ^= b;
		b ^= a;
		a ^= b;
	}
	return b;
}

int main()
{
	int v = fib(42);
	print_num(v);
	return v; /* => should be 701408733, mod 256 = 221 */
}
