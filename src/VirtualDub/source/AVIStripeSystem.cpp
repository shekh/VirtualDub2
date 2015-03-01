//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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
#include <vfw.h>

#include "ProgressDialog.h"
#include <vd2/system/error.h>

#include "AVIStripeSystem.h"

///////////////////////////////////////////////////////////////////////////
//
//	AVIStripe
//
///////////////////////////////////////////////////////////////////////////

void *AVIStripe::operator new(size_t stSize, int iNameBytes) {
	return malloc(stSize + iNameBytes);
}

void AVIStripe::operator delete(void *p) {
	free(p);
}

void AVIStripe::operator delete(void *p, int iNameBytes) {
	free(p);
}

///////////////////////////////////////////////////////////////////////////
//
//	AVIStripeSystem
//
///////////////////////////////////////////////////////////////////////////

void AVIStripeSystem::_construct(int nStripes) {
	int i;

	this->nStripes = nStripes;

	if (!(stripe = new AVIStripe *[nStripes]))
		throw MyMemoryError();

	for(i=0; i<nStripes; i++)
		stripe[i] = NULL;
}

AVIStripeSystem::AVIStripeSystem(int nStripes) {
	_construct(nStripes);
}

static bool get_line(char *buf, size_t buf_size, FILE *f) {
	char *s;

	do {
		if (!fgets(buf, buf_size, f))
			return false;

		s = buf;

		while(*s && isspace((unsigned char)*s) && *s!='\n')
			++s;

	} while(!*s || *s=='\n' || buf[0]=='#');

	return true;
}

AVIStripeSystem::AVIStripeSystem(const char *szFile) {
	FILE *f = NULL;

	stripe = NULL;

	try {
		char linebuf[512];
		int stripe_cnt, cur;
		int lineno = 2;
		char *s, *t;

		// Type of lines we are trying to parse:
		//
		//	0   i   131072     65536      e:\capture_master.avi
		//	0   v   4194304    1048576   "e:\capture video stripe 1.avi"
		//  -1  v	1048576    524288    "i:\capture video stripe 2.avi"


		f = fopen(szFile, "r");
		if (!f) throw MyError("Couldn't open stripe definition file \"%s\"", szFile);

		if (!get_line(linebuf, sizeof linebuf, f))
				throw MyError("Failure reading first line of stripe def file");

		if (1!=sscanf(linebuf, " %d \n", &stripe_cnt))
			throw MyError("First line of stripe definition file must contain stripe count");

		if (stripe_cnt<=0)
			throw MyError("Invalid number of stripes (%d)", stripe_cnt);

		_construct(stripe_cnt);

		for(cur=0; cur<stripe_cnt; cur++) {
			int iPri, iName;
			long lBuffer, lChunk;
			char cMode[2];
			int match_count;

			if (!get_line(linebuf, sizeof linebuf, f))
				throw MyError("Failure reading stripe definition file");

			match_count = sscanf(linebuf, " %d %1s %ld %ld %n", &iPri, cMode, &lBuffer, &lChunk, &iName);

			if (match_count != 4)
				throw MyError("Stripe definition parse error: line %d", lineno);

			t = s = linebuf + iName;
			if (*s=='"') {
				++s, ++t;
				while(*t && *t!='\n' && *t!='"') ++t;
			} else
				while(*t && *t!='\n' && !isspace((unsigned char)*t)) ++t;

			if (t<=s)
				throw MyError("Stripe definition parse error: line %d -- no stripe filename!", lineno);

			switch(tolower(cMode[0])) {
			case 'm':	cMode[0] = AVIStripe::MODE_MASTER; break;
			case 'i':	cMode[0] = AVIStripe::MODE_INDEX; break;
			case 'v':	cMode[0] = AVIStripe::MODE_VIDEO; break;
			case 'a':	cMode[0] = AVIStripe::MODE_AUDIO; break;
			case 'b':	cMode[0] = AVIStripe::MODE_BOTH; break;
			default:
				throw MyError("Invalid stripe mode '%c'", cMode[0]);
			};

			// Allocate a stripe structure and copy the data into it

			if (!(stripe[cur] = new(t+1-s) AVIStripe))
				throw MyMemoryError();

			*t=0;

			stripe[cur]->lBufferSize = lBuffer;
			stripe[cur]->lChunkSize	= lChunk;
			stripe[cur]->iNameLen	= t+1-s;
			stripe[cur]->cStripeMode	= cMode[0];
			stripe[cur]->scPriority	= (signed char)iPri;
			strcpy(stripe[cur]->szName, s);

			++lineno;
		}
	} catch(...) {
		if (f) fclose(f);
		_destruct();
		throw;
	}
	fclose(f);
}

