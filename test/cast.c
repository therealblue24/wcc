int main()
{
	long b = (unsigned int)-1;
	long c = (unsigned short)-5;
	long a = (int)b + (int)c;
	_Bool ok1 = a == 65530;

	unsigned char truncate = (unsigned char)((short)-1);
	long d = (unsigned long)truncate;
	_Bool ok2 = d == 255;

	return (int)ok1 == (short)ok2 && (char)ok1 == 1;
}
