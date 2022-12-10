#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

#include <stdio.h>
#include <sys/time.h>
#include <stdarg.h>
#include <unistd.h>

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


/* Use macro for checking log level to avoid evaluating arguments in cases log
 * should be ignored due to low level. */
// from redis
#define LOG(level, ...) do {\
        if (((level)&0xff) < LL_OUTPUT) break;\
        _log_impl(level, __VA_ARGS__);\
    } while(0)

const char *LEVEL_MAP[] = {"DEBUG", "INFO", "WARN", "ERROR"};

void _log_impl(int level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void log_raw(int level, const char *msg);
int anetListen(int socket_fd, struct sockaddr *sa, socklen_t len, int backlog);

typedef struct socketFds {
    int fd[16];
    int count;
} socketFds;

int createSocketAndListen(int _port, struct socketFds *sfd);

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
 * 允许 socket fd 可重用
 * 作用: 确保 基准测试 场景下, 频繁 开/关 socket 时不太影响性能
 * @param err
 * @param fd
 * @return ERR if set fail, else return OK
 */
static int anetSetReuseAddr(int fd) {
    int yes = 1;
    /* Make sure connection-intensive things like the redis benchmark
     * will be able to close/open sockets a zillion of times */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        LOG(LL_ERROR, "setsockopt SO_REUSEADDR: %d", fd);
        return ANET_ERR;
    }
    return ANET_OK;
}

int createSocketAndListen(int port, struct socketFds *sfd) {
    struct addrinfo hints, *info;
    char *host = NULL;
    char _port[6]; /* strlen("65535") */
    snprintf(_port, 6, "%d", port);
    int rv;
    // 初始化变量
    memset(&hints, 0, sizeof hints);

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

int main(int argc, char **argv) {
    // 如何查看 pid 占用的所有端口 | pid all port
    // https://unix.stackexchange.com/questions/157823/list-ports-a-process-pid-is-listening-on-preferably-using-iproute2-tools
    struct socketFds socketFds;
    memset(&socketFds, 0, sizeof(socketFds));
    createSocketAndListen(9527, &socketFds);
    LOG(LL_INFO, "success!");
    return 0;
}