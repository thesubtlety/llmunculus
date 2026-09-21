// q: what year is it according to the system clock
#include <jb.h>
#include <time.h>
int main(void) {
    time_t t = time(NULL); struct tm *tm = localtime(&t);
    printf("%d\n", tm->tm_year + 1900);   // tm_year counts from 1900
    return 0;
}
