#include "Win32Application.h"

#pragma warning(disable: 28251)
int WINAPI _tmain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	_tprintf(_T("Hello, World!\n"));

	return 0;
}
#pragma warning(default: 28251)
