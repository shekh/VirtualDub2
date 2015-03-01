#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "utils.h"

void oops(const char *format, ...) {
	va_list val;

	va_start(val, format);
	vprintf(format, val);
	va_end(val);
	getchar();
	exit(5);
}

void strtrim(char *s) {
	char *t = s;
	char *u = s;

	while(*t)
		++t;

	while(t>s && isspace((unsigned char)t[-1]))
		--t;

	while(u<t && isspace((unsigned char)*u))
		++u;

	memmove(s, u, t-u);
	s[t-u] = 0;
}

char *strtack(char *s, const char *t) {
	while(*s = *t)
		++s, ++t;

	return s;
}
