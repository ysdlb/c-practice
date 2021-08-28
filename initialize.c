//
// Created by admin on 2021/8/21.
//
#include <stdio.h>

void f(void) {
    // c的编译器会初始化静态变量为0，因为这只是在启动程序时的动作
    static int a = 3;
    static int b;
    // 变量 c 在栈空间里，如果要初始化，每次调用函数，都要在运行时初始化函数栈空间，这太费性能了
    int c;
    ++a; ++b; ++c;
    printf("a = %d\n", a);
    printf("b = %d\n", b);
    printf("c = %d\n", c);
}

int main(void) {
    f();
    f();
    f();
}
