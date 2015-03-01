#ifndef f_VD2_AMI_UTILS_H
#define f_VD2_AMI_UTILS_H

#include <string>

std::string ANSIify(const std::wstring& unicode);
void warning(const char *format, ...);
void __declspec(noreturn) fatal(const char *format, ...);
void __declspec(noreturn) fatal_internal(const char *fname, const int line);

std::basic_string<unsigned char> ConvertToSCSU(const std::wstring& s);

#endif
