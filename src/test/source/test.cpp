#define _WIN32_WINNT 0x0400
#include <windows.h>

bool ShouldBreak() {
	return !!IsDebuggerPresent();
}
