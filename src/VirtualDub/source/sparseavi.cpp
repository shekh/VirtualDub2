//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2004 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "stdafx.h"

#include <ctype.h>

#include <windows.h>
#include <commdlg.h>
#include <mmsystem.h>

#include <vd2/system/error.h>
#include <vd2/system/file.h>
#include <vd2/Dita/services.h>
#include "ProgressDialog.h"
#include "gui.h"

extern "C" unsigned long version_num;
extern const char g_szError[];

struct SparseAVIHeader {
	enum {
		kChunkID = 'VAPS',
		kChunkSize = 30
	};

	unsigned long		ckid;
	unsigned long		size;				// == size of this structure minus 8
	sint64				original_size;		// original file size, in bytes
	sint64				copied_size;		// amount of original data included (total size - sparsed data if no errors occur)
	sint64				error_point;		// number of source bytes processed before error or EOF occurred
	unsigned long		error_bytes;		// number of bytes copied beyond error point
	unsigned short		signature_length;	// length of name signature, without null terminating character

	// Here follows the null-terminated ANSI name of the application
	// that generated the sparsed file.

};

void CreateSparseAVI(const char *pszIn, const char *pszOut) {	
	try {
		VDFile infile(pszIn);
		VDFile outfile(pszOut, nsVDFile::kWrite | nsVDFile::kCreateAlways);

		// Generate header

		char buf[4096]={0};
		SparseAVIHeader spah;
		sint64 insize = infile.size();
		unsigned long ckidbuf[2][256];
		int ckidcount = 0;
		sint64 ckidpos = 0;

		int l = sprintf(buf, "VirtualDub2 build %d/%s", version_num,
#ifdef _DEBUG
		"debug"
#else
		"release"
#endif
				);

		spah.ckid				= SparseAVIHeader::kChunkID;
		spah.size				= SparseAVIHeader::kChunkSize;
		spah.original_size		= insize;
		spah.copied_size		= 0;		// we will fill this in later
		spah.error_point		= 0;		// we will fill this in later
		spah.error_bytes		= 0;		// we will fill this in later
		spah.signature_length	= (uint16)l;

		spah.size += l+1;

		outfile.write(&spah, 8 + SparseAVIHeader::kChunkSize);
		outfile.write(buf, (l+2)&~1);

		sint64 copy_start = outfile.size();

		// Sparse the AVI file!

		ProgressDialog pd(NULL, "Creating sparse AVI", "Processing source file", (long)((insize+1023)>>10), true);
		pd.setValueFormat("%ldK of %ldK");

		for(;;) {
			sint64 pos = infile.tell();
			FOURCC fcc;
			DWORD dwLen;

			pd.advance((long)(pos>>10));
			pd.check();

			if (!infile.readData(&fcc, 4) || !infile.readData(&dwLen,4) || !isprint((unsigned char)(fcc>>24))
					|| !isprint((unsigned char)(fcc>>16))
					|| !isprint((unsigned char)(fcc>> 8))
					|| !isprint((unsigned char)(fcc    ))
					|| pos+dwLen+8 > insize) {

				if (ckidcount) {
					sint64 pos2 = outfile.tell();

					outfile.seek(ckidpos);
					outfile.write(ckidbuf, sizeof ckidbuf);
					outfile.seek(pos2);
				}

				spah.copied_size = outfile.tell() - copy_start;
				spah.error_point = pos;

				if (infile.tell() < insize) {
					// error condition

					infile.seek(pos);

					long actual = infile.readData(buf, sizeof buf);

					if (actual > 0) {
						spah.error_bytes = actual;

						outfile.write(buf, actual);
					}
				}

				break;
			}

			if (!ckidcount) {
				ckidpos = outfile.tell();

				memset(ckidbuf, 0, sizeof ckidbuf);
				outfile.write(ckidbuf, sizeof ckidbuf);
			}

			ckidbuf[0][ckidcount] = fcc;
			ckidbuf[1][ckidcount] = dwLen;

			if (++ckidcount >= 256) {
				sint64 pos2 = outfile.tell();

				outfile.seek(ckidpos);
				outfile.write(ckidbuf, sizeof ckidbuf);
				outfile.seek(pos2);
				ckidcount = 0;
			}

			// Sparse any chunk that is of the form ##xx, or that are padding.

			if (fcc=='KNUJ' || (isdigit((unsigned char)fcc) && isdigit((unsigned char)(fcc>>8)))) {
				infile.skip(dwLen+(dwLen&1));
				continue;
			}

			// Break into LIST and RIFF chunks.

			if (fcc == 'TSIL' || fcc == 'FFIR') {
				infile.read(&fcc, 4);
				outfile.write(&fcc, 4);
				continue;

			}

			// Not sparsing, copy it.  Difference in 16-byte chunks at a time for better
			// compression on index blocks.

			char diffbuf[16]={0};
			int diffoffset = 0;

			dwLen = (dwLen+1)&~1;

			while(dwLen > 0) {
				unsigned long tc = 4096 - ((int)outfile.tell()&4095);

				if (tc > dwLen)
					tc = dwLen;

				infile.read(buf, tc);

				for(int i=0; i<tc; ++i) {
					char c = diffbuf[diffoffset];
					char d = buf[i];

					buf[i] = (char)(d - c);
					diffbuf[diffoffset] = d;
					diffoffset = (diffoffset+1)&15;
				}

				outfile.write(buf, tc);

				dwLen -= tc;
			}
		}

		// Rewrite the sparse file header.

		outfile.seek(0);
		outfile.write(&spah, SparseAVIHeader::kChunkSize);

	} catch(DWORD err) {
		throw MyWin32Error("Error creating sparse AVI: %%s", err);
	}
}

