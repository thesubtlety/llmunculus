/* Windows helpers for programs. Each returns 0 on success, -1 on failure with errno set.
   On any other OS they fail with ENOSYS. Text results are NUL terminated, lines end with \n. */
int jb_win_user(char *out, unsigned long cap);                 /* "DOMAIN\name S-1-5-21-..." of the current token   */
int jb_win_elevated(void);                                     /* 1 if the token is elevated (admin), 0 if not, -1 error */
int jb_win_privileges(char *out, unsigned long cap);           /* one line per privilege: "SeDebugPrivilege enabled"   */
int jb_win_services(char *out, unsigned long cap);             /* one line per service: "Spooler running"             */
int jb_win_service_status(const char *name, char *out, unsigned long cap); /* "running", "stopped", "start pending"...  */
int jb_win_service_control(const char *name, const char *action);         /* action: "start" or "stop"               */
