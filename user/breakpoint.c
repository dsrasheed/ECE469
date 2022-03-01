// program to cause a breakpoint trap

#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	int w, x, y, z;
	x = 2;
	y = 3;
	asm volatile("int $3");
	z = x + y;
	w = z + y;
	return;
}

