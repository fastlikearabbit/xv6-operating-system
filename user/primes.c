#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define LOW 2
#define HIGH 35
#define INT_SIZE 4

void reverse(char s[])
 {
     int i, j;
     char c;

     for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
         c = s[i];
         s[i] = s[j];
         s[j] = c;
     }
}

void itoa(int n, char s[])
 {
     int i, sign;

     if ((sign = n) < 0)  /* record sign */
         n = -n;          /* make n positive */
     i = 0;
     do {       /* generate digits in reverse order */
         s[i++] = n % 10 + '0';   /* get next digit */
     } while ((n /= 10) > 0);     /* delete it */
     if (sign < 0)
         s[i++] = '-';
     s[i] = '\0';
     reverse(s);
}


void
pipeline(int rpipe)
{
  int pipefd[2];
  pipe(pipefd);

  char buf[INT_SIZE+1];
  buf[INT_SIZE] = 0;


  if(fork() == 0) {
  int prime;
  if (read(rpipe, buf, INT_SIZE) <= 0) {
    exit(0);
  }
  prime = atoi(buf);
  printf("prime %d\n", prime);
    int n;
    for(;;){
      if(read(rpipe, buf, INT_SIZE) <=0)
        break;
      n = atoi(buf);
      if (n % prime != 0) {
        itoa(n, buf);
        write(pipefd[1], buf, INT_SIZE);
      }
    }
    close(rpipe);
    close(pipefd[1]);
    pipeline(pipefd[0]);
    exit(0);
  }
  close(pipefd[0]);
  close(pipefd[1]);
  wait(0);
}

int
main()
{
  int pipefd[2];
  pipe(pipefd);

  char buf[INT_SIZE+1];
  buf[INT_SIZE] = 0;

  for(int i = LOW; i < HIGH; ++i){
    itoa(i, buf);
    write(pipefd[1], buf, INT_SIZE);
  }
  close(pipefd[1]);
  pipeline(pipefd[0]);
  close(pipefd[0]);

  wait(0);
  exit(0);
}
