#include <netdb.h>

struct tw_hostent{
    int sockfd;
    int id;
    char name[30];
    char file[30];
    char key_word[30];
    unsigned char is_dns;
    unsigned char is_url;
    unsigned char first;
    struct hostent* dns;
    struct in_addr* ip[100];
    struct tw_hostent* prev;
    struct tw_hostent* next;
};

struct tw_head {
    int sum;
    int count;
    struct tw_hostent* head;
    struct tw_hostent* end;
};


extern void DoOnce(struct tw_hostent* p_host);
extern void find_url(struct tw_hostent* p_host);
extern void init_node(const char* p_name);
