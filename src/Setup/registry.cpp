#include <windows.h>
#include "registry.h"

HKEY OpenRegKey(HKEY hkBase, char *szKeyName) {
	HKEY hkey;

	return RegOpenKeyEx(hkBase, szKeyName, 0, KEY_ALL_ACCESS, &hkey)==ERROR_SUCCESS
			? hkey
			: NULL;
}

HKEY CreateRegKey(HKEY hkBase, char *szKeyName) {
	HKEY hkey;
	DWORD dwDisposition;

	return RegCreateKeyEx(hkBase, szKeyName, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, &dwDisposition)==ERROR_SUCCESS
			? hkey
			: NULL;
}

HKEY CreateRegKey64(HKEY hkBase, char *szKeyName) {
	HKEY hkey;
	DWORD dwDisposition;

	return RegCreateKeyEx(hkBase, szKeyName, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS|KEY_WOW64_64KEY, NULL, &hkey, &dwDisposition)==ERROR_SUCCESS
			? hkey
			: NULL;
}

BOOL DeleteRegValue(HKEY hkBase, char *szKeyName, char *szValueName) {
	HKEY hkey;
	BOOL success;

	if (!(hkey = OpenRegKey(hkBase, szKeyName)))
		return FALSE;

	success = (RegDeleteValue(hkey, szValueName) == ERROR_SUCCESS);

	RegCloseKey(hkey);

	return success;
}

BOOL QueryRegString(HKEY hkBase, char *szKeyName, char *szValueName, char *lpBuffer, int cbBuffer) {
	HKEY hkey;
	BOOL success;
	DWORD type;

	if (!(hkey = OpenRegKey(hkBase, szKeyName)))
		return FALSE;

	success = (ERROR_SUCCESS == RegQueryValueEx(hkey, szValueName, 0, &type, (LPBYTE)lpBuffer, (LPDWORD)&cbBuffer));

	RegCloseKey(hkey);

	return success;
}

BOOL SetRegString(HKEY hkBase, char *szKeyName, char *szValueName, char *lpBuffer) {
	HKEY hkey;
	BOOL success;

	if (!(hkey = CreateRegKey(hkBase, szKeyName)))
		return FALSE;

	success = (ERROR_SUCCESS == RegSetValueEx(hkey, szValueName, 0, REG_SZ, (LPBYTE)lpBuffer, strlen(lpBuffer)+1));

	RegCloseKey(hkey);

	return success;
}

BOOL SetRegString64(HKEY hkBase, char *szKeyName, char *szValueName, char *lpBuffer) {
	HKEY hkey;
	BOOL success;

	if (!(hkey = CreateRegKey64(hkBase, szKeyName)))
		return FALSE;

	success = (ERROR_SUCCESS == RegSetValueEx(hkey, szValueName, 0, REG_SZ, (LPBYTE)lpBuffer, strlen(lpBuffer)+1));

	RegCloseKey(hkey);

	return success;
}
