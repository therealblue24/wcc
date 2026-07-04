int main()
{
	int foo;
	_Alignas(16) short bar[5];
	int v = sizeof foo + (_Alignof(bar) * sizeof(bar));
	int k = sizeof(int) + (_Alignof(_Alignas(16) short[5]) * sizeof(short[5]));
	if(v == k) {
		return v;
	}
	return -1;
}
