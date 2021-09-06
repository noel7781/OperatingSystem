#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

int BIG_NUMBER = 1e4;

int cpu_occupy = 0;

int mlfq_time = 0;

int priority_cnt[4] = {0, 0, 0, 0};

int g_tid = 0;

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct Data{
	struct proc* proc;
	int priority;
	int move;
	int tickets;
	int stride;
};

struct Queue{
	struct Data Data[NPROC];
	int size;
};

void InitQueue();

struct proc base_proc;
struct Data base_data;
struct Queue g_process_queue;

void printProc() {
	struct proc* p; 
	
	int i = 0;
	cprintf("printProc Start\n");
  for(i = 0; i < 4; ++i) {
    cprintf("Priority (%d) cnt : (%d)\n", i, priority_cnt[i]);
  }
	for(p = ptable.proc; p < &ptable.proc[NPROC]; ++p) {
		if(p->state != 0)
			cprintf("(%d) : p pid (%d) tid (%d) state (%d) priority (%d) isLWP (%d) killed(%d) parentPid (%d)\n", (int)(p- ptable.proc), p->pid, p->tid, p->state, p->priority, p->isLWP, p->killed, p->parent->pid);
	}
	cprintf("printProc End\n");
}

void printQueue() {
	for(int i = 0;i < g_process_queue.size; ++i) {
		cprintf("%d pid : (%d) pass : (%d) stride : (%d) priority : (%d)\n", i,g_process_queue.Data[i].proc->pid, g_process_queue.Data[i].move,g_process_queue.Data[i].stride, g_process_queue.Data[i].priority);

	}
	cprintf("\n");
}

void InitQueue() {
	base_proc.priority = 0;
	base_proc.tickets = 100;
	base_data.proc = &base_proc;
	base_data.priority = 0;
	base_data.move = 0;
	base_data.tickets = 100;
	g_process_queue.Data[0] = base_data;
	g_process_queue.size = 1;
}

void swap(struct Data* p1, struct Data* p2) {
	struct Data tmp;
	tmp = *p1;
	*p1 = *p2;
	*p2 = tmp;
}

void heapify(int index, int max_size){
	int parent = index;
	int tmp = parent;
	while(tmp <= (max_size - 1) / 2){
		int p_run = g_process_queue.Data[tmp].move;
		int l_run = g_process_queue.Data[tmp * 2 + 1].move;
		int r_run = -1;
		if(tmp * 2 + 2 <= max_size) r_run = g_process_queue.Data[tmp * 2 + 2].move;
		if(r_run == -1){
			if(p_run > l_run){
				tmp = tmp * 2 + 1;
			} else {
				break;
			}	
		} else {
			if(p_run > l_run && p_run > r_run) {
				if(l_run > r_run) {
					tmp = tmp * 2 + 2;
				} else {
					tmp = tmp * 2 + 1;
				}
			} else if(p_run > l_run) {
				tmp = tmp * 2 + 1;
			} else if(p_run > r_run) {
				tmp = tmp * 2 + 2;
			} else {
				break;
			}
		}
	}
	swap(&g_process_queue.Data[parent], &g_process_queue.Data[tmp]);
}

void MakeHeap(int max_size){
	if(max_size == 0) return;
	int start = (max_size - 1) / 2;
	for(int i = start; i >= 0; --i){
		heapify(i, max_size);
	}
}

void PushQueue(struct proc* p) {
	struct Data d1;
	d1.proc = p;
	d1.priority = 3;
	d1.move = g_process_queue.Data[0].move;
	d1.tickets = p->tickets;
	d1.stride = BIG_NUMBER / p->tickets;
	g_process_queue.Data[g_process_queue.size++] = d1;
	for(int i = 0; i < g_process_queue.size; ++i) {
		if(g_process_queue.Data[i].priority == 0) {
			g_process_queue.Data[i].tickets = 100 - cpu_occupy;
			g_process_queue.Data[i].stride = BIG_NUMBER / g_process_queue.Data[i].tickets;
			break;
		}
	}
	MakeHeap(g_process_queue.size - 1);
}

