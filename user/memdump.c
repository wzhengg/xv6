#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void memdump(char *fmt, char *data, int len);

int
main(int argc, char *argv[])
{
  if (argc == 1) {
    printf("Example 1:\n");
    int a[2] = {61810, 2026};
    memdump("ii", (char *)a, sizeof(a));

    printf("Example 2:\n");
    memdump("S", "a string", sizeof("a string"));

    printf("Example 3:\n");
    char *s = "another";
    memdump("s", (char *)&s, sizeof(s));

    struct sss {
      char *ptr;
      int num1;
      short num2;
      char byte;
      char bytes[8];
    } example;

    example.ptr = "hello";
    example.num1 = 1819438967;
    example.num2 = 100;
    example.byte = 'z';
    strcpy(example.bytes, "xyzzy");

    printf("Example 4:\n");
    memdump("pihcS", (char *)&example, sizeof(example));

    printf("Example 5:\n");
    memdump("sccccc", (char *)&example, sizeof(example));
  } else if (argc == 2) {
    // format in argv[1], up to 512 bytes of data from standard input.
    char data[512];
    int n = 0;
    memset(data, '\0', sizeof(data));
    while (n < sizeof(data)) {
      int nn = read(0, data + n, sizeof(data) - n);
      if (nn <= 0)
        break;
      n += nn;
    }
    memdump(argv[1], data, n);
  } else {
    printf("Usage: memdump [format]\n");
    exit(1);
  }
  exit(0);
}

void
memdump(char *fmt, char *data, int len)
{
  int n;

  n = 0;
  for (; *fmt; ++fmt) {
    switch (*fmt) {
      case 'i':
        if (n+sizeof(uint32) > len) {
          fprintf(2, "memdump: not enough data for '%c'\n", *fmt);
          return;
        }
        printf("%u\n", *(uint32 *)data);
        data += sizeof(uint32);
        n += sizeof(uint32);
        break;
      case 'p':
        if (n+sizeof(uint64) > len) {
          fprintf(2, "memdump: not enough data for '%c'\n", *fmt);
          return;
        }
        printf("%lx\n", *(uint64 *)data);
        data += sizeof(uint64);
        n += sizeof(uint64);
        break;
      case 'h':
        if (n+sizeof(uint16) > len) {
          fprintf(2, "memdump: not enough data for '%c'\n", *fmt);
          return;
        }
        printf("%u\n", *(uint16 *)data);
        data += sizeof(uint16);
        n += sizeof(uint16);
        break;
      case 'c':
        if (n+sizeof(char) > len) {
          fprintf(2, "memdump: not enough data for '%c'\n", *fmt);
          return;
        }
        printf("%c\n", *(char *)data);
        data += sizeof(char);
        n += sizeof(char);
        break;
      case 's':
        if (n+sizeof(char **) > len) {
          fprintf(2, "memdump: not enough data for '%c'\n", *fmt);
          return;
        }
        printf("%s\n", *(char **)data);
        data += sizeof(char **);
        n += sizeof(char **);
        break;
      case 'S':
        for (; n < len && *data; ++n, ++data)
          printf("%c", *data);
        if (n < len && *data == '\0')
          ++n, ++data;
        printf("\n");
        break;
    }
  }
}
