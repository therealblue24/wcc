int main()
{
	/* assumes little endian */
	typedef union {
		char low_byte;
		int full;
	} test_union;

	test_union u;
	u.full = 0x12345678;
	char low = u.low_byte;
	return 119 + (low == 120); /* => 0x78, should be (decimal) 120 */
}
