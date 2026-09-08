#include "test/one.c"

struct __attribute__((packed)) baz {
	long b;
	short c[16];
};

int main()
{
	struct foo {
		int a;
	};
	struct bar {
		char x[5];
	};

	struct foo foo;
	struct bar bar;
	struct baz baz;

	foo.a = 1;
	for(int i = 0; i < 5; i++) {
		bar.x[i] = i + 1;
	}

	baz.b = 16;
	for(int i = 0; i < 16; i++) {
		baz.c[i] = i * i;
	}
	short total = 1240;
	for(int i = 0; i < 16; i++) {
		total -= baz.c[i];
	}

	int sum = (int)total; /* should be 0 */
	sum += foo.a;
	for(int i = 0; i < 5; i++) {
		sum += bar.x[i];
	}

	return sum + sizeof(struct baz) - _Alignof(struct baz); /* should be 55 */
}
