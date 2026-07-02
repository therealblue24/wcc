int main()
{
	struct {
		int a;
		char b;
		unsigned long l[5];
	} c;
	c.a = 2;
	c.b = 3;
	c.l[0] = 1;
	c.l[1] = 0;
	c.l[2] = 1;
	c.l[3] = 0;
	c.l[4] = 2;
	int sum = c.l[0] + c.l[1] + c.l[2] + c.l[3] + c.l[4]; // should be 4
	int v1 = (c.a + c.b + sum) - 4;

	struct {
		char super_secret_val;
	} other_struct;

	other_struct.super_secret_val = 42;
	int v2 = (&other_struct)->super_secret_val;

	return (v1 + v2) - 42;
}
