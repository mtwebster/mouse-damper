#ifndef GETTEXT_HELPERS_H
#define GETTEXT_HELPERS_H

#include <windows.h>
#include <libintl.h>
#include <locale.h>
#include <stdlib.h>

#define _(String) gettext(String)
#define N_(String) String

// UTF-8 to UTF-16 conversion
static inline wchar_t *utf8_to_wchar(const char *utf8_str) {
    if (!utf8_str) return NULL;

    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
    if (wlen <= 0) return NULL;

    wchar_t *wstr = (wchar_t *)malloc(wlen * sizeof(wchar_t));
    if (!wstr) return NULL;

    MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wstr, wlen);
    return wstr;
}

// Translate and convert to wchar_t
#define _W(String) utf8_to_wchar(_(String))

// Initialize gettext for Windows
static inline void init_gettext_windows(void) {
    setlocale(LC_ALL, "");

    // Determine locale dir relative to executable
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(NULL, exe_path, MAX_PATH);

    // Remove executable name (go up one level)
    wchar_t *last_sep = wcsrchr(exe_path, L'\\');
    if (last_sep) *last_sep = L'\0';

    // Go up one more level (from libexec/mousedamper to libexec)
    last_sep = wcsrchr(exe_path, L'\\');
    if (last_sep) *last_sep = L'\0';

    // Append share/locale path
    wcscat(exe_path, L"\\..\\share\\locale");

    // Convert to UTF-8 for gettext
    char locale_dir_utf8[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, exe_path, -1, locale_dir_utf8, MAX_PATH, NULL, NULL);

    bindtextdomain("mousedamper", locale_dir_utf8);
    textdomain("mousedamper");
    bind_textdomain_codeset("mousedamper", "UTF-8");
}

#endif // GETTEXT_HELPERS_H
