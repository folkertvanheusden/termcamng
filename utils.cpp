#include <fstream>
#include <optional>
#include <pthread.h>
#include <sstream>
#include <string>
#include <sys/stat.h>


void set_thread_name(std::string name)
{
#ifdef linux
	if (name.length() > 15)
		name = name.substr(0, 15);

	pthread_setname_np(pthread_self(), name.c_str());
#endif
}

bool file_exists(const std::string & file, size_t *const file_size)
{
        struct stat st { 0 };

        bool rc = stat(file.c_str(), &st) == 0;

        if (rc && file_size)
                *file_size = st.st_size;

        return rc;
}

std::optional<std::string> load_text_file(const std::string & filename)
{
        size_t size = 0;

        if (file_exists(filename, &size)) {
                std::ifstream t(filename);

                std::stringstream buffer;
                buffer << t.rdbuf();

                return buffer.str();
        }

        return { };
}
