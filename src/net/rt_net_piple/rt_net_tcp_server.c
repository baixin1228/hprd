
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#define MAX_EVENTS 1024
#define BUFLEN 1024
#define SERV_PORT 9999

#define ONE_MINUTE 60 * 1000

#define ONLISTEN 1
#define NOT_ACTIVE 0

static inline void perr(int true, const char *func_info)
{
  if (true)
  {
    printf("\n");
    perror(func_info);
    exit(EXIT_FAILURE);
  }
}

// 定义属于自己的my_event结构体
typedef struct my_event_st
{
  int fd;     //cfd listenfd
  int events; //EPOLLIN  EPLLOUT
  void (*callback_fn)(int epfd, void *ev);
  int status;
  char buf[BUFLEN];
  int len;
  time_t last_active;
} my_event_st;

typedef struct epoll_event epoll_event;

void callback_recvdata(int epfd, my_event_st *ev);
void callback_accept(int epfd, my_event_st *ev);
void callback_senddata(int epfd, my_event_st *ev);

// 维护全局my_event数组,全局变量自动初始化
my_event_st my_events[MAX_EVENTS + 1];
int my_events_len = 0;

/* create and add to my_events */
my_event_st *create_my_event(int fd, int events, void *callback_fn)
{
  if (my_events_len == MAX_EVENTS + 1)
  {
    printf("Max Events Limit\n");
    return 0;
  }
  my_events[my_events_len++] = (my_event_st){.fd = fd,
                                             .events = events,
                                             .callback_fn = (void (*)(int, void *))callback_fn,
                                             .status = 0,
                                             .len = 0,
                                             .last_active = time(0)};

  return &(my_events[my_events_len - 1]);
}

epoll_event create_epoll_event(my_event_st *ev)
{
  epoll_event ret = {
      .events = ev->events,
      .data.ptr = ev};
  return ret;
}

void register_event(int epfd, my_event_st *ev)
{
  int op = 0;
  if (ev->status == ONLISTEN)
  {
    op = EPOLL_CTL_MOD;
  }
  else
  {
    ev->status = ONLISTEN;
    op = EPOLL_CTL_ADD;
  }
  epoll_event event = create_epoll_event(ev);
  epoll_ctl(epfd, op, ev->fd, &event);
}

void remove_event(int epfd, my_event_st *ev)
{
  epoll_event evv = create_epoll_event(ev);
  int err = epoll_ctl(epfd, EPOLL_CTL_DEL, ev->fd, &evv);
  close(ev->fd);
  ev->status = NOT_ACTIVE;
}

void callback_accept(int epfd, my_event_st *ev)
{
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  int new_fd = accept(ev->fd, (struct sockaddr *)&addr, &len);
  perr(new_fd == -1, __func__);
  fcntl(new_fd, F_SETFL, O_NONBLOCK);
  ev = create_my_event(new_fd, EPOLLIN, (void *)callback_recvdata);
  register_event(epfd, ev);
  printf("[连接] addr:%s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
}

void callback_recvdata(int epfd, my_event_st *ev)
{
  int n = recv(ev->fd, ev->buf, BUFLEN, 0);
  perr(n == -1, __func__);
  if (n == 0)
  {
    // 客户端关闭了socket
    remove_event(epfd, ev);
    return;
  }
  ev->callback_fn = (void (*)(int, void *))callback_senddata;
  ev->len = n;
  ev->events = EPOLLOUT;
  printf("[收到] len=%d , data=%s\n", n, ev->buf);
  register_event(epfd, ev);
}

void callback_senddata(int epfd, my_event_st *ev)
{
  if (ev->len <= 0)
  {
    close(ev->fd);
  }
  int n = send(ev->fd, ev->buf, ev->len, 0);
  if (n == -1)
  {
    // Bad file descriptor , socket被client关闭
    remove_event(epfd, ev);
    return;
  }
  printf("[发回] len=%d , data=%s\n", n, ev->buf);

  ev->callback_fn = (void (*)(int, void *))callback_recvdata;
  memset(ev->buf, 0, BUFLEN);
  ev->len = 0;
  ev->events = EPOLLIN;
  register_event(epfd, ev);
}

int create_ls_socket(int port)
{
  int ls_fd = socket(AF_INET, SOCK_STREAM, 0);
  fcntl(ls_fd, F_SETFL, O_NONBLOCK);
  struct sockaddr_in ls_addr = {.sin_family = AF_INET,
                                .sin_addr.s_addr = INADDR_ANY,
                                .sin_port = htons(port)};
  bind(ls_fd, (struct sockaddr *)&ls_addr, sizeof(ls_addr));
  listen(ls_fd, 1024);
  return ls_fd;
}

int check_active(int epfd, int n)
{
  time_t now = time(0);
  for (int i = 1; i <= n; ++i)
  {
    if (i >= my_events_len)
      return 0;

    time_t duration = now - my_events[i].last_active;
    if (duration >= 60)
    {
      //1.取消注册 2.从全局my_events删除
      // 这里简单处理,只取消注册,否则删数组元素要集体迁移,用链表好一点
      remove_event(epfd, &my_events[i]);
    }
  }
}

int main()
{
  int epfd = epoll_create(MAX_EVENTS + 1);
  int ls_fd = create_ls_socket(SERV_PORT);
  my_event_st *ls_ev = create_my_event(ls_fd, EPOLLIN, (void *)callback_accept);
  register_event(epfd, ls_ev);
  epoll_event events[MAX_EVENTS + 1];
  while (1)
  {
    check_active(epfd, 10);
    int nfd = epoll_wait(epfd, events, MAX_EVENTS + 1, ONE_MINUTE);
    perr(nfd <= 0, "epoll_wait");
    for (int i = 0; i < nfd; i++)
    {
      my_event_st *ev = events[i].data.ptr;
      ev->callback_fn(epfd, ev);
    }
  }
}