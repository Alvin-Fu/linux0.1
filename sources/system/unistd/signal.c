#define __LIBRARY__
#include <unistd.h>
#include <signal.h>

extern void ___sig_restore();
extern void ___masksig_restore();

int ___ssetmask(int mask)
{
	long res;
	__asm__("int $0x80":"=a" (res)
		:"0" (__NR_ssetmask),"b" (mask));
	return res;
}

int ___sgetmask(void)
{
	long res;
	__asm__("int $0x80":"=a" (res)
		:"0" (__NR_sgetmask));
	return res;
}

void (*signal(int sig, void (*func)(int)))(int)
{
	void (*res)();

	__asm__("int $0x80":"=a" (res):
	"0" (__NR_signal),"b" (sig),"c" (func),"d" ((long)___sig_restore));
	return res;
}

int sigaction(int sig,struct sigaction * sa, struct sigaction * old)
{
	if (sa->sa_flags & SA_NOMASK)
		sa->sa_restorer=___sig_restore;
	else
		sa->sa_restorer=___masksig_restore;
	__asm__("int $0x80":"=a" (sig)
		:"0" (__NR_sigaction),"b" (sig),"c" (sa),"d" (old));
	if (sig>=0)
		return sig;
	errno = -sig;
	return -1;
}
