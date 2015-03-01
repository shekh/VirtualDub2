#include <vector>
#include "ruleset.h"

#define iterate_forward(type, obj, it) if(0);else for(type::const_iterator it = (obj).begin(), it##End = (obj).end(); it != it##End; ++it)

void dump_ia(std::vector<char>& dst, const tRuleSystem& rulesys) {
	long decomp_bytes = 0;
	long packed_bytes = 0;

	dst.resize(72, 0);
	dst.push_back(rulesys.size());

	iterate_forward(tRuleSystem, rulesys, it) {
		const ruleset& rs = *it;
		std::vector<std::pair<uint8, uint8> > last_match[4];
		std::string last_result[4];

		iterate_forward(std::list<rule>, rs.rules, itRule) {
			const rule& r = *itRule;
			std::vector<char>::size_type s;
			int prematch, postmatch;
			int i, x, ibest;
			
			int l = (int)r.match_stream.size();

			ibest = 0;
			prematch = postmatch = 0;

			for(i=0; i<4; ++i) {
				int l2 = (int)last_match[i].size();
				if (l2 > l)
					l2 = l;
				int tprematch = std::mismatch(last_match[i].begin(), last_match[i].begin() + l2, r.match_stream.begin()).first - last_match[i].begin();
				int tpostmatch = std::mismatch(last_match[i].rbegin(), last_match[i].rbegin() + l2, r.match_stream.rbegin()).first - last_match[i].rbegin();

				if (tprematch+tpostmatch > prematch+postmatch) {
					prematch = tprematch;
					postmatch = tpostmatch;
					ibest = i;
				}
			}

			if (prematch > 7)
				prematch = 7;

			if (postmatch > 7)
				postmatch = 7;

			if (postmatch > l - prematch)
				postmatch = l - prematch;

			dst.push_back(ibest*64 + postmatch*8 + prematch);
			dst.push_back(1+l - prematch - postmatch);

			for(x=prematch; x<l - postmatch; ++x) {
				dst.push_back(r.match_stream[x].first);
				dst.push_back(r.match_stream[x].second);
			}

			decomp_bytes += l*2+1;

			std::rotate(last_match, last_match+3, last_match+4);
			last_match[0] = r.match_stream;

			//////////////

			l = r.result.size();

			ibest = 0;
			prematch = postmatch = 0;

			const char *cur = r.result.data();

			for(i=0; i<4; ++i) {
				const char *last = last_result[i].data();
				const int lastsize = (int)last_result[i].size();
				const int maxmatch = l < lastsize ? l : lastsize;

				int tprematch = 0;
				int tpostmatch = 0;

				while(tprematch < maxmatch && last[tprematch] == cur[tprematch])
					++tprematch;

				while(tpostmatch < maxmatch && last[lastsize-tpostmatch-1] == cur[l-tpostmatch-1])
					++tpostmatch;

				if (tprematch+tpostmatch > prematch+postmatch) {
					prematch = tprematch;
					postmatch = tpostmatch;
					ibest = i;
				}
			}

			if (prematch > 7)
				prematch = 7;

			if (postmatch > 7)
				postmatch = 7;

			if (postmatch > l - prematch)
				postmatch = l - prematch;

			dst.push_back(ibest*64 + postmatch*8 + prematch);
			dst.push_back(1+l - prematch - postmatch);
			s = dst.size();
			if (prematch+postmatch < l) {
				dst.resize(s + l - prematch - postmatch);
				std::copy(r.result.begin() + prematch, r.result.begin() + l - postmatch, &dst[s]);
			}

			decomp_bytes += l+1;

			std::rotate(last_result, last_result+3, last_result+4);
			last_result[0] = r.result;
		}

		dst.push_back(0);
		dst.push_back(0);

		decomp_bytes += 2;
	}

#ifndef _M_AMD64
	static const char header[64]="[02|02] VirtualDub disasm module (IA32:P4/Athlon V1.05)\r\n\x1A";
#else
	static const char header[64]="[02|02] VirtualDub disasm module (AMD64:EM64T/Athlon64 V1.0)\r\n\x1A";
#endif

	memcpy(&dst[0], header, 64);

	packed_bytes = dst.size() - 72;
	memcpy(&dst[64], &packed_bytes, 4);

	decomp_bytes += (rulesys.size()+1)*sizeof(void *);
	memcpy(&dst[68], &decomp_bytes, 4);
}
