#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>

// tensorflow-
#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/string_util.h"
#include <random>
#include <vector>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>

#include "word_model.h"
#include "socket.h"
#include "util.h"

#define PORT 9999

static char ckpt_path[512] = "./checkpoint/model.ckpt";
static WordPredictionModel word_model;
int sfd = -1;

using namespace std;

unsigned char model[128 * 1024] = {0,};
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

void download_model(void) {
  int res;
  /*
  unsigned char *request_str = (unsigned char *)"download_tflite_model";
  int res;

  sfd = connect_sk("193.168.10.5");
  if (sfd < 0) {
    printf("connect_sk fail\n");
    exit(-1);
  } */

/*
  ssize_t size = send_and_receive_data(sfd, request_str, strlen((const char *)request_str), model, sizeof(model));
  if (size < 0) {
    printf("download_model: send_data error\n");
    exit(-1);
  } */
  read_model();
  printf("model read done: %ld\n", model_len);

  res = word_model.init(model, model_len);
  if (res == 0)
    printf("word model init done\n");
  else
    printf("word model init error\n");
}

void inference(unsigned char *input_word, unsigned char *out_prediction) {
  word_model.infer((char *)input_word, ckpt_path, out_prediction);
}

void training(unsigned char *input_word) {
  word_model.train((char *)input_word, ckpt_path);
}

unsigned char global_model[32 * 1024] = {0,};
unsigned char local_model[32 * 1024] = {0,};
int sock;
struct sockaddr_in target_addr;
int addr_len;

void update_model(void) {
  size_t len = read_file(ckpt_path, local_model, sizeof(local_model));
  if (len == 0) {
    printf("read_file error\n");
    return;
  }
  printf("current local_model_size: %ld\n", len);

  // upload a local model and download a new global model
  //int size = send_and_receive_data(sfd, local_model, len, global_model, sizeof(global_model));
  size_t send_size = sendto(sock, local_model, len, 0, (struct sockaddr*)&target_addr, addr_len);
  printf("send_size: %ld\n", send_size);
  //sleep(1);
  size_t recv_size = recvfrom(sock, global_model, len, 0, (struct sockaddr *)&target_addr, (socklen_t *)&addr_len);
  printf("new_global_model recv: %ld\n", recv_size);

  save_as_file(ckpt_path, global_model, recv_size);
  word_model.restore(ckpt_path);
}

void run_shell(void) {
  if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
      perror("socket ");
      return;
  }

  memset(&target_addr, 0x00, sizeof(target_addr));
  target_addr.sin_family = AF_INET;
  target_addr.sin_addr.s_addr = inet_addr("193.168.10.5");
  target_addr.sin_port = htons(PORT);
  addr_len = sizeof(target_addr);

  // make sure we have a proper model
  printf("before download_model\n");
  download_model();
  printf("after download_model\n");

  // main loop
  while (1) {
    unsigned char msg[2048] = {0,};
    unsigned char correct_answer[2048] = {0,};
    unsigned char out_prediction[2048] = {0,};

    // word model: requires a correct answer
    printf("\n");
    printf("Type characters: ");
    scanf("%s", msg);
    inference(msg, out_prediction);
    printf("Prediction: %s\n", out_prediction);
    printf("Type correct answer: ");
    scanf("%s", correct_answer);

    training(correct_answer);
    update_model();
  }
}

int main(int argc, char** argv) {
  printf("hello\n");
  run_shell();
  word_model.finalize();
  return 0;
}
