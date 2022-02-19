#include <algorithm>
#include <cctype>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "error.h"


std::vector<std::string> split(const std::string & in_in, const std::string & splitter)
{
	std::string in = in_in;

	std::vector<std::string> out;
	size_t splitter_size = splitter.size();

	for(;;)
	{
		size_t pos = in.find(splitter);
		if (pos == std::string::npos)
			break;

		std::string before = in.substr(0, pos);
		out.push_back(before);

		size_t bytes_left = in.size() - (pos + splitter_size);
		if (bytes_left == 0)
		{
			out.push_back("");
			return out;
		}

		in = in.substr(pos + splitter_size);
	}

	if (in.size() > 0)
		out.push_back(in);

	return out;
}

std::string myformat(const char *const fmt, ...)
{
	char *buffer = nullptr;
        va_list ap;

        va_start(ap, fmt);

        if (vasprintf(&buffer, fmt, ap) == -1)
		error_exit(true, "myformat: failed to convert string with format \"%s\"", fmt);

        va_end(ap);

	std::string result = buffer;
	free(buffer);

	return result;
}

std::string str_tolower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });

	return s;
}
