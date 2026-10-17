#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

char buf[512];

void
sixfive(int fd) {
  int i, n;
  int num, innum;
  char lc;

  num = innum = 0;
  lc = 0;
  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    for (i = 0; i < n; ++i) {
      if ('0' <= buf[i] && buf[i] <= '9') {
        if (innum || !lc || strchr(" -\r\t\n./,", lc)) {
          num = num*10 + buf[i]-'0';
          innum = 1;
        }
      } else if (innum) {
        if (strchr(" -\r\t\n./,", buf[i]) && (num%5 == 0 || num%6 == 0)) {
          fprintf(1, "%d\n", num);
        }
        num = innum = 0;
      }
      lc = buf[i];
    }
  }

  if (n < 0) {
    fprintf(2, "sixfive: read error\n");
    exit(1);
  }

  if (innum && (num%5 == 0 || num%6 == 0)) {
    fprintf(1, "%d\n", num);
  }
}

int
main(int argc, char *argv[])
{
  int fd, i;

  if (argc <= 1) {
    sixfive(0);
    exit(0);
  }

  for (i = 1; i < argc; ++i) {
    fd = open(argv[i], O_RDONLY);
    if (fd == -1) {
      fprintf(2, "sixfive: cannot open %s\n", argv[i]);
      continue;
    }
    sixfive(fd);
    close(fd);
  }

  exit(0);
}
