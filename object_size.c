#include <stdio.h>

struct X { int a; char b; int c; };
struct Y { int a; char b; int c; char d};
struct Z { int a; int b; char c; char d};
struct A { int* a; char* b;};

int main() {
    printf("%d,", sizeof(struct X));
    printf("%d,", sizeof(struct Y));
    printf("%d\n", sizeof(struct Z));

    printf("%d\n", sizeof(struct A));
}