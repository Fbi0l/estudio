#pragma once
// Abre o instalador baixado pela atualização (só Windows). Fica fora do Main.cpp para não
// misturar windows.h com o JUCE. Tenta algumas vezes porque o antivírus costuma travar o
// arquivo recém-baixado por alguns instantes; devolve o código de erro do Windows se falhar.
bool lancarInstalador (const wchar_t* caminho, const wchar_t* parametros, unsigned long& erro);

// Para a atualização sem instalador: o app novo espera o antigo fechar antes de abrir a janela.
unsigned long idDoProcesso();
void esperarProcesso (unsigned long pid, int milissegundos);
