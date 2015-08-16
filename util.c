#include <sys/time.h>
#include <stdint.h>
#include <unistd.h>

uint64_t get_timestamp() {
	struct timeval time;
	gettimeofday(&time,NULL);
	return time.tv_sec*(uint64_t)1000000+time.tv_usec;
}
