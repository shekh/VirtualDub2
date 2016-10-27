//#ifdef _M_IX86

extern "C" void *memcpy(void *dst0, const void *src0, size_t len);
#pragma function(memcpy)
extern "C" void *memcpy(void *dst0, const void *src0, size_t len) {
	char *dst = (char *)dst0;
	const char *src = (const char *)src0;

	while(len--)
		*dst++ = *src++;

	return dst0;
}

extern "C" void *memset(void *dst0, int fill, size_t len);
#pragma function(memset)
extern "C" void *memset(void *dst0, int fill, size_t len) {
	char *dst = (char *)dst0;

	while(len--)
		*dst++ = (char)fill;

	return dst0;
}

//#endif
