#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char* argv[]) {
  int cnt = 1;
  int pid = fork();
  while(cnt > 0) {
    if(pid > 0) {
      printf(1,"parent\n");
//	  printf(1,"%d", getlev());
	  wait();
    } else if(pid==0) {
      printf(1,"child\n");
//	  printf(1,"%d", getlev());
	  char *args[3] = {"echo", "echo is executed!", 0}; 
	  sleep(1);
	  exec("echo", args);
    } else {
		printf(1,"fork error\n");
	}
	cnt -= 1;
  }
  exit();
}
