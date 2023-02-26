#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main() {
  int pipe_to_c[2];
  int pipe_to_p[2];
  if (pipe(pipe_to_p) != 0 || pipe(pipe_to_c) != 0) {
    exit(-1);
  }
  char buf[256];
  int t = fork();
  if (t == -1) {
    exit(-1);
  }
  if (t == 0) {
    close(pipe_to_c[1]);
    close(pipe_to_p[0]);
    int cur = 0;
    while (1) {
      int res = read(pipe_to_c[0], buf + cur, 256 - cur);
      cur += res;
      if (res == 0) {
        break;
      } else if (res < 0 || cur == 256) {
        close(pipe_to_c[0]);
        close(pipe_to_p[1]);
        exit(-1);
      }
    }
    close(pipe_to_c[0]);
    printf("%d: got %s\n", getpid(), buf);

    char temp[] = "pong";
    if (write(pipe_to_p[1], temp, strlen(temp)) != strlen(temp)) {
      close(pipe_to_p[1]);
      exit(-1);
    }
    close(pipe_to_p[1]);

  } else {
    close(pipe_to_c[0]);
    close(pipe_to_p[1]);

    char temp[] = "ping";
    if (write(pipe_to_c[1], temp, strlen(temp)) != strlen(temp)) {
      close(pipe_to_c[1]);
      close(pipe_to_p[0]);
      exit(-1);
    }
    close(pipe_to_c[1]);

    int cur = 0;
    while (1) {
      int res = read(pipe_to_p[0], buf + cur, 256 - cur);
      cur += res;
      if (res == 0) {
        break;
      } else if (res < 0 || cur == 256) {
        close(pipe_to_p[0]);
        exit(-1);
      }
    }
    close(pipe_to_p[0]);
    printf("%d: got %s\n", getpid(), buf);
  }
  exit(0);
}