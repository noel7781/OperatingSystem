#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int
main()
{
  char buf[512];
  int fd, i, sectors;
  i=0;

  fd = open("big.file", O_CREATE | O_RDWR);
  if(fd < 0){
    printf(2, "big: cannot open big.file for writing\n");
    exit();
  }

  sectors = 0;
  //int cnt = 2 * 16 * 1024;
  int cnt = 100;
  while(cnt > 0){
	  cnt-= 1;
    *(int*)buf = sectors;
    int cc = pwrite(fd, buf, sizeof(buf), sectors*512);
    if(cc <= 0)
      break;
    sectors++;
	printf(2, "sectors : (%d)\n", sectors);
	//if (sectors % 100 == 0)
		//printf(2, ".");
  }

  printf(1, "\nwrote %d sectors\n", sectors);

  //sync();
  printf(1,"sync done\n");

  printf(1, "log remain size (%d)\n", get_log_num());
  
  for(i = 0; i < sectors; i++){
    int cc = pread(fd, buf, sizeof(buf), 512*i);
    if(cc <= 0){
      printf(2, "big: read error at sector %d\n", i);
      exit();
    }
    if(*(int*)buf != i){
      printf(2, "big: read the wrong data (%d) for sector %d\n",
             *(int*)buf, i);
      exit();
    }
  }
  close(fd);
  /*
  int fd2 = open("big.file2", O_CREATE | O_RDWR);
  if(fd < 0){
    printf(2, "big: cannot open big.file for writing\n");
    exit();
  }

  sectors = 0;
  cnt = 400;
  while(cnt > 0){
	  cnt-= 1;
    *(int*)buf = sectors;
    int cc = pwrite(fd, buf, sizeof(buf), sectors*512);
    if(cc <= 0)
      break;
    sectors++;
	printf(2, "sectors : (%d)\n", sectors);
	//if (sectors % 100 == 0)
		//printf(2, ".");
  }

  printf(1, "\nwrote %d sectors\n", sectors);

  sync();
  printf(1,"sync done\n");
  close(fd2);

  printf(1, "log remain size (%d)\n", get_log_num());
  
  for(i = 0; i < sectors; i++){
    int cc = pread(fd, buf, sizeof(buf), 512*i);
    if(cc <= 0){
      printf(2, "big: read error at sector %d\n", i);
      exit();
    }
    if(*(int*)buf != i){
      printf(2, "big: read the wrong data (%d) for sector %d\n",
             *(int*)buf, i);
      exit();
    }
  }
  close(fd);
  */
  /*
  close(fd);
  fd = open("big.file", O_RDONLY);
  if(fd < 0){
    printf(2, "big: cannot re-open big.file for reading\n");
    exit();
  }
  for(i = 0; i < sectors; i++){
    int cc = read(fd, buf, sizeof(buf));
    if(cc <= 0){
      printf(2, "big: read error at sector %d\n", i);
      exit();
    }
    if(*(int*)buf != i){
      printf(2, "big: read the wrong data (%d) for sector %d\n",
             *(int*)buf, i);
      exit();
    }
  }
  */
  printf(1, "done; ok\n"); 
  exit();
}
