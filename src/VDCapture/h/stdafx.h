// Compilation workaround for high char annoyance in the Platform SDK:
//
// c:\platsdk5\include\uuids.h : warning C4819: The file contains a character that cannot
// be represented in the current code page (932). Save the file in Unicode format to
// prevent data loss
#pragma warning(disable: 4819)

#define _WIN32_WINNT 0x0501
#include <vd2/system/vdtypes.h>
