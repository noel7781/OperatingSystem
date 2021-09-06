# LWP implementation & Interaction with other services in xv6 & Interaction with schedulers implemented in Project 1

# Contents
* Design
    * proc structure
    * Thread functions
    * System calls
    * Scheduling
* Testcode Result
* Modifications

## Design policy

### About proc structure
#### 명칭 정리
* 앞으로 위키를 작성하면서 명칭의 혼동을 막고자 사용하는 의미에 통일성을 두고자 한다. thread_create로 생성된 프로세스들은 lwp 또는 쓰레드라고 지칭할 것 이며, 그 외의 방식으로 생성된건 전부 프로세스라고 정했다. 물론 프로세스도 쓰레드를 가지고 있지만 둘 사이의 차별및 혼동을 방지하고자 한다. 예를들어  부모프로세스와 자식쓰레드라고 칭하는것은 부모프로세스가 thread_create로 자식을 만든 경우를 뜻하며, 부모프로세스와 자식프로세스라고 칭하는것은 부모프로세스가 fork등으로 자식을 만든 경우이다.

#### proc 구조체에 수정및 추가한 변수 및 이유
* thread를 관리하는 새로운 구조체를 만들기보다, proc구조체의 수정을 통해 쓰레드도 프로세스처럼 관리하고자 함이였고, 이를 통해 쓰레드라면 thread id를 뜻하는 tid를 구조체의 인덱스로 사용했다.

* 수정및 추가한 변수
    * qtime -> rtime 
        * 용어의 좀더 정확한 의미전달 및 이해를 위해서 round robin 하는 기준이 되는 time quantum의 변수명을 rtime으로 변경하였다.
    * sz
        * sz라는건 기존에 하는 역할이 size of process memory를 나타냈지만, 과제를하면서 스택의 가장 상위부분을 가리키도록 만들었다.  기존용도와 거의 비슷하다.
    * parent
        * parent가 일반 프로세스의 부모뿐 아니라 프로세스가 쓰레드를 생성할 경우 자식쓰레드에서 부모프로세스에 접근할 경우 parent변수로 접근한다. 
    * LWPcnt
        * 일반 프로세스들만 가질수 있는 값임과 동시에, 자식 쓰레드의 수를 세는 변수이다.
    * last
        * LWP간 스케쥴링시 proc 구조체 배열을 순회하는데, 마지막 실행된 프로세스의 인덱스를 기록함으로써 모든 프로세스가 최대한 공평하게 실행될수 있도록 보장해주는 변수이다.
    * pid
        * 모든 쓰레드는 동일한 pid를 가지고, 이는 부모프로세스와 같은값이다. 하지만 다른 lwp그룹이라면 다른 pid를 가지게된다.
    * tid
        * 쓰레드 ID를 나타내는 변수인데, 중복이없고 쓰레드가 아니라면 -1의 값을 가진다. 또한 이변수를 proc구조체의 
인덱스를 가지게 해서 tid를 안다면 proc구조체에 바로 접근가능하게 만들었다.
    * isLWP
        * 쓰레드라면 1의값을 가지고, 프로세스라면 0의값을 가진다.
    * base
        * 아무런 쓰레드도 만들지 않았을때의 sz값으로 base를 기준으로 빈공간을 찾는데 사용한다.

#### 메모리 관리를 위해 추가한 함수

~~~c 

int
thread_allocuvm(pde_t *pgdir, uint start, uint size)
{
  char *mem;
  uint a;
  pte_t* pte;
  uint loop_count = 0;
  a = PGROUNDUP(start);
  uint start_pgaddr = a;
  if(loop_count == 0) {
	  start_pgaddr = a;
  }
  while(loop_count < size) {
	  if(loop_count == 0) {
		  start_pgaddr = a;
	  }
	  mem = kalloc();
	  if(!mem) {
		  cprintf("thread_allocuvm error 1\n");
		  return -1;
	  }
	  memset(mem, 0, sizeof(mem));
	  for(;;a += PGSIZE) {
		if((pte = walkpgdir(pgdir, (char *)a, 1)) == 0) {
			cprintf("thread_allocuvm error2\n");
			kfree(mem);
			return -1;
		}
		if(*pte & PTE_P) {
			loop_count = 0;
			if(loop_count > 0) {
				kfree(mem);
				thread_deallocuvm(pgdir, a, start_pgaddr);
			}
			continue;
		}
		*pte = V2P(mem) | PTE_W | PTE_U | PTE_P;
		break;
	  }
	  a += PGSIZE;
	  loop_count++;
  }
  return a;
}
int
thread_deallocuvm(pde_t *pgdir, uint start_sp, uint end_sp) {
	//cprintf("start_sp (0x%x), end_sp (0x%x)\n", start_sp, end_sp);
  pte_t *pte;
  uint a, pa;

  if(start_sp <= end_sp)
    return end_sp;

  a = PGROUNDUP(end_sp);
  for(; a  < start_sp; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte) {
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
	}
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      char *v = P2V(pa);
      kfree(v);
      *pte = 0;
    }
  }
  return end_sp;
}

