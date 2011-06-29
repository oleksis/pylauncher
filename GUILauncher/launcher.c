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
 *
 * Based on the work of:
 *
 * Mark Hammond (original author)
 * Curt Hagenlocher (job management)
 */

#include <windows.h>
#include <stdio.h>
#include <tchar.h>

#define BUFSIZE 256
#define MSGSIZE 1024

static FILE * log_fp = NULL;

/* This function is here to minimise Visual Studio
 * warnings about security implications of getenv, and to
 * treat blank values as if they are absent.
 */
static wchar_t * get_env(wchar_t * key)
{
    wchar_t * result = _wgetenv(key);

    if (result) {
        while (*result && isspace(*result))
            ++result;
        if (*result == L'\0')
            result = NULL;
    }
    return result;
}

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
    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, rc, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        message, size, NULL);
}

static int error(int rc, wchar_t * format, ... )
{
    va_list va;
    wchar_t message[MSGSIZE];

    if (rc != 0) {  /* not a Windows error */
        va_start(va, format);
        _vsnwprintf_s(message, MSGSIZE, _TRUNCATE, format, va);
    }
    else {
        winerror(GetLastError(), message, MSGSIZE);
    }

#if !defined(_WINDOWS)
    fwprintf(stderr, L"%s\n", message);
#else
    MessageBox(NULL, message, TEXT("Error"), MB_OK); 
#endif
    ExitProcess(rc);
}

#if PEP397

#if defined(_WINDOWS)

#define PYTHON_EXECUTABLE L"pythonw.exe"

#else

#define PYTHON_EXECUTABLE L"python.exe"

#endif

#else

#if defined(_WINDOWS)

#define SCRIPT_SUFFIX L"-script.pyw"

#else

#define SCRIPT_SUFFIX L"-script.py"

#endif

#endif

#if PEP397

#define RC_NO_STD_HANDLES   100
#define RC_CREATE_PROCESS   101
#define RC_BAD_VIRTUAL_PATH 102
#define RC_NO_PYTHON        103

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

static int num_installed_pythons = 0;

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

static void locate_pythons_for_key(HKEY root)
{
    HKEY core_root, ip_key;
    LSTATUS status = RegOpenKey(root, CORE_PATH, &core_root);
    wchar_t message[MSGSIZE];
    int i, n;
    DWORD type, data_size, attrs;
    INSTALLED_PYTHON * ip;
    wchar_t ip_path[IP_SIZE];
    wchar_t * check;
    wchar_t ** checkp;

    if (status != ERROR_SUCCESS)
        debug(L"locate_pythons_for_key: unable to open PythonCore key");
    else {
        ip = &installed_pythons[num_installed_pythons];
        for (i = 0; num_installed_pythons < MAX_INSTALLED_PYTHONS; i++) {
            status = RegEnumKey(core_root, i, ip->version, MAX_VERSION_SIZE);
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
                status = RegOpenKey(root, ip_path, &ip_key);
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
                    data_size = data_size / sizeof(wchar_t) - 1;    /* -1 for NUL */
                    if (ip->executable[data_size - 1] == L'\\')
                        --data_size; /* reg value ended in a backslash */
                    /* ip->executable is data_size long */
                    for (checkp = location_checks; *checkp; ++checkp) {
                        check = *checkp;
                        _snwprintf_s(&ip->executable[data_size], MAX_PATH - data_size,
                                     MAX_PATH - data_size,
                                     L"%s%s", check, PYTHON_EXECUTABLE);
                        attrs = GetFileAttributes(ip->executable);
                        if ((attrs != INVALID_FILE_ATTRIBUTES) && ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0)) {
                            ip->bits = 32;
                            if (wcschr(ip->executable, L' ') != NULL) {
                                /* has spaces, so quote */
                                n = wcslen(ip->executable);
                                memmove(&ip->executable[1], ip->executable, n);
                                ip->executable[0] = L'\"';
                                ip->executable[n + 1] = L'\"';
                                ip->executable[n + 1] = L'\0';
                            }
                            ++num_installed_pythons;
                            ++ip;
                        }
                    }
                }
            }
        }
        RegCloseKey(core_root);
    }
}

