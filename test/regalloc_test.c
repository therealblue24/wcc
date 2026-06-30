#include "test/one.c"

int main()
{
	int a = one(), b = one(), c = one(), d = one(), e = one(), f = one(),
		g = one(), h = one(), i = one(), j = one(), k = one(), l = one(),
		m = one(), n = one(), o = one(), p = one();
	a = a + b;
	c = c + d;
	e = e + f;
	g = g + h;
	i = i + j;
	k = k + l;
	m = m + n;
	o = o + p;
	a = a + c;
	e = e + g;
	i = i + k;
	m = m + o;
	a = a + e;
	i = i + m;
	a = a + i;
	return a;
}
