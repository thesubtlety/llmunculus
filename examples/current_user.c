// q: what is my user name and home directory
#include <jb.h>
#include <pwd.h>
#include <unistd.h>
int main(void) {
    struct passwd *p = getpwuid(getuid()); if (!p) return 1;   // works without a terminal, unlike getlogin
    printf("%s %s uid=%d\n", p->pw_name, p->pw_dir, (int)getuid());
    return 0;
}