static int
compare_pythons(const void * ip1, const void * ip2)
{
    /* note reverse sorting */
    return wcscmp(((INSTALLED_PYTHON *) ip2)->version,
                  ((INSTALLED_PYTHON *) ip1)->version);
}

static void
locate_all_pythons()
{
    locate_pythons_for_key(HKEY_CURRENT_USER);
    locate_pythons_for_key(HKEY_LOCAL_MACHINE);
    qsort(installed_pythons, num_installed_pythons, sizeof(INSTALLED_PYTHON),
          compare_pythons);
}

static INSTALLED_PYTHON *
find_python_by_version(wchar_t const * wanted_ver)
{
    INSTALLED_PYTHON * result = NULL;
    INSTALLED_PYTHON * ip = installed_pythons;
    int i, n;
    int wlen = wcslen(wanted_ver);

    for (i = 0; i < num_installed_pythons; i++, ip++) {
        n = wcslen(ip->version);
        if (n > wlen)
            n = wlen;
        if (wcsncmp(ip->version, wanted_ver, n) == 0) {
            result = ip;
            break;
        }
    }
    return result;
}

static INSTALLED_PYTHON *
locate_python(wchar_t * wanted_ver)
{
    static wchar_t env_key [] = { L"PY_DEFAULT_PYTHONX" };
    static wchar_t * last_char = &env_key[sizeof(env_key) / sizeof(wchar_t) - 2];

    INSTALLED_PYTHON * result = NULL;
    int n = wcslen(wanted_ver);
    wchar_t * env_value;

    if (num_installed_pythons == 0)
        locate_all_pythons();

    if (n == 1) {   /* just major version specified */
        *last_char = *wanted_ver;
        env_value = get_env(env_key);
        if (env_value != NULL)
            wanted_ver = env_value;
    }
    if (*wanted_ver)
        result = find_python_by_version(wanted_ver);
    else {
        *last_char = L'\0'; /* look for an overall default */
        env_value = get_env(env_key);
        if (env_value)
            result = find_python_by_version(env_value);
        if (result == NULL)
            result = find_python_by_version(L"2");
        if (result == NULL)
            result = find_python_by_version(L"3");
    }
    return result;
}

/*
 * Process creation code
 */

static BOOL
safe_duplicate_handle(HANDLE in, HANDLE * pout)
{
    BOOL ok;
    HANDLE process = GetCurrentProcess();
    DWORD rc;

    *pout = NULL;
    ok = DuplicateHandle(process, in, process, pout, 0, TRUE,
                         DUPLICATE_SAME_ACCESS);
    if (!ok) {
        rc = GetLastError();
        if (rc == ERROR_INVALID_HANDLE) {
            debug(L"DuplicateHandle returned ERROR_INVALID_HANDLE");
            ok = TRUE;
        }
        else {
            debug(L"DuplicateHandle returned %d", rc);
        }
    }
    return ok;
}

static void
run_child(wchar_t * cmdline)
{
    HANDLE job;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
    DWORD rc;
    BOOL ok;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    debug(L"run_child: about to run '%s'\n", cmdline);
    job = CreateJobObject(NULL, NULL);
    ok = QueryInformationJobObject(job, JobObjectExtendedLimitInformation,
                                  &info, sizeof(info), &rc);
    if (!ok || (rc != sizeof(info)))
        error(0, L"Job information querying failed");
    info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                                             JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
    ok = SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info,
                                 sizeof(info));
    if (!ok)
        error(0, L"Job information setting failed");
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    ok = safe_duplicate_handle(GetStdHandle(STD_INPUT_HANDLE), &si.hStdInput);
    if (!ok)
        error(0, L"stdin duplication failed");
    ok = safe_duplicate_handle(GetStdHandle(STD_OUTPUT_HANDLE), &si.hStdOutput);
    if (!ok)
        error(0, L"stdout duplication failed");
    ok = safe_duplicate_handle(GetStdHandle(STD_ERROR_HANDLE), &si.hStdError);
    if (!ok)
        error(0, L"stderr duplication failed");
    si.dwFlags = STARTF_USESTDHANDLES;
    ok = CreateProcess(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    if (!ok)
        error(0, L"Unable to create process");
    AssignProcessToJobObject(job, pi.hProcess);
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    ok = GetExitCodeProcess(pi.hProcess, &rc);
    if (!ok)
        error(0, L"Failed to get exit code of process");
    debug(L"child process exit code: %d", rc);
    ExitProcess(rc);
}

