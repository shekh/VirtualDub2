#include <vd2/plugin/vdvideofiltold.h>
#include <vd2/plugin/vdvideoutil.h>
#include <math.h>
#include <stdio.h>
#include <windows.h>
#include "resource.h"

HINSTANCE g_hInst;



///////////////////////////////////

int tutorial_null_run(const FilterActivation *fa, const FilterFunctions *ff) {
	return 0;
}

long tutorial_null_param(FilterActivation *fa, const FilterFunctions *ff) {
	fa->dst.offset	= fa->src.offset;
	return 0;
}

FilterDefinition filterDef_tutorial_null={
	0,0,NULL,				// next, prev, module (reserved)
	"tutorial: null",		// name
	"Does nothing.",		// desc
	"Avery Lee",			// author
	NULL,					// private data
	0,						// inst_data_size
	NULL,NULL,				// initProc, deinitProc
	tutorial_null_run,		// runProc
	tutorial_null_param,	// paramProc
};


///////////////////////////////////

static const int rtab[256]={
	#define formula(x) 54*x+128
	vd_maketable256
	#undef formula
};

static const int gtab[256]={
	#define formula(x) 183*x
	vd_maketable256
	#undef formula
};

static const int btab[256]={
	#define formula(x) 19*x
	vd_maketable256
	#undef formula
};

int tutorial_grayscale_run(const FilterActivation *fa, const FilterFunctions *ff) {
	vd_transform_pixmap_inplace(fa->dst) {
		int r, g, b;

		vd_pixunpack(px, r, g, b);

//		px = 0x010101 * ((r*54 + g*183 + b*19 + 128) >> 8);
		px = vd_pixavg_up(px, 0x010101 * ((rtab[r] + gtab[g] + btab[b]) >> 8));
	}

	return 0;
}

long tutorial_grayscale_param(FilterActivation *fa, const FilterFunctions *ff) {
	fa->dst.offset	= fa->src.offset;
	return 0;
}

FilterDefinition filterDef_tutorial_grayscale={
	0,0,NULL,
	"tutorial: grayscale",
	"Converts the incoming image to grayscale.",
	"Avery Lee",
	NULL,
	0,
	NULL,NULL,
	tutorial_grayscale_run,
	tutorial_grayscale_param,
	NULL,
	NULL,
};


///////////////////////////////////

int tutorial_pixeldouble_run(const FilterActivation *fa, const FilterFunctions *ff) {
	vd_pixrow_iter srcrow(fa->src);
	vd_pixrow_iter dstrow1(fa->dst, 0, 0);
	vd_pixrow_iter dstrow2(fa->dst, 0, 1);

	dstrow1.mulstep(2);
	dstrow2.mulstep(2);

	const int srcw = fa->src.w;
	const int srch = fa->src.h;

	for(int y=0; y<srch; ++y) {
		for(int x=0; x<srcw; ++x) {
			dstrow1[x*2] = dstrow1[x*2+1] = dstrow2[x*2] = dstrow2[x*2+1] = srcrow[x];
		}

		++dstrow1;
		++dstrow2;
		++srcrow;
	}

	return 0;
}

long tutorial_pixeldouble_param(FilterActivation *fa, const FilterFunctions *ff) {
	fa->dst.w *= 2;
	fa->dst.h *= 2;
	fa->dst.AlignTo8();
	return FILTERPARAM_SWAP_BUFFERS;
}

FilterDefinition filterDef_tutorial_pixeldouble={
	0,0,NULL,
	"tutorial: pixeldouble",
	"Doubles an image's size.",
	"Avery Lee",
	NULL,
	0,
	NULL,NULL,
	tutorial_pixeldouble_run,
	tutorial_pixeldouble_param,
	NULL,
	NULL,
};


///////////////////////////////////

inline double lerp(double a, double b, double f) {
	return a + (b-a)*f;
}

