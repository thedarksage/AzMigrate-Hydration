#ifndef _LIBLOGGER_H
#define _LIBLOGGER_H

#ifndef _FLTLIB

int setup_logfile(char *file, int truncate);

#define log_msg(fmt, arg...) printf(fmt"\n", ## arg)
#define log_err(fmt, arg...) fprintf(stderr, fmt"\n", ## arg)

#define log_errno(fmt, arg...)                  \
        do {                                    \
            if (!errno)                         \
                errno = EINVAL;                 \
            perror("ERROR");                    \
            log_err(fmt, ## arg);               \
        } while(0)

#define log_err_exit(fmt, arg...)               \
        do {                                    \
            log_errno(fmt, ## arg);             \
            exit(-1);                           \
        } while(0)

#define log_err_usage(fmt, arg...)              \
        do {                                    \
            log_errno(fmt, ## arg);             \
            usage(-1);                          \
        } while (0)

/* CMD usage prototype */
void usage(int error);

#else

#error "Logging is only supported for end programs.\nLibraries should set appropriate errno and return"

#endif

#endif
