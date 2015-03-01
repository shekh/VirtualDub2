//	Asuka - VirtualDub Build/Post-Mortem Utility
//	Copyright (C) 2005 Avery Lee
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
#include <vd2/system/vdtypes.h>
#include <string>
#include <time.h>

#include "utils.h"

using namespace std;

void tool_verinc(bool amd64) {
	string machine_name(get_name());

	time_t systime;
	time(&systime);
	tm *ts = localtime(&systime);

	const int build = get_version();

	string build_time(asctime(ts));

	build_time.erase(build_time.size()-1);		// kill terminating newline

	static const char *const sMonths[12]={
		"January",
		"February",
		"March",
		"April",
		"May",
		"June",
		"July",
		"August",
		"September",
		"October",
		"November",
		"December"
	};

	char datestr[128];
	sprintf(datestr, "%s %d, %d", sMonths[ts->tm_mon], ts->tm_mday, 1900 + ts->tm_year);

	FILE *f = fopen("verstub.asm", "w");
	if (!f)
		fail("Unable to open verstub.asm for write.");

	if (amd64)
		fprintf(f,
			"\t"	"segment	.const\n"
			"\n"
			"\t"	"global\t"	"version_num\n"
			"\t"	"global\t"	"version_time\n"
			"\t"	"global\t"	"version_date\n"
			"\t"	"global\t"	"version_buildmachine\n"
			"\n"
			"version_num\t"	"dd\t"	"%ld\n"
			"version_time\t"	"db\t"	"\"%s\",0\n"
			"version_date\t"	"db\t"	"\"%s\",0\n"
			"version_buildmachine\t" "db\t" "\"%s\",0\n"
			"\n"
			"\t"	"end\n"
			,build
			,build_time.c_str()
			,datestr
			,machine_name.c_str());
	else
		fprintf(f,
			"\t"	"segment	.const\n"
			"\n"
			"\t"	"global\t"	"_version_num\n"
			"\t"	"global\t"	"_version_time\n"
			"\t"	"global\t"	"_version_date\n"
			"\t"	"global\t"	"_version_buildmachine\n"
			"\n"
			"_version_num\t"	"dd\t"	"%ld\n"
			"_version_time\t"	"db\t"	"\"%s\",0\n"
			"_version_date\t"	"db\t"	"\"%s\",0\n"
			"_version_buildmachine\t" "db\t" "\"%s\",0\n"
			"\n"
			"\t"	"end\n"
			,build
			,build_time.c_str()
			,datestr
			,machine_name.c_str());

	fclose(f);
}