inline uint32 bilinear_fetch(const VFBitmap& src, double x, double y) {
	// top, left clipping
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;

	// compute anchor pixel (top-left) and fractional offsets
	int xi = (int)x;
	int yi = (int)y;
	double xf = x - xi;
	double yf = y - yi;

	// bottom, right clipping
	if (xi >= src.w - 1) {
		xi = src.w - 2;
		xf = 1;
	}

	if (yi >= src.h - 1) {
		yi = src.h - 2;
		yf = 1;
	}

	// fetch 2x2 pixel square from source and unpack to RGB
	vd_pixrow_iter row1(src, xi, yi);
	vd_pixrow_iter row2(src, xi, yi+1);

	int r1, g1, b1, r2, g2, b2, r3, g3, b3, r4, g4, b4;

	vd_pixunpack(row1[0], r1, g1, b1);
	vd_pixunpack(row1[1], r2, g2, b2);
	vd_pixunpack(row2[0], r3, g3, b3);
	vd_pixunpack(row2[1], r4, g4, b4);

	// compute interpolated pixel and repack
	int r_out = (int)lerp(lerp(r1, r2, xf), lerp(r3, r4, xf), yf);
	int g_out = (int)lerp(lerp(g1, g2, xf), lerp(g3, g4, xf), yf);
	int b_out = (int)lerp(lerp(b1, b2, xf), lerp(b3, b4, xf), yf);

	return vd_pixpack(r_out, g_out, b_out);
}

int tutorial_pixeldouble_filtered_run(const FilterActivation *fa, const FilterFunctions *ff) {
	vd_pixrow_iter dstrow(fa->dst, 0, 0);

	const int dstw = fa->dst.w;
	const int dsth = fa->dst.h;

	for(int y=0; y<dsth; ++y) {
		for(int x=0; x<dstw; ++x)
			dstrow[x] = bilinear_fetch(fa->src, x*0.5-0.25, y*0.5-0.25);

		++dstrow;
	}

	return 0;
}

int tutorial_pixeldouble_rotated_run(const FilterActivation *fa, const FilterFunctions *ff) {
	vd_pixrow_iter dstrow(fa->dst, 0, 0);

	const int dstw = fa->dst.w;
	const int dsth = fa->dst.h;

	static const double s = sin(10 * 3.1415926535897932 / 180.0);
	static const double c = cos(10 * 3.1415926535897932 / 180.0);

	const double xcen = fa->src.w*0.5 - 0.5;
	const double ycen = fa->src.h*0.5 - 0.5;

	for(int y=0; y<dsth; ++y) {
		for(int x=0; x<dstw; ++x) {
			double xc = x*0.5 - 0.25 - xcen;
			double yc = y*0.5 - 0.25 - ycen;

			double x2 = xcen + xc*c + yc*s;
			double y2 = ycen - xc*s + yc*c;

			dstrow[x] = bilinear_fetch(fa->src, x2, y2);
		}

		++dstrow;
	}

	return 0;
}

long tutorial_pixeldouble_filtered_param(FilterActivation *fa, const FilterFunctions *ff) {
	fa->dst.w *= 2;
	fa->dst.h *= 2;
	fa->dst.AlignTo8();
	return FILTERPARAM_SWAP_BUFFERS;
}

FilterDefinition filterDef_tutorial_pixeldouble_filtered={
	0,0,NULL,
	"tutorial: pixeldouble filtered",
	"Doubles an image's size with bilinear filtering.",
	"Avery Lee",
	NULL,
	0,
	NULL,NULL,
	tutorial_pixeldouble_filtered_run,
	tutorial_pixeldouble_filtered_param,
	NULL,
	NULL,
};

FilterDefinition filterDef_tutorial_pixeldouble_rotated={
	0,0,NULL,
	"tutorial: pixeldouble rotated",
	"Doubles an image's size and rotates it 10 degrees with bilinear filtering.",
	"Avery Lee",
	NULL,
	0,
	NULL,NULL,
	tutorial_pixeldouble_rotated_run,
	tutorial_pixeldouble_filtered_param,
	NULL,
	NULL,
};

////////////////////////////////////////////////////////////

