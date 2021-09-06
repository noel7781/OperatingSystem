# Project3 Milestone1 (Expand Maximum Filesize)

## 구현해야되는 사항 (Not yet implemented 부분)

![image](/uploads/1bd8d8e3b64f9ea2185a0268ed86badc/image.png)

## 구현내용 및 변수 수정한 사항

*    struct inode  -> addrs변수를 addrs[NDIRECT+3]으로 바꿨다.
*    struct dinode  -> addrs변수를 addrs[NDIRECT+3]으로 바꿨다.
*     #define FSSIZE 40000 
*    NDIRECT = 10 // NDINDIRECT = NINDIRECT * NINDIRECT // NTINDIRECT = NDINDIRECT * NINDIRECT // MAXFILE = NDIRECT + NINDIRECT + NDINDIRECT + NTINDIRECT
*    NDINIRECT = DOUBLE // NTINDIRECT = TRIPLE

## 함수 수정사항
*    static uint bmap(struct inode *ip, uint bn)
*        NDINDIRECT와 NTINDIRECT부분을 추가했는데
![image](/uploads/345a4ade60a4c1ccf1dc391cbedcf5ea/image.png)
![image](/uploads/00e402d7f763958bc733704a3c670d0c/image.png)
각각의 블럭인덱스만 설명하자면 NDINDIRECT에서는 a번째 블럭의 b번째 블럭넘버는 128*a + b가 되고, 해당하는 a b를 찾으려면 
block_idx = bn / NINDIRECT(128) , addr_idx = bn %NINDIRECT가 된다.

NTINDIRECT에서는 a번째블럭의 b번째블럭의 c번째 블럭넘버가되고 128*128*a + 128*b + c가 되므로
firstblock_idx = bn / NINDIRECT / NINDIRECT, second block idx = bn / NINDIRECT % NINDIRECT, addr_idx = bn % NINDIRECT가 된다.
구현 내용은 만약 블럭을 순회하면서 빈공간이 있으면 balloc으로 할당받고, 없으면 더 상위블럭으로 돌아가 다시 순회하기를 반복한다.
그러나 현재 구현상 FSSIZE인 블럭넘버의 갯수가 제한되있어서 실험적으로 최대 38000~ 39000사이의 블럭밖에 생성하지 못한다.
더 큰 파일을 만들고싶으면 FSSIZE를 수정하면 가능하다.

*    static void itrunc(struct inode *ip)

bmap과 반대로 버퍼를 삭제하는 동작을 하며 i번째 블록의 할당된 버퍼를 모두 해제시켜버린다.
다음은 코드의 일부이다.
![image](/uploads/f8ed8b7fb29557ad7341ee499efd693f/image.png)


*    testcode는 https://pdos.csail.mit.edu/6.828/2018/homework/big.c 해당 링크를 보고 Milestone 1 2 3모두 조금씩 수정해서 확인해봤음을 밝힙니다.


# Project3 Milestone2 (pread & pwrite)

## 구현해야되는 사항 - pread & pwrite system calls

## 구현내용 
*    pread와 pwrite의 시스템콜을 추가해준다.
*    filepread, filepwrite를 추가해줬는데, 이것이 read와 write와 다른점은 read와 write는 파일에 읽거나 쓸경우 현재 파일오프셋에서 데이터를 읽거나 쓰고 그만큼 파일오프셋을 이동시킨다는 특징이 있다. 이와는 다르게 pread와 pwrite는 파일오프셋은 변화시키지않으며 원하는 offset위치에 읽고 쓸수있게 해준다.
*    따라서 입력에 offset이라는 추가변수가 들어가고 수정사항이 거의없어 구현 사진을 보면 구현은 다음과같다.
![image](/uploads/a7a58b1eca48bff3264861f53a8a25c7/image.png)
![image](/uploads/632b7df5fc7a484b1ba59250e5b905da/image.png)
여기서도 offset을 증가시켜주긴 하지만 저것은 파일의 오프셋이 아니므로 실제 파일의 오프셋을 변화시키지는 않는다.
그리고 기존 함수들과 차이점은 writei나 readi 의 인자에 원래 파일의 오프셋이 들어가는데, pread와 pwrite는 함수 인자의 offset이 들어간다.
추가적으로 filepwrite의 inode를 기록시키는 부분에서는 pwritei가 들어가는데 이 이유는
![image](/uploads/e0c23ecfa2051c476756eb02fa040f10/image.png)
offset을 비교하는 부분에서 pwrite에 현재 파일크기보다 큰 offset을 적을경우 write되지 않는 경우때문에 size의 상한선을 제거해줬다.


