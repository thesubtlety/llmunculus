// libc symbols the model's programs may call. one line each. keep it boring.
// declared as plain functions on purpose: the linker only needs the name, tcc gets the address.
#define SYMS(X) \
 X(printf) X(fprintf) X(sprintf) X(snprintf) X(puts) X(putchar) X(fputs) X(fputc) X(fgets) X(fgetc) X(getline) \
 X(fopen) X(fclose) X(fread) X(fwrite) X(fflush) X(fscanf) X(sscanf) X(perror) X(fileno) X(feof) X(ferror) X(fseek) X(ftell) X(rewind) \
 X(popen) X(pclose) X(system) \
 X(malloc) X(calloc) X(realloc) X(free) X(abort) X(atoi) X(atol) X(atof) X(strtol) X(strtoul) X(strtoll) X(strtoull) X(strtod) X(qsort) X(getenv) \
 X(strlen) X(strcmp) X(strncmp) X(strcasecmp) X(strncasecmp) X(strncpy) X(strcat) X(strncat) X(strchr) X(strrchr) X(strtok) X(strtok_r) X(strdup) X(strndup) X(strerror) X(strspn) X(strcspn) X(strpbrk) \
 X(memcpy) X(memset) X(memcmp) X(memmove) X(memchr) \
 X(isdigit) X(isspace) X(isalpha) X(isalnum) X(isupper) X(islower) X(isprint) X(ispunct) X(isxdigit) X(toupper) X(tolower) \
 X(sysconf) X(getpid) X(getppid) X(getuid) X(geteuid) X(getgid) X(gethostname) X(getlogin) X(getcwd) X(chdir) X(access) X(readlink) X(read) X(write) X(close) X(open) X(lseek) X(sleep) X(usleep) X(isatty) X(dup2) X(pipe) X(fork) X(execvp) X(waitpid) X(unlink) X(rmdir) X(mkdir) X(rename) X(kill) \
 X(opendir) X(readdir) X(closedir) X(rewinddir) X(scandir) X(alphasort) \
 X(stat) X(lstat) X(fstat) X(statvfs) X(fstatvfs) X(realpath) X(chmod) X(getrlimit) X(getrusage) \
 X(time) X(localtime) X(localtime_r) X(gmtime) X(gmtime_r) X(strftime) X(mktime) X(difftime) X(clock_gettime) X(nanosleep) X(ctime) X(asctime) \
 X(getpwuid) X(getpwnam) X(getpwent) X(setpwent) X(endpwent) X(getgrgid) X(getgrnam) X(getgrent) X(setgrent) X(endgrent) X(getgroups) \
 X(uname) X(sysinfo) X(getloadavg) X(getpagesize) \
 X(getifaddrs) X(freeifaddrs) X(inet_ntop) X(inet_pton) X(getaddrinfo) X(freeaddrinfo) X(gai_strerror) X(getnameinfo) X(socket) X(connect) X(setsockopt) X(send) X(recv) X(htons) X(ntohs) X(htonl) X(ntohl) \
 X(sqrt) X(pow) X(floor) X(ceil) X(fabs) X(log) X(log10) X(log2) X(exp) X(round) X(fmod) \
 X(setlocale) X(signal) X(alarm) X(__errno_location) X(stdin) X(stdout) X(stderr) X(environ) X(optarg) X(optind) \
 X(vprintf) X(vfprintf) X(vsnprintf) X(getc) X(getchar) X(putc) X(ungetc) X(setvbuf) X(setbuf) X(remove) X(tmpfile) X(fdopen) X(freopen) \
 X(getlogin_r) X(getpwuid_r) X(getpwnam_r) X(getgrgid_r) X(getgrnam_r) X(getdomainname) X(ttyname) X(getpgrp) X(getsid) X(nice) X(getpriority) \
 X(bsearch) X(abs) X(labs) X(llabs) X(rand) X(srand) X(random) X(srandom) X(atoll) X(strtof) X(strtold) X(strtoimax) X(strtoumax) X(qsort_r) X(getopt) X(setenv) X(unsetenv) X(putenv) \
 X(strsep) X(strcoll) X(strsignal) X(strnlen) X(strlcpy) X(strlcat) X(memccpy) X(mempcpy) X(basename) X(dirname) X(fnmatch) X(glob) X(globfree) X(regcomp) X(regexec) X(regfree) X(regerror) \
 X(readdir_r) X(fchdir) X(openat) X(fstatat) X(readlinkat) X(statfs) X(fstatfs) X(nftw) X(ftw) X(getdtablesize) X(dup) X(fcntl) X(ioctl) X(poll) X(select) X(pread) X(pwrite) X(fsync) X(ftruncate) X(umask) X(chown) X(utimes) X(truncate) X(symlink) X(link) \
 X(gettimeofday) X(timegm) X(tzset) X(clock) X(times) X(sched_getaffinity) X(sched_yield) X(getrandom) X(getentropy) \
 X(gethostbyname) X(getservbyname) X(getprotobyname) X(getsockopt) X(getsockname) X(getpeername) X(shutdown) X(sendto) X(recvfrom) X(inet_ntoa) X(inet_addr) \
 X(mmap) X(munmap) X(madvise) X(msync) X(execv) X(execve) X(execl) X(execlp) X(wait) X(raise) X(sigaction) X(sigemptyset) X(sigaddset) X(sigprocmask) X(pause) \
 X(syslog) X(openlog) X(closelog) X(getgrouplist) X(gethostname) \
 X(jb_https) X(jb_win_user) X(jb_win_elevated) X(jb_win_privileges) X(jb_win_services) X(jb_win_service_status) X(jb_win_service_control) \
 X(sin) X(cos) X(tan) X(asin) X(acos) X(atan) X(atan2) X(sinh) X(cosh) X(tanh) X(hypot) X(cbrt) X(trunc) X(lround) X(lrint) X(fmax) X(fmin) X(ldexp) X(frexp) X(modf) X(fabsf) X(sqrtf) X(powf) X(floorf) X(ceilf) X(roundf)
