// User-mode test client for the SimpleWindowsDriver driver.
//
// Build from a Developer Command Prompt:
//     cl /W4 /Fe:test_app.exe test_app.c
//
// Usage:
//     test_app.exe              demo sequence
//     test_app.exe read         print the counter
//     test_app.exe write 42     set the counter
//     test_app.exe reset        zero the counter
//     test_app.exe inc          increment, print new value

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Must match driver.c.
#define IOCTL_SWD_READ        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_SWD_WRITE       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_SWD_RESET       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_SWD_INCREMENT   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_WRITE_ACCESS)

#define DEVICE_PATH  "\\\\.\\SimpleWindowsDriver"

static HANDLE OpenDriver(void)
{
    HANDLE h = CreateFileA(
        DEVICE_PATH,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (h == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        fprintf(stderr, "Failed to open %s (error %lu).\n", DEVICE_PATH, err);
        if (err == ERROR_FILE_NOT_FOUND)
        {
            fprintf(stderr, "Is the driver loaded? Try: sc start SimpleWindowsDriver\n");
        }
        else if (err == ERROR_ACCESS_DENIED)
        {
            fprintf(stderr, "Try running from an elevated prompt.\n");
        }
    }
    return h;
}

static BOOL ReadCounter(HANDLE h, LONG *value)
{
    DWORD bytes = 0;
    BOOL ok = DeviceIoControl(h, IOCTL_SWD_READ, NULL, 0, value, sizeof(*value), &bytes, NULL);
    if (!ok || bytes != sizeof(*value))
    {
        fprintf(stderr, "IOCTL_SWD_READ failed (error %lu).\n", GetLastError());
        return FALSE;
    }
    return TRUE;
}

static BOOL WriteCounter(HANDLE h, LONG value)
{
    DWORD bytes = 0;
    BOOL ok = DeviceIoControl(h, IOCTL_SWD_WRITE, &value, sizeof(value), NULL, 0, &bytes, NULL);
    if (!ok)
    {
        fprintf(stderr, "IOCTL_SWD_WRITE failed (error %lu).\n", GetLastError());
        return FALSE;
    }
    return TRUE;
}

static BOOL ResetCounter(HANDLE h)
{
    DWORD bytes = 0;
    BOOL ok = DeviceIoControl(h, IOCTL_SWD_RESET, NULL, 0, NULL, 0, &bytes, NULL);
    if (!ok)
    {
        fprintf(stderr, "IOCTL_SWD_RESET failed (error %lu).\n", GetLastError());
        return FALSE;
    }
    return TRUE;
}

static BOOL IncrementCounter(HANDLE h, LONG *newValue)
{
    DWORD bytes = 0;
    BOOL ok = DeviceIoControl(h, IOCTL_SWD_INCREMENT, NULL, 0, newValue, sizeof(*newValue), &bytes, NULL);
    if (!ok || bytes != sizeof(*newValue))
    {
        fprintf(stderr, "IOCTL_SWD_INCREMENT failed (error %lu).\n", GetLastError());
        return FALSE;
    }
    return TRUE;
}

static int RunDemo(HANDLE h)
{
    LONG value = 0;

    if (!ReadCounter(h, &value)) return 1;
    printf("Initial counter: %ld\n", value);

    if (!WriteCounter(h, 100)) return 1;
    printf("Wrote 100.\n");

    if (!ReadCounter(h, &value)) return 1;
    printf("After write: %ld\n", value);

    if (!IncrementCounter(h, &value)) return 1;
    printf("After increment: %ld\n", value);

    if (!ResetCounter(h)) return 1;
    printf("Reset.\n");

    if (!ReadCounter(h, &value)) return 1;
    printf("After reset: %ld\n", value);

    return 0;
}

int main(int argc, char **argv)
{
    HANDLE h;
    int rc = 0;

    h = OpenDriver();
    if (h == INVALID_HANDLE_VALUE)
    {
        return 1;
    }

    if (argc < 2)
    {
        rc = RunDemo(h);
    }
    else if (_stricmp(argv[1], "read") == 0)
    {
        LONG value = 0;
        rc = ReadCounter(h, &value) ? 0 : 1;
        if (rc == 0) printf("%ld\n", value);
    }
    else if (_stricmp(argv[1], "write") == 0 && argc >= 3)
    {
        LONG value = (LONG)strtol(argv[2], NULL, 10);
        rc = WriteCounter(h, value) ? 0 : 1;
        if (rc == 0) printf("Wrote %ld.\n", value);
    }
    else if (_stricmp(argv[1], "reset") == 0)
    {
        rc = ResetCounter(h) ? 0 : 1;
        if (rc == 0) printf("Reset to 0.\n");
    }
    else if (_stricmp(argv[1], "inc") == 0 || _stricmp(argv[1], "increment") == 0)
    {
        LONG value = 0;
        rc = IncrementCounter(h, &value) ? 0 : 1;
        if (rc == 0) printf("%ld\n", value);
    }
    else
    {
        fprintf(stderr, "Usage: %s [read | write <n> | reset | inc]\n", argv[0]);
        rc = 2;
    }

    CloseHandle(h);
    return rc;
}
