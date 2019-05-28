#define __LIBRARY__
#include <unistd.h>

extern long _end;

static void * ___brk_addr = &_end;

int brk(void * end_data_seg)
{
	__asm__ volatile ("int $0x80"
		:"=a" (___brk_addr)
		:"0" (__NR_brk),"b" (end_data_seg));
	return -(___brk_addr != end_data_seg);
}

void * sbrk(ptrdiff_t increment)
{
	void * tmp = ___brk_addr+increment;

	__asm__ volatile ("int $0x80"
		:"=a" (___brk_addr)
		:"0" (__NR_brk),"b" (tmp));
	return (___brk_addr != tmp) ? ((void *) -1):(tmp-increment);
}
