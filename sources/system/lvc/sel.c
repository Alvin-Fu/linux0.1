/* This file contains the procedures for the handling of select
 *
 * The entry points into this file are
 *   do_select:	perform the SELECT system call
 */

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <asm/segment.h>
#include <asm/system.h>
#include <sys/types.h>
#include <string.h>
#include <const.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#define ALRMMASK (1 << (SIGALRM-1))

extern void multi_interruptible_sleep_on(struct task_struct **p[], int np);

static int multi_sleep_if_empty(struct task_struct **ts[],int nr)
{ int n = 1;
  cli();
  multi_interruptible_sleep_on(ts,nr);
  if (current->signal & ALRMMASK)
  { current->signal &= ~ALRMMASK;
    n = 0;
  }
  sti();
  return(n);
}

static int tty_select(unsigned channel, int *oper)
{
	int ready = 0;
	struct tty_struct * tty = TTY_TABLE(channel);
	if (!tty || !oper) return -EBADF;
	if (*oper & SEL_IN) 
	  if (tty->ttynum<FIRST_PTY_MASTER_DEV)
	    if (EMPTY(tty->secondary))
	      *oper &= ~SEL_IN;
	    else 
	      ready++;
	  else
	  { struct tty_struct *other;
	    if (tty->ttynum>=FIRST_PTY_SLAVE_DEV)
	      other = TTY_TABLE(tty->ttynum-64);
	    else
	      other = TTY_TABLE(tty->ttynum+64);
	    if  (EMPTY(tty->secondary) && EMPTY(tty->read_q) && 
	      EMPTY(other->write_q))
	      *oper &= ~SEL_IN;
	    else 
	      ready++;
	  }    

	if (*oper & SEL_OUT) 
	  if (FULL(tty->write_q))
	    *oper &= ~SEL_OUT;
	  else
	    ready++;
	*oper &= ~SEL_EX;
	return(ready);
}

/* struct file *find_filp( struct m_inode* rip, int bits )
{
  int i;
  for (i=0; i<NR_OPEN; i++ )
    if (current->filp[i] && (current->filp[i]->f_count != 0) &&
       (current->filp[i]->f_inode == rip) && (current->filp[i]->f_mode & bits))
      return(current->filp[i]);
  return(NIL_FILP);
}*/

  
static int pipe_select( struct m_inode *rip, int *oper)	
{
  int ready = 0;
  if (*oper & SEL_IN)
	if (PIPE_EMPTY(*rip))
		*oper &= ~SEL_IN; /* a read call would block */
	else
	  ready++;
  if (*oper & SEL_OUT)
	if (PIPE_FULL(*rip))
		*oper &= ~SEL_OUT; /* a write call of at least 2 bytes would block */
	else
	  ready++;
  *oper &= ~SEL_EX; /* no exception */
  return(ready);
}


