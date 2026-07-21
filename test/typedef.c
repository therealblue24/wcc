int main()
{
	typedef int cool_type;
	typedef long cooler_type;

	cool_type my_int = 5;

	typedef struct cool_struct {
		cool_type cool_foo;
		cooler_type cool_bar;
	} cool_struct;

	cool_struct s;
	s.cool_foo = my_int;
	s.cool_bar = (cooler_type)59;

	return my_int - s.cool_foo + s.cool_bar;
}
