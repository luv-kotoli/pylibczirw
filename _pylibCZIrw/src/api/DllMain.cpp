// dllmain.cpp : Defines the entry point for the DLL application.

#ifdef _WIN32
#include "inc_libCzi.h"

#include <Windows.h>
using namespace libCZI;

static void libCZISetSite()
{
	// In a Windows-environment, we can safely use the JPGXR-WIC-codec - which might be
	//  faster than the embedded JPGXR-decoder that comes with libCZI (although I never
	//  benchmarked it...).
	//  This site-object must be set before any calls to libCZI are made.
	libCZI::SetSiteObject(libCZI::GetDefaultSiteObject(libCZI::SiteObjectType::WithWICDecoder));
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		libCZISetSite();
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}

	return TRUE;
}

#endif
