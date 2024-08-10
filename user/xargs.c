#include "kernel/param.h"
#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

#define MAX_WORD_LEN 64
#define MAX_WORDS 64

void
xargs(int argc, char *argv[])
{
  char *args[MAXARG];
  char c;

  int i;
  for (i = 0; i < argc; i++) {
    args[i] = argv[i];
  }

  char *line[MAX_WORDS];
  for (int j = 0; j < MAX_WORDS; j++) {
    line[j] = malloc(sizeof(char) * MAX_WORD_LEN + 1);
  }

  int processed_words = 0;
  int word_offset = 0;
  
  while (1) {
    int n = read(0, &c, 1);
    if (n == 0) return;

    // done reading line, fork and exec
    if (c == '\n') {
      // possibly new word
      if (word_offset) {
        line[processed_words][word_offset] = 0;
        processed_words++;
      }
      // fork child
      if (fork() == 0) {
        // all args are in line
        // append to initial args
        for (int j = i; j - i < processed_words; j++) {
          args[j] = line[j - i];
        }
        args[i + processed_words] = 0;
        
        exec(argv[0], args);
        exit(1);
      } else {
        // wait for child to complete
        wait((int*)0);
      }

      // reset everything for parent process
      processed_words = 0;
      word_offset = 0;
      continue;
    }

    // new word
    if (c == ' ') {
      // null terminate the current word
      line[processed_words][word_offset] = 0;
      processed_words++;
      word_offset = 0;
      continue;
    }
    
    line[processed_words][word_offset] = c;
    word_offset++;
  }
  
  for (int j = 0; j < MAX_WORDS; j++)
    free(line[j]);
}

int
main(int argc, char *argv[])
{
  if (argc < 3) {
    fprintf(2, "usage: xargs [cmd] [args...]\n");
    exit(1);
  }
  xargs(argc - 1, argv + 1);
  exit(0);
}
