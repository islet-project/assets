#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#define MAX 80
#define SA struct sockaddr

void udp_test(char *ip, int port) {
  int sockfd;
  struct sockaddr_in addr;
  char buffer[1024];
  socklen_t addr_size;
  size_t res;
 
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr(ip);
 
  bzero(buffer, 1024);
  strcpy(buffer, "hello_world");
  res = sendto(sockfd, buffer, 1024, 0, (struct sockaddr*)&addr, sizeof(addr));
  printf("[+]Data send: %s, %d, %d\n", buffer, (int)res, errno);
 
  bzero(buffer, 1024);
  addr_size = sizeof(addr);
  recvfrom(sockfd, buffer, 1024, 0, (struct sockaddr*)&addr, &addr_size);
  printf("[+]Data recv: %s\n", buffer);
}

void tcp_func(int sockfd)
{
    char buff[MAX];
    int n;
    for (;;) {
        bzero(buff, sizeof(buff));
        strcpy(buff, "hello_world");

        /*
        printf("Enter the string : ");
        n = 0;
        while ((buff[n++] = getchar()) != '\n')
            ; */

        write(sockfd, buff, sizeof(buff));
        bzero(buff, sizeof(buff));

        read(sockfd, buff, sizeof(buff));
        printf("From Server : %s", buff);
        break;
        /*
        if ((strncmp(buff, "exit", 4)) == 0) {
            printf("Client Exit...\n");
            break;
        } */
    }
}

void tcp_test(char *ip, int port) {
  int sockfd, connfd;
    struct sockaddr_in servaddr, cli;
 
    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));
 
    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port = htons(port);
 
    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr))
        != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n");
 
    // function for chat
    tcp_func(sockfd);
 
    // close the socket
    close(sockfd);
}

void tls_test(char *ip, int port) {
}
 
int main(int argc, char **argv){
  if (argc != 4) {
    printf("Usage: %s <ip> <port> <cmd>\n", argv[0]);
    exit(0);
  }
 
  char *ip = argv[1];
  int port = atoi(argv[2]);
  int cmd = atoi(argv[3]);  // 0: udp, 1: tcp, 2: tls

  if (cmd == 0) {
    udp_test(ip, port);
  } else if (cmd == 1) {
    tcp_test(ip, port);
  } else if (cmd == 2) {
    tls_test(ip, port);
  }
 
  return 0;
}