pde_t*
thread_copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < myproc()->base; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
      kfree(mem);
      goto bad;
    }
  }
  for(i = sz - 2 *PGSIZE; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
      kfree(mem);
      goto bad;
    }
  }
  return d;

bad:
  freevm(d);
  return 0;
}

~~~

* 모두 쓰레드를위해 메모리의 원하는 부분을 할당,해제 또는 복사하기 위해 만든 함수들로 thread_allocuvm함수는 start부터 원하는 size (page의 수)만큼의 페이지를 생성하고 스택의 가장윗부분을 리턴해준다. 이를 호출할때 start는 proc구조체의 base가 되며 base부터 위로 올라가면서 사용가능한 페이지의 주소를 찾는다. thread_deallocuvm은 반대로 원하는 영역을 해제시켜주며 thread_copyuvm은 원하는 vm영역을 복사할수있게 해준다.

#### Part of Thread Create & Exit & Join 

~~~ c

int thread_create(thread_t* thread, void *(*start_routine)(void *), void *arg) {
	sz = curproc->sz;
	sz = PGROUNDUP(sz);
	sp = sz;
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
}
~~~
* Thread_create에서 특징적인 부분들만 본다면, 새로운 프로세스를 ptable에 추가시켜주고 memory를 관리하는 thread_allocuvm 을 사용해서 base부터 페이지사이즈 2개를 할당받고, 아래쪽 페이지를 가드페이지르 만든다. start routine을 eip로설정해서 실행시 forkret -> trapret -> eip로 start_routine이 들어가게 만들었고 부모프로세스를 설정하며 모든 쓰레드의 우선순위는 4로 고정하였다. 

