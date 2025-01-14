#include "fastfetch.h"
#include "common/processing.h"
#include "common/io/io.h"

#include <Windows.h>

enum { FF_PIPE_BUFSIZ = 4096 };

const char* ffProcessAppendOutput(FFstrbuf* buffer, char* const argv[], bool useStdErr)
{
    int timeout = instance.config.general.processingTimeout;

    FF_AUTO_CLOSE_FD HANDLE hChildPipeRead = CreateNamedPipeW(
        L"\\\\.\\pipe\\LOCAL\\",
        PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | (timeout < 0 ? 0 : FILE_FLAG_OVERLAPPED),
        0,
        1,
        FF_PIPE_BUFSIZ,
        FF_PIPE_BUFSIZ,
        0,
        NULL
    );
    if (hChildPipeRead == INVALID_HANDLE_VALUE)
        return "CreateNamedPipeW(L\"\\\\.\\pipe\\LOCAL\\\") failed";

    HANDLE hChildPipeWrite = CreateFileW(
        L"\\\\.\\pipe\\LOCAL\\",
        GENERIC_WRITE,
        0,
        &(SECURITY_ATTRIBUTES){
            .nLength = sizeof(SECURITY_ATTRIBUTES),
            .lpSecurityDescriptor = NULL,
            .bInheritHandle = TRUE,
        },
        OPEN_EXISTING,
        0,
        NULL
    );
    if (hChildPipeWrite == INVALID_HANDLE_VALUE)
        return "CreateFileW(L\"\\\\.\\pipe\\LOCAL\\\") failed";

    PROCESS_INFORMATION piProcInfo = {0};

    BOOL success;

    {
        STARTUPINFOA siStartInfo = {
            .cb = sizeof(siStartInfo),
            .dwFlags = STARTF_USESTDHANDLES,
        };
        if (useStdErr)
            siStartInfo.hStdError = hChildPipeWrite;
        else
            siStartInfo.hStdOutput = hChildPipeWrite;

        FF_STRBUF_AUTO_DESTROY cmdline = ffStrbufCreateF("\"%s\"", argv[0]);
        for(char* const* parg = &argv[1]; *parg; ++parg)
        {
            ffStrbufAppendC(&cmdline, ' ');
            ffStrbufAppendS(&cmdline, *parg);
        }

        success = CreateProcessA(
            NULL,          // application name
            cmdline.chars, // command line
            NULL,          // process security attributes
            NULL,          // primary thread security attributes
            TRUE,          // handles are inherited
            0,             // creation flags
            NULL,          // use parent's environment
            NULL,          // use parent's current directory
            &siStartInfo,  // STARTUPINFO pointer
            &piProcInfo    // receives PROCESS_INFORMATION
        );
    }

    CloseHandle(hChildPipeWrite);
    if(!success)
        return "CreateProcessA() failed";

    FF_AUTO_CLOSE_FD HANDLE hProcess = piProcInfo.hProcess;
    FF_MAYBE_UNUSED FF_AUTO_CLOSE_FD HANDLE hThread = piProcInfo.hThread;

    char str[FF_PIPE_BUFSIZ];
    DWORD nRead = 0;
    FF_AUTO_CLOSE_FD HANDLE hEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    OVERLAPPED overlapped = { .hEvent = hEvent };
    // ReadFile always completes synchronously if the pipe is not created with FILE_FLAG_OVERLAPPED
    do
    {
        if (!ReadFile(hChildPipeRead, str, sizeof(str), &nRead, &overlapped))
        {
            switch (GetLastError())
            {
            case ERROR_IO_PENDING:
                if (!timeout || WaitForSingleObject(hEvent, (DWORD) timeout) != WAIT_OBJECT_0)
                {
                    CancelIo(hChildPipeRead);
                    TerminateProcess(hProcess, 1);
                    return "WaitForSingleObject(hEvent) failed or timeout";
                }

                if (!GetOverlappedResult(hChildPipeRead, &overlapped, &nRead, FALSE))
                {
                    if (GetLastError() == ERROR_BROKEN_PIPE)
                        return NULL;

                    CancelIo(hChildPipeRead);
                    TerminateProcess(hProcess, 1);
                    return "GetOverlappedResult(hChildPipeRead) failed";
                }
                break;

            case ERROR_BROKEN_PIPE:
                return NULL;

            default:
                CancelIo(hChildPipeRead);
                TerminateProcess(hProcess, 1);
                return "ReadFile(hChildPipeRead) failed";
            }
        }
        ffStrbufAppendNS(buffer, nRead, str);
    } while (nRead > 0);

    return NULL;
}
