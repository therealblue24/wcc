int argument_waster3(int a, int b, int c, int d, int e, int f, int g, int h,
					 int i, int j, int k, int l);

#include "test/one.c"

int main()
{
	long n = 12;
	long sum = argument_waster(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, n);
	long sum2 = argument_waster3(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, n);
	long gauss_swag_route = (n * (n + 1)) / 2;

	return ((sum + sum2) / 2) + gauss_swag_route;
}
