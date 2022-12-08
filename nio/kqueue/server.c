#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <sys/time.h>
#include <stdarg.h>
#include <unistd.h>

/* DNS lookup */
#define NET_IP_STR_LEN 46       /* INET6_ADDRSTRLEN is 46 */

#define ANET_OK 0
#define ANET_ERR -1

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

//int create_socket_and_listen() {
//    struct addrinfo *addr;
//    struct addrinfo hints;
//    memset(&hints, 0, sizeof(struct addrinfo));
//    hints.ai_flags = AI_PASSIVE;
//    hints.ai_family = PF_UNSPEC;
//    hints.ai_socktype = SOCK_STREAM;
//    getaddrinfo("127.0.0.1", "8080", &hints, &addr);
//    int local_s = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
//    bind(local_s, addr->ai_addr, addr->ai_addrlen);
//    listen(local_s, 5);
//    return local_s;
//}
int create_socket_and_listen(char *host, char *port) {
    struct addrinfo hints, *info;
    int rv;
    // 初始化变量
    memset(&hints, 0, sizeof(hints));

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(host, port, &hints, &info)) != 0) {
        LOG(LL_INFO, "%s", gai_strerror(rv));
        return ANET_ERR;
    }

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

        if (anetListen(s_fd, p->ai_addr, p->ai_addrlen, 511) == ANET_ERR) {
            LOG(LL_ERROR, "listen error, address -> %s", ip_buf);
            continue;
        }
    }
    freeaddrinfo(info);
    return ANET_OK;
}

/**
 * todo va_list error msg
 */
int anetListen(int socket_fd, struct sockaddr *sa, socklen_t len, int backlog) {
    if (bind(socket_fd, sa, len) == -1) {
        LOG(LL_ERROR, "bind error");
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
    create_socket_and_listen("localhost", "9527");
    LOG(LL_INFO, "success!");
    return 0;
}