	.file	"signal.c"
gcc_compiled.:
.text
	.align 2
.globl _signal
_signal:
	pushl %ebx
	movl $48,%eax
	movl 8(%esp),%ebx
	movl 12(%esp),%ecx
	movl $____sig_restore,%edx
/APP
	int $0x80
/NO_APP
	popl %ebx
	ret
