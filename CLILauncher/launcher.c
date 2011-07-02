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
 * Mark Hammond (original author of Python version)
 * Curt Hagenlocher (job management)
 */

#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <tchar.h>

#define BUFSIZE 256
#define MSGSIZE 1024

/* Just for now - static definition */

static FILE * log_fp = NULL;

static wchar_t *
skip_whitespace(wchar_t * p)
{
    while (*p && isspace(*p))
        ++p;
    return p;
}

/*
 * This function is here to minimise Visual Studio
 * warnings about security implications of getenv, and to
 * treat blank values as if they are absent.
 */
static wchar_t * get_env(wchar_t * key)
{
    wchar_t * result = _wgetenv(key);

    if (result) {
        result = skip_whitespace(result);
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
    FormatMessageW(
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

#if defined(_WINDOWS)

#define PYTHON_EXECUTABLE L"pythonw.exe"

#else

#define PYTHON_EXECUTABLE L"python.exe"

#endif

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
        debug(L"locate_pythons_for_key: unable to open PythonCore key");
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
    locate_pythons_for_key(HKEY_CURRENT_USER, KEY_READ | KEY_WOW32_64KEY);
    locate_pythons_for_key(HKEY_LOCAL_MACHINE, KEY_READ | KEY_WOW32_64KEY);
#else
    locate_pythons_for_key(HKEY_CURRENT_USER, KEY_READ);
    locate_pythons_for_key(HKEY_LOCAL_MACHINE, KEY_READ);
#endif
    qsort(installed_pythons, num_installed_pythons, sizeof(INSTALLED_PYTHON),
          compare_pythons);
}

static INSTALLED_PYTHON *
find_python_by_version(wchar_t const * wanted_ver)
{
    INSTALLED_PYTHON * result = NULL;
    INSTALLED_PYTHON * ip = installed_pythons;
    size_t i, n;
    size_t wlen = wcslen(wanted_ver);

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


static wchar_t appdata_ini_path[MAX_PATH];
static wchar_t launcher_ini_path[MAX_PATH];

/*
 * Get a value either from the environment or a configuration file.
 * The key passed in will either be "python", "python2" or "python3".
 */
static wchar_t *
get_configured_value(wchar_t * key)
{
/*
 * Note: this static value is used to return a configrued value
 * obtained either from the environment or configuration file.
 * This should be OK since there wouldn't be any concurrent calls.
 */
    static wchar_t configured_value[MSGSIZE];
    wchar_t * result = NULL;
    DWORD size;

    /* First, search the environment. */
    _snwprintf_s(configured_value, MSGSIZE, _TRUNCATE, L"py_%s", key);
    result = get_env(configured_value);
    if (result == NULL) {
        /* Not in environment: check local configuration. */
        if (appdata_ini_path[0]) {
            size = GetPrivateProfileStringW(L"defaults", key, NULL,
                                            configured_value, MSGSIZE,
                                            appdata_ini_path);
            if (size > 0)
                result = configured_value;
            else if (launcher_ini_path[0]) {
                /* Not in environment or local: check global configuration. */
                size = GetPrivateProfileStringW(L"defaults", key, NULL,
                                                configured_value, MSGSIZE,
                                                launcher_ini_path);
                if (size > 0)
                    result = configured_value;
            }
        }
    }
    return result;
}

static INSTALLED_PYTHON *
locate_python(wchar_t * wanted_ver)
{
    static wchar_t config_key [] = { L"pythonX" };
    static wchar_t * last_char = &config_key[sizeof(config_key) /
                                             sizeof(wchar_t) - 2];
    INSTALLED_PYTHON * result = NULL;
    size_t n = wcslen(wanted_ver);
    wchar_t * configured_value;

    if (num_installed_pythons == 0)
        locate_all_pythons();

    if (n == 1) {   /* just major version specified */
        *last_char = *wanted_ver;
        configured_value = get_configured_value(config_key);
        if (configured_value != NULL)
            wanted_ver = configured_value;
    }
    if (*wanted_ver)
        result = find_python_by_version(wanted_ver);
    else {
        *last_char = L'\0'; /* look for an overall default */
        configured_value = get_configured_value(config_key);
        if (configured_value)
            result = find_python_by_version(configured_value);
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
    ok = CreateProcessW(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
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
            child_command_size = wcslen(executable) + wcslen(suffix) +
                                    wcslen(cmdline) + 3;
        }
        child_command = calloc(child_command_size, sizeof(wchar_t));
        if (child_command == NULL)
            error(0, L"unable to allocate %d bytes for child command.",
                  child_command_size);
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

static wchar_t * builtin_virtual_paths [] = {
    L"/usr/bin/env python",
    L"/usr/bin/python",
    L"/usr/local/bin/python",
    L"python",
    NULL
};

/* For now, a static array of commands. */

#define MAX_COMMANDS 100

typedef struct {
    wchar_t key[MAX_PATH];
    wchar_t value[MSGSIZE];
} COMMAND;

static COMMAND commands[MAX_COMMANDS];
static int num_commands = 0;

static COMMAND * find_command(wchar_t * name)
{
    COMMAND * result = NULL;
    COMMAND * cp = commands;
    int i;

    for (i = 0; i < num_commands; i++, cp++) {
        if (_wcsicmp(cp->key, name) == 0) {
            result = cp;
            break;
        }
    }
    return result;
}

static void
update_command(COMMAND * cp, wchar_t * name, wchar_t * cmdline)
{
    wcsncpy_s(cp->key, MAX_PATH, name, _TRUNCATE);
    wcsncpy_s(cp->value, MSGSIZE, cmdline, _TRUNCATE);
}

static void
add_command(wchar_t * name, wchar_t * cmdline)
{
    if (num_commands >= MAX_COMMANDS) {
        debug(L"can't add %s = '%s': no room\n", name, cmdline);
    }
    else {
        COMMAND * cp = &commands[num_commands++];

        update_command(cp, name, cmdline);
    }
}

static void
read_config_file(wchar_t * config_path)
{
    wchar_t keynames[MSGSIZE];
    wchar_t value[MSGSIZE];
    DWORD read;
    wchar_t * key;
    COMMAND * cp;

    read = GetPrivateProfileStringW(L"commands", NULL, NULL, keynames, MSGSIZE,
                                    config_path);
    if (read == MSGSIZE - 1) {
        debug(L"read_commands: %s: not enough space for names\n", config_path);
    }
    key = keynames;
    while (*key) {
        read = GetPrivateProfileString(L"commands", key, NULL, value, MSGSIZE,
                                       config_path);
        if (read == MSGSIZE - 1) {
            debug(L"read_commands: %s: not enough space for %s\n",
                  config_path, key);
        }
        cp = find_command(key);
        if (cp == NULL)
            add_command(key, value);
        else
            update_command(cp, key, value);
        key += wcslen(key) + 1;
    }
}

static void read_commands()
{
    if (launcher_ini_path[0])
        read_config_file(launcher_ini_path);
    if (appdata_ini_path[0])
        read_config_file(appdata_ini_path);
}

static BOOL
parse_shebang(wchar_t * shebang_line, int nchars, wchar_t ** command,
              wchar_t ** suffix)
{
    BOOL rc = FALSE;
    wchar_t ** vpp;
    size_t plen;
    wchar_t * p;
    wchar_t zapped;
    wchar_t * endp = shebang_line + nchars - 1;
    COMMAND * cp;

    *command = NULL;    /* failure return */
    *suffix = NULL;

    if ((*shebang_line++ == L'#') && (*shebang_line++ == L'!')) {
        shebang_line = skip_whitespace(shebang_line);
        if (*shebang_line) {
            *command = shebang_line;
            for (vpp = builtin_virtual_paths; *vpp; ++vpp) {
                plen = wcslen(*vpp);
                if (wcsncmp(shebang_line, *vpp, plen) == 0) {
                    rc = TRUE;
                    /* We can do this because all builtin commands contain
                     * "python".
                     */
                    *command = wcsstr(shebang_line, L"python");
                    break;
                }
            }
            if (*vpp == NULL) {
                /*
                 * Not found in builtins - look in customised commands.
                 *
                 * We can't permanently modify the shebang line in case
                 * it's not a customised command, but we can temporarily
                 * stick a NUL after the command while searching for it,
                 * then put back the char we zapped.
                 */
                p = wcspbrk(shebang_line, L" \t\r\n");
                if (p != NULL) {
                    zapped = *p;
                    *p = L'\0';
                }
                cp = find_command(shebang_line);
                if (p != NULL)
                    *p = zapped;
                if (cp != NULL) {
                    *command = cp->value;
                    if (p != NULL)
                        *suffix = skip_whitespace(p);
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

/*
 * Strictly, we don't need to handle UTF-16 anf UTF-32, since Python itself
 * doesn't. Never mind, one day it might - there's no harm leaving it in.
 */
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
    errno_t rc = _wfopen_s(&fp, *argv, L"rb");
    char buffer[BUFSIZE];
    wchar_t shebang_line[BUFSIZE + 1];
    size_t read;
    char *p;
    char * start;
    char * shebang_alias = (char *) shebang_line;
    BOM* bom;
    int i, j, nchars;
    int header_len;
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
            debug(L"maybe_handle_shebang: BOM found, code page %d\n",
                  bom->code_page);
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
            /*
             * Found line terminator - parse the shebang.
             *
             * Strictly, we don't need to handle UTF-16 anf UTF-32,
             * since Python itself doesn't.
             * Never mind, one day it might.
             */
            header_len = (int) (p - start);
            switch(bom->code_page) {
            case CP_UTF8:
                nchars = MultiByteToWideChar(bom->code_page,
                                             0,
                                             start, header_len, shebang_line,
                                             BUFSIZE);
                break;
            case CP_UTF16BE:
                if (header_len % 2 != 0) {
                    debug(L"maybe_handle_shebang: UTF-16BE, but an odd number \
of bytes: %d", header_len);
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
                    debug(L"UTF-16LE, but an odd number of bytes: %d",
                          header_len);
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
                    for (i = header_len, j = header_len / 2; i > 0; i -= 4,
                                                                    j -= 2) {
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
                    for (i = header_len, j = header_len / 2; i > 0; i -= 4,
                                                                    j -= 2) {
                        shebang_alias[j - 1] = start[i - 3];
                        shebang_alias[j - 2] = start[i - 4];
                    }
                    nchars = header_len / sizeof(wchar_t);
                }
                break;
            }
            if (nchars > 0) {
                shebang_line[--nchars] = L'\0';
                is_virt = parse_shebang(shebang_line, nchars, &command,
                                        &suffix);
                if (command != NULL) {
                    debug(L"parse_shebang: found command: %s\n", command);
                    if (!is_virt) {
                        invoke_child(command, suffix, cmdline);
                    }
                    else {
                        suffix = wcschr(command, L' ');
                        if (suffix != NULL) {
                            *suffix++ = L'\0';
                            suffix = skip_whitespace(suffix);
                        }
                        if (wcsncmp(command, L"python", 6))
                            error(RC_BAD_VIRTUAL_PATH, L"unknown virtual \
path '%s'", command);
                        ip = locate_python(command + 6);
                        if (ip == NULL) {
                            error(RC_NO_PYTHON, L"Requested Python version \
(%s) is not installed", command + 6);
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
        result = skip_whitespace(result);
    }
    return result;
}

static BOOL
validate_version(wchar_t * p)
{
    BOOL result = TRUE;

    if (!isdigit(*p))               /* expect major version */
        result = FALSE;
    else if (*++p) {                /* more to do */
        if (*p != L'.')             /* major/minor separator */
            result = FALSE;
        else {
            ++p;
            if (!isdigit(*p))       /* expect minor version */
                result = FALSE;
            else {
                ++p;
                if (*p) {           /* more to do */
                    if (*p != L'-')
                        result = FALSE;
                    else {
                        ++p;
                        if ((*p != '3') && (*++p != '2') && !*++p)
                            result = FALSE;
                    }
                }
            }
        }
    }
    return result;
}

static DWORD version_high = 0;
static DWORD version_low = 0;

static void
get_version_info(wchar_t * version_text, size_t size)
{
    WORD maj, min, rel, bld;

    if (!version_high && !version_low)
        wcsncpy_s(version_text, size, L"0.1", _TRUNCATE);   /* fallback */
    else {
        maj = HIWORD(version_high);
        min = LOWORD(version_high);
        rel = HIWORD(version_low);
        bld = LOWORD(version_low);
        _snwprintf_s(version_text, size, _TRUNCATE, L"%d.%d.%d.%d", maj,
                     min, rel, bld);
    }
}

static int
process(int argc, wchar_t ** argv)
{
    wchar_t * wp;
    wchar_t * command;
    wchar_t * p;
    int rc = 0;
    size_t plen;
    INSTALLED_PYTHON * ip;
    BOOL valid;
    DWORD size, attrs;
    HRESULT hr;
    wchar_t message[MSGSIZE];
    wchar_t version_text [MAX_PATH];
    void * version_data;
    VS_FIXEDFILEINFO * file_info;
    UINT block_size;

    wp = get_env(L"PYLAUNCH_DEBUG");
    if ((wp != NULL) && (*wp != L'\0'))
        log_fp = stderr;

    hr = SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata_ini_path);
    if (hr != S_OK) {
        debug(L"SHGetFolderPath failed: %X\n", hr);
        appdata_ini_path[0] = L'\0';
    }
    else {
        plen = wcslen(appdata_ini_path);
        p = &appdata_ini_path[plen];
        wcsncpy_s(p, MAX_PATH - plen, L"\\py.ini", _TRUNCATE);
        attrs = GetFileAttributesW(appdata_ini_path);
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            debug(L"File '%s' non-existent\n", appdata_ini_path);
            appdata_ini_path[0] = L'\0';
        }
    }
    plen = GetModuleFileNameW(NULL, launcher_ini_path, MAX_PATH);
    size = GetFileVersionInfoSizeW(launcher_ini_path, &size);
    if (size == 0) {
        winerror(GetLastError(), message, MSGSIZE);
        debug(L"GetFileVersionInfoSize failed: %s\n", message);
    }
    else {
        version_data = malloc(size);
        if (version_data) {
            valid = GetFileVersionInfoW(launcher_ini_path, 0, size,
                                        version_data);
            if (!valid)
                debug(L"GetFileVersionInfo failed: %X\n", GetLastError());
            else {
                valid = VerQueryValueW(version_data, L"\\", &file_info,
                                       &block_size);
                if (!valid)
                    debug(L"VerQueryValue failed: %X\n", GetLastError());
                else {
                    version_high = file_info->dwFileVersionMS;
                    version_low = file_info->dwFileVersionLS;
                }
            }
            free(version_data);
        }
    }
    p = wcsrchr(launcher_ini_path, L'\\');
    if (p == NULL) {
        debug(L"GetModuleFileNameW returned value has no backslash: %s\n",
              launcher_ini_path);
        launcher_ini_path[0] = L'\0';
    }
    else {
        wcsncpy_s(p, MAX_PATH - (p - launcher_ini_path), L"\\py.ini",
                  _TRUNCATE);
        attrs = GetFileAttributesW(launcher_ini_path);
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            debug(L"File '%s' non-existent\n", launcher_ini_path);
            launcher_ini_path[0] = L'\0';
        }
    }

    command = skip_me(GetCommandLineW());
    if (argc <= 1) {
        valid = FALSE;
        p = NULL;
    }
    else {
        p = argv[1];
        plen = wcslen(p);
        if (p[0] != L'-') {
            read_commands();
            maybe_handle_shebang(&argv[1], command);
        }
        /* No file with shebang, or an unrecognised shebang.
         * Is the first arg a special version qualifier?
         */
        valid = validate_version(&p[1]);
        if (valid) {
            ip = locate_python(&p[1]);
            if (ip == NULL)
                error(RC_NO_PYTHON, L"Requested Python version (%s) not \
installed", &p[1]);
            command += wcslen(p);
            command = skip_whitespace(command);
        }
    }
    if (!valid) {
        ip = locate_python(L"");
        if (ip == NULL)
            error(RC_NO_PYTHON, L"Can't find a default Python.");
        if ((argc == 2) && (!_wcsicmp(p, L"-h") || !_wcsicmp(p, L"--help"))) {
            get_version_info(version_text, MAX_PATH);
            fwprintf(stdout, L"Python Launcher for Windows Version %s\n\n",
                     version_text);
            fwprintf(stdout, L"usage: %s [ launcher-arguments ] script \
[ script-arguments ]\n\n",
                     argv[0]);
            fputws(L"Launcher arguments:\n\n", stdout);
            fputws(L"-2[.X]: launch using default or a specific Python 2.x \
version\n", stdout);
            fputws(L"-3[.X]: launch using default or a specific Python 3.x \
version\n", stdout);
            fputws(L"\nThe following help text is from Python:\n\n", stdout);
            fflush(stdout);
        }
    }
    invoke_child(ip->executable, NULL, command);
    return rc;
}

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