struct proc* ChoicePriority(int* priority) {
	MakeHeap(g_process_queue.size - 1);
	struct proc* ret_proc = g_process_queue.Data[0].proc;
	int select = g_process_queue.Data[0].priority;
	if(select != 3) {
		*priority = 0;
	} else {
		*priority = 3;
	}
	return ret_proc;
}

static struct proc *initproc;

int nextpid = 1;


extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;

  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->isLWP = 0;
  p->tid = -1;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

//
//
  p->priority = 0;
  p->ttime = 0;
  p->isLWP = 0;
  priority_cnt[0]++;
  p->tickets = 0;
  InitQueue();
//
//

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();
  struct proc* pproc = curproc;
  while(pproc->isLWP) {
	  pproc = pproc->parent;
  }
  acquire(&ptable.lock);
  sz = pproc->sz;
  if(n > 0){
	if((sz = allocuvm(pproc->pgdir, sz,  sz + n)) == 0) {
	  release(&ptable.lock);
	  return -1;
	}
  } else if(n < 0){
	if((sz = deallocuvm(pproc->pgdir, sz, sz + n)) == 0) {
	  release(&ptable.lock);
	  return -1;
	}
  }
  pproc->sz = sz;
  release(&ptable.lock);
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if(!curproc->isLWP) {
	  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
		kfree(np->kstack);
		np->kstack = 0;
		np->state = UNUSED;
		return -1;
	  }
  } else {
	  if((np->pgdir = thread_copyuvm(curproc->pgdir, curproc->sz)) == 0) {
		  kfree(np->kstack);
		  np->kstack = 0;
		  np->state = UNUSED;
		  return -1;
	  }
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

//
//
  np->priority = 0;
  np->isLWP = 0;
  priority_cnt[0]++;
  np->ttime = 0;
  np->tickets = 0;
  np->base = np->sz;
//
//
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  //cprintf("Exit Call : curproc pid(%d) tid(%d) state(%d) iskilled(%d) is calling exit\n", curproc->pid, curproc->tid, curproc->state, curproc->killed);
  struct proc *p;
  int fd;
  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }
  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);
  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);
  //cprintf("after wakeup, pcnt[0] (%d)\n",priority_cnt[0]);

  if(curproc->isLWP == 0) {
  // Pass abandoned children to init.
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
	  if(p->parent == curproc && p->pid != curproc->pid){
		p->parent = initproc;
		if(p->state == ZOMBIE) {
		  wakeup1(initproc);
		}
	  } else if(p->parent == curproc && p->pid == curproc->pid) {
		  p->killed = 1;
		  p->state = EMBRYO;
	  }
	}
	cleanup_rest();

	// Jump into the scheduler, never to return.
	curproc->state = ZOMBIE;
	priority_cnt[curproc->priority]--;
  //cprintf("after being zombie, pcnt[0] (%d)\n",priority_cnt[0]);
	cpu_occupy -= curproc->tickets;
	curproc->tickets = 0;
	
	if(curproc->priority == 3) {
		g_process_queue.Data[0].move = 0;
	  swap(&g_process_queue.Data[0], &g_process_queue.Data[g_process_queue.size - 1]);
	  g_process_queue.size--;
	  MakeHeap(g_process_queue.size - 1);
	  for(int i = 0; i < g_process_queue.size; ++i) {
		if(g_process_queue.Data[i].priority == 0) {
		  g_process_queue.Data[i].tickets = 100 - cpu_occupy;
		  g_process_queue.Data[i].stride = BIG_NUMBER / g_process_queue.Data[i].tickets;
		  break;
		}
	  }
	}
	curproc->priority = -1;
	
	//printProc();
  } else { // curproc is LWP
	  //cprintf("LWP pid(%d) tid(%d) state(%d) iskilled(%d) is calling exit\n", curproc->pid, curproc->tid, curproc->state, curproc->killed);
	  for(struct proc* p = ptable.proc; p < &ptable.proc[NPROC]; ++p) {
		  if(p->isLWP && (p->pid == curproc->pid)) {
			  p->killed = 1;
			  p->state = EMBRYO;
		  }
	  }
	  curproc->parent->killed = 1;
  }
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  //printProc();
  //cprintf("(%d) process calling wait\n", curproc->pid);

  acquire(&ptable.lock);
  if(curproc->priority == 3) {
	  priority_cnt[3]--;
	  curproc->tickets = 0;
	  curproc->priority = 0;
	  priority_cnt[0]++;
	  g_process_queue.Data[0].move = 0;
	  swap(&g_process_queue.Data[0], &g_process_queue.Data[g_process_queue.size - 1]);
	  g_process_queue.size--;
	  MakeHeap(g_process_queue.size - 1);
	  for(int i = 0; i < g_process_queue.size; ++i) {
		if(g_process_queue.Data[i].priority == 0) {
		  g_process_queue.Data[i].tickets = 100 - cpu_occupy;
		  g_process_queue.Data[i].stride = BIG_NUMBER / g_process_queue.Data[i].tickets;
		  break;
		}
	  }
  }
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;

		if(p->priority >= 0) {
			priority_cnt[p->priority]--;
		}
		p->priority = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p = ptable.proc, *p2 = ptable.proc;
  struct cpu *c = mycpu();
  c->proc = 0;
  int pri = 0;
  int rotate = 0;
  uint start = 0, end = 0;
  for(;;){
    // Enable interrupts on this processor.
    sti();
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
	if(rotate > 64) {
		end = ticks;
		g_process_queue.Data[0].move += g_process_queue.Data[0].stride * (end - start);
		if(end - start > 0) {
			MakeHeap(g_process_queue.size - 1);
		}
		rotate = 0;
		start = ticks;
	}
	p2 = ChoicePriority(&pri);
	if(pri == 3) {
	  c->proc = p2;
	  switchuvm(p2);
	  p2->state = RUNNING;
	  swtch(&(c->scheduler), p2->context);
	  switchkvm();
	  c->proc = 0;
	  rotate = 0;
	  start = ticks;
	  release(&ptable.lock);
	  continue;
	}
	for(p = (p==&ptable.proc[NPROC]) ? ptable.proc : p; p < &ptable.proc[NPROC] ; ++p){
		rotate++;
		if(rotate > 64) {
      		break;
    	}  
		while(priority_cnt[pri] == 0 && pri <= 2){
		  pri++;
		}
		if(pri == 3) {
			pri = 4;
		}
		if(p->priority != pri || p->state != RUNNABLE || p->tickets > 0) {
		  continue;
		}
		// Switch to chosen process.  It is the process's job
		// to release ptable.lock and then reacquire it
		// before jumping back to us.
		c->proc = p;
		switchuvm(p);
		p->state = RUNNING;
		swtch(&(c->scheduler), p->context);
		switchkvm();
		// Process is done running for now.
		// It should have changed its p->state before coming back.
		c->proc = 0;
	    rotate = 0;
	    start = ticks;
		p++;
		break;
	}
	release(&ptable.lock);  
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  //g_process_queue.Data[0].move += g_process_queue.Data[0].stride * myproc()->rtime;
  g_process_queue.Data[0].move += g_process_queue.Data[0].stride;
  struct proc* p = myproc();
  while(p->isLWP) {
	  p = p->parent;
  }
  MakeHeap(g_process_queue.size - 1);
  if(g_process_queue.Data[0].move >= 1e9)
	  for(int i = 0; i < g_process_queue.size; ++i){
		  g_process_queue.Data[i].move = 0;
	  }
  p->rtime = 0;
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}


// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  //
  //
  if(p->isLWP == 0) {
    if(p->priority >= 0) {
		priority_cnt[p->priority]--;
	}
  }
  //
  //
  lwp_sched();
  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p->state == SLEEPING && p->chan == chan){
      p->state = RUNNABLE;
	  //
      if(p->isLWP == 0) {
        priority_cnt[p->priority]++;
      }
	  //
    }
  }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
	  if(p->isLWP) {
		  p->parent->killed = 1;
	  }
	  for(struct proc* q = ptable.proc; q < &ptable.proc[NPROC]; ++q) {
		  if(q->pid == pid && q->isLWP) {
			  q->killed = 1;
			  q->state = EMBRYO;
		  }
	  }
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING && !p->isLWP) {
        p->state = RUNNABLE;
		p->priority = 0;
		priority_cnt[p->priority]++;
	  }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("pid(%d)  tid(%d) state(%s) name(%s) priority(%d)", p->pid, p->tid, state, p->name, p->priority);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

int getppid(void) {
  return myproc()->parent->pid;
}

int getlev(void) {
  return myproc()->priority;
}

int set_cpu_share(int percent) {
  struct proc* p = myproc();
  while(p->isLWP) {
	  p = p->parent;
  }
  p->tickets = percent;
  if(p->priority != SLEEPING)
	  priority_cnt[p->priority]--;
  p->priority = 3;
  priority_cnt[p->priority]++;
  PushQueue(p);
  return percent;
}
void inc_total_time(struct proc* p) {
	acquire(&ptable.lock);
	p->ttime++;
	inc_mlfq_time();
	if((p->priority == 0 && p->ttime >= 20) 
			|| (p->priority == 1 && p->ttime >= 40)) {
		if(p->state != SLEEPING)
			priority_cnt[p->priority]--;
		p->priority++;
		if(p->state != SLEEPING)
			priority_cnt[p->priority]++;
		p->rtime = 0;
		p->ttime = 0;
	}
	release(&ptable.lock);
}
void inc_mlfq_time() {
	//acquire(&ptable.lock);
	mlfq_time++;
	if(mlfq_time >= 200) {
	  for(struct proc* p = ptable.proc; p < &ptable.proc[NPROC]; ++p) {
		  if(p->priority < 3 && p->priority >= 0) p->priority = 0;
		  p->ttime = 0;
		  p->rtime = 0;
	  }
	  priority_cnt[0] += priority_cnt[1];
	  priority_cnt[0] += priority_cnt[2];
	  priority_cnt[1] = 0;
	  priority_cnt[2] = 0;
	  mlfq_time = 0;
	}
	//release(&ptable.lock);
}
void yield2() { // user program yield
  struct proc* p = myproc();
  while(p->isLWP) {
	  p = p->parent;
  }
  //p->ttime++;
  inc_total_time(p);
  acquire(&ptable.lock);  //DOC: yieldlock
  //g_process_queue.Data[0].move += g_process_queue.Data[0].stride * myproc()->rtime;
  g_process_queue.Data[0].move += g_process_queue.Data[0].stride;
  MakeHeap(g_process_queue.size - 1);
  if(g_process_queue.Data[0].move >= 1e9)
	  for(int i = 0; i < g_process_queue.size; ++i){
		  g_process_queue.Data[i].move = 0;
	  }
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

void lwp_sched() {
  int intena;
  struct proc *p = myproc();
  struct proc* pproc = p;
  struct proc* next = p;
  int is_found = 0;
  int loopcount = 0;
  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  while(pproc->isLWP != 0) {
	  pproc = pproc->parent;
  }
  if(pproc->LWPcnt == 0) {
    sched();
    return;
  } else {
    for(int i = pproc->last + 1; loopcount < 64; ++i, loopcount++) {
      	if(i >= NPROC) {
        	i %= NPROC;
		}
		if(pproc->pid == ptable.proc[i].pid) {
        	next = &ptable.proc[i];
		}
        if(p == next || next->state != RUNNABLE)
          	continue;
        pproc->last = i;
		is_found = 1;
        mycpu()->proc = next;
        next->state = RUNNING;
        if(next == 0)
          panic("switchuvm: no process");
        if(next->kstack == 0)
          panic("switchuvm: no kstack");
        if(next->pgdir == 0)
          panic("switchuvm: no pgdir");

        pushcli();
        mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                      sizeof(mycpu()->ts)-1, 0);
        mycpu()->gdt[SEG_TSS].s = 0;
        mycpu()->ts.ss0 = SEG_KDATA << 3;
        mycpu()->ts.esp0 = (uint)next->kstack + KSTACKSIZE;
        // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
        // forbids I/O instructions (e.g., inb and outb) from user space
        mycpu()->ts.iomb = (ushort) 0xFFFF;
        ltr(SEG_TSS << 3);
        popcli();
        //cprintf("lwp_sched : process pid (%d) , tid (%d) process state (%d), process ttime (%d)\n", p->pid, p->tid, p->state, p->ttime);
        //cprintf("lwp_sched : switch to pid (%d) tid (%d)  process state (%d)\n", next->pid, next->tid, next->state);
        swtch(&p->context, next->context);
  		mycpu()->intena = intena;
        break;
      }
  }
  if(!is_found) {
	  wakeup1(pproc);
	  sched();
  }
}

