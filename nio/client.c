#include <sys/event.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFSIZE 1024

/* 函数原型 */

/*
 * https://blog.51cto.com/flydean/5690198
 * https://www.jianshu.com/p/64aee4e7023c
 * https://blog.csdn.net/weixin_38387929/article/details/118584182
 */

void diep(const char *s);
int tcpopen(const char *host, int port);
void sendbuftosck(int sckfd, const char *buf, int len);



int main(int argc, char *argv[]) {
    struct kevent chlist[2];/* 我们要监视的事件 */
    struct kevent evlist[2];/* 触发的事件 */
    char buf[BUFSIZE];
    int sckfd, kq, nev, i;

    /* 检查参数数量 */
    if (argc != 3) {
        fprintf(stderr, "usage: %s host port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* 打开一个链接(host,port)pair */
    sckfd = tcpopen(argv[1], atoi(argv[2]));

    /* 创建一个新的内核事件队列 */
    if ((kq = kqueue()) == -1)
        diep("kqueue()");

    /* 初始化kevent结构体 */
    EV_SET(&chlist[0], sckfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
    EV_SET(&chlist[1], fileno(stdin), EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);


    /* 无限循环 */
    for (;;) {
        nev = kevent(kq, chlist, 2, evlist, 2, NULL);
        if (nev < 0){
            diep("kevent()");
        } else if (nev > 0) {
            if (evlist[0].flags & EV_EOF) {
                // 读取socket关闭指示
                exit(EXIT_FAILURE);
            }


            for (i = 0; i < nev; i++) {
                if (evlist[i].flags & EV_ERROR) {
                    /* 报告错误 */
                    fprintf(stderr, "EV_ERROR: %s\n", strerror(evlist[i].data));
                    exit(EXIT_FAILURE);
                }


                if (evlist[i].ident == sckfd) {
                    /* 我们从host接收到数据 */
                    memset(buf, 0, BUFSIZE);
                    if (read(sckfd, buf, BUFSIZE) < 0) {
                        diep("read()");
                    }
                    fputs(buf, stdout);
                } else if (evlist[i].ident == fileno(stdin)) {
                    /* stdin中有数据输入 */
                    memset(buf, 0, BUFSIZE);
                    fgets(buf, BUFSIZE, stdin);
                    sendbuftosck(sckfd, buf, strlen(buf));
                }
            }
        }
    }
    close(kq);
    return EXIT_SUCCESS;
}



void diep(const char *s) {
    perror(s);
    exit(EXIT_FAILURE);
}



int tcpopen(const char *host, int port) {
    struct sockaddr_in server;
    struct hostent *hp;
    int sckfd;

    if ((hp = gethostbyname(host)) == NULL) {
        diep("gethostbyname()");
    }

    /* 译者注：此处Linux系统应使用AF_INET，PF_INET用于BSD */
    if ((sckfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        diep("socket()");
    }


    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr = *((struct in_addr *)hp->h_addr);
    memset(&(server.sin_zero), 0, 8);

    if (connect(sckfd, (struct sockaddr *)&server, sizeof(struct sockaddr)) < 0) {
        diep("connect()");
    }
    return sckfd;
}



void sendbuftosck(int sckfd, const char *buf, int len) {
    int bytessent, pos;
    pos = 0;

    do {
        if ((bytessent = send(sckfd, buf + pos, len - pos, 0)) < 0) {
            diep("send()");
        }
        pos += bytessent;
    } while(bytessent > 0);
}