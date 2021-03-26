//
// Created by Freely Kathryne on 3/25/21.
//

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>

int g_id = 0;

typedef struct s_cli{
    int fd;
    int id;
    char* write_buf;
    char* read_buf;
    struct s_cli* next;
}               t_cli;

void wrong_args() {
    write(2, "Wrong number of arguments\n", 26);
    exit(1);
}

void fatal_error(){
    write(2, "Fatal error\n", 12);
    exit(1);
}

int extract_message(char **buf, char **msg)
{
    char	*newbuf;
    int	i;

    *msg = 0;
    if (*buf == 0)
        return (0);
    i = 0;
    while ((*buf)[i])
    {
        if ((*buf)[i] == '\n')
        {
            newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
            if (newbuf == 0)
                return (-1);
            strcpy(newbuf, *buf + i + 1);
            *msg = *buf;
            (*msg)[i + 1] = 0;
            *buf = newbuf;
            return (1);
        }
        i++;
    }
    return (0);
}

char *str_join(char *buf, char *add)
{
    char	*newbuf;
    int		len;

    if (buf == 0)
        len = 0;
    else
        len = strlen(buf);
    newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
    if (newbuf == 0)
        return (0);
    newbuf[0] = 0;
    if (buf != 0)
        strcat(newbuf, buf);
    free(buf);
    strcat(newbuf, add);
    return (newbuf);
}

int main(int ac, char** av) {
    if (ac != 2)
        wrong_args();

    int sockfd, connfd, maxfd, ret;
    struct sockaddr_in servaddr, cli;
    socklen_t len = sizeof (cli);
    t_cli* clients = NULL;
    fd_set fd_read, fd_write, cp_fd_read, cp_fd_write;
    char* buf_one = calloc(101000, 1);
    char* buf = NULL;
    char* msg = NULL;
    unsigned int addr = 2130706433;
    uint16_t port = atoi(av[1]);

    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = addr >> 24 | addr << 24; //127.0.0.1
    servaddr.sin_port = port >> 8 | port << 8;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        fatal_error();
    if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
        fatal_error();
    if (listen(sockfd, 10) != 0)
        fatal_error();

    maxfd = sockfd;
    FD_ZERO(&fd_read);
    FD_ZERO(&fd_write);
    FD_SET(maxfd, &fd_read);
    FD_SET(maxfd, &fd_write);

    while (1) {
        cp_fd_read = fd_read;
        cp_fd_write = fd_write;
        select(maxfd + 1, &cp_fd_read, &cp_fd_write, NULL, 0);
        if (FD_ISSET(sockfd, &cp_fd_read)) {
            connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
            if (connfd) {
                t_cli * tmp = clients;
                t_cli * new;
                new = calloc(sizeof (t_cli), 1);
                if (!new)
                    fatal_error();
                new->fd = connfd;
                new->id = g_id++;
                new->read_buf = NULL;
                new->write_buf = NULL;
                new->next = NULL;

                if (!clients)
                    clients = new;
                else {
                    while (tmp->next)
                        tmp = tmp->next;
                    tmp->next = new;
                }
                FD_SET(connfd, &fd_read);
                FD_SET(connfd, &fd_write);
                maxfd = (connfd > maxfd) ? connfd : maxfd;
                sprintf(buf_one, "server: client %d just arrived\n", g_id - 1);
                for (t_cli* x = clients; x; x = x->next) {
                    if (x->fd != connfd)
                        x->write_buf = str_join(x->write_buf, buf_one);
                }
                bzero(buf_one, 100);
            }
        }
        for (t_cli* cur= clients; cur; cur = cur->next) {
            if (FD_ISSET(cur->fd, &cp_fd_read) && cur->fd != sockfd) {
                buf = calloc(4097, 1);
                if (!buf)
                    fatal_error();
                ret = recv(cur->fd, buf, 4096, 0);
                if (ret <= 0) {
                    close(cur->fd);
                    sprintf(buf_one, "server: client %d just left\n", cur->id);
                    for (t_cli* tmp = clients; tmp; tmp = tmp->next) {
                        if (tmp->fd != cur->fd)
                            tmp->write_buf = str_join(cur->write_buf, buf_one);
                    }
                    if (cur->read_buf)
                        free(cur->read_buf);
                    if (cur->write_buf)
                        free(cur->write_buf);
                    t_cli * tmp = clients;
                    t_cli * del;
                    if (tmp->fd == cur->fd) {
                        clients = tmp->next;
                        free(tmp);
                    } else {
                        while (tmp->next && tmp->next->fd != cur->fd)
                            tmp = tmp->next;
                        del = tmp->next;
                        tmp->next = tmp->next->next;
                        free(del);
                    }
                    FD_CLR(cur->fd, &fd_read);
                    FD_CLR(cur->fd, &fd_write);
                    bzero(buf_one, 100);
                } else {
                    cur->read_buf = str_join(cur->read_buf, buf);
                    while (extract_message(&cur->read_buf, &msg)) {
                        sprintf(buf_one, "client %d: %s", cur->id, msg);
                        for (t_cli* tmp = clients; tmp; tmp = tmp->next) {
                            if (tmp->fd != cur->fd)
                                tmp->write_buf = str_join(cur->write_buf, buf_one);
                        }
                        free(msg);
                        msg = NULL;
                        bzero(buf_one, 101000);
                    }
                }
                free(buf);
            }
        }
        for (t_cli* cur = clients; cur; cur = cur->next) {
            if (FD_ISSET(cur->fd, &cp_fd_write) && cur->write_buf) {
                ret = send(cur->fd, cur->write_buf, strlen(cur->write_buf), 0);
                if (ret == -1) {
                    close(cur->fd);
                    sprintf(buf_one, "server: client %d just left\n", cur->id);
                    for (t_cli* tmp = clients; tmp; tmp = tmp->next) {
                        if (tmp->fd != cur->fd)
                            tmp->write_buf = str_join(cur->write_buf, buf_one);
                    }
                    if (cur->read_buf)
                        free(cur->read_buf);
                    if (cur->write_buf)
                        free(cur->write_buf);
                    t_cli * tmp = clients;
                    t_cli * del;
                    if (tmp->fd == cur->fd) {
                        clients = tmp->next;
                        free(tmp);
                    } else {
                        while (tmp->next && tmp->next->fd != cur->fd)
                            tmp = tmp->next;
                        del = tmp->next;
                        tmp->next = tmp->next->next;
                        free(del);
                    }
                    FD_CLR(cur->fd, &fd_read);
                    FD_CLR(cur->fd, &fd_write);
                    bzero(buf_one, 100);
                } else if (ret != (int)strlen(cur->write_buf)) {
                    msg = str_join(msg, cur->write_buf + ret);
                    free(cur->write_buf);
                    cur->write_buf = msg;
                    free(msg);
                    msg = NULL;
                } else{
                    free(cur->write_buf);
                    cur->write_buf = NULL;
                }
            }
        }
    }
}