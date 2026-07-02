#include "test/one.c"

int main()
{
	int a = one(), b = two(one()), c = one() + two(2), d = 3;
line100:
	if(a == 1 && b == 2) {
		goto line120;
	}
	// fallthru
line110:
	if(c == 5 && d == 3) {
		return 42;
	}
line120:
	c = 4;
	if(a + c + d == 6) {
		goto line130;
	}
	d--;
	a = --d;
	goto line100;
line130:
	return 24;
}
