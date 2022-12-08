#include <stdarg.h>
#include <stdio.h>

// https://blog.csdn.net/u014630623/article/details/101107023
void test(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

void test(const char* fmt, ...) {
    va_list ap;
    char buffer[1024];

    va_start(ap, fmt);
    // 将可变参数格式化输出到字符数组
    // 参考: https://www.ibm.com/docs/en/i/7.4?topic=functions-vsnprintf-print-argument-data-buffer
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);
    fprintf(stdout, "%s\n", buffer);
}

int main() {
    char *s = "123%s %d";
    test(s, "hhhhh", 321);
}