void lwp_yield() { // lwp  yield
  acquire(&ptable.lock);  //DOC: yieldlock
  g_process_queue.Data[0].move += g_process_queue.Data[0].stride;
  myproc()->state = RUNNABLE;
  struct proc* p = myproc();
  while(p->isLWP) {
	  p = p->parent;
  }
  lwp_sched();
  release(&ptable.lock);
}

int thread_create(thread_t* thread, void *(*start_routine)(void *), void *arg) {
	int i;
  	struct proc *np;
  	struct proc *curproc = myproc();
	uint sz = 0, sp = 0;
	uint ustack[3];

	while(curproc->isLWP) {
		curproc = curproc->parent;
	}
  	// Allocate process.
  	if((np = allocproc()) == 0){
		cprintf("cannot allocate more process\n");
		return -1;
  	}
	sz = curproc->sz;
	sz = PGROUNDUP(sz);
	sp = sz;
	//cprintf("before allocation, sp (0x%x)\n", sp);
	np->pgdir = curproc->pgdir;
	if((sz = thread_allocuvm(curproc->pgdir, curproc->base, 2)) == 0) {
		kfree(np->kstack);
		np->kstack = 0;
		np->state = UNUSED;
		return -1;
	}
	curproc->sz = sz;
	np->sz = sz;
	np->base = curproc->base;
	clearpteu(curproc->pgdir, (char*)(sz - 2 * PGSIZE));

  	np->parent = curproc;
	sp = sz;
	//cprintf("after allocation, sp (0x%x)\n", sp);
	ustack[0] = 0xffffffff;
	ustack[1] = (uint)arg;
	ustack[2] = 0;
	sp -= sizeof(ustack);
	if(copyout(curproc->pgdir, sp, ustack, sizeof(ustack)) < 0) {
		return -1;
	}
	memset(np->tf, 0, sizeof(np->tf));
    *np->tf = *curproc->tf;
	np->tf->eip = (uint)start_routine;
	np->tf->esp = sp;
	np->tf->ebp = sp;
  	for(i = 0; i < NOFILE; i++)
		if(curproc->ofile[i])
	  		np->ofile[i] = filedup(curproc->ofile[i]);
  	np->cwd = idup(curproc->cwd);
  	safestrcpy(np->name, curproc->name, sizeof(curproc->name));
  	acquire(&ptable.lock);
    
    g_tid = (int)(np - ptable.proc);
	np->pid = curproc->pid;
    np->tid = g_tid;
	*thread = g_tid;
	curproc->LWPcnt++;
	np->pid = curproc->pid;
	np->isLWP = 1;
	np->priority = 0;
	np->tickets = curproc->tickets;
    np->state = RUNNABLE;
	np->priority = 4; // FOR FIX
    release(&ptable.lock);
    return 0;
}


