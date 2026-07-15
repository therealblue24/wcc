#include "test/one.c"
int main()
{
	struct {
		int a;
		char b;
		unsigned long l[5];
	} c;
	struct {
		int a;
		char b;
		unsigned long l[5];
	} d;
	c.a = 2;
	c.b = 3;
	c.l[0] = 1;
	c.l[1] = 0;
	c.l[2] = 1;
	c.l[3] = 0;
	c.l[4] = 2;
	d = c;
	int sum = c.l[0] + c.l[1] + c.l[2] + c.l[3] + c.l[4]; // should be 4
	int sum2 = d.l[0] + d.l[1] + d.l[2] + d.l[3] + d.l[4];
	int v1 = (c.a + c.b + sum) - 4;
	int v1d = (d.a + d.b + sum) - 4;
	int v1t = (v1 + v1d) >> 1;

	struct {
		char super_secret_val;
	} other_struct;

	other_struct.super_secret_val = 42;
	int v2 = (&other_struct)->super_secret_val;

	return (v1t + v2) - 42;
}
