#define __LIBRARY__
#include <time.h>
#include <unistd.h>

int select( int nd, fd_set * in, fd_set * out, fd_set * ex, 
  struct timeval * tv)
{ unsigned long buffer[5];
  buffer[0] = (unsigned long)nd;
  buffer[1] = (unsigned long)in;
  buffer[2] = (unsigned long)out;
  buffer[3] = (unsigned long)ex;
  buffer[4] = (unsigned long)tv;
  {  
  long __res;  
  __asm__ volatile ("int $0x80"  
	  : "=a" (__res)  
	  : "0" (__NR_select),"b" ((long)(buffer)));  
  if (__res >= 0)  
	  return (int) __res;  
  errno = -__res;  
  return -1;  
  }
}


