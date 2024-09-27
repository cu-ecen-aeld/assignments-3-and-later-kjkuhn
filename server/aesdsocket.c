#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "string.h"
#include "sys/socket.h"
#include "sys/types.h"
#include "netinet/in.h"
#include "arpa/inet.h"
#include "errno.h"
#include "error.h"
#include "syslog.h"
#include "signal.h"
#include "time.h"
#include "sys/queue.h"
#include "pthread.h"




#define BUFFER_SIZE 1024

#ifndef USE_AESD_CHAR_DEVICE
#define USE_AESD_CHAR_DEVICE 1
#endif


#if USE_AESD_CHAR_DEVICE == 1
#define ASSIGNMENT_8
#endif


#ifdef ASSIGNMENT_8
#define OFN "/dev/aesdchar"
#else
#define OFN "/var/tmp/aesdsocketdata"
#endif /* ASSIGNMENT_8 */

struct client_t {
    struct sockaddr_in addr;
    int addr_len;
    int sd;
    pthread_t tid;
    LIST_ENTRY(client_t) entries;
};


int server;
volatile int run;
LIST_HEAD(client_list, client_t) cl_head;
pthread_mutex_t wr_mtx;

void sd_handler(int sig)
{
    if(sig == SIGINT || sig == SIGTERM)
    {
        syslog(LOG_INFO, "Caught signal, exiting");
        run = 0;
        shutdown(server, SHUT_RDWR);
    }
}

void* thread_entry(void *args)
{
    int recv_len;
    char buffer[BUFFER_SIZE];
    FILE *file;
    struct client_t *c = (struct client_t*)args;
    pthread_mutex_lock(&wr_mtx);
    file = fopen(OFN, "a");
    while(1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        recv_len = recv(c->sd, buffer, BUFFER_SIZE-1, 0);
        if(recv_len < 0)
        {
            goto t_exit_with_error;
        }
        else if(recv_len == 0)
        {
            //client terminated connection
            break;
        }
        fputs(buffer, file);
        if(recv_len < BUFFER_SIZE && buffer[recv_len-1] == '\n')
        {
            file = freopen(OFN, "r", file);
            while(fgets(buffer, BUFFER_SIZE, file) != 0)
            {
                if(send(c->sd, buffer, strlen(buffer), 0) < 0)
                {
                    goto t_exit_with_error;
                }
            }
            break;
        }
    }
    fclose(file);
    pthread_mutex_unlock(&wr_mtx);
    close(c->sd);
    syslog(LOG_INFO, "Closed connection from %s\n", inet_ntoa(c->addr.sin_addr));
    return 0;

t_exit_with_error:
    syslog(LOG_ERR, "%s (CODE %d)", strerror(errno), errno);
    close(c->sd);
    fclose(file);
    pthread_mutex_unlock(&wr_mtx);
    return 0;
}


void wait_for_threads()
{
    struct client_t *it;

    LIST_FOREACH(it, &cl_head, entries)
    {
        pthread_join(it->tid, 0);
    }
    while(!LIST_EMPTY(&cl_head))
    {
        it = LIST_FIRST(&cl_head);
        LIST_REMOVE(it, entries);
        free(it);
    }

}

#ifndef ASSIGNMENT_8
void timer_handler(union sigval args)
{
    FILE *f;
    time_t ct;
    struct tm *ti;
    char ts[100];

    time(&ct);
    ti = localtime(&ct);
    strftime(ts, 100, "timestamp:%a, %d %b %Y %H:%M:%S %z", ti);
    f = fopen(OFN, "a");
    pthread_mutex_lock(&wr_mtx);
    fprintf(f, "%s\n", ts);
    pthread_mutex_unlock(&wr_mtx);
    fclose(f);
}
#endif

int main(int argc, char **argv)
{
    struct sockaddr_in sa;
    struct client_t c;
    char buffer[BUFFER_SIZE];
    size_t len;
    int recv_len;
    FILE *file;
    int daemon = 0;
    int opt;
    struct client_t *entry;
#ifndef ASSIGNMENT_8
    timer_t timer;
    struct sigevent sev;
    struct itimerspec its;
#endif

    LIST_INIT(&cl_head);
    pthread_mutex_init(&wr_mtx, 0);

    while((opt = getopt(argc, argv, "d")) != -1)
    {
        switch(opt)
        {
            case 'd':
                daemon = 1;
                break;
        }
    }

    run = 1;
    signal(SIGINT, sd_handler);
    signal(SIGTERM, sd_handler);
    

    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER); 
    server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server < 0)
    {
        syslog(LOG_ERR, "[ERROR %d] %s\n", errno, strerror(errno));
        return -1;
    }
    memset(&sa, 0, sizeof(sa));
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(9000);
    if(setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &run, sizeof(run)) < 0)
    {
        goto return_error;
    }
    if(bind(server, (struct sockaddr*)&sa, sizeof(sa)) < 0)
    {
        goto return_error;
    }
    if(listen(server, 5) < 0 )
    {
        goto return_error;
    }

    if(daemon)
    {
        daemon = fork();
        if(daemon == -1)
            goto return_error;
        else if(daemon != 0)
        {
            syslog(LOG_INFO, "running daemonized");
            return 0;
        }
    }

#ifndef ASSIGNMENT_8
    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = timer_handler;
    sev.sigev_value.sival_ptr = NULL;
    timer_create(CLOCK_MONOTONIC, &sev, &timer);
    memset(&its, 0, sizeof(its));
    its.it_value.tv_sec = 10;
    its.it_interval.tv_sec = 10;
    timer_settime(timer, 0, &its, 0);
#endif
    while(run)
    {
        syslog(LOG_INFO, "waiting for connections on port %hd", ntohs(sa.sin_port));
        memset(&c, 0, sizeof(struct client_t));
        entry = (struct client_t*)calloc(1, sizeof(struct client_t));
        entry->sd = accept(server, (struct sockaddr*)&entry->addr, &entry->addr_len);
        if(entry->sd < 0)
        {
            free(entry);
            if(errno == EINTR || errno == EINVAL)
            {
                syslog(LOG_INFO, "Shutting down\n");
                break;
            }
            goto return_error;
        }
        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(c.addr.sin_addr));
        
        //TODO: create thread
        pthread_create(&entry->tid, 0, thread_entry, entry);
        LIST_INSERT_HEAD(&cl_head, entry, entries);
    }

    wait_for_threads();
    close(server);
#ifndef ASSIGNMENT_8
    remove(OFN);
#endif /* ASSIGNMENT_8 */
    closelog();
    return 0;


return_error:
    syslog(LOG_ERR, "[ERROR %d] %s\n", errno, strerror(errno));
    wait_for_threads();
    closelog();
    close(server);
    if(c.sd != 0)
        close(c.sd);
    return -1;
}