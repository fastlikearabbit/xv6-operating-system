#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int nsec; 
  if(argc < 2) {
    fprintf(2, "Usage: sleep [secs]\n");
    exit(1);
  }
  
  nsec = atoi(argv[1]);

  sleep(nsec);

  exit(0);
}
