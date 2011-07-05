#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <zlib.h>

#include "spider.h"

#define USERAGENT "Wget/1.10.2"
#define ACCEPT "text/html"
#define ACCEPTLANGUAGE "zh-cn,zh;q=0.5"
#define ACCEPTENCODING "gzip,deflate"
#define ACCEPTCHARSET "gb2312,utf-8;q=0.7,*;q=0.7"
#define KEEPALIVE "300"
#define CONNECTION "close"
#define CONTENTTYPE "application/x-www-form-urlencoded"


static int sockfd;
static struct sockaddr_in server_addr;
static char request[409600] = "", buffer[1024] = "", httpheader[1024] = "";

static int ConnectWeb(struct tw_hostent* p_host) { /* connect to web server */
    //printf("%s\n", p_host->name);
    /* create a socket descriptor */
    if((sockfd=socket(PF_INET,SOCK_STREAM,0))==-1)
    {
        fprintf(stderr,"\tSocket Error:%s\a\n",strerror(errno));
        return 1;
    }

    /* bind address */
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    if(p_host->first == 1) server_addr.sin_addr = *((struct in_addr *)(*(p_host->dns->h_addr_list)));
    else server_addr.sin_addr = *(p_host->ip[0]);


    /* connect to the server */
    if(connect(sockfd, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr)) == -1)
    {
        fprintf(stderr, "%s \tConnect Error:%s\a\n", p_host->name, strerror(errno));
        return 2;
    }
    return 0;
}

static void SendRequest(void) { /* send my http-request to web server */
    int dsend = 0,totalsend = 0;
    int nbytes=strlen(request);
    while(totalsend < nbytes) {
        dsend = write(sockfd, request + totalsend, nbytes - totalsend);
        if(dsend==-1)  {fprintf(stderr, "\tsend error!%s\n", strerror(errno));exit(0);}
        totalsend+=dsend;
        //fprintf(stdout, "\n\tRequest.%d bytes send OK!\n", totalsend);
    }
}

static void ReceiveResponse(struct tw_hostent* p_host) 
{ /* get response from web server */
    fd_set readfds;
    struct timeval tival;
    int retry = 0, write_count;
    FILE * localfp = NULL;
    char *p;

    int i=0,j = 0, ret = 0, nbytes;

__ReCeive:
    FD_ZERO(&readfds);
    tival.tv_sec = 10;
    tival.tv_usec = 0;
    if(sockfd > 0) FD_SET(sockfd, &readfds);
    else 
    {
        fprintf(stderr, "\n\tError, socket is negative!\n"); 
        exit(0);
    }

    ret = select(sockfd + 1, &readfds, NULL, NULL, &tival);
    if(ret ==0 ) 
    {
        if(retry++ < 10) goto __ReCeive;
    }
    if(ret <= 0) 
    {
        fprintf(stderr, "\n\tError while receiving!\n"); 
        exit(0);
    }

    if(FD_ISSET(sockfd, &readfds))    
    {
        memset(buffer, 0, 1024);
        memset(httpheader, 0, 1024);
        if((localfp = fopen(p_host->file, "w")) == NULL) 
        {
            fprintf(stderr, "create file '%s' error\n", p_host->file); 
            return;
        }
        /* receive data from web server */
        while((nbytes=read(sockfd,buffer,1024)) > 0)
        {
            
//            if(i < 4)  { /* 获取 HTTP 消息头 */
//                if(buffer[0] == '\r' || buffer[0] == '\n')  i++;
//                else i = 0;
//                memcpy(httpheader + j, buffer, 1); j++;
//            }
//            else  { /* 获取 HTTP 消息体 */
//               fprintf(localfp, "%c", buffer[0]); /* print content on the screen */
//                i++;
//            }
            p = buffer;            
            while((write_count = fwrite(p, 1, nbytes, localfp)) > 0)
            {
                if (write_count == nbytes) {
                    break;
                }
                else
                {
                    p += write_count;
                    nbytes -= write_count;
                }
            }
        }
    }
    fclose(localfp);
}

static void BuildRequest(struct tw_hostent* p_host)
{
    char UserAgent[1024] = "", Accept[1024] = "", AcceptLanguage[1024] = "", AcceptEncoding[1024] = "", AcceptCharset[1024] = "", KeepAlive[1024] = "", Connection[1024] = "", ContentType[1024] = "";

    memcpy(UserAgent, USERAGENT, strlen(USERAGENT));
    memcpy(Accept, ACCEPT, strlen(ACCEPT));
    memcpy(AcceptLanguage, ACCEPTLANGUAGE, strlen(ACCEPTLANGUAGE));
    memcpy(AcceptEncoding, ACCEPTENCODING, strlen(ACCEPTENCODING));
    memcpy(AcceptCharset, ACCEPTCHARSET, strlen(ACCEPTCHARSET));
    memcpy(KeepAlive, KEEPALIVE, strlen(KEEPALIVE));
    memcpy(Connection, CONNECTION, strlen(CONNECTION));
    memcpy(ContentType, CONTENTTYPE, strlen(CONTENTTYPE));

    sprintf(request, "GET / HTTP/1.1\r\nHost: %s\r\nUser-Agent: %s\r\nAccept: %s\r\nConnection: %s\r\n\r\n", p_host->name, UserAgent, Accept, Connection);

}

void DoOnce(struct tw_hostent* p_host) { /* send and receive */
    if(ConnectWeb(p_host) != 0) goto __close; /* connect to the web server */

    /*Build a request*/
    BuildRequest(p_host);

    /* send a request */
    SendRequest();

    /* receive a response message from web server */
    ReceiveResponse(p_host);

__close:
    close(sockfd); /* because HTTP protocol do something one connection, so I can close it after receiving */
}

static void check_url(char* p_url, struct tw_hostent* p_host)
{
    char* pa, *pb;
    
    pa = strchr(p_url, '/');
    if (pa != NULL) {
        *pa = '\0';
    }
    if((pb = strchr(p_url, '.'))!= NULL)
    {
        if(strcmp(p_host->key_word, pb + 1) == 0)
        {
            init_node(p_url);
            //printf("find a url:%s\n", p_url);
        }
    }
    return;
}

void find_url(struct tw_hostent* p_host)
{
    int fd, len;
    int flength = 0;
    char* pa, *pb, *pc, *my_url, *mapped_mem;

    fd = open(p_host->file, O_RDONLY);
    if(fd == -1)        
        goto __AnalyzeDone;
    flength = lseek(fd, 1, SEEK_END);
    write(fd, "\0", 1);
    lseek(fd, 0, SEEK_SET);
    mapped_mem = mmap(0, flength, PROT_READ, MAP_SHARED, fd, 0);
    pa = mapped_mem;
    do {
        if((pb = strstr(pa, "href='http://"))){
            pc = strchr(pb + 13, '\'');
            len = strlen(pb + 13) - strlen(pc);
            my_url = (char*)malloc(len + 1);
            memcpy(my_url, pb + 13, len);
            check_url(my_url, p_host);
        }
        else if((pb = strstr(pa, "href=\"http://"))){
            pc = strchr(pb + 13, '"');
            len = strlen(pb + 13) - strlen(pc);
            my_url = (char*)malloc(len + 1);
            memcpy(my_url, pb + 13, len);
            check_url(my_url, p_host);
        }
        else {goto __returnLink ;}
        if(pc + 1) pa = pc + 1;
    }while(pa);
__returnLink:
    close(fd);
    munmap(mapped_mem, flength);
__AnalyzeDone:
    close(fd);
}

