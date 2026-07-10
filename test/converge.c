#include "test/one.c"
int main()
{
	int b = 3 + one();
	int a;
	if(b == 4) {
		a = 2 + one();
	} else {
		a = 1 + one();
	}
	return a;
}
