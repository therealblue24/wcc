int main()
{
	int a = 24;
	int b = 60;

	while(b) {
		int t = b;
		b = a % b;
		a = t;
	}

	return a;
}
