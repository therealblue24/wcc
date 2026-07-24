static char static_var = 1, static_var2 = 2;

static int private(int x)
{
	return 5 + x;
}

int main()
{
	return private(42) + static_var + static_var2;
}
