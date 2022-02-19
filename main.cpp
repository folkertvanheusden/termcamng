#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "picio.h"
#include "terminal.h"


int main(int argc, char *argv[])
{
	font f("t/FONTS/SYSTEM/FREEDOS/CPIDOS30/CP850.F16");

	terminal t(&f, 80, 25);

	t.process_input("Hallo,\r\nDit is een test.\r\n\r\n");
	t.process_input("Hallo,\r\nDit is een test.\r\n\r\n");
	t.process_input("Hallo,\r\nDit is een test.\r\n\r\n");
	t.process_input("Hallo,\r\nDit is een test.\r\n\r\n");
	t.process_input("Hallo,\r\nDit is een test.\r\n\r\n");
	t.process_input("A");

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
