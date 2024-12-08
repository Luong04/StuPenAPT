#define _XOPEN_SOURCE 600
#include <time.h>
#include <stdio.h>
#include <unistd.h>

void timestamp(char *prologue, char *epilogue)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	printf("%s %10ld.%06ld %s",prologue, ts.tv_sec, ts.tv_nsec / 1000, epilogue);
}
