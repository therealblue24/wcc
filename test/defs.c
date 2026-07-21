#define X a
#define X b

#define INDIRECT Z
#define Z Y
#define Y ORIGINAL_X
#define ORIGINAL_X a

int main()
{
	int a = 5;
	int b = 3;
	return X + INDIRECT;
}
