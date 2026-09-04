int collatz(int n)
{
	int ln = n;
	return ln & 1 ? ln + ln + ln + 1 : ln >> 1;
}

int terminates(int n)
{
	int cnt = 0;
	int ln = n;
	while((ln = collatz(ln))) {
		cnt++;
		if(ln == 1) {
			return cnt;
		}
	}

	/* fun part */
	*(char *)0 = 0;
	return 0;
}

int main()
{
	return terminates(12345);
}
