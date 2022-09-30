#include <iostream>
#include <Windows.h>

struct Error
{
    DWORD error;
    Error(DWORD error) : error(error) {}
};
std::ostream& operator<<(std::ostream& os, const Error& error)
{
    return os << "Error: " << error.error;
    // LPVOID lpMsgBuf;
    // DWORD dw = error.error;
    // FormatMessage(
    //     FORMAT_MESSAGE_ALLOCATE_BUFFER |
    //     FORMAT_MESSAGE_FROM_SYSTEM |
    //     FORMAT_MESSAGE_IGNORE_INSERTS,
    //     NULL,
    //     dw,
    //     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    //     (LPTSTR) &lpMsgBuf,
    //     0, NULL );

    // auto& ret = os << "Error: " << (LPCTSTR)lpMsgBuf;
    // LocalFree(lpMsgBuf);
    // return ret;
}

int main()
{
    std::cout << "Hello World!" << std::endl;
    system("pause");

    auto file = CreateFile(L"\\??\\ROOT#SAMPLE#0000#{c9b7d8ce-7a5f-4165-b0f9-ee1a683cfbd8}", GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);

    if (file == INVALID_HANDLE_VALUE)
    {
        std::cout << "Failed to open AudioRouter device: " << Error(GetLastError()) << std::endl;
        system("pause");
        return EXIT_FAILURE;
    }

    std::cout << "Successfully opened AudioRouter device" << std::endl;

    const char* message = "Hello World!";
    DWORD bytesWritten = 0;
    if (!WriteFile(file, message, strlen(message), &bytesWritten, NULL))
    {
        std::cout << "Failed to write to AudioRouter device: " << Error(GetLastError()) << std::endl;
        system("pause");
        return EXIT_FAILURE;
    }

    std::cout << "Successfully wrote " << bytesWritten << " bytes to AudioRouter device" << std::endl;

    return EXIT_SUCCESS;
}
