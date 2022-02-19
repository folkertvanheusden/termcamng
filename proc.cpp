#include <fcntl.h>
#include <string>
#include <tuple>
#include <unistd.h>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>

#include "error.h"
#include "str.h"


// this code needs more error checking TODO
std::tuple<pid_t, int, int> exec_with_pipe(const std::string & command, const std::string & dir)
{
        int pipe_to_proc[2], pipe_from_proc[2];

        if (pipe(pipe_to_proc) == -1 || pipe(pipe_from_proc) == -1)
		error_exit(true, "exec_with_pipe: pipe() failed");

        pid_t pid = fork();
        if (pid == 0) {
                setsid();

                if (dir.empty() == false && chdir(dir.c_str()) == -1)
                        error_exit(true, "exec_with_pipe: chdir to %s for %s failed", dir.c_str(), command.c_str());

                close(0);

                dup(pipe_to_proc[0]);
                close(pipe_to_proc[1]);
                close(1);
                close(2);
                dup(pipe_from_proc[1]);

                int stderr = open("/dev/null", O_WRONLY);
                close(pipe_from_proc[0]);

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

        close(pipe_to_proc[0]);
        close(pipe_from_proc[1]); 

        std::tuple<pid_t, int, int> out(pid, pipe_to_proc[1], pipe_from_proc[0]);

        return out;
}
