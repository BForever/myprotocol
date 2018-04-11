#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#define SERVER_PORT 2277
#define BUF_SIZE 4096
#define CMD_LENGTH 200
#define CLIENT_NAME_SIZE 32

enum msg_type
{
    UNKNOWN = 0, TIME, NAME, LIST, MESSAGE, REPLY, EXIT, DIS
};
typedef struct
{
    enum msg_type type;
    int length;
} Data;

struct socketinfo
{
    int sa;
    char name[CLIENT_NAME_SIZE];
    char ip[16];
    int port;
};

void fatal(char *string);

void setblocking(int sa, int blocking);

enum msg_type getcmd(char *);

void *listen_msg(void *s);

pthread_mutex_t msg_mutex = PTHREAD_MUTEX_INITIALIZER;
int quit = 0;

int main(int argc, char **argv)
{
    int c, s, bytes, i, n;
    char buf[BUF_SIZE];
    char ip[16];
    struct hostent *h;
    struct sockaddr_in channel;
    pthread_mutex_unlock(&msg_mutex);//init mutex

    Data data;
    char cmd[CMD_LENGTH];

    while (1)
    {
        printf("\n -------------------------------- \n");
        printf("| Welcome to Client version 0.1! |\n");
        printf("|--------------------------------|\n");
        printf("| Type server ip to connect:     |\n");
        printf("| (or just \"exit\")               |\n");
        printf(" -------------------------------- \n");
        printf("[client]");
        fflush(stdout);
        scanf("%s", ip);
        if (!strcasecmp(ip, "exit"))
        {
            printf("[client] Exited. Welcome back!\n");
            break;
        }

        h = gethostbyname(ip); /* look up host’s IP address */
        if (!h)
        {
            fatal("[client]gethostbyname failed.");
            continue;
        }


        s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s < 0)
        {
            fatal("[client]create socket failed.");
            continue;
        }

        memset(&channel, 0, sizeof(channel));
        channel.sin_family = AF_INET;
        memcpy(&channel.sin_addr.s_addr, h->h_addr, h->h_length);
        channel.sin_port = htons(SERVER_PORT);
        c = connect(s, (struct sockaddr *) &channel, sizeof(channel));
        if (c < 0)
        {
            fatal("[client]connect failed.");
            continue;
        }


        // Send client name
        FILE *p = popen("hostname", "r");
//    read(p->_file, buf, CLIENT_NAME_SIZE);
        fgets(buf, CLIENT_NAME_SIZE, p);
        buf[CLIENT_NAME_SIZE - 1] = 0;
        buf[strlen(buf) - 1] = 0;
        write(s, buf, CLIENT_NAME_SIZE);
        pclose(p);
        getchar();
        pthread_t listener;
        quit = 0;
        pthread_create(&listener, NULL, listen_msg, &s);

        while (1)
        {
            if (quit)break;
            pthread_mutex_unlock(&msg_mutex);
            printf("\n ------------------------------------------- \n");
            printf("| Choose what you want to do:               |\n");
            printf("| @ time: get server's time now             |\n");
            printf("| @ name: get server's name                 |\n");
            printf("| @ list: list all online clients           |\n");
            printf("| @ msg : send message to certain client    |\n");
            printf("| @ dis : disconnect from present server    |\n");
            printf("| @ exit: exit the client                   |\n");
            printf(" ------------------------------------------- \n");
            printf("[client]");
            fflush(stdout);
            scanf("%s", cmd);

            //printf("%s\n", cmd);

            data.type = getcmd(cmd);

            pthread_mutex_lock(&msg_mutex);
            switch (data.type)
            {
                case TIME:
                {
//                printf("time cmd,send request\n");
                    data.length = 0;
                    write(s, &data, sizeof(Data));
//                printf("req sent\n");
                    read(s, &data, sizeof(Data));
//                printf("response recved\n");
                    read(s, buf, (size_t) data.length);
//                printf("time recved\n");
                    struct tm *tp = (struct tm *) buf;
                    printf("[client] Server time: %02d:%02d:%02d %02d/%02d/%4d\n", tp->tm_hour, tp->tm_min,
                           tp->tm_sec,
                           tp->tm_mday,
                           1 + tp->tm_mon, 1900 + tp->tm_year);
                    break;
                }
                case NAME:
                {
//                printf("name cmd,send request\n");
                    data.length = 0;
                    write(s, &data, sizeof(Data));
//                printf("req sent\n");

                    read(s, &data, sizeof(Data));
//                printf("response recved\n");
                    read(s, buf, (size_t) data.length);
                    buf[data.length - 1] = 0;
//                printf("name recved\n");

                    printf("[client] Server name: %s\n", buf);
                    break;
                }
                case LIST:
                {
//                printf("list cmd,send request\n");
                    data.length = 0;
                    write(s, &data, sizeof(Data));
//                printf("req sent\n");

                    read(s, buf, sizeof(int));
                    n = *(int *) buf;
                    printf("[client] Connected clients: %d\n", n);
                    printf("id\tname\n");
                    for (i = 0; i < n; i++)
                    {
                        read(s, buf, sizeof(int));
                        printf("%d\t", *(int *) buf);
                        read(s, buf, CLIENT_NAME_SIZE);
                        printf("%s\n", buf);
                    }
                    break;
                }
                case MESSAGE:
                {
                    int id;
                    printf("[client] target client id:\n");
                    scanf("%d", &id);
                    printf("[client] Write the message you want to send and enter:\n");
                    scanf("%s", cmd);

                    data.length = strlen(cmd) + sizeof(int) + 1;
                    *(int *) buf = id;
                    memcpy(buf + sizeof(int), cmd, data.length);
                    write(s, &data, sizeof(Data));
                    write(s, buf, data.length);
//                printf("req sent\n");
                    read(s, &data, sizeof(Data));
                    if (data.length)
                    {
                        printf("[client] message successfully sent.\n");
                    } else
                    {
                        printf("[client] target client not found.\n");
                    }
                    break;
                }
                case DIS:
                {
                    void *ret;
                    quit = 1;
                    pthread_join(listener, &ret);
                    continue;
                }
                case EXIT:
                {
                    printf("[client] Exited. Welcome back!\n");
                    exit(0);
                }
                default:
                {
                    break;
                }
            }
        }
    }


}

