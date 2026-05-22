#include "SizeFormat.h"
#include <cwchar>
namespace wit::core { std::wstring FormatSize(std::uint64_t b){ const wchar_t* u[]={L"B",L"KB",L"MB",L"GB",L"TB"}; double v=(double)b; int i=0; while(v>=1024 && i<4){v/=1024;++i;} wchar_t buf[64]; swprintf_s(buf,L"%.2f %s",v,u[i]); return buf; } }
