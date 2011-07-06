#include <windows.h>
#include <stdio.h>
#include "rc.h"

#define PYTHON_EXECUTABLE L"python.exe"

#define MSGSIZE 1024
#define MAX_VERSION_SIZE    4

typedef struct {
    wchar_t version[MAX_VERSION_SIZE]; /* m.n */
    int bits;   /* 32 or 64 */
    wchar_t executable[MAX_PATH];
} INSTALLED_PYTHON;

/*
 * To avoid messing about with heap allocations, just assume we can allocate
 * statically and never have to deal with more versions than this.
 */
#define MAX_INSTALLED_PYTHONS   100

static INSTALLED_PYTHON installed_pythons[MAX_INSTALLED_PYTHONS];

static size_t num_installed_pythons = 0;

/* to hold SOFTWARE\Python\PythonCore\X.Y\InstallPath */
#define IP_BASE_SIZE 40
#define IP_SIZE (IP_BASE_SIZE + MAX_VERSION_SIZE)
#define CORE_PATH L"SOFTWARE\\Python\\PythonCore"

static wchar_t * location_checks[] = {
    L"\\",
    L"\\PCBuild\\",
    L"\\PCBuild\\amd64\\",
    NULL
};

static FILE * log_fp = NULL;

static void
debug(wchar_t * format, ...)
{
    va_list va;

    if (log_fp != NULL) {
        va_start(va, format);
        vfwprintf_s(log_fp, format, va);
    }
}

static void winerror(int rc, wchar_t * message, int size)
{
    FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, rc, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        message, size, NULL);
}


