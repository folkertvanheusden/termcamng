#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <unistd.h>

#include "picio.h"
#include "proc.h"
#include "terminal.h"


void process_program(terminal *const t)
{
	auto proc = exec_with_pipe("/usr/bin/httping -K 172.29.0.1", "/tmp");

        pid_t pid  = std::get<0>(proc);
        int   w_fd = std::get<1>(proc);
        int   r_fd = std::get<2>(proc);

	for(;;) {
		char buffer[4096];

		int  rc = read(r_fd, buffer, sizeof buffer);

		if (rc == -1)
			break;

		if (rc == 0)
			break;

		t->process_input(buffer, rc);
	}
}

int main(int argc, char *argv[])
{
	font f("t/FONTS/SYSTEM/FREEDOS/CPIDOS30/CP850.F16");

	terminal t(&f, 80, 25);

	std::thread thread_handle([&t] { process_program(&t); });

	sleep(5);

	uint8_t *out = nullptr;
	int      out_w = 0;
	int      out_h = 0;
	t.render(&out, &out_w, &out_h);

	printf("%d x %d\n", out_w, out_h);

	FILE *fh = fopen("test.png", "w");

	write_PNG_file(fh, out_w, out_h, out);

	fclose(fh);

	free(out);

	return 0;
}
