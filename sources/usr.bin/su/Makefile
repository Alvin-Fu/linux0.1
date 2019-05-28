AS	=gas
LD	=gld
CC	=gcc
CFLAGS	=-Wall -O -fstrength-reduce -fomit-frame-pointer \
-fcombine-regs -mstring-insns -I/usr/include
CPP	=cpp -nostdinc -I/usr/include
SU = su.o putenv.o
TARGET = su

.c.o:
	$(CC) $(CFLAGS) \
	-nostdinc -c -o $*.o $<

all: $(TARGET)

su: $(SU)
	$(CC) $(SU) -o su

objects: $(SU)
	sync



install: su
	mv -v su  /bin

	
clean:
	rm -f *.o tmp_make

backup: clean
	(cd .. ; tar cf - su | compress - > linuxsu.tar.Z)
	sync

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c;do echo -n "./";$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile

### Dependencies:
./putenv.o : putenv.c /usr/include/sys/types.h /usr/include/errno.h \
  /usr/include/string.h 
./su.o : su.c /usr/include/limits.h /usr/include/getopt.h \
  /usr/include/sys/time.h /usr/include/time.h /usr/include/stdio.h \
  /usr/include/pwd.h /usr/include/ansidecl.h /usr/include/sys/types.h \
  /usr/include/grp.h /usr/include/stddef.h /usr/include/string.h \
  /usr/include/unistd.h /usr/include/sys/stat.h /usr/include/sys/times.h \
  /usr/include/sys/utsname.h /usr/include/utime.h 
