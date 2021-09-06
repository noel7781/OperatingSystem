# 실제구현사항

## proc구조체에 추가한 항목
* priority
 * 각 proc들의 우선순위를 표시하기위해 추가한 변수
* ttime
  * 각 proc들이 실제로 수행된 시간을 나타내는 변수, priority 가 0이고 ttime이 5이거나 priority가 1이고 ttime이 15이면 우선순위가 각각 0 -> 1 / 1 -> 2로 변한다.
* qtime
 *  time quantum을 나타내는 변수로 priority가 0이면 1tick마다, priority가 1이면 2tick마다, priority가 2면 4tick마다 yield()를 실행한다.
* tickets
 * 각 프로세스가 가지고 있는 티켓, mlfq scheduling에 의해 관리되는 프로세스들은 해당사항이 없고 stride scheduling을 하는 프로세스들만 cpu점유비율에 해당하는 값을 가지고 있다.

## 구현한 자료구조
* 우선순위큐
** Data라는 구조체를 추가했는데, 이 Data구조체는 process의 address와 move(pass), stride, priority 등을 가지고 있고, 처음에 mlfq를 위해 사용할 base Data를 한개 추가해서 priority를 0으로 정하고, 다른 mlfq process들은 이 Data 구조체를 사용하지않는다. (후에 stride scheduling과 mlfq scheduling을  사용할 때 어떤 프로세스가 실행되어야 하는지 판단하기위해 mlfq는 1개만 넣는다.)
## 선언한 변수
* cpu_occupy 
 * set_cpu_share(int)를 호출한 함수들이 점유하고 있어야 하는 cpu의 비율로 최대 80까지의 값을 가지고, 그 이상을 가지려는 요청이 들어올시 에러메세지를 출력한다.
* mlfq_time
 * mlfq process 실행시 1tick 씩 올라가며 100틱이 누적되면 priority boosting을 하는 기능을 하는 변수
* prirority_cnt[4]
 * mlfq scheduling시 priority를 0,1,2를 가지며 stride scheduling시 priority 3을 가지게 되는데, 각각의 갯수를 담고 있는 카운트배열 변수

## 구현한 system call
* int getlet(void)
 * 프로세스의 priority level을 return하는 함수
* int set_cpu_share(int)
 * 프로세스가 stride scheduling을 사용하고 싶어할 경우 cpu share가 80을 넘지 않는다면 priority를 3으로 바꿔주고 프로세스를 우선순위 큐에 담음
* int yield(void)
 * 커널레벨에서 사용되는 yield와 유저프로세스가 실행하는 yield2 함수를 따로 구현했으며 둘의 차이는 time quantum을 초기화시켜주느냐, 초기화시켜주지 않느냐의 차이를 가진다.

## MLFQ scheduling
* mlfq scheduling은 priority 0 / 1 / 2 의 3가지 우선순위를 가지며 낮은수 일수록 우선순위가 높다. 각 프로세스는 trap의 timer interrupt에 의해 mlfq_time을 증가시키며 mlfq_time이 100이 되면 모든 프로세스의 우선순위를 0번으로 보내는 priority boosting이 일어나고 priority가 0이아닌 프로세스들을 priority_cnt[0]의 배열의 값에 더해주고 0으로 초기화해준다. 그리고 마찬가지고 timer interrupt에 의해 ttime을 증가시키는데, 이건 해당 time allotment를 모두 사용시 우선순위가 떨어지는 역할을 한다. 
* scheduler함수에서, 어떤 우선순위의 프로세스를 검색할지를 정하는 pri 변수를 가지고 우선 priority_cnt 배열에서 배열의 시작부터, priority_cnt의 값이 0인지 0이 아닌지 확인하고, 0이라면 pri를 증가시킨다. 만약 pri를 찾았다면, 그 우선순위를 가지는 프로세스를 찾을때까지 for문을 돈다. 만약 성공적으로 찾았으면 swtch 함수를 실행한다.

## STRIDE scheduling
* 프로세스가 set_cpu_share(int)를 호출했을때 시작되는 스케쥴링으로 우선순위 큐를 가지게 되는데, 우선순위 큐의 항목으로는 아무 프로세스와도 관련이 없지만 mlfq scheduling의 실행에 관여하는 priority 0을 가지는 Data가 하나있고, 각각 set_cpu_share(int)가 호출될때마다 우선순위큐에 프로세스를 push해준다. 힙에서 항상 move(pass)가 가장 작은 값을 얻어서 stride만큼 나아가는 기능을 수행한다. stride의 값을 얻는 방법은 1000 / ticket으로 정의했다. 그리고 이 우선순위 큐에서 만약 일정값 이상의 move(pass)를 가진다면 모든 프로세스의 move(pass)를 0으로 초기화시켜줬다.(overflow 방지)
* stride scheduling 프로세스만 남고 mlfq scheduling 하는 프로세스가 없는경우, mlfq 프로세스가 없어서 우선순위 큐에서 stride만큼 나아가지 못하는데, rotate라는 변수를 사용해서 64번(NPROC의 갯수)을 다 돌아서 priority 0 , 1 , 2인것이 없으면 경과한 시간만큼 time * stride의 크기로 move(pass)를 이동시켜서, set_cpu_share(int)이 호출하는 비율의 점유권을 보장해주려고 노력했다.

