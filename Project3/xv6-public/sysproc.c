#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getppid(void)
{
  return getppid();
}


int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;
  struct proc* p = myproc();

  if(argint(0, &n) < 0)
    return -1;
  while(p->isLWP) {
	  p = p->parent;
  }
  addr = p->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  //cprintf("sleep time %d, %d\n", ticks, ticks - ticks0);
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


int
sys_yield(void)
{
  yield2();
  return 0;
}

int 
sys_getlev(void) 
{
  return getlev();
}
int
sys_set_cpu_share(void) 
{
  int ticket;
  if(argint(0, &ticket) < 0)
    return -1;
  if(ticket + cpu_occupy > 80) {
	  cprintf("CPU OCCUPY OVER ERROR\nWill act like normal MLFQ process\n");
	return -1;  // unable to use Stride Scheduling  
  }
  cpu_occupy += ticket;
  return set_cpu_share(ticket);
}
int
sys_thread_create(void)
{
	int thread, start_routine, arg;
	if(argint(0, &thread) < 0)
		return -1;
	if(argint(1, &start_routine) < 0)
		return -1;
	if(argint(2, &arg) < 0)
		return -1;
	thread_create((thread_t *)thread, (void *)start_routine, (void *)arg);
	return 0;
}
int
sys_thread_join(void)
{
	int thread, ret_val;
	if(argint(0, &thread) < 0)
		return -1;
	if(argint(1, &ret_val) < 0)
		return -1;

	thread_join((thread_t)thread, (void **)ret_val);
	return 0;
}
int
sys_thread_exit(void)
{
	int ret_val;
	if(argint(0, &ret_val) < 0) {
		return -1;
	}
	
	thread_exit((void *)ret_val);
	return 0;
}

int
sys_lwp_yield(void) 
{
  lwp_yield();
  return 0;
}

int
sys_gettid(void) 
{
	return myproc()->tid;
}