int sys_select( unsigned long *buffer )
{
/* Perform the select(nd, in, out, ex, tv) system call. */
int nd;fd_set *inp; fd_set *outp; fd_set *exp; struct timeval *tvp;
fd_set in, out, ex;
fd_set insave, outsave, exsave;
struct timeval tv;
unsigned long time;
fd_mask mask,  *ip, *op, *ep;
struct file *filptr;
struct m_inode *inoptr;
unsigned short i_mode;
dev_t dev;
int oper, cum = 0, min, tty_cnt, setsize, ready = 0, ready_normal = 0;
int i, num;
char *cp;
struct task_struct **ts[NR_OPEN];
long newalarm, oldalarm;

  nd = (int)get_fs_long(buffer++);
  inp = (fd_set *)get_fs_long(buffer++);
  outp = (fd_set *)get_fs_long(buffer++);
  exp = (fd_set *)get_fs_long(buffer++);
  tvp = (struct timeval*)get_fs_long(buffer);
  if (cp = (char *)tvp)
  { for (i=0; i<sizeof(tv); i++)
      ((int *)&tv)[i] = get_fs_byte(cp+i);
    time = tv.tv_sec * HZ + (tv.tv_usec * HZ) / 1000000;
#define MAX_P_LONG (0xFFFFFFFFL - jiffies)
    if (time >= MAX_P_LONG) time = MAX_P_LONG - 1L;
  } else time = MAX_P_LONG; /* indefinite timeout */
  newalarm = jiffies+time;
  setsize = ((nd+7)>>3);
  if (cp = (char *)inp)
    for (i=0; i<setsize; i++)
      ((char *)&insave)[i] = get_fs_byte(cp+i);
  else
    FD_ZERO(&insave);
  if (cp = (char *)outp)
    for (i=0; i<setsize; i++)
      ((char *)&outsave)[i] = get_fs_byte(cp+i);
  else
    FD_ZERO(&outsave);
  if (cp = (char *)exp)
    for (i=0; i<setsize; i++)
      ((char *)&exsave)[i] = get_fs_byte(cp+i);
  else
    FD_ZERO(&exsave);

 check_fds:
  in = insave;  out = outsave; ex = exsave;  
  /*current->fp_sel_tty = -1;*/	/* assume: no /dev/tty */
  ip = (fd_mask *)&in; op = (fd_mask *)&out; ep = (fd_mask *)&ex;
  tty_cnt = 0;
  mask = 1;
  for (i = 0; i < nd; i++) {
    oper  = (*ip & mask)? SEL_IN : 0;
    if (*op & mask) oper += SEL_OUT;
    if (*ep & mask) oper += SEL_EX;
    
    if (oper) { /* test this fd */
      if ((filptr = current->filp[i]) == NIL_FILP) {
        cum = -EBADF;
        break;
      }  
      inoptr = filptr->f_inode;
      /*inoptr->i_sel_proc = who;
      inoptr->i_sel_fd = i; */
      i_mode = inoptr->i_mode & I_TYPE;
      if (inoptr->i_pipe) {
        ready += pipe_select(inoptr, &oper );
        if ((oper & SEL_IN ) == 0) *ip &= ~mask; /* reset in flag */
        if ((oper & SEL_OUT) == 0) *op &= ~mask; /* reset out flag */
        if ((oper & SEL_EX ) == 0) *ep &= ~mask; /* reset ex flag */
	if (!oper) ts[tty_cnt++] = &(inoptr->i_wait);
      } else if (i_mode == I_CHAR_SPECIAL) {
        dev = (dev_t) inoptr->i_zone[0];
        switch (MAJOR(dev)) {
          case 5: /* /dev/tty */
            dev = current->tty; /* fall into */
            /*current->fp_sel_tty = i; */
          case 4: /* /dev/ttyXYZ */
            if (dev) {
              min = MINOR(dev);
              if ((num = tty_select(min, &oper)) >= 0)
                ready += num;
              else
                cum = num;
              if ((oper & SEL_IN ) == 0) 
              { *ip &= ~mask; /* reset in flag */
                ts[tty_cnt++] = &(TTY_TABLE(min)->secondary.proc_list);
              }
              if ((oper & SEL_OUT) == 0) 
              { *op &= ~mask; /* reset out flag */
                ts[tty_cnt++] = &(TTY_TABLE(min)->write_q.proc_list);
              }  
              if ((oper & SEL_EX ) == 0) *ep &= ~mask; /* reset ex flag */
            } else { /* /dev/tty = /dev/null */
              *ep &= ~mask;
            }
            break;
          default :
            *ep &= ~mask;
        }
      } else { /* normal file or block special */
        /* default : read, write OK, no exception */
        if (*ip & SEL_IN)
          ready_normal++;
        if (*op & SEL_OUT)
          ready_normal++;
        *ep &= ~mask;
      }
    }
    
    mask <<= 1;
    if (mask == 0) {
      mask = 1;
      ip++; op++; ep++;
    }
  } /* for */
   
  if ((!cum) && (!ready) && tty_cnt && time)
  { /* set an alarm and sleep/wait */
      int result;
      oldalarm = current->alarm;
      if (!oldalarm || newalarm<oldalarm)
        current->alarm = newalarm;
      result = multi_sleep_if_empty(ts,tty_cnt);
      current->alarm = oldalarm;
      /*printk("result=%d, newalarm=%d, jiffies=%d\n",result,newalarm,jiffies);*/
      if (!result)
        return(0);
      if (newalarm>jiffies)
        goto check_fds;
      else 
	return(0);
  }

  ready += ready_normal;
  if (!ready || cum)
  { return(cum);
  }
    /* remove select marks */
/*    for (inoptr = inode; inoptr < &inode[NR_INODES]; inoptr++)
      if (inoptr->i_sel_proc == who) inoptr->i_sel_proc = 0; */
  if (inp)
    for (i=0; i<setsize; i++)
      put_fs_byte(((char*)&in)[i],((char *)inp)+i);
  if (outp)
    for (i=0; i<setsize; i++)
      put_fs_byte(((char*)&out)[i],((char*)outp)+i);
  if (exp)
    for (i=0; i<setsize; i++)
      put_fs_byte(((char*)&ex)[i],((char*)exp)+i);
    
  return(ready);
}

