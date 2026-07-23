int main()
{
	int a;
	int b;
	int c = 4;
	int d = 3;
	b = (a = d, d = c, c);
	return b == 4 && a == 3 && d == 4;
}
