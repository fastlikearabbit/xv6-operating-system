#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define BUF_SIZE 128 


void 
reverse(char *str)
{
  int n = strlen(str);

  for (int i = 0; i < n / 2; i++) {
    char tmp = str[i];
    str[i] = str[n - 1 - i];
    str[n - 1 - i] = tmp;
  }
}

void
find(char *path, char *file) 
{
  char buf[BUF_SIZE], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(path, O_RDONLY)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }
  
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }
  switch (st.type) {
    case T_DEVICE:
    case T_FILE: {
      char suffix[BUF_SIZE];
      int pathlen = strlen(path);
      int i = pathlen - 1;
    
      while (i >= 0) {
        if (path[i] == '/') break;
        suffix[pathlen - i - 1] = path[i];
        --i;
      }
      suffix[pathlen - i - 1] = 0;
      reverse(suffix);

      if (!strcmp(suffix, file)) {
        printf("%s\n", path);
      }
      break;
    }
    
    case T_DIR: {
      if (strlen(path) + DIRSIZ + 1 > sizeof buf) {
        printf("find: path too long\n");
        break;
      }
      strcpy(buf, path);
      p = buf + strlen(buf);
      *p++ = '/';
      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0 || !strcmp(de.name, "")) {
          continue;
        }
        if (!strcmp(de.name, ".") || !strcmp(de.name, "..")) {
          continue;
        }

        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        find(buf, file);
      }
      
    }
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if (argc <= 2) {
    fprintf(2, "usage: find [dirname] [filename]\n");
    exit(1);
  }

  char *dirname = argv[1];
  char *filename = argv[2];

  find(dirname, filename);

  exit(0);
}
