/* not meant to be run as a test, only to inspect code generated */

typedef unsigned long uint64_t;
typedef unsigned int uint32_t;

/* TODO: yeah make a proper memopt pass with function args */
uint64_t rol64(uint64_t x_, uint64_t c_)
{
	uint64_t x = x_, c = c_;
	return (x << c) | (x >> (64 - c));
}

uint64_t ror64(uint64_t x_, uint64_t c_)
{
	uint64_t x = x_, c = c_;
	return (x >> c) | (x << (64 - c));
}

uint32_t rol32(uint32_t x_, uint32_t c_)
{
	uint32_t x = x_, c = c_;
	return (x << c) | (x >> (32 - c));
}

uint32_t ror32(uint32_t x_, uint32_t c_)
{
	uint32_t x = x_, c = c_;
	return (x >> c) | (x << (32 - c));
}
