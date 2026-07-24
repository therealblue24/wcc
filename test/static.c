static char static_var = 1, static_var2 = 2;
static short short_var = 1;
static int int_var = 1;
static long long_var = 1;

static int private(int x)
{
	return 5 + x;
}

int main()
{
	return private(42) + static_var + static_var2 +
		   (static_var + short_var + int_var + long_var - 4);
}
