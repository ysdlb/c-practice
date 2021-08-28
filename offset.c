#include <stdio.h>

void array() {
    int a[5];
    printf("%x\n", a);
    printf("%x\n", a+1);
    printf("%x\n", &a);
    printf("%x\n", &a+1);
}

int main(void) {
    array();
}