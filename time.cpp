#include <stdint.h>
#include <sys/time.h>


uint64_t get_ms()
{
	struct timeval tv { 0 };

	gettimeofday(&tv, nullptr);

	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
