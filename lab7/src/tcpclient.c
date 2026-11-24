#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SADDR struct sockaddr
#define SIZE sizeof(struct sockaddr_in)

int main(int argc, char *argv[]) {
  int fd;
  int nread;
  struct sockaddr_in servaddr;

  if (argc < 4) {
    fprintf(stderr, "Usage: %s <server_ip> <port> <bufsize>\n", argv[0]);
    exit(1);
  }

  int port = atoi(argv[2]);
  int bufsize = atoi(argv[3]);

  if (port <= 0 || bufsize <= 0) {
    fprintf(stderr, "Invalid port or bufsize\n");
    exit(1);
  }

  char *buf = malloc(bufsize);
  if (!buf) {
    perror("malloc");
    exit(1);
  }

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket creating");
    free(buf);
    exit(1);
  }

  memset(&servaddr, 0, SIZE);
  servaddr.sin_family = AF_INET;

  if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0) {
    perror("bad address");
    free(buf);
    close(fd);
    exit(1);
  }

  servaddr.sin_port = htons(port);

  if (connect(fd, (SADDR *)&servaddr, SIZE) < 0) {
    perror("connect");
    free(buf);
    close(fd);
    exit(1);
  }

  write(1, "Input message to send\n", 22);
  while ((nread = read(0, buf, bufsize)) > 0) {
    if (write(fd, buf, nread) < 0) {
      perror("write");
      free(buf);
      close(fd);
      exit(1);
    }
  }

  free(buf);
  close(fd);
  exit(0);
}
