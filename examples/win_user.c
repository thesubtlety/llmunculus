// q: on Windows, which user am I and am I elevated
#include <jb.h>
#include <jb_win.h>
int main(void) {
    char user[512]; if (jb_win_user(user, sizeof user)) { perror("jb_win_user"); return 1; }
    printf("%s elevated=%d\n", user, jb_win_elevated());
    return 0;
}
