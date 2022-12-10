#include <string.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    char *addr = "-::*";

    //-| 字符串的第一个字符是否为 '-'
    //-| 如果是, 则舍弃第一个字符, 比如 '-::*' -> '::*'
    int optional = *addr == '-';
    if (optional) addr ++;

    char *chr = strchr(addr, ':');
    if (chr) {
        fprintf(stdout, "%s", "yes");
    }
    fprintf(stdout, "%s", chr);
}