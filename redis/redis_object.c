#include <stdio.h>

typedef unsigned char     uint8_t;

struct __attribute__ ((__packed__)) sdshdr8 {
    uint8_t len; /* used */
    uint8_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};

struct s_sdshdr8 {
    uint8_t len; /* used */
    uint8_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};

struct __attribute__ ((__packed__, aligned(4))) sdshdr8_align4 {
    uint8_t len; /* used */
    uint8_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};

struct RA {
    char a;
    char b;
    char c;
};

struct RB {
    char a;
    int b;
    char c;
};

struct RC {
    char a;
    short b;
    char c;
};

// aligned(1)
struct __attribute__ ((__packed__)) RD {
    char a;
    short b;
    char c;
};

struct __attribute__ ((__packed__, aligned(4))) RE {
    char a;
    short b;
    char c;
    char d;
};

struct __attribute__ ((aligned(4))) RF {
    char a;
    short b;
    char c;
};

int main(void) {
    printf("sdshdr8 size -> %lu\n", sizeof(struct sdshdr8));
    printf("s_sdshdr8 size -> %lu\n", sizeof(struct s_sdshdr8));
    printf("sdshdr8 size -> %lu\n", sizeof(struct sdshdr8_align4));
    printf("RA size -> %lu\n", sizeof(struct RA));
    printf("RB size -> %lu\n", sizeof(struct RB));
    printf("RC size -> %lu\n", sizeof(struct RC));
    printf("RD size -> %lu\n", sizeof(struct RD));
    printf("RE size -> %lu\n", sizeof(struct RE));
    printf("RF size -> %lu\n", sizeof(struct RF));
}
