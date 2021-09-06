#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char* argv[]) {
  int cnt = 1000;
  int pid = fork();
  while(1) {
    if(pid > 0) {
      printf(1,"parent\n");
//	  printf(1,"%d", getlev());
    } else if(pid==0) {
      printf(1,"child\n");
//	  printf(1,"%d", getlev());
    } else {
		printf(1,"fork error\n");
	}
	cnt -= 1;
  }
  exit();
}
