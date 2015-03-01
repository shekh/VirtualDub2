#include <stdafx.h>

#ifdef _MSC_VER
	#pragma hdrstop
#endif

// compiler/setup checks

#if defined(_MSC_VER)
	#if _MSC_VER < 1300
		#include <windows.h>

		#line 1 " \n \n \n***** You do not have the correct version of the Microsoft Platform SDK installed *****\nPlease see Docs\\index.html for details.\n \n \n"
		namespace { const DWORD PlatformSDKTest = INVALID_SET_FILE_POINTER; }
		#line 1 ""

		#pragma warning(disable: 4505)
		#line 1 " \n \n \n***** You do not have the Visual C++ Processor Pack installed *****\nPlease see Docs\\index.html for details.\n \n \n"
		static void VCPPCheck() { __asm { sfence } }
		#line 1 ""
	#endif
#endif
