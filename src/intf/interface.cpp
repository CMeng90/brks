// This autogenerated skeleton file illustrates how to build a server.
// You should copy it to another filename to avoid overwriting it.


#include "Logger.h"
#include "event.h"
#include "events_def.h"
#include "interface.h"

#include <functional>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>


#define BUF_SIZE   1024
#define MAX_EVENTS 64

int Interface::create_and_bind_socket(unsigned short port)
{
    int sfd;
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0)
    {
        LOG_ERROR("create socket failed");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0)
    {
	    LOG_ERROR("bind to port = %d failed.", port);
	    return -1;
    }

    // set SO_REUSEADDR
    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(opt));

    return sfd;
}

int Interface::set_socket_non_block(int sfd)
{
    int flags, res;

    flags = fcntl(sfd, F_GETFL);
    if (flags == -1)
    {
        LOG_ERROR("cannot get socket flags!\n");
        return -1;
    }

    flags |= O_NONBLOCK;
    res    = fcntl(sfd, F_SETFL, flags);
    if (res == -1)
    {
        LOG_ERROR("cannot set socket flags!\n");
        return -1;
    }

    return 0;
}

int Interface::nio_write(int fd, char* buf, int len)
{
    int write_pos = 0;
    int left_len = len;

    while (left_len > 0)
    {
        int writed_len = 0;
        if ((writed_len = write(fd, buf + write_pos, left_len)) <= 0)
        {
            if (errno == EAGAIN)
            {
               writed_len = 0;
            }
            else return -1; //表示写失�?
        }
        left_len -= writed_len;
        write_pos += writed_len;
    }

    return len;
}


bool Interface::start(int socket)
{
    struct epoll_event events[MAX_EVENTS] = {0};
    struct epoll_event event;
    memset(&event, 0, sizeof(event));

    // create server socket and set to non block
    //int server_socket = create_and_bind_socket(port);
    //set_socket_non_block(server_socket);

    int server_socket = socket;

    int ret = listen(server_socket, SOMAXCONN);
    if (ret == -1)
    {
        LOG_ERROR("cannot listen at the given server_socket!\n");
        return false;
    }
    else
    {
        LOG_INFO("process %d listend on 9090 port success.", getpid());
    }

    int epoll_fd = epoll_create(1);
    if (epoll_fd == -1)
    {
        LOG_ERROR("cannot create epoll_fd!\n");
        return false;
    }

    event.events  = EPOLLIN | EPOLLOUT | EPOLLET;
    event.data.fd = server_socket;
    ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event);
    if (ret == -1)
    {
        LOG_ERROR("can not add event to epoll_fd!\n");
        return false;
    }

    while (1)
    {
        int cnt = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        for (int i = 0; i < cnt; i++)
        {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)
                || !(events[i].events & EPOLLIN))
            {
                LOG_ERROR("server_socket fd error!\n");
                ::close(events[i].data.fd);
                continue;

            }
            else if (events[i].data.fd == server_socket)
            {
                struct sockaddr client_addr;
                int addrlen = sizeof(struct sockaddr);

                int client_fd = accept(server_socket, &client_addr, &addrlen);
                if (client_fd == -1)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        break;
                    }
                    else
                    {
                        LOG_ERROR("cannot accept new server_socket!\n");
                        continue;
                    }
                }

                ret = set_socket_non_block(client_fd);
                if (ret == -1)
                {
                    LOG_ERROR("cannot set flags!\n");
                    exit(1);
                }

                event.data.fd = client_fd;
                event.events  = EPOLLET | EPOLLIN;
                ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
                if (ret == -1)
                {
                    LOG_ERROR("cannot add to epoll_fd!\n");
                    continue;
                }
            }
            else
            {
                int cnt;
                char buf[BUF_SIZE] = {0};
                {
                    memset(buf, 0, sizeof(buf));
                    cnt = read(events[i].data.fd,  buf, BUF_SIZE);
                    if (cnt == -1)
                    {
                        if (errno == EAGAIN)
                        {
                            break;
                        }

                        LOG_ERROR("read error!\n");
                        return false;

                    }
                    else if (cnt == 0)
                    {
                        ::close(events[i].data.fd);
                    }

                    LOG_DEBUG("receive client data : %s\n", buf);
                    nio_write(events[i].data.fd, buf, strlen(buf));
                    break;
                }
            }
        }
    }

    return true;
}

bool Interface::close()
{

    return true;
}