void ExpandSparseAVI(HWND hwndParent, const char *pszIn, const char *pszOut) {
	try {
		VDFile infile(pszIn);
		VDFile outfile(pszOut, nsVDFile::kWrite | nsVDFile::kCreateAlways);

		// Read header

		char buf[4096]={0};
		SparseAVIHeader spah;
		unsigned long ckidbuf[2][256];
		int ckidcount = 0;

		infile.read(&spah, 8);
		if (spah.size < SparseAVIHeader::kChunkSize)
			throw MyError("Invalid sparse header.");

		infile.read(&spah.original_size, SparseAVIHeader::kChunkSize);
		infile.skip((spah.size - SparseAVIHeader::kChunkSize + 1)&~1);

		// Expand the sparse AVI file.

		{
			ProgressDialog pd(NULL, "Expanding sparse AVI", "Writing output file", (long)((spah.error_point + spah.error_bytes +1023)>>10), true);
			pd.setValueFormat("%ldK of %ldK");

			while(outfile.tell() < spah.error_point) {
				FOURCC fcc;
				DWORD dwLen;

				pd.advance((long)(outfile.tell()>>10));
				pd.check();

				if (!ckidcount)
					infile.read(ckidbuf, sizeof ckidbuf);

				fcc = ckidbuf[0][ckidcount];
				dwLen = ckidbuf[1][ckidcount];
				ckidcount = (ckidcount+1)&255;

				outfile.write(&fcc, 4);
				outfile.write(&dwLen, 4);

				// Sparse any chunk that is of the form ##xx, or that are padding.

				if (fcc=='KNUJ' || (isdigit((unsigned char)fcc) && isdigit((unsigned char)(fcc>>8)))) {
					memset(buf, 0, sizeof buf);

					dwLen = (dwLen+1)&~1;

					while(dwLen > 0) {
						unsigned long tc = sizeof buf;
						if (tc > dwLen)
							tc = dwLen;

						outfile.write(buf, tc);
						dwLen -= tc;
					}

					continue;
				}

				// Break into LIST and RIFF chunks.

				if (fcc == 'TSIL' || fcc == 'FFIR') {
					infile.read(&fcc, 4);
					outfile.write(&fcc, 4);
					continue;

				}

				// Not sparsed, copy it

				char diffbuf[16]={0};
				int diffoffset = 0;

				dwLen = (dwLen+1)&~1;

				while(dwLen > 0) {
					unsigned long tc = 4096 - ((int)outfile.tell()&4095);

					if (tc > dwLen)
						tc = dwLen;

					infile.read(buf, tc);

					for(int i=0; i<tc; ++i) {
						uint8 v = (uint8)(diffbuf[diffoffset] + buf[i]);
						buf[i] = diffbuf[diffoffset] = v;
						diffoffset = (diffoffset+1)&15;
					}

					outfile.write(buf, tc);

					dwLen -= tc;
				}
			}

			// Copy over the error bytes.

			unsigned long total = spah.error_bytes;

			while(total>0) {
				unsigned long tc = sizeof buf;

				if (tc > total)
					tc = total;

				infile.read(buf, tc);
				outfile.write(buf, tc);

				total -= tc;
			}
		}

		guiMessageBoxF(hwndParent, "VirtualDub notice", MB_OK|MB_ICONINFORMATION,
			"Sparse file details:\n"
			"\n"
			"Original file size: %I64d\n"
			"Copied bytes: %I64d\n"
			"Error location: %I64d\n"
			"Error bytes: %ld\n"
			,spah.original_size
			,spah.copied_size
			,spah.error_point
			,spah.error_bytes);

	} catch(DWORD err) {
		throw MyWin32Error("Error creating sparse AVI: %%s", err);
	} catch(const MyError&) {
		throw;
	}
}

void CreateExtractSparseAVI(HWND hwndParent, bool bExtract) {
	static const wchar_t avifilter[]=L"Audio-video interleave (*.avi)\0*.avi\0All files (*.*)\0*.*\0";
	static const wchar_t sparsefilter[]=L"Sparsed AVI file (*.sparse)\0*.sparse\0";

	const VDStringW infile(VDGetLoadFileName(VDFSPECKEY_LOADVIDEOFILE, (VDGUIHandle)hwndParent, bExtract ? L"Select sparse AVI file" : L"Select source AVI file", bExtract ? sparsefilter : avifilter, bExtract ? L"sparse" : L"avi"));

	if (!infile.empty()) {
		const VDStringW outfile(VDGetSaveFileName(VDFSPECKEY_LOADVIDEOFILE, (VDGUIHandle)hwndParent, L"Select filename for output", bExtract ? avifilter : sparsefilter, bExtract ? L"avi" : L"sparse"));

		if (!outfile.empty()) {
			VDStringA infileA(VDTextWToA(infile));
			VDStringA outfileA(VDTextWToA(outfile));

			try {
				if (bExtract)
					ExpandSparseAVI(hwndParent, infileA.c_str(), outfileA.c_str());
				else
					CreateSparseAVI(infileA.c_str(), outfileA.c_str());
				MessageBox(hwndParent, bExtract ? "Sparse AVI expansion complete." : "Sparse AVI creation complete.", "VirtualDub notice", MB_ICONINFORMATION);
			} catch(const MyError& e) {
				e.post(hwndParent, g_szError);
			}
		}
	}
}

