#include "ALTUSModule.h"


void ALTUS_PrintLogo() {
	std::wstring main_logo = ASCII_ALTUS_LOGO;
	WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), main_logo.c_str(), (DWORD)main_logo.size(), nullptr, nullptr);
}

WSADATA wsa;
void ALTUS_Initialize() {
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		exit(-1);
	}
}

void ALTUS_Terminate() {
	WSACleanup();
}