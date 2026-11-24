#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SADDR struct sockaddr

int main(int argc, char *argv[]) {
  const size_t kSize = sizeof(struct sockaddr_in);

  int lfd, cfd;
  int nread;
  struct sockaddr_in servaddr;
  struct sockaddr_in cliaddr;

  if (argc < 3) {
    fprintf(stderr, "Usage: %s <port> <bufsize>\n", argv[0]);
    exit(1);
  }

  int port = atoi(argv[1]);
  int bufsize = atoi(argv[2]);

  if (port <= 0 || bufsize <= 0) {
    fprintf(stderr, "Invalid port or bufsize\n");
    exit(1);
  }

  char *buf = malloc(bufsize);
  if (!buf) {
    perror("malloc");
    exit(1);
  }

  if ((lfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    free(buf);
    exit(1);
  }

  memset(&servaddr, 0, kSize);
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);

  if (bind(lfd, (SADDR *)&servaddr, kSize) < 0) {
    perror("bind");
    free(buf);
    close(lfd);
    exit(1);
  }

  if (listen(lfd, 5) < 0) {
    perror("listen");
    free(buf);
    close(lfd);
    exit(1);
  }

  printf("TCP SERVER listening on port %d...\n", port);

  while (1) {
    unsigned int clilen = kSize;

    if ((cfd = accept(lfd, (SADDR *)&cliaddr, &clilen)) < 0) {
      perror("accept");
      free(buf);
      close(lfd);
      exit(1);
    }
    printf("connection established\n");

    while ((nread = read(cfd, buf, bufsize)) > 0) {
      write(1, buf, nread);
    }

    if (nread == -1) {
      perror("read");
      close(cfd);
      free(buf);
      close(lfd);
      exit(1);
    }
    printf("connection closed by client\n");
    close(cfd);
  }

  free(buf);
  close(lfd);
  return 0;
}
