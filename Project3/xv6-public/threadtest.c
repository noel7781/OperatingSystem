#include "types.h"
#include "stat.h"
#include "user.h"
#define NUM_THREAD 7
int g_count = 0;

void*
printthread(void *arg)
{
	printf(1,"child thread \n");
	g_count++;
	thread_exit(0);
	exit();
}
void*
forkthreadmain(void *arg)
{
  printf(1, "arg : (%d), M2 Thread create\n",(int)arg);
  g_count++;
  int i = 0;
  thread_t threads[NUM_THREAD];
  void* retval;
  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], printthread, (void*)0) != 0){
      printf(1,"panic at thread_create\n");
      return 0;
    }
  }
  for (i = 0; i < NUM_THREAD; i++){
    if (thread_join(threads[i], &retval) != 0){
      printf(1,"panic at thread_join\n");
      return 0;
    }
  }
  thread_exit(0);
  exit();
}

int
main(void)
{
  thread_t threads[NUM_THREAD];
  int i;
  void *retval;

  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], forkthreadmain, (void*)0) != 0){
      printf(1,"panic at thread_create\n");
      return -1;
    }
  }
  for (i = 0; i < NUM_THREAD; i++){
    if (thread_join(threads[i], &retval) != 0){
      printf(1,"panic at thread_join\n");
      return -1;
    }
  }
  printf(1 ,"global count (%d)\n", g_count);
  exit();
}

/*
void* testfunc(void* arg) {
	int cnt = 0;
	for(int i = 0; i < 1000000; ++i) {
		cnt++;
		g_count++;
	}
	printf(1,"local cnt : %d\n", cnt);
	//exit();
	thread_exit(arg);
	return 0;
}

int main() {
	thread_t t[THREAD_NUM];
	int ret = 0;
	int arg = 1234;
	for(int i = 0; i < THREAD_NUM; ++i) {
		thread_create(&t[i], testfunc, &arg);
		printf(1, "tid : %d\n", t[i]);
	}
	
	for(int i = 0; i < THREAD_NUM; ++i) {
		thread_join(t[i], (void *)&ret);
		printf(1, "return from thread : %d\n", *(int*)ret);
	}
	
	for(int i = 0; i < THREAD_NUM; ++i) {
		thread_create(&t[i], testfunc, &arg);
		printf(1, "tid : %d\n", t[i]);
	}
	
	for(int i = 0; i < THREAD_NUM; ++i) {
		thread_join(t[i], (void *)&ret);
	}
	
	printf(1, "global cnt : %d\n", g_count);
	printf(1, "return from thread : %d\n", *(int*)ret);
	exit();
}
*/
