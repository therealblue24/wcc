int argument_waster3(int a, int b, int c, int d, int e, int f, int g, int h,
					 int i, int j, int k, int l);

int argument_waster4(int a, int b, int c, int d, int e, int f, int g, int h,
					 int i, int j, int k, int l, int m)
{
	return a + b + c + d + e + f + g + h + i + j + k + l + m;
}

#include "test/one.c"

int main()
{
	long n = 12;
	long sum = argument_waster(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, n);
	long sum2 = argument_waster3(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, n);
	long sum3 = argument_waster4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, n, 0);
	sum2 += sum3;
	sum2 /= 2;
	long gauss_swag_route = (n * (n + 1)) / 2;

	return ((sum + sum2) / 2) + gauss_swag_route;
}
