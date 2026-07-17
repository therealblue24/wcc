#include "test/one.c"

/* thx https://wiki.osdev.org/CRC32 */

unsigned int crc32_poly(unsigned char x)
{
	unsigned int out = x;
	unsigned int poly = 0xedb88320;

	for(int i = 0; i < 8; i++) {
		unsigned int lsb = out & 1;
		out >>= 1;
		/* bitwise tricks */
		out ^= poly & (-lsb);
	}
	return out;
}

unsigned int crc32_do(unsigned char *data, unsigned long size)
{
	unsigned int chksum = ~0u, p, l;

	for(unsigned long i = 0; i < size; i++) {
		l = data[i] ^ (chksum & 255);
		p = crc32_poly(l);
		chksum = p ^ (chksum >> 8);
	}

	return ~chksum;
}

int strlen(char *n);

int main()
{
	char *str = "Hello, World!\n";
	int len = strlen(str);
	unsigned int chksum = crc32_do((unsigned char *)str, len);
	print_num_unsignd(chksum);
	return chksum;
}
