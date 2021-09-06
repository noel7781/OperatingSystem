명세서 내용-
1. MLFQ scheduling
   MLFQ는 3개의 큐로 이뤄져 있고 각각의 큐는 RR방식으로 이뤄진다.
   만약 한개의 프로세스라도 우선순위 큐 상 높은 우선순의를 가지면 우선순위가 낮은 큐에있는 프로세스는 절대로 실행되지 못한다.
   각 큐마다 RR정책에 시간점유가 다르다.
   1) 가장 우선순위가 높은 큐는 1tick / 중간 우선순위는 2tick / 가장 낮은 우선순위 큐는 4ticks
   2) 각각의 큐는 다른 time allotment를 가진다. 가장 높은 우선순위큐는 5ticks / 중간 우선순위 큐는 10ticks이다.
   3) starvation을 예방하기 위해서 우선순위 부스팅이 주기적으로 이뤄저야한다. 100ticks마다 주기적으로 발생
   4) MLFQ는 CPU의 적어도 20%를 항상 점유해야한다.

2. Stride scheduling
   만약 프로세스가 얼마나 CPU 점유권을 가진지 알고 싶으면, CPU share을 볼 수 있는 시스템콜을 부른다.
   만약 프로세스가 새로 생성되면, 기본적으로 MLFQ에 진입한다. set_cpu_share()함수가 호출되었을때만 stride scheduler에 의해 관리된다.
   Stride queue의 CPU 점유는 80%를 넘으면 안된다. 예외처리가 적절히 구현되어야한다.


필요한 시스템콜 - yield / getlev / set_cpu_share


추가할것 - 우선 proc 구조체에 우선순위를 나타내는 prioriry 변수와 time을 나타내는 변수, Stride scheduling을 위한 ticket을 추가해준다. 그 후 allocproc함수에서 구조체의 추가한 변수들을 초기화해준다. MLFQ에서 사용할 3종류의 큐를 구성한다. 하지만 의미상의 큐일뿐 우선순위를 카운트하는 전역변수 배열을 하나 만들어서 각각의 priority에 몇개의 프로세스가 있는지 카운트하는 배열을 사용할것이다. 편의상 가장 우선순위가 높은 큐를 0, 그다음을 1, 가장낮은 우선순위를 가지는 큐를 2이라고 하겠다. 그리고 stride scheduling에 돌입한 프로세스는 우선순위를 3으로 주겠다.
         큐의 사이즈는, 프로세스의 최대 갯수인 64개로 하고 각각의 프로세스가 time allotment보다 적게 사용했으면 그냥 프로세스를 돌리고, 만약 time allotment를 초과할 경우 우선순위를 한단계 내리는 작업을 진행한다.( 0 -> 1, 1->2)
         그러면 프로세스가 time allotment를 초과했는지 판단하는 변수가 필요하고, 그것을 proc구조체의 time으로 활용할 계획이다. 그리고 starvation을 방지하기 위한 boosting도 필요한데, 만약 boosting이 일어난다면 가장 우선순위가 높은 큐의 작업들은
         여태까지의 time 을 0으로 초기화할수도있고 기존의 것을 유지할수도 있다고 생각하는데, 나는 모든 프로세스의 time을 초기화시켜줘 가장 우선순위가 높은 큐에서 돌던 프로세스들도 새롭게 시작한것처럼 만들어줄것이다. 그래서 ptable의 proc구조체 전부의 time allotment를 0으로 만들어줄것이다.
 set_cpu_share함수를 실행시켰을때만 stride scheduler에의해
         관리되게 만드는데, 프로세스를 새로 할당할때 기본적으로 priority를 1로 설정해서 MLFQ에 의해 동작하도록 만들다가, set_cpu_share()함수 호출시 priority를 -1로 바꾸고 cpu share의 비율을 보고 만약 20퍼센트라면 80퍼센트의 시간동안은 MLFQ를 CPU를 이용해서 프로세스를 진행하고, 20퍼센트를 stride방식으로 하겠다. cpu share의 값은 MLFQ일 경우 -1, Stride방식의 경우 set_cpu_share(int)의 인자만큼의 값을 줘서 ticket값을 구한다. ticket값은 100에서 cpu share만큼을 나눈 크기로, 소숫점을 계산할 계획이나 성능이나 구현상의 문제가 발생할 경우 최소공배수로 계산해서 stride를 구해서 프로세스를 진행시키고, 만약 새로운 프로세스가 stride process에 진입시 stride 프로세스들 중 여태껏 가장 적게 걸어간 프로세스의 총 걸음수를 걸은 상태에서 시작하도록 구현하겠다.
