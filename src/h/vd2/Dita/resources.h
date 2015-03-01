#ifndef f_VD2_DITA_RESOURCES_H
#define f_VD2_DITA_RESOURCES_H

void VDInitResourceSystem();
void VDDeinitResourceSystem();

bool VDLoadResources(int moduleID, const void *src, int length);
void VDUnloadResources(int moduleID);

void VDLoadStaticStringTableA(int moduleID, int tableID, const char *const *pStrings);
void VDLoadStaticStringTableW(int moduleID, int tableID, const wchar_t *const *pStrings);

const wchar_t *VDLoadString(int moduleID, int table, int id);
const wchar_t *VDTryLoadString(int moduleID, int table, int id);
const unsigned char *VDLoadDialog(int moduleID, int id);
const unsigned char *VDLoadTemplate(int moduleID, int id);

void VDLogAppMessage(int loglevel, int table, int id);
void VDLogAppMessage(int loglevel, int table, int id, int args, ...);
void VDLogAppMessageLimited(int& count, int loglevel, int table, int id, int args, ...);

#endif