struct TutorialSaturationFilterData {
	double saturation;

	IFilterPreview *ifp;
};

int tutorial_saturation_init(FilterActivation *fa, const FilterFunctions *ff) {
	TutorialSaturationFilterData *pfd = (TutorialSaturationFilterData *)fa->filter_data;

	pfd->saturation = 0.5;
	return 0;
}

int tutorial_saturation_run(const FilterActivation *fa, const FilterFunctions *ff) {
	TutorialSaturationFilterData *pfd = (TutorialSaturationFilterData *)fa->filter_data;

	const int isat			= (int)((1.0 - pfd->saturation) * 256.0);
	const int inv_isat_256	= (256-isat) * 256;

	vd_transform_pixmap_inplace(fa->dst) {
		int r, g, b;
		vd_pixunpack(px, r, g, b);

		const int y = (r*54 + g*183 + b*19 + 128) * isat;

		r = (r * inv_isat_256 + y + 32768) >> 16;
		g = (g * inv_isat_256 + y + 32768) >> 16;
		b = (b * inv_isat_256 + y + 32768) >> 16;

		px = vd_pixpack(r, g, b);
	}

	return 0;
}

long tutorial_saturation_param(FilterActivation *fa, const FilterFunctions *ff) {
	fa->dst.offset	= fa->src.offset;
	return 0;
}

INT_PTR CALLBACK tutorial_saturation_dlgproc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	TutorialSaturationFilterData *pfd = (TutorialSaturationFilterData *)GetWindowLongPtr(hdlg, DWLP_USER);

	switch(msg) {
		case WM_INITDIALOG:
			SetWindowLongPtr(hdlg, DWLP_USER, lParam);
			pfd = (TutorialSaturationFilterData *)lParam;

			if (pfd->ifp)
				pfd->ifp->InitButton(GetDlgItem(hdlg, IDC_PREVIEW));

			SetDlgItemInt(hdlg, IDC_VALUE, (int)(0.5 + pfd->saturation*100), FALSE);
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				{
					BOOL bValid;
					UINT v = GetDlgItemInt(hdlg, IDC_VALUE, &bValid, FALSE);

					if (!bValid || v > 100) {
						MessageBeep(MB_ICONEXCLAMATION);
						SetFocus(GetDlgItem(hdlg, IDC_VALUE));
						return FALSE;
					}

					pfd->saturation = (int)v / 100.0;
				}
				EndDialog(hdlg, TRUE);
				return TRUE;

			case IDCANCEL:
				EndDialog(hdlg, FALSE);
				return TRUE;

			case IDC_VALUE:
				if (HIWORD(wParam) == EN_KILLFOCUS) {
					BOOL bValid;
					UINT v = GetDlgItemInt(hdlg, IDC_VALUE, &bValid, FALSE);

					if (bValid && v <= 100) {
						pfd->saturation = (int)v / 100.0;
						if (pfd->ifp)
							pfd->ifp->RedoFrame();
					}
				}
				return TRUE;

			case IDC_PREVIEW:
				if (pfd->ifp)
					pfd->ifp->Toggle(hdlg);
				return TRUE;
			}
			break;

	}
	return FALSE;
}

int tutorial_saturation_config(FilterActivation *fa, const FilterFunctions *ff, HWND hWnd) {
	TutorialSaturationFilterData *pfd = (TutorialSaturationFilterData *)fa->filter_data;

	// cache filter preview interface
	pfd->ifp = fa->ifp;

	// cache original configuration
	TutorialSaturationFilterData fd_temp(*pfd);

	// display dialog
	if (!DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_TUTORIAL_SATURATION), hWnd, tutorial_saturation_dlgproc, (LPARAM)fa->filter_data)) {
		// user hit cancel -- rollback changes
		*pfd = fd_temp;
		return TRUE;
	}

	return FALSE;
}

void tutorial_saturation_string(const FilterActivation *fa, const FilterFunctions *ff, char *buf) {
	fa->filter->stringProc2(fa, ff, buf, 100);
}

