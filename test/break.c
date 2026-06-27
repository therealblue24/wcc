int main()
{
	int i;
	for(i = 0; i < 2147483646; i++) {
		if(i == 42)
			break;
	}
	return i;
}
