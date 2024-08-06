#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int p[2];

  if(pipe(p) < 0){
    fprintf(2, "pipe error\n");
    exit(1);
  }

  int pid = fork();

  if(pid < 0){
    fprintf(2, "fork error\n");
    exit(1);
  }

  if(pid == 0){
    // child process
    char parent_byte;
    read(p[0], &parent_byte, 1);
    write(p[1], &parent_byte, 1);
    
    fprintf(1, "%d: received ping\n", getpid());
    exit(0);
  } else {
    char chld_byte;
    write(p[1], "p", 1);
    read(p[0], &chld_byte, 1);

    wait(0);
    fprintf(1, "%d: received pong\n", getpid());
    exit(0);
  }
}
