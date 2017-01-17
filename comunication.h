
#define MAX_RECV_STR_LENGTH 4096
#define RECV_TIMEOUT_SEC 10

int com_recv_str(int socfd,char *str,int size,int timeout_mode);
int com_send_str(int sockfd,char *send_str,int str_len);
int com_rsa_send(int sockfd,char *pub_key,char *send_buf);
int com_rsa_recv(int sockfd,char *prvi_key,char *recv_buf,int buf_size,int timeout_mode);
int com_rsa_send_aeskey(int sockfd,char *pub_key,unsigned char *aes_key,int n_bits);
