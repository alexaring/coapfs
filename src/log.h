#ifndef __LOG_H__
#define __LOG_H__

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#define DATE_BUFFER_SIZE 80

typedef enum {
	NONE = 0x1,
	INFO = 0x2,
	ERROR = 0x4,
	DEBUG = 0x8
} llevel;

struct logger {
	llevel loglevel;
};

int log_init();
void log_free();
int log_set_log_level(llevel loglevel);
int log_print(llevel loglevel, const char* format, ...);
int log_print_perror();

#endif
