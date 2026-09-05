
/* tests range-opt */
int in_range(int i)
{
	return (i >= 500) & (i <= 750);
}

int main()
{
	for(int i = 0; i < 10000; i++) {
		if(in_range(i) && (i < 500 || i > 750)) {
			return 123;
		}
	}
	return 456;
}
