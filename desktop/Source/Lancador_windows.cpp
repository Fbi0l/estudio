#include "Lancador.h"
#ifdef _WIN32
#ifndef NOMINMAX
 #define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <appmodel.h>
#pragma comment(lib, "dwmapi.lib")
#include <string>

bool lancarInstalador (const wchar_t* caminho, const wchar_t* parametros, unsigned long& erro)
{
    erro = 0;
    for (int tentativa = 0; tentativa < 8; ++tentativa)
    {
        // 1) ShellExecuteEx: o mesmo caminho de um clique duplo (pede permissão se precisar)
        SHELLEXECUTEINFOW sei {};
        sei.cbSize = sizeof (sei);
        sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
        sei.lpVerb = L"open";
        sei.lpFile = caminho;
        sei.lpParameters = parametros;
        sei.nShow = SW_SHOWNORMAL;
        if (ShellExecuteExW (&sei)) return true;
        erro = GetLastError();
        if (erro == ERROR_CANCELLED) return false;   // a pessoa recusou a permissão

        // 2) CreateProcess direto, caso o shell esteja indisponível
        std::wstring linha = L"\"" + std::wstring (caminho) + L"\" " + parametros;
        STARTUPINFOW si {}; si.cb = sizeof (si);
        PROCESS_INFORMATION pi {};
        if (CreateProcessW (nullptr, &linha[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
        {
            CloseHandle (pi.hThread); CloseHandle (pi.hProcess);
            return true;
        }
        erro = GetLastError();
        Sleep (750);   // antivírus ainda verificando o arquivo: espera e tenta de novo
    }
    return false;
}

unsigned long idDoProcesso() { return GetCurrentProcessId(); }

void esperarProcesso (unsigned long pid, int milissegundos)
{
    if (HANDLE h = OpenProcess (SYNCHRONIZE, FALSE, pid))
    {
        WaitForSingleObject (h, (DWORD) milissegundos);
        CloseHandle (h);
    }
}
void barraEscura (void* janelaNativa)
{
    auto h = (HWND) janelaNativa;
    if (h == nullptr) return;
    BOOL escuro = TRUE;
    DwmSetWindowAttribute (h, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &escuro, sizeof (escuro));
    COLORREF fundo = RGB (0x0e, 0x0f, 0x0c), texto = RGB (0xf2, 0xf2, 0xee);   // só no Windows 11; no 10 fica o escuro padrão
    DwmSetWindowAttribute (h, 35 /* DWMWA_CAPTION_COLOR */, &fundo, sizeof (fundo));
    DwmSetWindowAttribute (h, 36 /* DWMWA_TEXT_COLOR */, &texto, sizeof (texto));
}
bool empacotadoNaLoja()
{
    UINT32 n = 0;
    return GetCurrentPackageFullName (&n, nullptr) != APPMODEL_ERROR_NO_PACKAGE;
}
#else
bool empacotadoNaLoja() { return false; }
void barraEscura (void*) {}
bool lancarInstalador (const wchar_t*, const wchar_t*, unsigned long& erro) { erro = 0; return false; }
unsigned long idDoProcesso() { return 0; }
void esperarProcesso (unsigned long, int) {}
#endif
