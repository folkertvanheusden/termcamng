#include <pthread.h>
#include <string>

void set_thread_name(std::string name)
{
#ifdef linux
	if (name.length() > 15)
		name = name.substr(0, 15);

	pthread_setname_np(pthread_self(), name.c_str());
#endif
}
