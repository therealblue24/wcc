long one(void)
{
	return 1;
}

long blackbox(long x)
{
	return x;
}

long two(long x)
{
	return 2 * x;
}

long argument_waster(long a, long b, long c, long d, long e, long f, long g,
					 long h, long i, long j, long k, long l)
{
	return a + b + c + d + e + f + g + h + i + j + k + l;
}

void putchar(int c);

void print_num_helper(long num)
{
	/* https://stackoverflow.com/a/59389473 */

	if(num >= 10l) {
		print_num_helper(num / 10l);
	}

	putchar((num % 10l) + '0');

	return;
}

void print_num(long num)
{
	if(num < 0) {
		putchar('-');
		num = -num;
	}
	print_num_helper(num);
	putchar('\n');
	return;
}

void print_num_helper_unsignd(unsigned long num)
{
	/* https://stackoverflow.com/a/59389473 */

	if(num >= 10ul) {
		print_num_helper_unsignd(num / 10ul);
	}

	putchar((num % 10ul) + '0');

	return;
}

void print_num_unsignd(unsigned long num)
{
	print_num_helper_unsignd(num);
	putchar('\n');
	return;
}
