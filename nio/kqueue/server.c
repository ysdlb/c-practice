#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

#include <stdio.h>
#include <sys/time.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/event.h>

/* DNS lookup */
#define NET_IP_STR_LEN 46       /* INET6_ADDRSTRLEN is 46 */

#define ANET_OK 0
#define ANET_ERR (-1)

/* Log levels */
#define LL_DEBUG 0
#define LL_INFO 1
#define LL_WARN 2
#define LL_ERROR 3
#define LL_RAW (1<<10) /* Modifier to log without timestamp */

#define LL_OUTPUT LL_DEBUG

#define AE_NONE 0       /* No events registered. */
#define AE_READABLE 1   /* Fire when descriptor is readable. */
#define AE_WRITABLE 2   /* Fire when descriptor is writable. */
#define AE_BARRIER 4    /* With WRITABLE, never fire the event if the
                           READABLE event already fired in the same event
                           loop iteration. Useful when you want to persist
                           things to disk before sending replies, and want
                           to do that in a group fashion. */


/* Use macro for checking log level to avoid evaluating arguments in cases log
 * should be ignored due to low level. */
// from redis
#define LOG(level, ...) do {\
        if (((level)&0xff) < LL_OUTPUT) break;\
        _log_impl(level, __VA_ARGS__);\
    } while(0)

const char *LEVEL_MAP[] = {"DEBUG", "INFO", "WARN", "ERROR"};

struct client_data {
    int fd
};
struct client_data clients[128];

