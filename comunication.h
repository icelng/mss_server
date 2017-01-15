
#define MAX_RECV_STR_LENGTH 4096
#define RECV_TIMEOUT_SEC 10

int com_recv_str(int socfd,char *str,int size,int timeout_mode);
int com_send_str(int sockfd,char *send_str,int str_len);
