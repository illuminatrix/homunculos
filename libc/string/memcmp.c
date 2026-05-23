#include <string.h>

int
memcmp( const void * a_ptr, const void * b_ptr, size_t len )
{
	const unsigned char *a = (const unsigned char *)a_ptr;
	const unsigned char *b = (const unsigned char *)b_ptr;
	size_t i;

	for (i = 0; i < len; i++) {
		if (a[i] != b[i])
			return a[i] < b[i] ? -1 : 1;
	}
	return 0;
}
