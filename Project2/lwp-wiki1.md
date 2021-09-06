# LWP wiki

## Process / Thread
* Process
 * 프로세스는 현재 실행중인 프로그램으로 메모리를 차지하게 되는데, 코드영역과 데이터영역과 스택영역과 힙영역과 커널스택등을 메모리에 불러온다.
* Thread
 *  쓰레드는 프로세스 내에서 실행되는 여러 흐름의 단위로 프로세스 내에 존재하고, 스택을 제외한 프로세스의 리소스를 공유한다. 이 때문에 IPC를 이용하지 않아도 서로의 data에 접근이 가능하다.
* Context Switch
 *  context swtich시 process는 CR3레지스터의 페이지테이블을 새로운 프로세스의 페이지테이블로 교체한다. 또한 MMU에 있는 TLB를 모두 invalid 상태로 만들어주는데, 이에비해 thread는 context switch가 발생해도 process가 하는것처럼 TLB를 invalid상태로 만들거나 페이지테이블을 교체할 필요가 없기 때문에 process간 context swtich보다 비용이 싸다.

## POSIX thread
* pthread_create
 * pthread_create 는 쓰레드를 새로 생성하는 함수로  인자로 4가지를 받는다.
  * pthread_t *thread : 만들 쓰레드 변수를 선언하고 그 쓰레드의 주소를 인자로 넘겨준다.
  * const pthread_attr_t *attr : attr을 설정하는 인자이지만, 대부분의 경우 NULL을 넣어준다.
  * void *(*start_routine)(void *) : 실행할 루틴을 인자로 받는다.
  * void *arg : 실행할 루틴에서 필요한 인자들을 전달해주는 매개변수이다.
* pthread_join
 * pthread_join 은 인자로 받은 쓰레드가 종료할때 까지 프로세스를 종료시키지않고 기다리는 역할을 수행한다.
  * pthread_t thread : 기다릴 쓰레드를 인자로 넣어준다.
  * void **ret_val : 쓰레드 종료시 쓰레드에서 반환하는 값을 pthread_join의 ret_val인자로 얻을 수 있다.
* pthread_exit
 * pthread_exit 은 쓰레드를 종료시키면서, 반환하고싶은 값을 정해서 리턴시키는 역할을 한다.
  * void *ret_val : 쓰레드가 종료시 반환하는 값이며 이 값은 pthread_join의 ret_val에 전달된다.

## Design Basic LWP Operation for xv6
* Understanding of milestone 2
 * LWP를 구현하기 위해 유닉스 시스템들은 pthread를 사용하고 있다. 그와 비슷한 행동을 하는 thread를 만드는게 milestone 2이다. pthread와 마찬가지로, 각 쓰레드들은 주소공간을 공유하고, 독립적인 스택을 가지게 된다. 이렇게 주소공간을 공유하면 context switching시 오버헤드가 작다는 장점이 있다. 내가 이해하기로는 한 프로세스 기준으로 LWP 내에서 LWP group이라는 걸 가지게되는데 이것은 한 프로세스내에서 LWP들의 집합을 뜻한다. LWP group에 속하게되면 각 LWP 마다 기본적으로는 RR방식으로 스케쥴링이 일어나며,  1tick마다 LWP group 내에 있는 LWP사이에서 context switch가 발생한다. 그러므로 LWP간 context switch와 일반 프로세스간 context switch는 다른 방식으로 정의되고, LWP와 스케쥴러 간 context switch는 발생하지 않는다. 그리고 이에 해당하는 구현을 하면서 MLFQ 와 STRIDE scheduling의 방식도 조금씩 바꾸게 된다.
  * MLFQ의 경우  RR의 time quantum을 highest priority의 경우 5ticks, middle prioirty 의 경우 10ticks, lowest prioirty의 경우 20 ticks를 가진다. 그리고 highest priority의 time allotment의 경우 20ticks, middle priority의 경우 40ticks를 차지하고,  200ticks마다 priority boosting을 발생시킨다. LWP group내에 존재하는 LWP들은 모두 time quantum과 time allotment를 공유한다.
  * STRIDE의 경우 default time quantum을 5ticks로 설정한다. MLFQ와 마찬가지로  LWP group내에 존재하는 LWP들은 모두 time quantum과 time allotment를 공유한다. 그리고 LWP group 내의 한 LWP가 set_cpu_share을 호출한다면, 그 그룹 전부 STRIDE scheduling으로 관리하게 된다. project 1과 동일하게 cpu share의 총합은 80을 넘지 않는다.
* Rough Design of milestone 2
  * 우선 thread를 생성하고, 종료시키고, 종료를 기다리는 pthread like function을 구현하고 thread_create 호출시 address space를 공유하면서 독립적인 stack을 가지는 LWP를 만들어낸다. 일반 process to process 와 LWP to LWP의 스케쥴링 방식이 다르므로 LWP group이 사용할 수 있는 특별한 스케쥴러 함수를 하나 더 만들어서, 원래 가지고 있던 scheduler함수 내에서 context switch로 인해 LWP를 가지는 프로세스에게 cpu 점유가 넘어갔을때,  LWP 프로세스 내에서는 새로 정의한 LWP_scheduler()함수를 호출해서 time quantum과 time allotment를 마찬가지로 소비시키지만 기본적으로 1tick 마다 context swtich를 일으키는등 행동방식을 다른식으로 해야 될것이다. 지금 생각으론 어차피 page table은 kernel의 page table로 교체하게 되므로 page table의 교체는 필연적이지만 TLB의 경우 flag를 변화시키지 않는 방식으로 invalid시키지 않고 valid를 유지할 수 있을것 같다.