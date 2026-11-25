#include "Lancador.h"
#ifdef _WIN32
#ifndef NOMINMAX
 #define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
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
#else
bool lancarInstalador (const wchar_t*, const wchar_t*, unsigned long& erro) { erro = 0; return false; }
#endif
