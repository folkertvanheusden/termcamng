#include <fcntl.h>
#include <pty.h>
#include <stdlib.h>
#include <string>
#include <tuple>
#include <unistd.h>
#include <vector>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "error.h"
#include "str.h"


// this code needs more error checking TODO
std::tuple<pid_t, int, int> exec_with_pipe(const std::string & command, const std::string & dir, const int width, const int height)
{
	int fd_master { -1 };

	struct winsize terminal_dimensions { 0 };
	terminal_dimensions.ws_col = width;
	terminal_dimensions.ws_row = height;

	pid_t pid = forkpty(&fd_master, nullptr, nullptr, &terminal_dimensions);
	if (pid == -1)
		error_exit(true, "exec_with_pipe: forkptry failed");

        if (pid == 0) {
                setsid();

		std::string columns = myformat("%d", width);
		std::string lines   = myformat("%d", height);

		if (setenv("COLUMNS", columns.c_str(), 1) == -1)
			error_exit(true, "exec_with_pipe: setenv(COLUMNS) failed");

		if (setenv("LINES", lines.c_str(), 1) == -1)
			error_exit(true, "exec_with_pipe: setenv(LINES) failed");

		if (setenv("TERM", "ansi", 1) == -1)
			error_exit(true, "exec_with_pipe: setenv(TERM) failed");

                if (dir.empty() == false && chdir(dir.c_str()) == -1)
                        error_exit(true, "exec_with_pipe: chdir to %s for %s failed", dir.c_str(), command.c_str());

                close(2);
                int stderr_fd = open("/dev/null", O_WRONLY);

                // TODO: a smarter way?
                int fd_max = sysconf(_SC_OPEN_MAX);
                for(int fd=3; fd<fd_max; fd++)
                        close(fd);

                std::vector<std::string> parts = split(command, " ");

                size_t n_args = parts.size();
                char **pars = new char *[n_args + 1];
                for(size_t i=0; i<n_args; i++)
                        pars[i] = (char *)parts.at(i).c_str();
                pars[n_args] = nullptr;

                if (execv(pars[0], &pars[0]) == -1)
                        error_exit(true, "Failed to invoke %s", command.c_str());
        }

        std::tuple<pid_t, int, int> out(pid, fd_master, fd_master);

        return out;
}
