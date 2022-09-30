#include <iostream>
#include <Windows.h>
#include <strsafe.h>

struct Error
{
    DWORD error;
    Error(DWORD error) : error(error) {}
};
std::wostream& operator<<(std::wostream& os, const Error& error)
{
    LPVOID lpMsgBuf;
    DWORD dw = error.error;

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    os << "Error " << dw << ": " << (LPCTSTR)lpMsgBuf;
    LocalFree(lpMsgBuf);
    return os;
}

int main()
{
    std::cout << "Hello World!" << std::endl;

    auto file = CreateFile(L"\\??\\ROOT#SAMPLE#0000#{c9b7d8ce-7a5f-4165-b0f9-ee1a683cfbd8}", GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);

    if (file == INVALID_HANDLE_VALUE)
    {
        std::wcout << "Failed to open AudioRouter device: " << Error(GetLastError()) << std::endl;
        system("pause");
        return EXIT_FAILURE;
    }

    std::cout << "Successfully opened AudioRouter device" << std::endl;

    const char* message = "Hello World!";
    DWORD bytesWritten = 0;
    if (!WriteFile(file, message, strlen(message), &bytesWritten, NULL))
    {
        std::wcout << "Failed to write to AudioRouter device: " << Error(GetLastError()) << std::endl;
        system("pause");
        return EXIT_FAILURE;
    }

    std::cout << "Successfully wrote " << bytesWritten << " bytes to AudioRouter device" << std::endl;

    system("pause");

    return EXIT_SUCCESS;
}
