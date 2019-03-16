#ifndef f_VD2_HOTKEYEXCONTROL_H
#define f_VD2_HOTKEYEXCONTROL_H

#include <vd2/system/event.h>
#include <vd2/system/unknown.h>

struct VDUIAccelerator;
class IVDUIHotKeyExControl;

#define VDUIHOTKEYEXCLASS "VDHotKeyEx"

bool VDUIRegisterHotKeyExControl();
IVDUIHotKeyExControl *VDGetUIHotKeyExControl(VDGUIHandle h);

class IVDUIHotKeyExControl : public IVDRefUnknown {
public:
	enum { kTypeID = 'uihk' };

	virtual void GetAccelerator(VDUIAccelerator& accel) = 0;
	virtual void SetAccelerator(const VDUIAccelerator& accel) = 0;
	virtual void Clear() = 0;

	virtual VDEvent<IVDUIHotKeyExControl, VDUIAccelerator>& OnChange() = 0;
};

#endif
