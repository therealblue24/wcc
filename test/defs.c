#define X a
#undef X
#define X b

#define INDIRECT Z
#define Z Y
#define Y ORIGINAL_X
#define ORIGINAL_X b
#undef ORIGINAL_X
#define ORIGINAL_X a

int main()
{
	int a = 5;
	int b = 2 + __wcc__;
	return X + INDIRECT;
}