void tutorial_saturation_string2(const FilterActivation *fa, const FilterFunctions *ff, char *buf, int maxlen) {
	TutorialSaturationFilterData *pfd = (TutorialSaturationFilterData *)fa->filter_data;

	_snprintf(buf, maxlen, " (%d%%)", (int)(0.5 + 100.0*pfd->saturation));
}

bool tutorial_saturation_fss(FilterActivation *fa, const FilterFunctions *ff, char *buf, int maxlen) {
	TutorialSaturationFilterData *pfd = (TutorialSaturationFilterData *)fa->filter_data;

	_snprintf(buf, maxlen, "Config(%d)", (int)(0.5 + 10000.0*pfd->saturation));
	return true;
}

static void tutorial_saturation_script_config(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterActivation *fa = (FilterActivation *)lpVoid;
	TutorialSaturationFilterData *pfd = (TutorialSaturationFilterData *)fa->filter_data;

	pfd->saturation = argv[0].asInt() / 10000.0;
}

static ScriptFunctionDef tutorial_saturation_func_defs[]={
	{ (ScriptFunctionPtr)tutorial_saturation_script_config, "Config", "0i" },
	{ NULL },
};

static CScriptObject tutorial_saturation_obj={
	NULL, tutorial_saturation_func_defs
};

FilterDefinition filterDef_tutorial_saturation={
	0,0,NULL,
	"tutorial: saturation",
	"Adjusts a video's saturation.",
	"Avery Lee",
	NULL,
	sizeof(TutorialSaturationFilterData),
	tutorial_saturation_init,
	NULL,
	tutorial_saturation_run,
	tutorial_saturation_param,
	tutorial_saturation_config,
	tutorial_saturation_string,
	NULL,
	NULL,
	&tutorial_saturation_obj,
	tutorial_saturation_fss,
	tutorial_saturation_string2
};

////////////////////////////////////////////////////////////

extern FilterDefinition filterDef_supersample;

namespace {
	FilterDefinition *const g_filters[]={
		&filterDef_tutorial_null,
		&filterDef_tutorial_grayscale,
		&filterDef_tutorial_pixeldouble,
		&filterDef_tutorial_pixeldouble_filtered,
		&filterDef_tutorial_pixeldouble_rotated,
		&filterDef_tutorial_saturation,
	};

	enum {
		kFilterCount = sizeof g_filters / sizeof g_filters[0]
	};

	FilterDefinition *g_installedFilters[kFilterCount];
}

////////////////////////////////////////////////////////////

extern "C" int __declspec(dllexport) __cdecl VirtualdubFilterModuleCheckVersion();
extern "C" int __declspec(dllexport) __cdecl VirtualdubFilterModuleInit2(FilterModule *fm, const FilterFunctions *ff, int& vdfd_ver, int& vdfd_compat);
extern "C" void __declspec(dllexport) __cdecl VirtualdubFilterModuleDeinit(FilterModule *fm, const FilterFunctions *ff);

int __declspec(dllexport) __cdecl VirtualdubFilterModuleCheckVersion() {
	return VIRTUALDUB_FILTERDEF_VERSION;
}

int __declspec(dllexport) __cdecl VirtualdubFilterModuleInit2(FilterModule *fm, const FilterFunctions *ff, int& vdfd_ver, int& vdfd_compat) {

	for(int i=0; i<kFilterCount; ++i)
		g_installedFilters[i] = ff->addFilter(fm, g_filters[i], sizeof(FilterDefinition));

	vdfd_ver = VIRTUALDUB_FILTERDEF_VERSION;
	vdfd_compat = 9;		// copy constructor support required

	g_hInst = fm->hInstModule;

	return 0;
}

void __declspec(dllexport) __cdecl VirtualdubFilterModuleDeinit(FilterModule *fm, const FilterFunctions *ff) {
	for(int i=0; i<kFilterCount; ++i)
		if (g_installedFilters[i]) {
			ff->removeFilter(g_installedFilters[i]);
			g_installedFilters[i] = 0;
		}
}
