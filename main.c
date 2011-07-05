#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/select.h>
#include <signal.h>
#include <unistd.h>

#include "spider.h"
#include "adns.h"


struct tw_head head;
int epfd;
struct epoll_event events[1000];
struct tw_hostent* hostent_index[1000];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void init_head(struct tw_head* p_head)
{
    p_head->count = 0;
    p_head->head = NULL;
    p_head->end = NULL;
    p_head->sum = 0;

    return;
}

static void join_list(struct tw_hostent **p_host)
{
    if (head.count == 0) {
        if((*p_host)->is_dns == 0) head.count++;
        head.head = head.end = *p_host;
        head.sum++;
    }
    else {
        if((*p_host)->is_dns == 0) head.count++;
        head.end->next = *p_host;
        (*p_host)->prev = head.end;
        head.end = *p_host;
        head.sum++;
    }
}

static unsigned char find_record(const char *p_name)
{
    struct tw_hostent *p_record;

    if(head.head == NULL) return 1;
    for(p_record = head.head; p_record != NULL; p_record = p_record->next)
    {
        if(strcmp(p_record->name, p_name) == 0) return 0;
    }
    return 1;
    
}

void init_node(const char* p_name)
{
    struct tw_hostent* p_host;
    char* pa;
    int i;


//    pthread_mutex_lock(&mutex);

    if(find_record(p_name) == 1)
    {
        //printf("find a url:%s\n", p_name);
        p_host = (struct tw_hostent*)malloc(sizeof(struct tw_hostent));
        memcpy(p_host->name, p_name, 30);
        memcpy(p_host->file, p_name, 30);
        if ((pa = strchr(p_name, '.')) != NULL) {
            memcpy(p_host->key_word, pa + 1, 30);
        }
        p_host->is_dns = 0;
        p_host->is_url = 0;
        p_host->prev = NULL;
        p_host->next = NULL;
        p_host->id = head.sum;
        p_host->first = 0;
        hostent_index[p_host->id] = p_host;    
        for (i = 0; i < 100; i++) {
            p_host->ip[i] = malloc(4);
        }
        p_host->dns = NULL;

        query_dns(p_host);

        join_list(&p_host);    
    }
    
//    pthread_mutex_unlock(&mutex);
    return;

}

static void disp_result(void)
{
    struct tw_hostent *p;
    for(p = head.head; p != NULL; p = p->next)
    {
        printf("find a url:%s\n", p->name);
    }
    exit(1);
}

void *dns_recv(void *arg)
{
    int i, nfds;

    while(1)
    {
        nfds = epoll_wait(epfd, events, 1000, -1);
        
        for (i=0; i<nfds; i++)
        {
            if(events[i].events & EPOLLIN) 
            {
                //printf("a dns recv\n");
                dns_poll(events[i].data.fd);
            }
        }
    }

    return NULL;

}

static void sig_int(int signo)
{
    char flag = 'y';
    char ip[20];
    char cmd[50];

    signal(SIGINT, SIG_DFL);
    if(signo == SIGINT)
        printf("receive SIGINT\n");
    while (flag == 'y') {
        memset(ip, 0, 20);
        memset(cmd, 0, 50);
        memcpy(cmd, "./syn.pl ", 10);
        printf("Which ip you want to check:\n");
        fscanf(stdin, "%s", ip);
        //printf("ip is %s\n", ip);
        strcat(cmd, ip);
        //printf("%s\n", cmd);
        system(cmd);
        printf("do you want to continue:(y/n)\n");
        fgetc(stdin);
        flag = fgetc(stdin);
    }
    
    kill(getpid(), SIGINT);
}

int main(int argc, const char *argv[])
{
    struct tw_hostent* p_host, *p;
    int i, j, retry = 0;
    char *pa;
    pthread_t dns_recv_pthread;

    if (argc < 2) {
        fprintf(stderr, "%s website1, website2 ...", argv[0]);
    }

    if(signal(SIGINT, sig_int) == SIG_ERR){
        perror("signal error");
    }

    init_head(&head);

    init_dns();
    epfd = epoll_create(1000);

    for (i = 0; i < argc - 1; i++) {
        p_host = (struct tw_hostent*)malloc(sizeof(struct tw_hostent));
        memcpy(p_host->name, argv[i + 1], 30);
        memcpy(p_host->file, argv[i + 1], 30);
        if ((pa = strchr(argv[i + 1], '.'))!= NULL) {
            memcpy(p_host->key_word, pa + 1, 30);
        }
        if((p_host->dns = gethostbyname(argv[i + 1])) == NULL){
            fprintf(stderr, "gethostbyname error");
            return 1;
        }
        p_host->is_dns = 1;
        p_host->first = 1;
        p_host->prev = p_host->next = NULL;
        join_list(&p_host);

        DoOnce(p_host);
        find_url(p_host);
        p_host->is_url = 1;

    }

    pthread_create(&dns_recv_pthread, NULL, dns_recv, NULL);

__again:
    for(p = head.head; p != NULL; p = p->next)
    {
        if(p->is_dns == 0) 
        {
           // printf("still in __again\n");
            goto __again; 
        }
    }
    printf("1 round is well done!\n");
    for(p = head.head; p != NULL; p = p->next)
    {
        if(p->is_url == 0){
            DoOnce(p);
            find_url(p);
            p->is_url = 1;
        }
    }
    printf("2 round is well done!\n");

//      disp_result();

    return 0;
}
