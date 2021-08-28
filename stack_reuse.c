#include <stdio.h>

void foo(void) {
    int a;
    // int b;
    printf("a = %d\n", a);
    // printf("b = %d\n", b);
}

void bar(void) {
    int a = 12;
    // int b = 22;
    // int c = 32;
}

int main(void) {
    bar();
    foo();
}
