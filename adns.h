#include <sys/epoll.h>

struct header 
{
	uint16_t	tid;		/* Transaction ID */
	uint16_t	flags;		/* Flags */
	uint16_t	nqueries;	/* Questions */
	uint16_t	nanswers;	/* Answers */
	uint16_t	nauth;		/* Authority PRs */
	uint16_t	nother;		/* Other PRs */
	unsigned char	data[1];	/* Data, variable length */
};

extern struct sockaddr_in dns_server;
extern int dns_ip[3][4];
extern int epfd;
extern struct tw_hostent* hostent_index[1000];


extern void init_dns(void);
extern void query_dns(struct tw_hostent* p_host);
extern void dns_poll(int sockfd);
