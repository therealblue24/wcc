int isalpha(int x)
{
	switch(x) {
	case 'a' ... 'z':
		return 1;
	case 'A' ... 'Z':
		return 1;
	case '!' ... '!':
		return 2;
	case 255 ... 250:
		return 3;
	default:
		return 0;
	}
	return -1;
}

int main()
{
	return isalpha('H') && isalpha('e') && isalpha('l') && isalpha('l') &&
		   isalpha('o');
}
