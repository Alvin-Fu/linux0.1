#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#define TAB_FILE "/etc/mtab"
#define LOCK_FILE "/etc/mtab~"

int tab = -1;
int lock = -1;

volatile void die(void)
{
	if (lock >= 0)
		unlink(LOCK_FILE);
	exit(1);
}

main(int argc, char ** argv)
{
	int i;
	char buffer[1024];

	if (argc !=3) {
		fprintf(stderr,"mount: usage: mount dev dir\n");
		die();
	}
	lock = open(LOCK_FILE,O_CREAT | O_EXCL,0644);
	if (lock < 0) {
		fprintf(stderr,"mount: unable to open lock-file\n");
		die();
	}
	chmod(LOCK_FILE,0644);
	tab = open(TAB_FILE,O_RDONLY);
	if (mount(argv[1],argv[2],0)) {
		fprintf(stderr,"mount: error %d\n",errno);
		die();
	}
	if (tab >= 0)
		while ((i=read(tab,buffer,1024))>0)
			write(lock,buffer,i);
	i=strlen(argv[1]);
	argv[1][i]=' ';
	write(lock,argv[1],i+1);
	i=strlen(argv[2]);
	argv[2][i]='\n';
	write(lock,argv[2],i+1);
	unlink(TAB_FILE);
	link(LOCK_FILE,TAB_FILE);
	unlink(LOCK_FILE);
	return 0;
}
