#include <wdm.h>

#define NT_DEVICE_NAME      L"\\Device\\MyDriver"
#define DOS_DEVICE_NAME     L"\\DosDevices\\MyDriver"

#if DBG
#define MYDRIVER_KDPRINT(_x_) \
                DbgPrint("MYDRIVER.SYS: ");\
                DbgPrint _x_;

#else
#define MYDRIVER_KDPRINT(_x_)
#endif

// Function declarations
DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD MyDriverUnload;
_Dispatch_type_(IRP_MJ_CREATE)
_Dispatch_type_(IRP_MJ_CLOSE)
DRIVER_DISPATCH MyDriverCreateClose;
_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH MyDriverDeviceControl;

// Entry point of the driver
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS ntStatus;
    UNICODE_STRING ntUnicodeString;
    UNICODE_STRING ntWin32NameString;
    PDEVICE_OBJECT deviceObject = NULL;

    UNREFERENCED_PARAMETER(RegistryPath);

    MYDRIVER_KDPRINT(("DriverEntry Called\n"));

    // Initialize the Unicode string for the device name
    RtlInitUnicodeString(&ntUnicodeString, NT_DEVICE_NAME);

    // Create the device object
    ntStatus = IoCreateDevice(
        DriverObject,
        0,
        &ntUnicodeString,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject
    );

    if (!NT_SUCCESS(ntStatus))
    {
        MYDRIVER_KDPRINT(("Couldn't create the device object\n"));
        return ntStatus;
    }

    // Set up the driver's dispatch entry points
    DriverObject->MajorFunction[IRP_MJ_CREATE] = MyDriverCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = MyDriverCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MyDriverDeviceControl;
    DriverObject->DriverUnload = MyDriverUnload;

    // Initialize the Unicode string for the symbolic link name
    RtlInitUnicodeString(&ntWin32NameString, DOS_DEVICE_NAME);

    // Create a symbolic link between the device name and a Win32-accessible name
    ntStatus = IoCreateSymbolicLink(&ntWin32NameString, &ntUnicodeString);

    if (!NT_SUCCESS(ntStatus))
    {
        MYDRIVER_KDPRINT(("Couldn't create symbolic link\n"));
        IoDeleteDevice(deviceObject);
    }

    return ntStatus;
}

// Handle create and close requests
NTSTATUS
MyDriverCreateClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    MYDRIVER_KDPRINT(("Create or Close request\n"));

    // Complete the IRP with success status
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

// Unload routine for the driver
VOID
MyDriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    PDEVICE_OBJECT deviceObject = DriverObject->DeviceObject;
    UNICODE_STRING uniWin32NameString;

    PAGED_CODE();

    MYDRIVER_KDPRINT(("Unload Called\n"));

    // Delete the symbolic link
    RtlInitUnicodeString(&uniWin32NameString, DOS_DEVICE_NAME);
    IoDeleteSymbolicLink(&uniWin32NameString);

    // Delete the device object
    if (deviceObject != NULL)
    {
        IoDeleteDevice(deviceObject);
    }
}

// Handle device control requests
NTSTATUS
MyDriverDeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    PIO_STACK_LOCATION irpSp;
    NTSTATUS ntStatus = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    irpSp = IoGetCurrentIrpStackLocation(Irp);

    MYDRIVER_KDPRINT(("Device Control Request\n"));

    // Handle the specific IOCTL codes here
    switch (irpSp->Parameters.DeviceIoControl.IoControlCode)
    {
    default:
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        MYDRIVER_KDPRINT(("ERROR: Unrecognized IOCTL %x\n",
            irpSp->Parameters.DeviceIoControl.IoControlCode));
        break;
    }

    // Complete the IRP with the status
    Irp->IoStatus.Status = ntStatus;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return ntStatus;
}