void _log_impl(int level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void log_raw(int level, const char *msg);
int anetListen(int socket_fd, struct sockaddr *sa, socklen_t len, int backlog);
void sendWelcomeMsg(int fd);
void recvMsg(int s);


typedef struct socketFds {
    int fd[16];
    int count;
} socketFds;

void eventLoop(int kqfd, socketFds *sfd);
int createSocketAndListen(int _port, socketFds *sfd);
int isNeedAccept(int ident, socketFds *sfd);
void aeApiAddEvent(int kq_fd, int socket_fd, int mask);
void aeApiRMEvent(int kq_fd, int socket_fd, int mask);

void _log_impl(int level, const char *fmt, ...) {
    va_list ap;
    char msg[1024];

    va_start(ap, fmt);
    // 不要用成 snprintf
    // https://stackoverflow.com/questions/46485639/what-is-the-difference-between-vsnprintf-and-vsprintf-s
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    log_raw(level, msg);
}

void log_raw(int level, const char *msg) {
    FILE *fp = stdout;
    const char *level_info = LEVEL_MAP[level];

    // time
    // 关于 time() vs gettimeofday() https://stackoverflow.com/questions/22917318/time-and-gettimeofday-return-different-seconds
    // https://stackoverflow.com/questions/22917318/time-and-gettimeofday-return-different-seconds
    time_t timer;
    char time_buf[26];
    struct tm *tm_info;

    timer = time(NULL);
    tm_info = localtime(&timer);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    // debug 模式下, printf 可能不会立马输出, 所以这里用 fprintf
    fprintf(fp, "[%s] [%s] [%d] - %s\n", level_info, time_buf, getpid(), msg);
}

/**
 * 1. resolve address: function -> netdb.h#getaddrinfo
 * 2. create socket: function -> socket.h#socket(int, int, int)
 * 3. bind socket to address
 * 4. listen on the socket for incoming connections
 * @param port
 * @param sfd
 * @return
 */
int createSocketAndListen(int port, struct socketFds *sfd) {
    struct addrinfo hints, *info;
    char *host = NULL;
    char _port[6]; /* strlen("65535") */
    snprintf(_port, 6, "%d", port);
    int rv;
    // 初始化变量
    memset(&hints, 0, sizeof hints);

    /**
     * af 为 AF_UNSPEC, 且 getaddrinfo host 参数为 NULL 时, 会返回两个地址, 一个 ipv4, 一个为 ipv6
     * 若只生成 ipv4 地址, 则 ai_family = AF_INET
     * 若只生成 ipv6 地址, 则 ai_family = AF_INET6
     */
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(host, _port, &hints, &info)) != 0) {
        LOG(LL_INFO, "%s", gai_strerror(rv));
        return ANET_ERR;
    }

    int i = 0;
    for (struct addrinfo *p = info; p != NULL; p = p->ai_next) i++;
    // int fds[i];
    // sfd->fd = fds;

    for (struct addrinfo *p = info; p != NULL; p = p->ai_next) {
        char ip_buf[NET_IP_STR_LEN];
        char *fmt = "%s inet to net presentation is %s";
        if (p->ai_family == AF_INET) {
            struct sockaddr_in *sa = (struct sockaddr_in *) p->ai_addr;
            // 网络地址转换函数 https://blog.csdn.net/zyy617532750/article/details/58595700
            inet_ntop(AF_INET, &(sa->sin_addr), ip_buf, sizeof(ip_buf));
            LOG(LL_INFO, fmt, host, ip_buf);
        } else if (p->ai_family == AF_INET6) {
            struct sockaddr_in6 *sa = (struct sockaddr_in6 *) p->ai_addr;
            inet_ntop(AF_INET6, &(sa->sin6_addr), ip_buf, sizeof(ip_buf));
            LOG(LL_INFO, fmt, host, ip_buf);
        } else {
            LOG(LL_INFO, "unknown");
            continue;
        }
        int s_fd;
        if ((s_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            LOG(LL_ERROR, "socket create error, address -> %s", ip_buf);
            continue;
        }
        // 这里会出现两个地址, ipv6 的 "::" 和 ipv4 的 "0.0.0.0"
        // ipv6 的地址必需要设置 socket 参数, 否则 ipv4 0.0.0.0 bind 必然失败,
        // 返回 socket.error: [Errno 48] Address already in use <error.h#EADDRINUSE>
        // 原因不了解, 参考: https://stackoverflow.com/questions/22126940/c-server-sockets-bind-error
        int yes = 1;
        if (p->ai_family == AF_INET6 && setsockopt(s_fd, IPPROTO_IPV6, IPV6_V6ONLY, &yes,sizeof(yes)) == -1) {
            LOG(LL_ERROR, "setsockopt: %s", strerror(errno));
            return ANET_ERR;
        }

        /**
         * 允许 socket fd 可重用
         * 作用: 确保 基准测试 场景下, 频繁 开/关 socket 时不太影响性能
         */
        LOG(LL_INFO, "yes -> %d", yes);
        if (setsockopt(s_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            LOG(LL_ERROR, "setsockopt SO_REUSEADDR: %s", strerror(errno));
            return ANET_ERR;
        }

        if (anetListen(s_fd, p->ai_addr, p->ai_addrlen, 511) == ANET_ERR) {
            LOG(LL_ERROR, "anetListen error, address -> %s", ip_buf);
            continue;
        }
        sfd->fd[sfd->count++] = s_fd;
    }
    freeaddrinfo(info);
    return ANET_OK;
}

/**
 * todo va_list error msg
 * 1. bind socket to address: function -> socket.h#bind(int, const struct sockaddr *, socklen_t) __DARWIN_ALIAS(bind);
 * 2. listen on the socket for incoming connections -> socket.h#listen(int, int) __DARWIN_ALIAS(listen)
 */
int anetListen(int socket_fd, struct sockaddr *sa, socklen_t len, int backlog) {
    if (bind(socket_fd, sa, len) == -1) {
        LOG(LL_ERROR, "bind error: %d", errno);
        close(socket_fd);
        return ANET_ERR;
    }

    // 传统 listen accept
    // https://stackoverflow.com/questions/24842051/listen-does-not-block-process
    if (listen(socket_fd, backlog) == -1) {
        LOG(LL_ERROR, "listen error");
        close(socket_fd);
        return ANET_ERR;
    }
    return ANET_OK;
}

void eventLoop(int kqfd, socketFds *sfd) {
    struct kevent keList[100];
    struct sockaddr_storage addr;
    size_t addr_len = sizeof addr;

    while (1) {
        struct timespec timeout;
        timeout.tv_sec = 1;
        int num_events = kevent(kqfd, NULL, 0, keList, 100, &timeout);
        for(int i = 0; i < num_events; i++) {
            struct kevent e = keList[i];
            int ident = (int) e.ident;
            if (isNeedAccept(ident, sfd)) {
                // client 和 server 的交流将持续在这个 fd 上交互
                int fd = accept(ident, (struct sockaddr *)&addr, (socklen_t *) &addr_len);
                aeApiAddEvent(kqfd, fd, AE_READABLE);
                sendWelcomeMsg(fd);
            } else if (e.flags & EV_EOF) {
                int fd = ident;
                aeApiRMEvent(kqfd, fd, AE_READABLE);
                LOG(LL_INFO, "client %d disconnected", fd);
            } else if (e.filter == EVFILT_READ) {
                int fd = ident;
                // accept 到的 fd
                recvMsg(fd);
            }
        }
    }
}

int isNeedAccept(int ident, struct socketFds *sfd) {
    for (int i = 0; i < sfd->count; i++) {
        if (ident == sfd->fd[i])
            return ident;
    }
    return 0;
}

/**
 * bind kqueue fd and socket fd
 */
void aeApiAddEvent(int kq_fd, int socket_fd, int mask) {
    struct kevent ke;
    if (mask & AE_READABLE) {
        EV_SET(&ke, socket_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
        if (kevent(kq_fd, &ke, 1, NULL, 0, NULL) == -1) {
            LOG(LL_ERROR, "AE_READABLE bind %d and %d error", kq_fd, socket_fd);
        }
    }

    if (mask & AE_WRITABLE) {
        EV_SET(&ke, socket_fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
        if (kevent(kq_fd, &ke, 1, NULL, 0, NULL) == -1) {
            LOG(LL_ERROR, "AE_WRITABLE bind %d and %d error", kq_fd, socket_fd);
        }
    }
}

void aeApiRMEvent(int kq_fd, int socket_fd, int mask) {
    struct kevent ke;
    if (mask & AE_READABLE) {
        EV_SET(&ke, socket_fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        if (kevent(kq_fd, &ke, 1, NULL, 0, NULL) == -1) {
            LOG(LL_ERROR, "AE_READABLE delete %d and %d error", kq_fd, socket_fd);
        }
    }

    if (mask & AE_WRITABLE) {
        EV_SET(&ke, socket_fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
        if (kevent(kq_fd, &ke, 1, NULL, 0, NULL) == -1) {
            LOG(LL_ERROR, "AE_WRITABLE delete %d and %d error", kq_fd, socket_fd);
        }
    }
}

void sendWelcomeMsg(int fd) {
    char msg[80];
    sprintf(msg, "welcome! you are client #%d!\n", fd);
    send(fd, msg, strlen(msg), 0);
}

void recvMsg(int s) {
    char buf[1024];
    ssize_t bytes_read = recv(s, buf, sizeof(buf) - 1, 0);
    buf[bytes_read] = 0;
    printf("client #%d: %s", s, buf);
    fflush(stdout);
}

/**
 * nio 的 kqueue 实现, 主要参考
 * 1. redis
 * 2. https://nima101.github.io/kqueue_server
 */
int main(int argc, char **argv) {
    // 如何查看 pid 占用的所有端口 | pid all port
    // https://unix.stackexchange.com/questions/157823/list-ports-a-process-pid-is-listening-on-preferably-using-iproute2-tools
    struct socketFds socketFds;
    memset(&socketFds, 0, sizeof(socketFds));
    createSocketAndListen(9527, &socketFds);
    LOG(LL_INFO, "createSocketAndListen success!");

    // 创建 kqueue fd 和 event 数组
    int kqfd = kqueue();

    // 绑定 socket 和 event 数组
    // 绑定 kevent 和 socket
    for (int i = 0; i < socketFds.count; i++) {
        aeApiAddEvent(kqfd, socketFds.fd[i], AE_READABLE);
    }

    // nio 监听
    eventLoop(kqfd, &socketFds);
    return 0;
}