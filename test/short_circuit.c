int puts(char *s);

int did_one;
int did_two;
int did_three;
int did_four;
int did_five;

int should_one(void)
{
	did_one = 1;
	puts("one");
	return 1;
}

int should_two(void)
{
	did_two = 1;
	puts("two");
	return 1;
}

int should_four(void)
{
	did_four = 1;
	puts("four");
	return 1;
}

int should_five(void)
{
	did_five = 1;
	puts("five");
	return 1;
}

int main()
{
	did_one = did_two = did_three = did_four = did_five = 0;
	if(should_one() && should_two()) {
		did_three = 1;
		puts("three");
	}

	if(should_four() || should_five()) {
		did_five = 1;
		puts("five");
	}

	/* look at console output aswell */
	return !(did_one & did_two & did_three & did_four & did_five);
}
