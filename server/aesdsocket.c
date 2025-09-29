#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>

#define PORT 9000

static volatile sig_atomic_t exit_requested = 0;
static int socketFd = -1;
static int fd = -1;

void signalHandler(int signo)
{
    exit_requested = 1;
    if (socketFd != -1)
    {
        shutdown(socketFd, SHUT_RDWR);
        close(socketFd);
        socketFd = -1;
    }
    if (fd != -1)
    {
        close(fd);
        fd = -1;
    }
    syslog(LOG_INFO, "Caught signal, exiting");
}

int daemonize(void)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        return -1;
    }
    if (pid > 0)
    {
    /* parent exits */
        exit(EXIT_SUCCESS);
    }
    if (setsid() < 0)
    {
        return -1;
    }

    if (chdir("/") != 0)
    {
        return -1;
    }
    umask(0);
    /* close standard fds */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    return 0;
}

int main(int argc, char *argv[])
{
    int isDaemon = 0;
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-d") == 0) 
        {
            isDaemon = 1;
        }
        else
        {
            fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }
    /* Setup signal handlers */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    char buffer[4096] = { 0 };
    openlog("aesdsocket", 0, LOG_USER);

    socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if(socketFd < 0)
    {
        syslog(LOG_ERR, "socket");
        return EXIT_FAILURE;
    }
    int opt = 1;
    if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        syslog(LOG_ERR, "setsockopt failed: %s", strerror(errno));
        close(socketFd);
        return EXIT_FAILURE;
    }
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    int retval = bind(socketFd, (struct sockaddr*)&address, sizeof(address));
    if (retval < 0)
    {
        syslog(LOG_ERR, "bind");
        close(socketFd);
        return EXIT_FAILURE;
    }

    if (isDaemon)
    {
        if (daemonize() != 0)
        {
            syslog(LOG_ERR, "daemonize failed: %s", strerror(errno));
            close(socketFd);
            return EXIT_FAILURE;
        }
    }

    retval = listen(socketFd,255);
    if (retval < 0)
    {
        syslog(LOG_ERR, "listen");
        close(socketFd);
        return EXIT_FAILURE;
    }
    char* writefile = "/var/tmp/aesdsocketdata";
    fd = open(writefile, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd == -1)
    {
        syslog(LOG_ERR, "Could not open file %s", writefile);
        return 1;
    }
    while(!exit_requested)
    {
        char clientIp[INET_ADDRSTRLEN];
        socklen_t clientlen = sizeof(address);
        int newSocket = accept(socketFd, (struct sockaddr*)&address, &clientlen);
        if (newSocket < 0)
        {
            close(socketFd);
            return EXIT_FAILURE;
        }
        else
        {
            //Log accepted connection
            inet_ntop(AF_INET, &address.sin_addr, clientIp, sizeof(clientIp));
            syslog(LOG_INFO, "Accepted connection from %s", clientIp);
        }
        ssize_t writelen;
        char *packetbuf = NULL;      // growing dynamic buffer
        size_t packetlen = 0;
        size_t packetsize = 0;
        while((writelen = recv(newSocket, buffer, sizeof(buffer), 0)) > 0)
        {
            for (ssize_t i = 0; i < writelen; i++)
            {
                // grow buffer if needed
                if (packetlen + 1 >= packetsize)
                {
                    size_t newsize = packetsize == 0 ? 1024 : packetsize * 2;
                    char *tmp = realloc(packetbuf, newsize);
                    if (!tmp)
                    {
                        syslog(LOG_ERR, "malloc/realloc failed");
                        free(packetbuf);
                        packetbuf = NULL;
                        packetlen = packetsize = 0;
                        close(newSocket);
                        return EXIT_FAILURE;
                    }
                    packetbuf = tmp;
                    packetsize = newsize;
                }

                packetbuf[packetlen++] = buffer[i];

                if (buffer[i] == '\n')
                {
                    // write packet
                    write(fd, packetbuf, packetlen);

                    // reset packet
                    packetlen = 0;

                    // send back full file
                    lseek(fd, 0, SEEK_SET);
                    char sendbuf[4096];
                    ssize_t r;
                    while ((r = read(fd, sendbuf, sizeof(sendbuf))) > 0)
                    {
                        send(newSocket, sendbuf, r, 0);
                    }
                }
            }
        }
        close(newSocket);
        syslog(LOG_INFO, "Closed connection from %s", clientIp);
        free(packetbuf);
    }
    if (socketFd != -1)
    {
        close(socketFd);
        socketFd = -1;
    }
    if (fd != -1)
    {
        int status = close(fd);
        if (status == -1)
        {
            syslog(LOG_ERR, "Could not close file %s", writefile);
            return 1;
        }
    }
    closelog();
    return 0;
}