*  추가적으로 cat을 했을때 만약 첫블록에 아무것도 적혀있지 않다면 (첫블록 뿐만아니라 한 블록이 통째로 비어있는경우, pwrite로 offset을 건너띄어 적은경우 발생하는 경우에) log out of trans에러가 나는걸 확인했다. 리눅스에서 구현해보니 전혀 에러가 나지 않아, pwritei를 할 때 만약 현재 적어야하는 offset이 파일의 offset보다 큰 경우 그 사이를 0으로 채우는 방식을 추가함으로써 cat에서도 에러가 나지 않도록 수정했다.
![image](/uploads/8dd2786e2d30c712159bd9be9dce4b3e/image.png)

# Project3 Milestone3 (sync system call)

## 구현해야되는 사항 - sync system call & get_log_num system call
*    데이터를 버퍼에만 쓰기
*    sync system call호출시 버퍼캐시에 있는 데이터를 디스크로 내리기
*    log space 꽉차면 버퍼캐시에 있는 데이터를 디스크로 내리기

## 구현내용 

*    내가 생각하기로 현재 xv6는 write through 방식으로 로그를 수정하고, 바로 디스크에 내리는 방식으로 데이터를 관리하고있는데, 이것을 먼저 버퍼에 적고 나중에 한꺼번에 디스크에 내리는 write back방식으로 구현을 바꾸는것이라고 생각했다.
*    따라서 이미 write시 버퍼에 적는것을 알 수있다. 여기서 고쳐야 하는점이라면 write시 begin_op() 와 end_op()를 통해서 데이터블록을 디스크까지 내려버리는데, 이것을 막기위하여 end_op()의 수정을 했다.
*    우선 log구조체에 sync라는 변수를 추가해서 sync call이 불린상태인지, 아닌지를 구별할 수 있는 flag를 설정하였다. 
*    sync syscall 호출시 sync_all이라는 함수를 호출하는데, 이 함수는 begin_op() -> log.sync = 1 -> end_op()단계로만 구성되어있다. sync call은 언제나 성공한다. 그리고     추가적으로 get_log_num() - 로그의 번호를 얻어오는 시스템콜 - 을 구현하였고  시스템콜에서 호출하는 함수도 추가하였다.
![image](/uploads/d44a66ff6594dc3e73e7c60795cdc1e9/image.png)

![image](/uploads/a1288422aa6e6ed81382c5f43ac95f1e/image.png)
*    버퍼캐시에 있는 로그를 디스크에 내리는 경우
    *        1. log.outstanding이 0이면서 log구조체의 가질수있는 최대사이즈인 21을 지정했는데 이 이유는 begin op에서 log가 적힐수 있는 최대치를 제한시킴으로써 commit하는 단위를 나누기 때문이다.
    *        2. log.sync == 1, 즉 sync call이 불렸을때 commit 하게 만들었다. commit()내에서 sync를 다시 0으로 초기화시켜준다.

*    해당 과정들을 진행시 1. 데이터를 버퍼에만 적용하며 2. sync가 불렸을때 버퍼캐시에 있는 데이터를 디스크로 내리며 3. log space가 가득찼을때도 버퍼캐시에 있는 데이터를 디스크로 내린다.


*    pwrite로 큰 offset을 줄 경우 올바르게 작동하지 못하는 경우가 생긴다.  transaction 단위가 커서 log_write과정에서 로그가 가득차는 경우 log_write내에서 panic을 맞게된다고 나오는데 우선 싱글쓰레드 환경에서 작업하니 begin_op의 두번째 if문을 삭제하였다. 실험을 해보니 Indirect block에 접근하는 경우 write_log단계에서 log.start.tail + 1을 읽는데, direct -> indirect로 바뀔때 락을 잡고 안놓는 문제가 생긴다. 이를 해결하기 위하여 log_write과정에서 만약 락을 놓아주고 sync후 다시 잡는방식으로 구현을하였다.
![image](/uploads/19e2c054e70139df4ec8f141ad6c026b/image.png)
![image](/uploads/febb345ae95e630764acf85439f85f67/image.png)



*    다만 마지막으로 남아있는 문제점이라면 만약 pwrite로 정말 큰 offset을 줄 경우 버퍼에 가득차게되고, 모든 버퍼가 dirty page상태이므로 그누구도 evict 될 수 없는 상태가 된다. 하지만 dirty page는 이미 파일을 쓰는 과정에서 dirty 상태이고 transaction단위가 커서 그누구도 evict될수없는상태이다. 즉 일반 write를 여러번하면 dirty page에서 벗어나 commit이 가능해지지만 pwrite로 큰 오프셋을 줄경우 하나의 write내 이므로 dirty page가 evict되지 못한다. 이것을 해결하려면 더 큰 사이즈의 버퍼와 로그사이즈만이 해결법이라고 생각한다.

