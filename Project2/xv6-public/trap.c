#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;
int mlfq_time;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
	cprintf("error state process kstack(%d) parentPid(%d) state(%d)\n",myproc()->kstack,  myproc()->parent->pid, myproc()->state);
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER) {
	  //cprintf("Timer Interrupt !\n");
	  struct proc* p = myproc();
	  while(p->isLWP) {
		  p = p->parent;
	  }
	int my_priority = p->priority;
	p->rtime++;
	if(my_priority != 3 && p->tickets == 0) {
		inc_total_time(p);
		//inc_mlfq_time();
		//cprintf("pid (%d) tid (%d) isLWP? (%d) LWPcnt ? (%d) priority (%d) ttime (%d) rtime (%d)\n",p->pid, myproc()->tid, p->isLWP, p->LWPcnt, my_priority,p->ttime, p->rtime);
		if(my_priority == 0 && p->rtime >= 5) {
		  //cprintf("P0 normal yield called pid(%d)\n",my_priority);
		  yield();
		} else if(my_priority == 1 && p->rtime >= 10) {
		  //cprintf("P1 normal yield called pid(%d)\n",my_priority);
		  yield();
		} else if(my_priority == 2 && p->rtime >= 20) {
		  //cprintf("P2 normal yield called pid(%d)\n",my_priority);
		  yield();
		} else if(p->LWPcnt != 0 || myproc()->isLWP) {
		  //cprintf("lwp_yield called pid(%d)\n",my_priority);
		  lwp_yield();
		}
	} else {
		//cprintf("process pid (%d) tid (%d) state(%d)\n",myproc()->pid,myproc()->tid, myproc()->state);
		if(p->rtime >= 5) {
			//cprintf("p->rtime (%d) :: yield calls\n", p->rtime);
			yield();
		} else {
			//cprintf("p->rtime (%d) :: lwp_yield calls\n", p->rtime);
			lwp_yield();
		}
	}
  }

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
	  exit();
}
