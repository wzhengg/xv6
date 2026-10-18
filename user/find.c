#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

void
find(char *path, char *target, char **cmd, int cmdlen) {
  char buf[512], *p;
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
  case T_DIR:
    if (strlen(path)+1 + DIRSIZ+1 > sizeof(buf)) {
      printf("find: path too long\n");
      break;
    }

    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0)
        continue;
      if (strcmp(".", de.name) == 0 || strcmp("..", de.name) == 0)
        continue;

      strcpy(buf, path);
      p = buf + strlen(buf);
      *p++ = '/';
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;

      if (strcmp(target, de.name) == 0) {
        if (cmd) {
          switch (fork()) {
            case -1:
              fprintf(2, "find: fork failed\n");
              break;
            case 0:
              if (cmdlen >= MAXARG) {
                fprintf(2, "find: too many args to exec\n");
                break;
              }
              char *argv[MAXARG] = {0};
              memcpy(argv, cmd, cmdlen*sizeof(char *));
              argv[cmdlen] = buf;
              if (exec(argv[0], argv)) {
                fprintf(2, "find: exec failed\n");
              }
              break;
            default:
              wait(0);
              break;
          }
        } else {
          printf("%s\n", buf);
        }
      }

      find(buf, target, cmd, cmdlen);
    }

    break;
  }

  close(fd);
}

int
main(int argc, char *argv[]) {
  char *usage = "Usage: find start target [-exec cmd...]\n";

  switch (argc) {
  case 0:
  case 1:
  case 2:
  case 4:
    fprintf(2, usage);
    exit(1);

  case 3:
    find(argv[1], argv[2], 0, 0);
    break;

  default:
    if (strcmp(argv[3], "-exec")) {
      fprintf(2, usage);
      exit(1);
    }
    find(argv[1], argv[2], &argv[4], argc-4);
  }

  exit(0);
}
