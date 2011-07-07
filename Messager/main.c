/*
 * Copyright (C) 2011 Vinay Sajip. All rights reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <windows.h>
#include <stdlib.h>

/* ----------------------------------------------------------------*/

typedef int (__stdcall *MSGBOXWAPI)(IN HWND hWnd, 
        IN LPCWSTR lpText, IN LPCWSTR lpCaption, 
        IN UINT uType, IN WORD wLanguageId, IN DWORD dwMilliseconds);

int MessageBoxTimeoutW(IN HWND hWnd, IN LPCWSTR lpText, 
    IN LPCWSTR lpCaption, IN UINT uType, 
    IN WORD wLanguageId, IN DWORD dwMilliseconds);

#define MB_TIMEDOUT 32000

int MessageBoxTimeoutW(HWND hWnd, LPCWSTR lpText, 
    LPCWSTR lpCaption, UINT uType, WORD wLanguageId, DWORD dwMilliseconds)
{
    static MSGBOXWAPI MsgBoxTOW = NULL;

    if (!MsgBoxTOW) {
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32)
            MsgBoxTOW = (MSGBOXWAPI)GetProcAddress(hUser32, 
                                      "MessageBoxTimeoutW");
        else {
            //stuff happened, add code to handle it here 
            //(possibly just call MessageBox())
            return 0;
        }
    }

    if (MsgBoxTOW)
        return MsgBoxTOW(hWnd, lpText, lpCaption, uType, wLanguageId,
                         dwMilliseconds);

    return 0;
}
/* ----------------------------------------------------------------*/

#define MSGSIZE 1024
#define MAX_TOKENS 10

#define ICON_INDEX      0
#define DETAIL_INDEX    1
#define CAPTION_INDEX   2
#define DELAY_INDEX     3
#define MAX_DELAY       10000
#define DEFAULT_DELAY   2000
#define QUOTE_(x)       L#x
#define QUOTE(x)        QUOTE_(x)

int WINAPI wWinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPWSTR lpCmdLine, int nShow)
{
    wchar_t line[MSGSIZE];
    wchar_t * context;
    wchar_t * tokens[MAX_TOKENS];
    int i;
    UINT icon;
    UINT delay;
    
    wcsncpy_s(line, MSGSIZE, lpCmdLine, _TRUNCATE);
    for (i = 0; i < MAX_TOKENS; i++) {
        tokens[i] = wcstok_s((i == 0) ? line : NULL, L"~", &context);
        if (tokens[i] == NULL)
            break;
    }
    if (i == DELAY_INDEX) {
        tokens[i++] = QUOTE(DEFAULT_DELAY);
    }
    if (i >= DELAY_INDEX) {
        icon = (UINT) wcstol(tokens[ICON_INDEX], &context, 16);
        if ((icon != MB_ICONINFORMATION) && (icon != MB_ICONERROR) &&
            (icon != MB_ICONWARNING) && (icon != MB_ICONQUESTION))
            icon = MB_ICONINFORMATION;
        delay = (UINT) wcstol(tokens[DELAY_INDEX], &context, 10);
        if (delay > MAX_DELAY)
            delay = DEFAULT_DELAY;
        if (delay == 0) {
            MessageBoxW(NULL, tokens[DETAIL_INDEX], tokens[CAPTION_INDEX],
                       icon);
        }
        else {
            MessageBoxTimeoutW(NULL, tokens[DETAIL_INDEX], tokens[CAPTION_INDEX],
                               icon, 0, delay);
        }
    }
    return 0;
}