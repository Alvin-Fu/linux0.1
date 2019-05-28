#include <errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/segment.h>

/* linux/kernel/chr_drv/lp.c c.1991-1992 James Wiegand */
/* caveat: my machine only has 1 printer @ lpt2 so lpt1 & lpt3 are 
   implemented but UNTESTED */

/* i suppose this should go into some sort of header file :-0 */
#define LP_EXIST 0x0001
#define LP_SELEC 0x0002
#define LP_BUSY	 0x0004
#define LP_OFFL	 0x0008
#define LP_NOPA  0x0010

#define NO_LPS 3
#define LP_TIMEOUT 2000
/* 
really brain-dead. for now. 
since we are dealing with a horribly slow device
I don't see the need for a queue
*/
struct lp_struct
       	{
	int base;
	int flags;
	/* needed for busy determination */
	struct task_struct *lp_task;
	};	

/* 
the BIOS manuals say there can be up to 4 lpt devices
but I have not seen a board where the 4th address is listed
*/
   
struct lp_struct lp_table[] = {
				{ 0x3bc, 0, },
				{ 0x378, 0, },
				{ 0x278, 0, }
			      }; 

/* 
bit defines for 8255 status port
 status port
base + 1
*/
#define BUSY	0x80 /* active low */
#define ACK     0x40 /* active low */
#define OUTPA	0x20
#define SELECD	0x10
#define ERRORP	0x08 /*å active low*/
#define IRQ	0x04 /* active low */

/* 
   defines for 8255 control port
   base + 2 
*/
#define IRQEN	0x10
#define SELECP	0x08
#define INITP	0x04  /* active low */
#define AUTOLF	0x02
#define STROBE	0x01

/* 
  the value written to ports to test existence. PC-style ports will 
   return the value written. AT-style ports will return 0. so why not
   make them the same ? 
*/
#define DUMMY	0x0

/* this is the port delay time. your mileage may vary */
#define DELAY 	50000

void lp_init ( void )
{
int offset = 0;
unsigned int testvalue = 0;

	/* take on all known port values */
	for( offset = 0; offset < NO_LPS; offset ++ )
	  {
	  /* write to port & read back to check */
	  outb( lp_table[ offset ].base, DUMMY );
	  for( testvalue = 0; testvalue < DELAY; testvalue ++ );
	  testvalue = inb( lp_table[ offset ].base );
	  /* port exists? fill in table */
	  if( testvalue == DUMMY || testvalue == 0 )
	    {
	    lp_table[ offset ].flags |= LP_EXIST;
	    outb(( 0 ), ( lp_table[ offset ].base + 2 ));
	    for( testvalue = 0; testvalue < DELAY; testvalue ++ );
	    outb(( SELECP | INITP ),  ( lp_table[ offset ].base + 2 ));
	    printk( "lp_init: lp%d exists\n", offset );
	    }
	  }	
}

int lp_char( char lpchar, int minor )
{
int retval = 0;
int count  = 0; 

	/* 
	   send char to lpt1
	   pr_table[ 0 ] 
	*/
	outb( lpchar, lp_table[ minor ].base );

	/* wait for ready */
	do
		{
		/* pr_table[ 0 ] + 1 */
		retval = inb(( lp_table[ minor ].base + 1 ));
		/* give up time to the kernel - i think */
		/* this loop reeaaaalllly goes to Helllll when the print
		   buffer gets filled up */
		schedule();
		count ++;
		}
	while( !( retval & BUSY ) && count < LP_TIMEOUT );

	/* control port pr_table[ 0 ] + 2
	   take strobe high */
	outb(( SELECP | INITP | STROBE ), ( lp_table[ minor ].base + 2 ));
	/* take strobe low */
	outb(( SELECP | INITP ), ( lp_table[ minor ].base + 2 ));


	/* get something meaningful for return value */
	retval = inb((lp_table[ minor ].base + 1 ));

	return retval;
}

int lp_write( unsigned minor, char *buf, int count )
{
int  retval;
int  loop;
char c, *temp = buf;

	/* check for lp existence */
	/* check for valid no */
	if( minor > NO_LPS - 1 )
		return -ENODEV;
	/* check lp table */
	if(( lp_table[ minor ].flags & LP_EXIST ) == 0 )
		return -ENODEV;

	/* check for process in use */
	/* this is pretty crappy but the spaces were for debugging */
	if(( lp_table[ minor ].flags & LP_BUSY ))
	  {
	  if( lp_table[ minor ].lp_task->state == 3 )
	    {
	    }
	  else
	    if( lp_table[ minor ].lp_task->pid == 0 )
	      {
	      }
	    else
	      if( lp_table[ minor ].lp_task-> pid == current->pid )
		{
		}
	      else
	      	return -EBUSY;
	  }

	/* ALWAYS reset the task when not busy
	   otherwise we would never wake up    */
	lp_table[ minor ].lp_task = current;
	lp_table[ minor ].flags |= LP_BUSY;

	temp = buf;

	for( loop = 0; loop < count; loop ++, temp ++ )
		{
		/* took me a while to figure this out */
		c = get_fs_byte( temp );
		retval = lp_char( c, minor );
		/* the required out-of-paper notice */
		if( retval & OUTPA )
			{
			return -ENOSPC;
			}
		/* offline fault */
		if( !( retval & SELECD ))
			{
			return -EFAULT;
			}
		}

	return count;
}

