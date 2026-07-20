unsigned int blackbox2(unsigned int x)
{
	return x;
}

int main()
{
	unsigned int x = blackbox2(4);
	unsigned int y = (x * 512) / 256;
	return y;
}