void thread_exit(void* retval) {
  	struct proc *curproc = myproc();
	struct proc* pproc = curproc;
	while(pproc->isLWP) {
		pproc = pproc->parent;
	}
	int fd;
	// Close all open files.
	//
	for(fd = 0; fd < NOFILE; fd++){
		if(curproc->ofile[fd]){
		fileclose(curproc->ofile[fd]);
		curproc->ofile[fd] = 0;
		}
	}
	begin_op();
	iput(curproc->cwd);
	end_op();
	curproc->cwd = 0;
  	acquire(&ptable.lock);
  	curproc->state = EMBRYO;
	wakeup1(pproc->parent);
	curproc->tf->eax = (uint)retval;
  	lwp_sched();
  	panic("thread exit err");
}

int thread_join(thread_t thread, void** retval) {
	struct proc* curproc = myproc();
	struct proc* p;
	struct proc* pproc = curproc;
	while(pproc->isLWP) {
		pproc = pproc->parent;
	}
	acquire(&ptable.lock);
	p = &ptable.proc[thread];
	if(p->state == UNUSED) {
		release(&ptable.lock);
		lwp_sched();
		return 0;
	} else if(p->state == ZOMBIE) {
		release(&ptable.lock);
		wait();
		return 0;
	}
	while(p->state != EMBRYO) {
		if(p->tid == -1) {
			break;
		}
		sleep(pproc, &ptable.lock);
	}
	*retval = (void *)p->tf->eax;

	if(p->isLWP) {
		if(cleanup_thread(p) < 0) {
			cprintf("cleanup fail\n");
		}
	} else {
		if(p->state == ZOMBIE) {
			release(&ptable.lock);
			wait();
			return 0;
		}
	}
	release(&ptable.lock);
	return 0;
}

