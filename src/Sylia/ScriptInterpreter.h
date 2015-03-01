#ifndef f_SYLIA_SCRIPTINTERPRETER_H
#define f_SYLIA_SCRIPTINTERPRETER_H

class VDScriptValue;
class VDScriptError;
struct VDScriptObject;
class IVDScriptInterpreter;
struct VDScriptFunctionDef;

typedef VDScriptValue (*VDScriptRootHandlerPtr)(IVDScriptInterpreter *,char *,void *);

class IVDScriptInterpreter {
public:
	virtual	~IVDScriptInterpreter() {}

	virtual void SetRootHandler(VDScriptRootHandlerPtr, void *)	=0;

	virtual void ExecuteLine(const char *s)						=0;

	virtual void ScriptError(int e)								=0;
	virtual const char* TranslateScriptError(const VDScriptError& cse)	=0;
	virtual char** AllocTempString(long l)						=0;

	virtual VDScriptValue LookupObjectMember(const VDScriptObject *obj, void *, char *szIdent) = 0;

	virtual const VDScriptFunctionDef *GetCurrentMethod() = 0;
	virtual int GetErrorLocation() = 0;
	virtual VDScriptValue	DupCString(const char *) = 0;
};

IVDScriptInterpreter *VDCreateScriptInterpreter();

#define VDSCRIPT_EXT_ERROR(x)	if(true){isi->ScriptError(VDScriptError::x); VDNEVERHERE;}else

#endif
