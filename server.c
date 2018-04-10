#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <arpa/inet.h>


#define SERVER_PORT 2277
#define BUF_SIZE 4096
#define QUEUE_SIZE 10
#define CLIENT_NAME_SIZE 32

enum msg_type
{
    UNKNOWN = 0, TIME, NAME, LIST, MESSAGE, REPLY
};
typedef struct
{
    enum msg_type type;
    int length;
} Data;

typedef struct clientrecord *Client;
struct socketinfo
{
    int sa;
    char name[CLIENT_NAME_SIZE];
    char ip[16];
    int port;
};
struct clientrecord
{
    pthread_mutex_t *mutex;
    pthread_t tid;
    struct socketinfo info;
    char *buf;
    char *message;
    int mesflag;
    Client next;
};


void fatal(char *);

void setblocking(int sa, int blocking);

void *reply(void *arg);

pthread_mutex_t head_mutex = PTHREAD_MUTEX_INITIALIZER;
Client head = NULL;
int headcnt = 0;

int main(int argc, char *argv[])
{
    int s, b, l, fd, bytes, on = 1;
    char buf[BUF_SIZE];
    struct sockaddr_in channel;
    pthread_mutex_unlock(&head_mutex);// Init


    memset(&channel, 0, sizeof(channel));
    channel.sin_family = AF_INET;
    channel.sin_addr.s_addr = htonl(INADDR_ANY);
    channel.sin_port = htons(SERVER_PORT);

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) fatal("[server] socket failed");

    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on));
    b = bind(s, (struct sockaddr *) &channel, sizeof(channel));
    if (b < 0) fatal("[server] bind failed");

    l = listen(s, QUEUE_SIZE); /* specify queue size */
    if (l < 0) fatal("[server] listen failed");

    while (1)
    {
//        printf("malloc client.\n");
        Client newclient = malloc(sizeof(struct clientrecord));

        newclient->info.sa = accept(s, 0, 0);

        // Get ip address
        struct sockaddr_in addr;
        memset(&addr,0,sizeof(addr));
        unsigned int len = sizeof(addr);
        getsockname(newclient->info.sa,(struct sockaddr*)&addr,&len);
        char* ip = inet_ntoa(addr.sin_addr);
        ip[15]=0;
        memcpy(newclient->info.ip,inet_ntoa(addr.sin_addr),strlen(ip));
        newclient->info.port=ntohs(addr.sin_port);
        printf("new connection from %s:%d\n",newclient->info.ip,newclient->info.port);


//        printf("new connection\n");
        if (newclient->info.sa < 0)
        {
            fatal("[server] accept failed");
            free(newclient);
        } else
        {
            // Set new thread
//            printf("set mutex\n");
            newclient->mutex = malloc(sizeof(pthread_mutex_t));
            pthread_mutex_init(newclient->mutex, NULL);
//            printf("unlock mutex\n");
            pthread_mutex_unlock(newclient->mutex);

//            printf("set buf\n");
            newclient->buf = malloc(BUF_SIZE);
            newclient->message = malloc(BUF_SIZE);

//            printf("read name\n");
            read(newclient->info.sa, newclient->info.name, CLIENT_NAME_SIZE);

            newclient->mesflag = 0;

            // Add to clients list
//            printf("add to list\n");
            pthread_mutex_lock(&head_mutex);
            newclient->next = head;
            head = newclient;
            headcnt++;
            pthread_mutex_unlock(&head_mutex);
            // Start new thread
            printf("[server] new connection from %s, id %d assigned.\n",newclient->info.name,newclient->info.sa);
            pthread_create(&newclient->tid, NULL, reply, (void *) newclient);
        }
    }
}

