#define Conn(x,y) x##y
//#define ToChar(x) #@x
#define ToString(x) #x

#include <stdio.h>

int main() {
    printf("%d\n", Conn(123, 456));
    char* str = ToString(123456);
    printf("%s\t%lu\n", str, sizeof(*str));
    printf("%c\n", str[-1]);
    return 0;
}
