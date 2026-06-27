#include "test/one.c"
int main()
{
	int a = 0;
	int b = 0;
	int c = 0;
	int d = 4;
	switch(d) {
	case 0:
		a = 1;
	case 1:
		b = 2;
		break;
	case 4:
		c = 3;
	case 2:
		d = 1;
		a = 2;
		break;
	case 3:
		c = 4;
		break;
	}

	return c == 3 && d == 1 && a == 2 && b == 0;
}