static void locate_pythons_for_key(HKEY root, REGSAM flags)
{
    HKEY core_root, ip_key;
    LSTATUS status = RegOpenKeyExW(root, CORE_PATH, 0, flags, &core_root);
    wchar_t message[MSGSIZE];
    DWORD i;
    size_t n;
    BOOL ok;
    DWORD type, data_size, attrs;
    INSTALLED_PYTHON * ip, * pip;
    wchar_t ip_path[IP_SIZE];
    wchar_t * check;
    wchar_t ** checkp;

    if (status != ERROR_SUCCESS)
        debug(L"locate_pythons_for_key: unable to open PythonCore key\n");
    else {
        ip = &installed_pythons[num_installed_pythons];
        for (i = 0; num_installed_pythons < MAX_INSTALLED_PYTHONS; i++) {
            status = RegEnumKeyW(core_root, i, ip->version, MAX_VERSION_SIZE);
            if (status != ERROR_SUCCESS) {
                if (status != ERROR_NO_MORE_ITEMS) {
                    /* unexpected error */
                    winerror(status, message, MSGSIZE);
                    debug(L"%s\n", message);
                }
                break;
            }
            else {
                _snwprintf_s(ip_path, IP_SIZE, _TRUNCATE,
                             L"%s\\%s\\InstallPath", CORE_PATH, ip->version);
                status = RegOpenKeyExW(root, ip_path, 0, flags, &ip_key);
                if (status != ERROR_SUCCESS) {
                    winerror(status, message, MSGSIZE);
                    debug(L"%s: %s\n", message, ip_path);
                    continue;
                }
                data_size = sizeof(ip->executable) - 1;
                status = RegQueryValueEx(ip_key, NULL, NULL, &type,
                                         (LPBYTE) ip->executable, &data_size);
                RegCloseKey(ip_key);
                if (status != ERROR_SUCCESS) {
                    winerror(status, message, MSGSIZE);
                    debug(L"%s: %s\n", message, ip_path);
                    continue;
                }
                if (type == REG_SZ) {
                    data_size = data_size / sizeof(wchar_t) - 1;  /* for NUL */
                    if (ip->executable[data_size - 1] == L'\\')
                        --data_size; /* reg value ended in a backslash */
                    /* ip->executable is data_size long */
                    for (checkp = location_checks; *checkp; ++checkp) {
                        check = *checkp;
                        _snwprintf_s(&ip->executable[data_size],
                                     MAX_PATH - data_size,
                                     MAX_PATH - data_size,
                                     L"%s%s", check, PYTHON_EXECUTABLE);
                        attrs = GetFileAttributesW(ip->executable);
                        if ((attrs == INVALID_FILE_ATTRIBUTES) ||
                            (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                            debug(L"locate_pythons_for_key: %s: invalid file \
attributes: %X\n",
                                  ip->executable, attrs);
                        }
                        else {
                            /* check the executable type. */
                            ok = GetBinaryTypeW(ip->executable, &attrs);
                            if (!ok) {
                            }
                            else {
                                if (attrs == SCS_64BIT_BINARY)
                                    ip->bits = 64;
                                else if (attrs == SCS_32BIT_BINARY)
                                    ip->bits = 32;
                                else
                                    ip->bits = 0;
                                if (ip->bits == 0) {
                                    debug(L"locate_pythons_for_key: %s: \
invalid binary type: %X\n",
                                          ip->executable, attrs);
                                }
                                else {
                                    if (wcschr(ip->executable, L' ') != NULL) {
                                        /* has spaces, so quote */
                                        n = wcslen(ip->executable);
                                        memmove(&ip->executable[1],
                                                ip->executable, n);
                                        ip->executable[0] = L'\"';
                                        ip->executable[n + 1] = L'\"';
                                        ip->executable[n + 1] = L'\0';
                                    }
                                    ++num_installed_pythons;
                                    pip = ip++;
                                    if (num_installed_pythons >=
                                        MAX_INSTALLED_PYTHONS)
                                        break;
                                    /* Copy over the attributes for the next */
                                    *ip = *pip;
                                }
                            }
                        }
                    }
                }
            }
        }
        RegCloseKey(core_root);
    }
}

static int
compare_pythons(const void * p1, const void * p2)
{
    INSTALLED_PYTHON * ip1 = (INSTALLED_PYTHON *) p1;
    INSTALLED_PYTHON * ip2 = (INSTALLED_PYTHON *) p2;
    /* note reverse sorting on version */
    int result = wcscmp(ip2->version, ip1->version);

    if (result == 0)
        result = ip2->bits - ip1->bits; /* 64 before 32 */
    return result;
}

static void
locate_all_pythons()
{
#if defined(_M_X64)
    locate_pythons_for_key(HKEY_CURRENT_USER, KEY_READ | KEY_WOW64_64KEY);
    locate_pythons_for_key(HKEY_LOCAL_MACHINE, KEY_READ | KEY_WOW64_64KEY);
    locate_pythons_for_key(HKEY_CURRENT_USER, KEY_READ | KEY_WOW64_32KEY);
    locate_pythons_for_key(HKEY_LOCAL_MACHINE, KEY_READ | KEY_WOW64_32KEY);
#else
    locate_pythons_for_key(HKEY_CURRENT_USER, KEY_READ);
    locate_pythons_for_key(HKEY_LOCAL_MACHINE, KEY_READ);
#endif
    qsort(installed_pythons, num_installed_pythons, sizeof(INSTALLED_PYTHON),
          compare_pythons);
}

/* --------------------------------------------------------------------*/

BOOL CentreWindow(HWND hwnd)
{
    HWND hwndParent;
    RECT rect, rectP;
    int width, height;      
    int screenwidth, screenheight;
    int x, y;

    //make the window relative to its parent

    screenwidth  = GetSystemMetrics(SM_CXSCREEN);
    screenheight = GetSystemMetrics(SM_CYSCREEN);

    hwndParent = GetParent(hwnd);

    GetWindowRect(hwnd, &rect);
    if (hwndParent) {
        GetWindowRect(hwndParent, &rectP);
    }
    else {
        rectP.left = rectP.top = 0;
        rectP.right = screenwidth;
        rectP.bottom = screenheight;
    }

    width  = rect.right  - rect.left;
    height = rect.bottom - rect.top;

    x = ((rectP.right-rectP.left) -  width) / 2 + rectP.left;
    y = ((rectP.bottom-rectP.top) - height) / 2 + rectP.top;


    //make sure that the dialog box never moves outside of

    //the screen

    if(x < 0) x = 0;

    if(y < 0) y = 0;

    if (x + width  > screenwidth)
        x = screenwidth  - width;
    if (y + height > screenheight)
        y = screenheight - height;

    MoveWindow(hwnd, x, y, width, height, FALSE);
    return TRUE;
}

INT_PTR CALLBACK DialogProc (HWND hwnd, UINT message, WPARAM wParam, 
                             LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
/*
        try
        {
            control = new Controller (hwnd);
        }
        catch (WinException e)
        {
            MessageBox (0, e.GetMessage (), "Exception",
                        MB_ICONEXCLAMATION | MB_OK);
            PostQuitMessage(1);
        }
        catch (...)
        {
            MessageBox (0, "Unknown", "Exception",
                        MB_ICONEXCLAMATION | MB_OK);
            PostQuitMessage(2);
        }
 */
        return TRUE;
    case WM_COMMAND:
        /* control->Command(hwnd, LOWORD (wParam), HIWORD (wParam)); */
        return TRUE;
    case WM_HSCROLL:
        /* control->Scroll (hwnd, LOWORD (wParam), HIWORD (wParam)); */
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return TRUE;
    case WM_CLOSE:
        DestroyWindow (hwnd);
        return TRUE;
    }
    return FALSE;
}

int WINAPI wWinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPWSTR lpCmdLine, int nShow)
{
    MSG  msg;
    HWND hDialog = 0;
    HICON hIcon;
    int status;
    wchar_t * wp;

    wp = _wgetenv(L"PYASSOC_DEBUG");
    if ((wp != NULL) && (*wp != L'\0'))
        log_fp = stderr;

    hDialog = CreateDialog(hInstance, MAKEINTRESOURCE (DLG_MAIN), 0,
                           DialogProc);

    if (!hDialog)
    {
        wchar_t buf [100];
        swprintf_s(buf, 100, L"Error x%x", GetLastError());
        MessageBoxW(0, buf, L"CreateDialog", MB_ICONEXCLAMATION | MB_OK);
        return 1;
    }

    CentreWindow(hDialog);
    hIcon = LoadIcon( GetModuleHandle(NULL), MAKEINTRESOURCE(DLG_ICON) );
    if( hIcon )
    {
       SendMessage(hDialog, WM_SETICON, ICON_BIG, (LPARAM)hIcon );
       SendMessage(hDialog, WM_SETICON, ICON_SMALL, (LPARAM)hIcon );
       DestroyIcon(hIcon);
    }

    while ((status = GetMessage (& msg, 0, 0, 0)) != 0)
    {
        if (status == -1)
            return -1;
        if (!IsDialogMessage (hDialog, & msg))
        {
            TranslateMessage ( & msg );
            DispatchMessage ( & msg );
        }
    }

    return msg.wParam;
}
