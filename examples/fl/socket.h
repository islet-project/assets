#define TEST_SIZE (2048)

void listen_and_receive_data(const char* host_name, void (*callback)(int, char *, size_t));
ssize_t send_and_receive_data(int sfd, unsigned char *msg, ssize_t len, unsigned char *out, ssize_t outlen);
int connect_sk(const char* host_name);
size_t udp_server_write(int sock, char *msg, size_t len);