static void
invoke_child(wchar_t * executable, wchar_t * suffix, wchar_t * cmdline)
{
    wchar_t * child_command;
    size_t child_command_size;
    BOOL no_suffix = (suffix == NULL) || (*suffix == L'\0');
    BOOL no_cmdline = (*cmdline == L'\0');

    if (no_suffix && no_cmdline)
        run_child(executable);
    else {
        if (no_suffix) {
            /* add 2 for space separator + terminating NUL. */
            child_command_size = wcslen(executable) + wcslen(cmdline) + 2;
        }
        else {
            /* add 3 for 2 space separators + terminating NUL. */
            child_command_size = wcslen(executable) + wcslen(suffix) + wcslen(cmdline) + 3;
        }
        child_command = calloc(child_command_size, sizeof(wchar_t));
        if (child_command == NULL)
            error(0, L"unable to allocate %d bytes for child command.", child_command_size);
        if (no_suffix)
            _snwprintf_s(child_command, child_command_size,
                         child_command_size - 1, L"%s %s",
                         executable, cmdline);
        else
            _snwprintf_s(child_command, child_command_size,
                         child_command_size - 1, L"%s %s %s",
                         executable, suffix, cmdline);
        run_child(child_command);
        free(child_command);
    }
}

static wchar_t * virtual_paths [] = {
    L"/usr/bin/env ",
    L"/usr/bin/",
    NULL
};

static BOOL
parse_shebang(wchar_t * shebang_line, int nchars, wchar_t ** command)
{
    BOOL rc = FALSE;
    wchar_t ** vpp;
    int plen;
    wchar_t * endp = shebang_line + nchars - 1;

    *command = NULL;    /* failure return */
    if ((*shebang_line++ == L'#') && (*shebang_line++ == L'!')) {
        while (*shebang_line && isspace(*shebang_line)) {
            ++shebang_line;
        }
        if (*shebang_line) {
            *command = shebang_line;
            for (vpp = virtual_paths; *vpp; ++vpp) {
                plen = wcslen(*vpp);
                if (wcsncmp(shebang_line, *vpp, plen) == 0) {
                    rc = TRUE;
                    *command += plen;
                    break;
                }
            }
            /* remove trailing whitespace */
            while ((endp > shebang_line) && isspace(*endp))
                --endp;
            if (endp > shebang_line)
                endp[1] = L'\0';
        }
    }
    return rc;
}

/* #define CP_UTF8             65001 defined in winnls.h */
#define CP_UTF16LE          1200
#define CP_UTF16BE          1201
#define CP_UTF32LE          12000
#define CP_UTF32BE          12001

typedef struct {
    int length;
    char sequence[4];
    UINT code_page;
} BOM;

static BOM BOMs[] = {
    { 3, { 0xEF, 0xBB, 0xBF }, CP_UTF8 },           /* UTF-8 - keep first */
    { 2, { 0xFF, 0xFE }, CP_UTF16LE },              /* UTF-16LE */
    { 2, { 0xFE, 0xFF }, CP_UTF16BE },              /* UTF-16BE */
    { 4, { 0xFF, 0xFE, 0x00, 0x00 }, CP_UTF32LE },  /* UTF-32LE */
    { 4, { 0x00, 0x00, 0xFE, 0xFF }, CP_UTF32BE },  /* UTF-32BE */
    { 0 }                                           /* sentinel */
};

static BOM *
find_BOM(char * buffer)
{
/*
 * Look for a BOM in the input and return a pointer to the
 * corresponding structure, or NULL if not found.
 */
    BOM * result = NULL;
    BOM *bom;

    for (bom = BOMs; bom->length; bom++) {
        if (strncmp(bom->sequence, buffer, bom->length) == 0) {
            result = bom;
            break;
        }
    }
    return result;
}

