#define C char

int main()
{
	C a[1 + 1]; // 2
	C b[5 - 2]; // 3
	C c[3 * 5]; // 15
	C d[6 / 2]; // 3
	C e[7 % 2]; // 1
	C f[2]; // 2
	C g[-(-3)]; // 3
	C h[~(-3)]; // 2
	C i[!0]; // 1
	C j[1 << 2]; // 4
	C k[5 >> 1]; // 2
	C l[3 & 2]; // 2
	C m[3 ^ 2]; // 1
	C n[3 | 2]; // 3
	C o[3 || 2]; // 1
	C p[3 && 2]; // 1
	C q[3 == 2]; // 0
	C r[3 != 2]; // 1
	C s[3 < 2]; // 0
	C t[3 <= 2]; // 0
	C u[3 > 2]; // 1
	C v[3 >= 2]; // 1
	return sizeof a + sizeof b + sizeof c + sizeof d + sizeof e + sizeof f +
		   sizeof g + sizeof h + sizeof i + sizeof j + sizeof k + sizeof l +
		   sizeof m + sizeof n + sizeof o + sizeof p + sizeof q + sizeof r +
		   sizeof s + sizeof t + sizeof u + sizeof v;
}
