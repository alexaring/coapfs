#include "log.h"

static struct logger *log;

int log_print(llevel loglevel, const char *format, ...)
{
	int err;
	va_list args;
	time_t timestamp;
	struct tm *ts;
	char buffer[DATE_BUFFER_SIZE];

	if (!log) {
		err = log_init();
		if (err < 0)
			return err;
	}

	if (log->loglevel & NONE)
		return 0;
	
	timestamp = time(NULL);
	ts = localtime(&timestamp);
	strftime(buffer, sizeof(char)*DATE_BUFFER_SIZE, "%a %Y-%m-%d %H:%M:%S %Z", ts);

	if ((log->loglevel & ERROR) && (loglevel == ERROR)) { 
		fprintf(stderr, "%s: %s - ", buffer, "ERROR");
	        va_start(args, format);
	        vfprintf(stderr, format, args);
	        va_end(args);
	}
	
	if ((log->loglevel & INFO) && (loglevel == INFO)) { 
		fprintf(stdout, "%s: %s - ", buffer, "INFO");
        
		va_start(args, format);
	        vfprintf(stdout, format, args);
		va_end(args);
	}
	
	if ((log->loglevel & DEBUG) && (loglevel == DEBUG)) { 
		fprintf(stdout, "%s: %s - ", buffer, "DEBUG");
		va_start(args, format);
	        vfprintf(stdout, format, args);
	        va_end(args);
	}

	return 0;
}

int log_print_perror()
{
	int err;
	time_t timestamp;
	struct tm *ts;
	char buffer[DATE_BUFFER_SIZE];
	char* errstr;

	if (!log) {
		err = log_init();
		if (err < 0)
			return err;
	}

	if (log->loglevel & NONE)
		return 0;

	timestamp = time(NULL);
	ts = localtime(&timestamp);
	strftime(buffer, sizeof(char)*DATE_BUFFER_SIZE, "%a %Y-%m-%d %H:%M:%S %Z", ts);
	errstr = strerror(errno);
	fprintf(stderr, "%s: %s - %s\n", buffer, "ERROR", errstr);
	
	return 0;
}

int log_set_log_level(llevel loglevel)
{
	int err;

	if (!log) {
		err = log_init();
		if (err < 0)
			return err;
	}
	
	log->loglevel = loglevel;

	return 0;
}

int log_init()
{
	log = malloc(sizeof(struct logger));
	if (!log)
		return -ENOMEM;
	
	log->loglevel = INFO;

	return 0;
}

void log_free()
{
	free(log);
}
