typedef unsigned char uint8_t;

void *memcpy(void *dst, void *src, unsigned long n);
int strcmp(char *s1, char *s2);

void decode(uint8_t *msg_, int len_)
{
	/* to make optimizer work better */
	uint8_t *msg = msg_;
	int len = len_;

	uint8_t tab[256];
	for(int i = 0; i < 256; i++) {
		tab[i] = i;
	}

	for(int i = 0; i < len; i++) {
		uint8_t rank = msg[i];
		uint8_t sym = tab[rank];
		uint8_t j = rank;
		while(j) {
			tab[j] = tab[j - 1];
			j--;
		}
		tab[0] = sym;
		msg[i] = sym;
	}
	return;
}

int puts(char *s);

int main()
{
	char *encoded = "Hel\0o0%Z\x03r\x05h)\00";
	char *decoded = "Hello, World!";
	char work[14];
	work[13] = 0;
	memcpy(work, encoded, sizeof work);
	decode((uint8_t *)work, 13);
	puts(work);
	return strcmp(decoded, work) == 0;
}
