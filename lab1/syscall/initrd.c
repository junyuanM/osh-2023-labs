#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>

#define BUFFER_SIZE1 10
#define BUFFER_SIZE2 60

int main() {
    char buffer1[BUFFER_SIZE1];
    char buffer2[BUFFER_SIZE2];
    long int ret1 = syscall(548, buffer1, BUFFER_SIZE1);
    if (ret1 == 0) {
        printf("%s\n", buffer1);
    } else if (ret1 == -1) {
        printf("THe size is not long enough\n");
    } else {
        printf("Failed: %ld\n", ret1);
    }
    long int ret2 = syscall(548, buffer2, BUFFER_SIZE2);
    if (ret2 == 0) {
        printf("%s\n", buffer2);
    } else if (ret2 == -1) {
        printf("THe size is not long enough\n");
    } else {
        printf("Failed: %ld\n", ret2);
    }
    while(1){}
}