static char *
find_terminator(char * buffer, int len, BOM *bom)
{
    char * result = NULL;
    char * end = buffer + len;
    char  * p;
    char c;
    int cp;

    for (p = buffer; p < end; p++) {
        c = *p;
        if (c == '\r') {
            result = p;
            break;
        }
        if (c == '\n') {
            result = p;
            break;
        }
    }
    if (result != NULL) {
        cp = bom->code_page;

        /* adjustments to include all bytes of the char */
        /* no adjustment needed for UTF-8 or big endian */
        if (cp == CP_UTF16LE)
            ++result;
        else if (cp == CP_UTF32LE)
            result += 3;
        ++result; /* point just past terminator */
    }
    return result;
}

static void
maybe_handle_shebang(wchar_t ** argv, wchar_t * cmdline)
{
/*
 * Look for a shebang line in the first argument.  If found
 * and we spawn a child process, this never returns.  If it
 * does return then we process the args "normally".
 *
 * argv[0] might be a filename with a shebang.
 */
    FILE * fp;
    errno_t rc = _tfopen_s(&fp, *argv, L"rb");
    char buffer[BUFSIZE];
    wchar_t shebang_line[BUFSIZE + 1];
    size_t read;
    char *p;
    char * start;
    char * shebang_alias = (char *) shebang_line;
    BOM* bom;
    int i, j, nchars, header_len;
    BOOL is_virt;
    wchar_t * command;
    wchar_t * suffix;
    INSTALLED_PYTHON * ip;

    if (rc == 0) {
        read = fread(buffer, sizeof(char), BUFSIZE, fp);
        debug(L"maybe_handle_shebang: read %d bytes\n", read);
        fclose(fp);

        /* Look for BOM */
        bom = find_BOM(buffer);
        if (bom == NULL) {
            start = buffer;
            debug(L"maybe_handle_shebang: BOM not found, using UTF-8\n");
            bom = BOMs; /* points to UTF-8 entry - the default */
        }
        else {
            debug(L"maybe_handle_shebang: BOM found, code page %d\n", bom->code_page);
            start = &buffer[bom->length];
        }
        p = find_terminator(start, BUFSIZE, bom);
        /*
         * If no CR or LF was found in the heading,
         * we assume it's not a shebang file.
         */
        if (p == NULL) {
            debug(L"maybe_handle_shebang: No line terminator found");
        }
        else {
            /* found line terminator - parse shebang */
            header_len = p - start;
            switch(bom->code_page) {
            case CP_UTF8:
                nchars = MultiByteToWideChar(bom->code_page,
                                             0,
                                             start, header_len, shebang_line,
                                             BUFSIZE);
                break;
            case CP_UTF16BE:
                if (header_len % 2 != 0) {
                    debug(L"maybe_handle_shebang: UTF-16BE, but an odd number of bytes: %d", header_len);
                    nchars = 0;
                }
                else {
                    for (i = header_len; i > 0; i -= 2) {
                        shebang_alias[i - 1] = start[i - 2];
                        shebang_alias[i - 2] = start[i - 1];
                    }
                    nchars = header_len / sizeof(wchar_t);
                }
                break;
            case CP_UTF16LE:
                if ((header_len % 2) != 0) {
                    debug(L"UTF-16LE, but an odd number of bytes: %d", header_len);
                    nchars = 0;
                }
                else {
                    /* no actual conversion needed. */
                    memcpy(shebang_line, start, header_len);
                    nchars = header_len / sizeof(wchar_t);
                }
                break;
            case CP_UTF32BE:
                if (header_len % 4 != 0) {
                    debug(L"UTF-32BE, but not divisible by 4: %d", header_len);
                    nchars = 0;
                }
                else {
                    for (i = header_len, j = header_len / 2; i > 0; i -= 4, j -= 2) {
                        shebang_alias[j - 1] = start[i - 2];
                        shebang_alias[j - 2] = start[i - 1];
                    }
                    nchars = header_len / sizeof(wchar_t);
                }
                break;
            case CP_UTF32LE:
                if (header_len % 4 != 0) {
                    debug(L"UTF-32LE, but not divisible by 4: %d", header_len);
                    nchars = 0;
                }
                else {
                    for (i = header_len, j = header_len / 2; i > 0; i -= 4, j -= 2) {
                        shebang_alias[j - 1] = start[i - 3];
                        shebang_alias[j - 2] = start[i - 4];
                    }
                    nchars = header_len / sizeof(wchar_t);
                }
                break;
            }
            if (nchars > 0) {
                shebang_line[--nchars] = L'\0';
                is_virt = parse_shebang(shebang_line, nchars, &command);
                if (command != NULL) {
                    debug(L"parse_shebang: found command: %s\n", command);
                    if (!is_virt) {
                        invoke_child(command, NULL, cmdline);
                    }
                    else {
                        suffix = wcschr(command, L' ');
                        if (suffix != NULL) {
                            *suffix++ = L'\0';
                            while (*suffix && isspace(*suffix))
                                ++suffix;
                        }
                        if (wcsncmp(command, L"python", 6))
                            error(RC_BAD_VIRTUAL_PATH, L"unknown virtual path '%s'", command);
                        ip = locate_python(command + 6);
                        if (ip == NULL) {
                            error(RC_NO_PYTHON, L"Requested Python version (%s) is not installed", command + 6);
                        }
                        else {
                            invoke_child(ip->executable, suffix, cmdline);
                        }
                    }
                }
            }
        }
    }
}

