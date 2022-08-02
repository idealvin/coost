
#ifndef IATHOOK_H
#define IATHOOK_H
#ifdef _WIN32
#include <Windows.h>
#ifdef _MSC_VER
#pragma comment(lib, "Shlwapi.lib")
#endif
class IAT
{
public:
	static LPVOID Hook(LPCSTR lpModuleName, LPCSTR lpFunctionName, const LPVOID lpFunction, LPCSTR lpTargetModuleName);
	static LPVOID Hook(LPCSTR lpModuleName, LPCSTR lpFunctionName, const LPVOID lpFunction);
private:
	static LPVOID GetCurrentProcessModule();
};

#endif
#endif