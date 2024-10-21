#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <time.h>
#include <arpa/inet.h>

#define PORT 9999

// server-side functions
static struct sockaddr_in* init_sockaddr_in(uint16_t port_number) {
  struct sockaddr_in *socket_address = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
  memset(socket_address, 0, sizeof(*socket_address));
  socket_address -> sin_family = AF_INET;
  socket_address -> sin_addr.s_addr = htonl(INADDR_ANY);
  socket_address -> sin_port = htons(port_number);
  return socket_address;
}

static char* process_operation(char *input) {
  size_t n = strlen(input) * sizeof(char);
  char *output = (char *)malloc(n);
  memcpy(output, input, n);
  return output;
}

static bool open_server_socket(const char* host_name, int port, int* soc) {
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int sfd, s;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_protocol = 0;
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  char port_str[16] = {};
  sprintf(port_str, "%d", port);

  s = getaddrinfo(host_name, port_str, &hints, &result);
  if (s != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
    return false;
  }

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    int option = 1;
    sfd = socket(rp->ai_family, rp->ai_socktype,
                 rp->ai_protocol);
    if (sfd == -1)
      continue;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
      break;

    close(sfd);
  }

  if (rp == NULL) {
    fprintf(stderr, "Could not bind\n");
    return false;
  }

  freeaddrinfo(result);

  if (listen(sfd, 10) != 0) {
    printf("open_server_socket: cant listen\n");
    return false;
  }

  *soc = sfd;
  return true;
}

// public functions
char recv_buffer[32 * 1024] = {0,};
void listen_and_receive_data_tcp(const char* host_name, void (*callback)(int, char *, size_t)) {
  int   server_socket;
   int   client_socket;
   int   client_addr_size;
   struct sockaddr_in   server_addr;
   struct sockaddr_in   client_addr;

   server_socket  = socket(AF_INET, SOCK_STREAM, 0);
   if(-1 == server_socket)
   {
      printf( "server socket error\n");
      exit(1);
   }

   memset(&server_addr, 0, sizeof( server_addr));
   server_addr.sin_family      = AF_INET;
   server_addr.sin_port        = htons(PORT);
   server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

   if(-1 == bind( server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr))){
      printf("bind error\n");
      exit(1);
   }

   if(-1 == listen(server_socket, 5))
   {
      printf("listen() error\n");
      exit(1);
   }

   while(1)
   {
      client_addr_size  = sizeof(client_addr);
      client_socket     = accept(server_socket, (struct sockaddr*)&client_addr, (socklen_t *)&client_addr_size);

      if (-1 == client_socket)
      {
         printf("no clnt conn\n");
         exit(1);
      }

      while (1) {
        size_t size = read (client_socket, recv_buffer, sizeof(recv_buffer));
        printf("receive: %ld\n", size);
        callback(client_socket, recv_buffer, size);
        
        //sprintf(buff_snd, "%d : %s", strlen( buff_rcv), buff_rcv);
        //write(client_socket, buff_snd, strlen( buff_snd)+1);
      }
      close(client_socket);
   }
}

struct sockaddr_in client_addr;
int client_addr_len;
void listen_and_receive_data_udp(const char* host_name, void (*callback)(int, char *, size_t)) {
  int sock;
  struct sockaddr_in addr;
  int recv_len;

  if((sock = socket(AF_INET, SOCK_DGRAM, 0)) <0){
      perror("socket ");
      return;
  }

  memset(&addr, 0x00, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(host_name);
  addr.sin_port = htons(PORT);

  if(bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0){
      perror("bind ");
      return;
  }

  printf("waiting for messages\n");
  while (1) {
    client_addr_len = sizeof(client_addr);
    recv_len = recvfrom(sock, recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
    printf("recv_len: %ld\n", recv_len);

    if (recv_len > 0)
      callback(sock, recv_buffer, recv_len);
  }
  //sendto(sock, recv_buffer, strlen(recv_buffer), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
  close(sock);
}
size_t udp_server_write(int sock, char *msg, size_t len) {
  sendto(sock, msg, len, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
}

void listen_and_receive_data(const char* host_name, void (*callback)(int, char *, size_t)) {
  listen_and_receive_data_udp(host_name, callback);
}

int connect_sk(const char* host_name) {
  int    client_socket;
  struct sockaddr_in   server_addr;

  client_socket = socket(AF_INET, SOCK_STREAM, 0);
  if(-1 == client_socket)
  {
    printf("socket error\n");
    exit(1);
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family     = AF_INET;
  server_addr.sin_port       = htons(PORT);
  server_addr.sin_addr.s_addr= inet_addr(host_name);

  if(-1 == connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr) ) )
  {
    printf("connect error\n");
    exit(1);
  }

  return client_socket;
}

ssize_t send_and_receive_data(int sock, unsigned char *msg, ssize_t len, unsigned char *out, ssize_t outlen) {
  ssize_t send_len = 0;
  ssize_t recv_len = 0;

  send_len = write(sock, msg, len);
  printf("send_len: %ld\n", send_len);

  recv_len = read(sock, out, outlen);
  if (recv_len <= 0) {
    printf("read error\n");
    return -1;
  }
  printf("recv_len: %ld\n", recv_len);

  return recv_len;
}