int cleanup_thread(struct proc* p) {
	if(p->isLWP) {
		struct proc* pproc = p;
		while(!pproc->isLWP) {
			pproc = pproc->parent;
		}
		int fd;
		// Close all open files.
		//
		release(&ptable.lock);
		for(fd = 0; fd < NOFILE; fd++){
			if(p->ofile[fd]){
			fileclose(p->ofile[fd]);
			p->ofile[fd] = 0;
			}
		}
		acquire(&ptable.lock);
		p->parent->LWPcnt--;
		//cprintf("deallocate from (0x%x) to (0x%x)\n", p->sz, p->sz - 2*PGSIZE);
		if(thread_deallocuvm(pproc->pgdir, p->sz, p->sz - 2* PGSIZE) == 0) {
			panic("deallovuvm err\n");
			return -1;
		}
		p->sz = 0;
		p->base = 0;
		p->tid = -1;
		p->isLWP = 0;
		p->pid = 0;
		p->parent = 0;
		p->name[0] = 0;
		p->killed = 0;
		kfree(p->kstack);
		p->kstack = 0;
		p->state = UNUSED;
		return 0;
	} else {
		return -1;
	}
}

void cleanup_rest() {
	struct proc* curproc = myproc();
	struct proc* p;
	for(p = ptable.proc; p < &ptable.proc[NPROC]; ++p) {
		if(p==curproc) {
			continue;
		}
		if(p->killed == 1 && p->state == EMBRYO && p->isLWP) {
			cleanup_thread(p);
		}
	}
	return;
}

void thread_promotion(struct proc* curproc) {
	  struct proc* parent = curproc->parent;
	  acquire(&ptable.lock);
	  curproc->parent->LWPcnt--;
	  curproc->parent = curproc->parent->parent;
	  curproc->pid = nextpid++;
	  curproc->priority = 0;
	  priority_cnt[0]++;
	  curproc->isLWP = 0;
	  curproc->tid = -1;
	  

	  
	  int fd;
	  for(fd = 0; fd < NOFILE; fd++){
		if(parent->ofile[fd]){
		  fileclose(parent->ofile[fd]);
		  parent->ofile[fd] = 0;
		}
	  }
	  
	  for(struct proc* p = ptable.proc; p < &ptable.proc[NPROC]; ++p) {
		  if(p->parent == parent) {
			  cleanup_thread(p);
		  }
	  }
	  kfree(parent->kstack);
	  parent->kstack = 0;
	  freevm(parent->pgdir);
	  parent->pid = 0;
	  parent->parent = 0;
	  parent->name[0] = 0;
	  parent->killed = 0;
	  parent->tickets = 0;

	  if(parent->state != SLEEPING && parent->priority >= 0) {
		  priority_cnt[parent->priority]--;
	  }
	  parent->priority = 0;
	  parent->state = UNUSED;

	  release(&ptable.lock);
}
