#include <string>
#include <tuple>


std::tuple<pid_t, int, int> exec_with_pipe(const std::string & command, const std::string & dir, const int width, const int height, const int restart_interval, const bool stderr_to_stdout);
