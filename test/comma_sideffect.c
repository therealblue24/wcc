int puts(char *s);

int print_one()
{
	puts("one");
	return 1;
}

int print_two()
{
	puts("two");
	return 2;
}

int print_three()
{
	puts("three");
	return 3;
}

int main()
{
	int a, b;
	b = (a = print_one(), a = print_two(), a = print_three());
	return a == b && a == 3;
}
