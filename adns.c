#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include "spider.h"
#include "adns.h"

#define ARRAY_TO_NUM(p) ((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | (p[3]))

struct sockaddr_in dns_server;
int dns_ip[3][4] = {{202, 119, 230, 8}, {218, 2, 135, 1}, {61, 147, 37, 1}};

void init_dns(void)
{
    dns_server.sin_family = AF_INET;
    dns_server.sin_port = htons(53);
    dns_server.sin_addr.s_addr = htonl(ARRAY_TO_NUM(dns_ip[0]));

    return;
}

static void init_sock(struct tw_hostent *p_host)
{
    int opt = 1;
    struct epoll_event ev;

    p_host->sockfd = socket(PF_INET, SOCK_DGRAM, 17);
    //printf("%s socket is %d\n", p_host->name, p_host->sockfd);

    ioctl(p_host->sockfd, FIONBIO, &opt);

    memset(&ev, 0, sizeof(ev));
    ev.events  = EPOLLIN | EPOLLET;
    ev.data.fd = p_host->sockfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, p_host->sockfd, &ev);

    return;
}

static void dns_request(struct tw_hostent *p_host)
{
    int	i, n, name_len, m;
	struct header	*header = NULL;
    char *name;

	const char 	*s;
	char pkt[2048], *p;
	
	header		     = (struct header *) pkt;
	header->tid	     = p_host->id;
	header->flags	 = htons(0x100); 
	header->nqueries = htons(1);	    
	header->nanswers = 0;
	header->nauth	 = 0;
	header->nother	 = 0;


	// Encode DNS name 
    name = (char *)&p_host->name;
	name_len = strlen(p_host->name);
	p = (char *) &header->data;	/* For encoding host name into packet */
	
	do {
		if ((s = strchr(name, '.')) == NULL)
			s = name + name_len;

		n = s - name;			/* Chunk length */
		*p++ = n;			            /* Copy length */
		for (i = 0; i < n; i++)		    /* Copy chunk */
			*p++ = name[i];

		if (*s == '.')
			n++;

		name += n;
		name_len -= n;

	} while (*s != '\0');

	*p++ = 0;			/* Mark end of host name */
	*p++ = 0;			/* Well, lets put this byte as well */
	*p++ = 1;	        /* Query Type */

	*p++ = 0;
	*p++ = 1;			/* Class: inet, 0x0001 */

	n = p - pkt;			/* Total packet length */
		
	if ((m = sendto(p_host->sockfd, pkt, n, 0, (struct sockaddr *) &dns_server, sizeof(dns_server))) != n) 
	{
		//printf("%s error\n", p_host->name);
        perror("sendto error");
	}

    return;
}

void query_dns(struct tw_hostent* p_host)
{
    init_dns();

    init_sock(p_host);

    dns_request(p_host);

    return;
}

/*
static void *download_page(struct tw_hostent *p_host)
{

    DoOnce(p_host);
    find_url(p_host);
    p_host->is_url = 1;
    
    pthread_exit((void *)1);

}
*/
static void parse_udp(const unsigned char *pkt, int len)
{
	const unsigned char	*p, *e, *s;
	struct header   *header = NULL;
    struct tw_hostent *p_host = NULL;
//    pthread_t download_page_thread;
	
	uint16_t    type;
	int			found, stop, dlen, nlen, i;
	
	header = (struct header *) pkt;
	if (ntohs(header->nqueries) != 1)
		return;
    p_host = hostent_index[header->tid];
	
	/* Skip host name */
	for (e = pkt + len, nlen = 0, s = p = &header->data[0];
	    p < e && *p != '\0'; p++)
		nlen++;

#define	NTOHS(p)	(((p)[0] << 8) | (p)[1])

	/* We sent query class 1, query type 1 */
	if (&p[5] > e || NTOHS(p + 1) != 0x01)
		return;

	/* Go to the first answer section */
	p += 5;
	
	/* Loop through the answers, we want A type answer */
	for (found = stop = 0; !stop && &p[12] < e; ) {

		/* Skip possible name in CNAME answer */
		if (*p != 0xc0) {
			while (*p && &p[12] < e)
				p++;
			p--;
		}

		type = htons(((uint16_t *)p)[1]);

		if (type == 5) 
		{
			/* CNAME answer. shift to the next section */
			dlen = htons(((uint16_t *) p)[5]);
			p += 12 + dlen;
		} 
		else if (type == 0x01) 
		{

            dlen = htons(((uint16_t *) p)[5]);
		    p += 12;

		    if (p + dlen <= e) 
		    {			
                //if(p_host->ip[0] == NULL) printf("!\n");
                memcpy(p_host->ip[found], p, dlen); 
		    }
            p += dlen;
            if(++found == header->nanswers) stop = 1;
            
		}
		else
		{
			stop = 1;
		}
	}

    printf("find a url:%s=>", p_host->name);
    for (i = 0; i < found; i++) {
        printf(" %s ", inet_ntoa(*(p_host->ip[i])));
    }
    printf("\n");
    p_host->is_dns = 1;

//    pthread_create(&download_page_thread, NULL, download_page, p_host);

}

void dns_poll(int sockfd)
{
    unsigned char pkt[2048];
    int n;

    while ((n = recvfrom(sockfd, pkt, sizeof(pkt), 0, NULL, NULL)) > 0 && n > (int) sizeof(struct header))
		parse_udp(pkt, n);
}
