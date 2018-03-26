#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define SERVER_PORT 12345
#define BUF_SIZE 4096
#define CMD_LENGTH 200

void fatal(char *string);

int main(int argc, char **argv)
{
    int c, s, bytes;
    char buf[BUF_SIZE];
    struct hostent *h;
    struct sockaddr_in channel;

    if (argc != 3)
        fatal("Usage: client server-name file-name");

    h = gethostbyname(argv[1]); /* look up host’s IP address */
    if (!h)
        fatal("gethostbyname failed");

    s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0)
        fatal("socket");

    memset(&channel, 0, sizeof(channel));
    channel.sin_family = AF_INET;
    memcpy(&channel.sin_addr.s_addr, h->h_addr, h->h_length);
    channel.sin_port = htons(SERVER_PORT);
    c = connect(s, (struct sockaddr *) &channel, sizeof(channel));
    if (c < 0)
        fatal("connect failed");

    char cmd[CMD_LENGTH];
    while (1)
    {
        scanf("%s\n", cmd);
        write(s, cmd, strlen(cmd) + 1);
        while (1)
        {
            bytes = read(s, buf, BUF_SIZE);
            if (bytes <= 0)
                break;
            write(1, buf, bytes);
        }
    }
}

void fatal(char *string)
{
    printf("%s\n", string);
}