void *listen_msg(void *psocket)
{
    int s = *(int *) psocket;
    int status;
    char *buf = malloc(sizeof(BUF_SIZE));
    Data data;
    while (1)
    {
        pthread_mutex_lock(&msg_mutex);
        setblocking(s, 0);
        status = read(s, &data, sizeof(Data));
        if (status == 0)
        {
            // Connection lost
            setblocking(s, 1);
            pthread_mutex_unlock(&msg_mutex);
            break;
        } else if (status > 0)
        {
            struct socketinfo info;
            // Get msg
            read(s, buf, data.length);
            read(s, &data, sizeof(Data));
            read(s, &info, data.length);
            if (data.length != sizeof(info))
            {
                fatal("data length are not the same, abort message.");
                setblocking(s, 1);
                pthread_mutex_unlock(&msg_mutex);
                usleep(5000);
                continue;
            }
            buf[data.length] = 0;// For safety
            printf("Message from client %d: %s(%s:%d)\n", info.sa, info.name, info.ip, info.port);
            printf("%s\n[client]", buf);
            fflush(stdout);
        }
        setblocking(s, 1);
        pthread_mutex_unlock(&msg_mutex);
        usleep(5000);
        if (quit)break;
    }
    free(buf);
    close(s);
    return NULL;
}

void fatal(char *string)
{
    printf("%s\n", string);
}

enum msg_type getcmd(char *cmd)
{
    if (!strcasecmp(cmd, "time"))return TIME;
    if (!strcasecmp(cmd, "name"))return NAME;
    if (!strcasecmp(cmd, "list"))return LIST;
    if (!strcasecmp(cmd, "msg"))return MESSAGE;
    if (!strcasecmp(cmd, "dis"))return DIS;
    if (!strcasecmp(cmd, "exit"))return EXIT;
    return UNKNOWN;
}

void setblocking(int sa, int blocking)
{
    int flags = fcntl(sa, F_GETFL, 0);
    if (flags == -1)
    {
        printf("[client] Errors while setting socket non-blocking!(get flag)\n");
        return;
    }
    flags = (blocking ? flags & ~O_NONBLOCK : flags | O_NONBLOCK);
    if (fcntl(sa, F_SETFL, flags) != 0)
    {
        printf("[client] Errors while setting socket non-blocking!(set flag)\n");
        return;
    }
//    printf("Socket setting finished.\n");
}