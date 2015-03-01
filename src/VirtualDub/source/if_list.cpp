#include "stdafx.h"
#include "plugins.h"

extern const VDXPluginInfo g_inputDrv_plugindef_TestId;

static const VDPluginInfo *const g_inputDrv_list[]={
	&g_inputDrv_plugindef_TestId,
	NULL
};

void VDInitBuiltinInputDrivers() {
	VDAddInternalPlugins(g_inputDrv_list);
}