void AVIStripeSystem::_destruct() {
	int i;

	if (stripe)
		for(i=0; i<nStripes; i++)
			delete stripe[i];

	delete stripe;
}

AVIStripeSystem::~AVIStripeSystem() {
	_destruct();
}


int AVIStripeSystem::getStripeCount() {
	return nStripes;
}

AVIStripe *AVIStripeSystem::getStripeInfo(int nStripe) {
	return stripe[nStripe];
}


///////////////////////////////////////////////////////////////////////////
//
//	AVIStripeIndexLookup
//
///////////////////////////////////////////////////////////////////////////

AVIStripeIndexLookup::AVIStripeIndexLookup(IAVIReadStream *pasIndex) {
	index_table		= NULL;

	try {
		AVIStripeIndexEntry *asieptr;
		LONG lStart, lCur, lEnd;

		if (-1 == (lStart = (long)pasIndex->Start())
			|| -1 == (lEnd = (long)pasIndex->End()))

			throw MyError("Stripe index: can't get start/end of index stream");

		ProgressDialog pd(NULL, "AVI Striped Import Filter", "Reading stripe index", lEnd-lStart, true);

		pd.setValueFormat("Frame %ld/%ld");

		index_table_size = lEnd - lStart;

		if (!(index_table = new AVIStripeIndexEntry[index_table_size]))
			throw MyMemoryError();

		asieptr = index_table;

		pasIndex->BeginStreaming(lStart, lEnd, 100000);

		lCur = lStart;

		while(lCur < lEnd) {
			HRESULT err;
			LONG lActualBytes, lActualSamples;

			err = pasIndex->Read(lCur, lEnd-lCur, asieptr, sizeof(AVIStripeIndexEntry)*(lEnd-lCur), &lActualBytes, &lActualSamples);

			if (err == AVIERR_OK && !lActualSamples)
				err = AVIERR_FILEREAD;

			if (err != AVIERR_OK) throw MyAVIError("AVIStripeIndex",err);

			if ((size_t)lActualBytes != lActualSamples * sizeof(AVIStripeIndexEntry))
				throw MyError("Stripe index: bad index marks! (not 16 bytes)");

			lCur += lActualSamples;
			asieptr += lActualSamples;

			pd.advance(lCur);
			pd.check();
		}

		pasIndex->EndStreaming();

	} catch(...) {
		delete index_table;
		throw;
	}
}

AVIStripeIndexLookup::~AVIStripeIndexLookup() {
	delete index_table;
}

AVIStripeIndexEntry *AVIStripeIndexLookup::lookup(long sample) {
	long l, pivot, r;

//	_RPT1(0, "AVIStripeIndexEntry: looking up %ld\n", sample);

	l = 0;
	r = index_table_size-1;

	while(l <= r) {
		pivot = (l+r)/2;

		if (sample > index_table[pivot])
			l = pivot+1;
		else if (sample < index_table[pivot])
			r = pivot-1;
		else {
//			_RPT2(0,"\tFound in stripe %ld at %ld\n", index_table[pivot].lStripe+1, index_table[pivot].lStripeSample);
			return &index_table[pivot];
		}
	}

	_RPT0(0,"\tNot found!\n");

	return NULL;
}
