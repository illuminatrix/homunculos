#ifndef _STDLIB_H
#define _STDLIB_H 1

#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((__noreturn__))
void abort(void);

char *getenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);
int putenv(char *string);
int clearenv(void);

#ifdef __cplusplus
}
#endif

#endif
