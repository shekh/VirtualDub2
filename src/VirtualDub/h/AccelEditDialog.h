#ifndef f_VD2_ACCELEDITDIALOG_H
#define f_VD2_ACCELEDITDIALOG_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/vdstl.h>
#include <vd2/Dita/accel.h>

#include <vd2/VDLib/UIProxies.h>

bool VDShowDialogEditAccelerators(VDGUIHandle hParent, const VDAccelToCommandEntry *commands, uint32 commandCount, VDAccelTableDefinition& accelTable, VDAccelTableDefinition& defaultAccelTable);

#endif
