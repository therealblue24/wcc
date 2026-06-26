int main()
{
	int i;
	for(i = 0; i < 2147483647; i++) {
		if(i == 42)
			break;
	}
	return i;
}
