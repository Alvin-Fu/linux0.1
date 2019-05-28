#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#define TAB_FILE "/etc/mtab"
#define LOCK_FILE "/etc/mtab~"

#define CHECK 0
#define WRITE 1
#define NOWRITE 2

int tab = -1;
int lock = -1;

static int read_char(void)
{
	static char buffer[1024];
	static int i=0;
	static int pos=0;

	if (tab < 0)
		return -1;
	if (pos == i) {
		i = read(tab,buffer,1024);
		pos = 0;
	}
	if (i <= 0)
		return -1;
	return (unsigned) buffer[pos++];
}

static void write_char(int c)
{
	static char buffer[1024];
	static int i=0;

	if (c<0) {
		if (i)
			write(lock,buffer,i);
		i = 0;
		return;
	}
	buffer[i++] = c;
	if (i<1024)
		return;
	write(lock,buffer,i);
	i=0;
}

volatile void die(void)
{
	if (lock >= 0)
		unlink(LOCK_FILE);
	exit(1);
}

main(int argc, char ** argv)
{
	int c,i,len;
	int state;

	if (argc != 2) {
		fprintf(stderr,"umount: usage: umount dev\n");
		die();
	}
	lock = open(LOCK_FILE,O_CREAT | O_EXCL,0600);
	if (lock < 0) {
		fprintf(stderr,"umount: unable to open lock-file\n");
		die();
	}
	tab = open(TAB_FILE,O_RDONLY);
	if (umount(argv[1])) {
		fprintf(stderr,"umount: error %d\n",errno);
		die();
	}
	len = strlen(argv[1]);
	state = CHECK;
	i = 0;
	do {
		c = read_char();
		if (state==CHECK && !argv[1][i] && (c==' ' || c=='\n'))
			state=NOWRITE;
		if (c=='\n') {
			if (state != NOWRITE)
				write_char(c);
			state = CHECK;
			i = 0;
			continue;
		}
		if (state == NOWRITE)
			continue;
		if (state == WRITE) {
			write_char(c);
			continue;
		}
		if (c == argv[1][i])
			i++;
		else {
			for (state=0 ; state<i ;state++)
				write_char((unsigned)argv[1][state]);
			state = WRITE;
			i = 0;
			write_char(c);
		}
	} while (c >= 0);
	write_char(-1);
	unlink(TAB_FILE);
	link(LOCK_FILE,TAB_FILE);
	unlink(LOCK_FILE);
	return 0;
}