~~~ c
void thread_exit(void* retval) {
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
~~~
* 파일을 닫고 혹시나 자고있을 부모 프로세스를 깨우며 종료시 ZOMBIE가 되는것이 아닌 EMBRYO상태로 만드는데, 이것은 일반 프로세스의 종료상태와 쓰레드의 종료상태를 조금 다르게 처리하고자 해당 방법을 사용했다.

~~~ c
int thread_join(thread_t thread, void** retval) {
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
}
~~~
* Join의 일부분만 살펴본다면 현재 쓰레드id를 ptable의 인덱스를 사용해서 관리하고 있기때문에, 만약 ptable의 인덱스를 확인했는데 unused 상태일경우 이미 종료된 쓰레드이므로 신경쓰지않고 zombie일 경우 일반적인 thread_exit으로 종료되지 않음을 뜻해서 정리시켜준다.  그리고 sleep and wakeup 방식을 통해 만약 기다리는 프로세스가 이미 종료되었다면 embryo상태일텐데, embryo라면 잠에들지 않지만, 기다리는 프로세스가 runnable 또는 running, sleeping상태라면 join을 호출한 프로세스는 잠이든다. ptable lock과 condition (p->state != EMBRYO)가 있으므로 lost wakeup이 발생하지 않는다. 그리고 cleanup_thread로 쓰레드라면 남은 자원을 모두 정리해준다.

####  System call

* 시스템콜과 관련하여 기본적인 시스템콜이 하는 역할이나 프로세스가 시스템콜을 불렀을 경우를 제외한 쓰레드가 시스템콜을 불렀을 경우의 flow만 살펴 보겠다.

* exit
 
~~~ c
void
exit(void)
{
  struct proc *curproc = myproc();
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
  wakeup1(curproc->parent);

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
  } else {
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
~~~

* exit 시스템콜 호출시 exit호출을 한 주체가 쓰레드라면, 해당 LWP그룹은 모두 종료되어야 한다. 이를 위해 호출한 주체가 쓰레드라면 모든 쓰레드들을 killed = 1과 state를 embryo로 바꾸고, 부모프로세스또한 killed = 1로설정한다. 그러면 다음 타이머 인터럽트때 proc()->killed = 1이 되면서 부모프로세스도 또한 exit을 호출하게 되는데, 이경우와 마찬가지로 부모프로세스가 exit을 호출한 경우 모든 자식들을 killed=1, embryo상태로만들고 모두 cleanup시킨다. 그리고 부모프로세스는 프로세스이므로 zombie상태로 변하면서 이 프로세스의 부모가 자원을 정리할 때 까지 zombie상태로 남아있는다.
 정리하자면 모든 쓰레드를 정리하는건 해당하는 부모프로세스이며, 부모프로세스는 모든 쓰레드를 정리하기 전까지는 zombie상태로 변하지않는다. zombie상태가 됬다는 의미는 이미 자식쓰레드가 하나도 없다는 의미이다.

* fork

~~~ c
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
~~~

* fork시 다른부분은 전부 동일하지만  쓰레드는 fork를 호출시 해당프로세스의 스택과 호출한 쓰레드1개의 페이지만을 복사한다.

* exec

~~~ c
int exec(char *path, char **argv) {
...
  if(curproc->isLWP)  {
	  thread_promotion(curproc);
  } else {
	  freevm(oldpgdir);
  }
...
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
~~~

* 만약 쓰레드가 exec을 실행한다면 부모프로세스를 포함한 모든 LWP그룹은 정리되고 호출한 쓰레드만 새로운 프로세스로 바뀐다. 이를 위해 thread가 프로세스가 되는 thread_promotion함수를 만들었는데, 해당 함수호출시 쓰레드가 프로세스가 가져야하는 요소들과, 쓰레드가 아니라면 갖지말아야할 요소들을 처리해주며, 모든 쓰레드들을 cleanup 한다. 부모인 프로세스도 정리되어야 하므로 파이프를 정리해주고 부모의 상태를 정리해준다. 이부분이 상당히 부자연스럽다고 느껴지지만 여기서 부모프로세스를 정리해주지 않으면 추후에 wait을 하는 프로세스 하나가 이 쓰레드였던 프로세스의 부모자원을 정리하게되고 새로만들어진 프로세스는 initproc에 의해 정리된다. 그것을 방지하기 위해, 미리 부모의 자원을 정리해줌으로써 후에 자연스럽게 정리될 수 있도록 만들었다.

* sbrk

~~~ c
int
sys_sbrk(void)
{
...
  while(p->isLWP) {
	  p = p->parent;
  }
  addr = p->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}
~~~

* sbrk는  기존의 sbrk와 growproc과 유사하지만 , 바꾼부분은 sbrk를 호출할때 addr의 기준이 항상 프로세스가 되도록 해서 요청된 크기만큼 페이지에 할당해주는데 1000할당후 1000은 총 2000이 되므로 처음 1000때 새로운 페이지 하나 생성후 그다음 요청에는 새로운 페이지를 생성하지 않는다. 그리고 race condition을 방지하기 위해 기존의 growproc에 ptable의 lock을 추가했다.

* kill

~~~ c
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
~~~

* kill은 exit과 유사하게 동작하는 부분이 있는데, 그것은 호출시 모든 쓰레드와함께 부모까지 종료시킨다는 것이다. 그 방법으로는 같은 pid를 가진다면 killed=1로설정하고 (쓰레드라면 embryo상태까지 만듬) 종료한다. 그러면 다음 타이머 인터럽트에 의해 자원이 정리될 수 있다.



* pipe
    * pipe는 모든 프로세스와 쓰레드의 fileopen과 fileclose를 해줬다.
 
* sleep
    * sleep은 변경한 점이 없는데, 이는 lwp group안의 lwp가 terminate될 경우 이 쓰레드도 마찬가지로 terminate와 자원정리를 해주면 되는데, exit과 kill system call에서 이 쓰레드가 현재 sleeping인지 따지지 않고 상태를 embryo, killed = 1로만들기 때문에 아무리 자신이 sleep하고싶어도 lwp group이  terminate될경우 정리된다.

* other cases
    * thread create thread를 구현했는데, ptable sturct를 사용하고있는 자원상의 한계로 쓰레드 + 프로세스의 갯수가 64개를 넘지 못함을 전제로 하고 thread에서 thread를 생성할 경우 이 쓰레드는 부모가 맨처음 쓰레드의 부모인 프로세스로 A 프로세스가 B 쓰레드를 만들고 B쓰레드가 C쓰레드를 만든다고 해도 C 쓰레드의 부모는 A 쓰레드 이며, 자원의 정리 역시 A프로세스로 하여금 일어난다.

####  Scheduling

* MLFQ & STRIDE

    * 기본적으로 lwp는 lwp_sched라는 함수에의해 스케쥴링되고, 프로세스는 scheduler에 의해 스케쥴링된다. 


    * rrtime이 가득 찰 경우 yield(), 유저프로세스가 yield함수를 부를경우 yield2(), 그 외의 경우에는 lwp_yield()를 실행한다. 한번 실행할때마다 stride scheduling을 위한 pass가 stride만큼 증가하지만, lwp_yield의 경우 실행되는 lwp group이 바뀌면 안되므로 heap을 새로 생성하지 않고, 그외의 경우에는 heap을 새로만들어서 다음 선택될 프로세스를 결정한다.

    * 기본적으로 lwp -> lwp 방식으로 context switch가 일어나게 만들었는데, 특정한 경우 불가피하게 lwp -> scheduler인 경우가 생긴다. 그 예로 rrtime이 가득차거나 해서 발생하는 yield()가 호출되었을때는, yield가 호출될때 만약 쓰레드가 실행되고 있다면 lwp -> lwp parent -> scheduler방식으로 context switch가 일어나야 과제의 명세에 나온  context switching between an LWP and the scheduler never occurs.에 부합하는 조건이 되겠지만 이는 불필요한 context switch가 한번 발생하므로 더 비효율적이라고 판단했고, 불가피하게 lwp -> scheduler로의 context switch가 발생한다. 또한 스케쥴러 내부에서 context switch를 할 프로세스를 선택할 경우 모든 쓰레드의 부모프로세스가 자고있다면 그 경우에만  여기서 찾는 priority를 4로 바꿔서 (모든 쓰레드는 priority를 4를 가지도록 설정했다.) 이경우 또한 scheduler -> lwp로의 context switch가 발생한다. 

    * set_cpu_share로 부모프로세스의 stride만 조절한다. 호출할 당시 cpu time의 비율이 80을 초과할 경우 mlfq 프로세스처럼 동작한다.
~~~ c
void lwp_sched() {
...
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
        swtch(&p->context, next->context);
  	mycpu()->intena = intena;
...
}
~~~
 
    * lwp_sched는 switchuvm의 behavior을 mimic했으나 다만 cr3 레지스터를 변경시키지 않으므로 pgdir의 변경이 이루어지지 않는다. 그리고 마지막으로 선택된 프로세스의 인덱스를 부모의 last변수에 저장해서 최대한 공평하게 실행될 수 있도록 여건을 보장해준다. 

## Testcode Result

* Testcode를 실행하는도중 조건을 바꾼것은, stride test의 시간을 sleep(500)에서 sleep(30000)으로 수정했다.
여러번 실행해봤지만 sleep(500)만 하는경우 wake up은 제때 될 수있으나 스케쥴이 되지 않아서 stride scheduling의 비율이 정확도가 매우 떨어져 보일 수 있다. (특히 stride 비율이 낮은 2%를 가지고 실험하면 더 심하게 나타났다. 더욱 스케쥴링하는 비율이 적어서 그런것같다.) 그래서 조금 더 sleep time을 늘려서 테스트해보았고, stride 비율을 거의 맞출 수 있었다. 이외의 경우 sleep(500)을 하더라도 20% 20%정도로 cpu 점유를 주면 비슷하게 나타난다.

![image](/uploads/5bc35a087d4c1b8b12464ddb49797e7d/image.png)

![image](/uploads/55134e1b34e5a095882a7b3faae81399/image.png)

![image](/uploads/c2235631e471736e76174742515b641e/image.png)


## Modification

* Project 1에서 악의적으로 프로세스가  yield 호출시 game할수 있다는 지적을 받았고, 이에 user에의해 yield가 호출되면 yield2()가 실행되면서 process의 time을 올린다. 그러면 프로세스가 1tick을 온전히 사용하지 못했다고 해도 yield2()가 time allotment를 올리고 결국 더 빠르게 priority가 낮출 수 있도록 수정했다.