# MLFQ + STRIDE scheduling
* schduler함수에서 struct proc* p2를 추가로 선언하고 ChoicePriority(int* priority) 함수를 사용해서 만약 우선순위 큐에서 선택된 프로세스의 priority가 0(mlfq를 사용하는 프로세스라는 의미를 가짐)이라면 p2는 무시되고, priority의 값은 0으로 함수를 탈출해서 내부의 for문으로 들어가고, priority가 3(stride 를 사용하는 프로세스)이라면 해당 프로세스를 p2의 값으로 저장해서 스케쥴링을 진행한다.
## 프로세스 종료시(exit)
* 각 프로세스들은 running상태에서 exit에 진입한다. 만약 stride scheduling을 사용하는 프로세스 였다면, ticket의 값만큼 cpu_occupy를 내리고 힙에서 자기자신을 제거한후 무사히 제거된다.
* priority boosting은 온전히 mlfq의 시간에만 영향을 받으며 stride scheduling의 cpu점유시간과는 무관하다. 왜냐하면 stride scheduling이 cpu의 많은부분을 차지하게 되면 starve한 process를 실행시키고자하는 원래의 목적과 부합하지 않게 의미없는 priority boosting이 자주 일어날 수 있기 때문에 priority boosting은 mlfq process에 의해서만 100tick마다 일어나게 만들었다.

# 실행결과

## 4 MLFQ with no yield
![image](/uploads/64b753025fea998c631dd69a9d1a8b0b/image.png)
![image](/uploads/4fc489b72b8aae2a75cbab7d66c656d4/image.png)

대체로 1  : 2 : 2의 비율을 보이는데, 이것은 각 priority 마다 yield의 주기가 1tick, 2tick, 4tick이고 time allotment가 5, 10 이므로
priority 0일때 1 * 5 * 4 = 20 tick , priority 1 일때 4 * 10 = 40 tick 이고 priority boosting이 100tick 마다 일어나므로
20tick : 40tick : 40tick = 1 : 2 : 2가 성립한다.


## 4 MLFQ with 4 yield
![image](/uploads/118e0de48737492387c605671a24ae5d/image.png)

yield를 실행하는것은 1초마다 process를 바꾸는 것일뿐 time allotment는 여전히 지나므로 no yield인 상태와 마찬가지로
대체적으로 1 : 2 : 2의 비율을 보인다. (다음에 오는 아래 상황을 생각해 봤을때는 역시나 다양한 결과를 얻을수도 있다고 생각한다)

## 4 MLFQ with 2 no yield 2 yield
![image](/uploads/b463c315e9c5e55e51fe428c11af8129/image.png)
내 예상으로 
4개의 프로세스가 priority 0일때 20tick을 사용하고, no yield 2개의 프로세스는 30tick 동안 돌면서 priority 2로 drop이 일어나고 no yield process는 priority 2, yield process 는 process 1인 상태에서 50tick을 사용하게 되는데 그러면 전체적인 비율은 priority 0일때 20tick, priority 1일때 40tick, priority 2일때 40 tick을 사용한다.
그러면 위의 케이스들과 비슷한 결과를 얻어야하는데, 그러지 못했다. 이것은 아마도 user program 에서 yield를 사용시 mlfq time은 1tick이 증가하는데 실제로는 yield를 수행하면 온전히 1tick을 사용하지 못하므로 정확한 결과가아닌 실행할때마다 다양한 결과를 얻게 된것같다. 또한 mlfq에서 yield 와 no yield를 동시에 실행시켜도 no yield process가 먼저 끝나게 되는데, 그러면 yield 프로세스만 남게되어서 yield 프로세스만 실행하면 5: 10 : 85, 즉 1: 2:17의 비율과 섞이게 되어 최종 값이 약간의 불규칙성을 가진다고 생각한다.

## 4 STRIDE with 5 , 10, 20 ,40 percent cpu share
![image](/uploads/d83ea0fbc5595a29c12f2454ced0e266/image.png)

가장 예측하기 쉬운 방식으로 각 cpu share의 비율에 준하는 비로 결과를 얻을수 있었다. (1:2:4:8)

## 2 STRIDE with 10, 40 percent cpu share / 2 MLFQ with 1 yield and 1 no yield
![image](/uploads/b65f2309427020934570a95a24bc030f/image.png)

일단 stride process들은 1:4의 비율을 가지고 , yield하는 프로세스보다 no yield하는 프로세스가 priority 2에서 좀더 오래 있는것을 확인할 수 있다.
