#ifndef f_REGISTRY_H
#define f_REGISTRY_H

#include <windows.h>

HKEY OpenRegKey(HKEY hkBase, char *szKeyName);
HKEY CreateRegKey(HKEY hkBase, char *szKeyName);
BOOL DeleteRegValue(HKEY hkBase, char *szKeyName, char *szValueName);
BOOL QueryRegString(HKEY hkBase, char *szKeyName, char *szValueName, char *lpBuffer, int cbBuffer);
BOOL SetRegString(HKEY hkBase, char *szKeyName, char *szValueName, char *lpBuffer);

#endif
