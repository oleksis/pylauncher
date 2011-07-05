// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

extern "C" {

__declspec(dllexport) UINT __stdcall
associate_python(MSIHANDLE hModule)
{
    FILE * fp;
    errno_t rc = fopen_s(&fp, "c:\\temp\\setup.log", "a");

    if (rc == 0) {
        fprintf(fp, "associate_python called\n");
        fclose(fp);
    }
    return ERROR_SUCCESS;
}

__declspec(dllexport) UINT __stdcall
collect_pythons(MSIHANDLE hModule)
{
    FILE * fp;
    errno_t rc = fopen_s(&fp, "c:\\temp\\setup.log", "a");

    if (rc == 0) {
        fprintf(fp, "collect_pythons called\n");
        fclose(fp);
    }
    return ERROR_SUCCESS;
}

}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
/*
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
 */
    return TRUE;
}

