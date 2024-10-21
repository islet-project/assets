#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <random>
#include <vector>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>

#include "socket.h"
#include "util.h"

using namespace std;

// global model
unsigned char model[1024 * 128] = {0,};
size_t model_len = 0;

void read_model() {
  FILE *ptr;
  int res;

  ptr = fopen("model.tflite", "rb");
  if (ptr == NULL) {
    printf("file open error: %s\n", strerror(errno));
    return;
  }

  model_len = fread(model, 1, sizeof(model), ptr);
  printf("model read done, size: %d\n", model_len);
  if (model_len == 0) {
    printf("model read fail\n");
    return;
  }

/*
  res = word_model.init(model, len);
  if (res == 0)
    printf("word model intialize done!\n");
  else
    printf("word model init error\n"); */
  fclose(ptr);
}

void callback(int fd, char *inmsg, size_t inlen) {
  //printf("recv msg: %s\n", inmsg);
  if (inlen > 8) {
    ssize_t len = 0;
    memcpy(model, inmsg, inlen);
    len = udp_server_write(fd, (char *)model, inlen);
    printf("local model update: input %ld, output %ld\n", inlen, len);
  }
}

int main(int argc, char** argv) {
  printf("hello\n");

  read_model();
  listen_and_receive_data("193.168.10.5", callback);
  return 0;
}
