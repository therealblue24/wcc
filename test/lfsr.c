#include "test/one.c"

/* https://graphics.stanford.edu/~seander/bithacks.html#ParityParallel */
unsigned char parity(unsigned char x)
{
	unsigned char v = x;
	v ^= v >> 4;
	v &= 0xf;
	return (0x6996 >> v) & 1;
}

unsigned char lfsr(unsigned char state)
{
	unsigned char lstate = state;
	unsigned char bit = parity(lstate & 0x1d);
	return (lstate >> 1) | (bit << 7);
}

unsigned char lfsr8(unsigned char *state)
{
	unsigned char acc = 0;
	for(int i = 0; i < 8; i++) {
		*state = lfsr(*state);
		acc <<= 1;
		acc |= (((*state) >> 7) & 1) ^ 1;
	}
	return ~acc;
}

int main()
{
	unsigned char state = 0x42;
	unsigned char num = lfsr8(&state);
	print_num(num);
	return num == 127u;
}
