enum apple {
	foo = 1,
	bar,
};

enum orange { citrus = 5, smth = -1, idk = 42 };

int main()
{
	enum apple some_var = bar;
	enum orange other_var = smth;
	enum lemon {
		zesty = 11,
	};

	enum lemon excellent_var = zesty;

	return zesty * (some_var + other_var) == excellent_var * foo;
}