static wchar_t *
skip_me(wchar_t * cmdline)
{
    BOOL quoted;
    wchar_t c;
    wchar_t * result = cmdline;

    quoted = cmdline[0] == L'\"';
    if (!quoted)
        c = L' ';
    else {
        c = L'\"';
        ++result;
    }
    result = wcschr(result, c);
    if (result == NULL) /* when, for example, just exe name on command line */
        result = L"";
    else {
        ++result; /* skip past space or closing quote */
        while (*result && isspace(*result))
            ++result;
    }
    return result;
}

static int
process(int argc, wchar_t ** argv)
{
    wchar_t * wp;
    wchar_t * command;
    wchar_t * p;
    int rc = 0;
    int plen;
    INSTALLED_PYTHON * ip;
    BOOL valid;

    wp = get_env(L"PYLAUNCH_DEBUG");
    if ((wp != NULL) && (*wp != L'\0'))
        log_fp = stderr;

    command = skip_me(GetCommandLine());
    if (argc <= 1)
        valid = FALSE;
    else {
        p = argv[1];
        plen = wcslen(p);
        if (p[0] != L'-')
            maybe_handle_shebang(&argv[1], command);
        /* No file with shebang, or an unrecognised shebang.
         * Is the first arg a special version qualifier?
         */
        if (plen == 2)
            valid = isdigit(p[1]);
        else if (plen == 4)
            valid = isdigit(p[1]) && (p[2] == L'.') && isdigit(p[3]);
        else
            valid = FALSE;
        if (valid) {
            ip = locate_python(&p[1]);
            if (ip == NULL)
                error(RC_NO_PYTHON, L"Requested Python version (%s) not installed", &p[1]);
            command += wcslen(p);
            while (*command && isspace(*command))
                ++command;
        }
    }
    if (!valid) {
        ip = locate_python(L"");
        if (ip == NULL)
            error(RC_NO_PYTHON, L"Can't find a default Python.");
    }
    invoke_child(ip->executable, NULL, command);
    return rc;
}

#else

static int process(int argc, char ** argv)
{
    wchar_t script_name[BUFSIZE];
    DWORD n;

    n = GetModuleFileName(NULL, script_name, BUFSIZE);
    if (n == 0)
        return error("Unable to get executable name", NULL);
    script_name[n] = '\0';
    _tcsncpy_s(&script_name[n], BUFSIZE - n, SCRIPT_SUFFIX, _tcslen(SCRIPT_SUFFIX));
    return 0;
}

#endif

#if defined(_WINDOWS)

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPTSTR lpstrCmd, int nShow)
{
    return process(__argc, __targv);
}

#else

int cdecl wmain(int argc, wchar_t ** argv)
{
    return process(argc, argv);
}

#endif