void *reply(void *arg)
{
    Client this = arg;
    int sa = this->info.sa;
    char *buf = this->buf;
    Data data;
    int bytes;
//    printf("new thread created.\n");


    while (1)
    {
        //read command type
        setblocking(sa, 0);
        bytes = read(sa, &data, sizeof(Data));

        if (bytes > 0)
        {
            setblocking(sa, 1);
//            printf("request recved.\n");
            switch (data.type)
            {
                case TIME:
                {
//                    printf("time req.\n");
                    // Get time
                    time_t t;
                    time(&t);
                    struct tm *tp;
                    tp = localtime(&t);
//                    printf("Server time: %02d:%02d:%02d %02d/%02d/%4d\n", tp->tm_hour, tp->tm_min, tp->tm_sec,
//                    tp->tm_mday,
//                            1 + tp->tm_mon, 1900 + tp->tm_year);
                    // Get struct size
                    bytes = sizeof(struct tm);

                    // Encapsulate
                    memcpy(buf, tp, bytes);
                    data.length = bytes;

                    // Send
                    write(sa, &data, sizeof(Data));
                    write(sa, buf, bytes);
//                    printf("time sent.\n");
                    break;
                }

                case NAME:
                {
                    int i;
//                    printf("name req.\n");
                    // Get name
                    FILE *p = popen("hostname", "r");

                    // Get struct size
                    bytes = read(fileno(p), buf, BUF_SIZE);

                    (*(char *) (buf + CLIENT_NAME_SIZE - 1)) = 0;
                    (*(char *) (buf + strlen(buf) - 1)) = 0;
                    data.length = bytes;
                    write(sa, &data, sizeof(Data));
                    write(sa, buf, bytes);
                    pclose(p);

//                    printf("name sent.\n");
                    break;
                }
                case LIST:
                {
                    Client tmp;
//                    printf("list req.\n");

                    pthread_mutex_lock(&head_mutex);
                    tmp = head;
                    write(sa, &headcnt, sizeof(int));
                    while (tmp)
                    {
                        write(sa, &tmp->info.sa, sizeof(int));
                        write(sa, tmp->info.name, CLIENT_NAME_SIZE);
                        tmp = tmp->next;
                    }
                    pthread_mutex_unlock(&head_mutex);
//                    printf("list sent.\n");
                    break;
                }
                case MESSAGE:
                {
                    data.type=REPLY;
                    read(sa, buf, data.length);
                    int target = *(int *) buf;
                    char *text = buf + sizeof(int);
                    // Find target client
                    Client tmp;
                    pthread_mutex_lock(&head_mutex);
                    tmp = head;
                    while (tmp && tmp->info.sa != target)tmp = tmp->next;
                    pthread_mutex_unlock(&head_mutex);
                    if (tmp)
                    {
                        pthread_mutex_lock(tmp->mutex);
                        // Wait for the former message to be sent
                        while (tmp->mesflag)
                        {
                            pthread_mutex_unlock(tmp->mutex);
                            usleep(5000);
                            pthread_mutex_lock(tmp->mutex);
                        }
                        memcpy(tmp->message, text, data.length);
                        memcpy(tmp->buf,&this->info, sizeof(struct socketinfo));
                        tmp->mesflag = 1;
                        pthread_mutex_unlock(tmp->mutex);

                        data.length=1;
                        write(sa,&data, sizeof(Data));
                    } else
                    {
                        // Client not found, reply negative
                        data.length=0;
                        write(sa,&data, sizeof(Data));
                    }
                    break;
                }
                default:
                    break;

            }
        } else if (!bytes)
        {
            // Connection lost
            printf("[server] %d:%s lost connection.\n", this->info.sa,this->info.name);
            pthread_mutex_lock(&head_mutex);
            Client tmp = head;
            Client prev;
            while (tmp != this && tmp)
            {
                prev = tmp;
                tmp = tmp->next;
            }
            if (tmp)
            {
                if (head == tmp)
                {
                    head = head->next;
                } else
                {
                    prev->next = tmp->next;
                }
                pthread_mutex_destroy(tmp->mutex);
                free(tmp->buf);
                free(tmp->message);
                close(tmp->info.sa);
                free(tmp);
                headcnt--;
            } else
            {
                fatal("[server] Thread not found!!!");
                while (1);
            }
            pthread_mutex_unlock(&head_mutex);
            break;

        } else
        {
            // Check message
            // printf("no bytes received.\n");
            if (this->mesflag)
            {
                // Get message from client
                pthread_mutex_lock(this->mutex);
                Data data;
                data.type = MESSAGE;
                data.length = strlen(this->message)+1;
                write(sa, &data, sizeof(Data));
                write(sa, this->message, data.length);
                data.length= sizeof(struct socketinfo);
                write(sa, &data, sizeof(Data));
                write(sa,this->buf, sizeof(struct socketinfo));

                this->mesflag = 0;
                pthread_mutex_unlock(this->mutex);
            }
            usleep(5000);

        }


    }
    return NULL;
}

void fatal(char *string)
{
    printf("%s\n", string);
}

void setblocking(int sa, int blocking)
{
    int flags = fcntl(sa, F_GETFL, 0);
    if (flags == -1)
    {
        printf("[server] Errors while setting socket non-blocking!(get flag)\n");
        return;
    }
    flags = (blocking ? flags & ~O_NONBLOCK : flags | O_NONBLOCK);
    if (fcntl(sa, F_SETFL, flags) != 0)
    {
        printf("[server] Errors while setting socket non-blocking!(set flag)\n");
        return;
    }
//    printf("Socket setting finished.\n");
}