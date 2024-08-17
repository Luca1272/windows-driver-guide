#include <wdm.h>

#define NT_DEVICE_NAME      L"\\Device\\MyDriver"
#define DOS_DEVICE_NAME     L"\\DosDevices\\MyDriver"

#define IOCTL_MYDRIVER_READ    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_MYDRIVER_WRITE   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_WRITE_ACCESS)

// TODO: Remove or disable these in production builds
#if DBG
#define MYDRIVER_KDPRINT(_x_) \
                DbgPrint("MYDRIVER.SYS: ");\
                DbgPrint _x_;
#else
#define MYDRIVER_KDPRINT(_x_)
#endif

// Global variable
static LONG g_SharedCounter = 0;

// Spin lock for synchronization
static KSPIN_LOCK g_SpinLock;

// Timer object
static KTIMER g_Timer;
static KDPC g_TimerDpc;

// Function declarations
DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD MyDriverUnload;
_Dispatch_type_(IRP_MJ_CREATE)
_Dispatch_type_(IRP_MJ_CLOSE)
DRIVER_DISPATCH MyDriverCreateClose;
_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH MyDriverDeviceControl;
KDEFERRED_ROUTINE TimerDpcRoutine;
IO_WORKITEM_ROUTINE WorkItemRoutine;

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

    // Initialize the spin lock
    KeInitializeSpinLock(&g_SpinLock);

    // Initialize the Unicode string for the device name
    RtlInitUnicodeString(&ntUnicodeString, NT_DEVICE_NAME);

    // Create the device object
    ntStatus = IoCreateDevice(
        DriverObject,
        0,
        &ntUnicodeString,
        FILE_DEVICE_UNKNOWN,
        0,
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
        return ntStatus;
    }

    // Initialize and start the timer
    KeInitializeTimer(&g_Timer);
    KeInitializeDpc(&g_TimerDpc, TimerDpcRoutine, deviceObject);
    LARGE_INTEGER dueTime;
    dueTime.QuadPart = -10000000LL; // 1 second
    KeSetTimerEx(&g_Timer, dueTime, 1000, &g_TimerDpc); // 1 second periodic

    return STATUS_SUCCESS;
}

// Handle create and close requests
NTSTATUS
MyDriverCreateClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    // This routine should only be called at PASSIVE_LEVEL
    PAGED_CODE();

    MYDRIVER_KDPRINT(("Create or Close request\n"));

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

    MYDRIVER_KDPRINT(("Unload Called\n"));

    // Cancel the timer
    KeCancelTimer(&g_Timer);
    
    // Wait for any pending DPCs to complete
    KeFlushQueuedDpcs();

    // Delete the symbolic link
    RtlInitUnicodeString(&uniWin32NameString, DOS_DEVICE_NAME);
    IoDeleteSymbolicLink(&uniWin32NameString);

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
    ULONG inBufLength, outBufLength;
    PVOID inBuf, outBuf;

    UNREFERENCED_PARAMETER(DeviceObject);

    // This routine should only be called at PASSIVE_LEVEL
    PAGED_CODE();

    irpSp = IoGetCurrentIrpStackLocation(Irp);
    inBufLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    outBufLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    inBuf = Irp->AssociatedIrp.SystemBuffer;
    outBuf = Irp->AssociatedIrp.SystemBuffer;

    MYDRIVER_KDPRINT(("Device Control Request\n"));

    Irp->IoStatus.Information = 0;  // Initialize to 0

    switch (irpSp->Parameters.DeviceIoControl.IoControlCode)
    {
    case IOCTL_MYDRIVER_READ:
        if (outBufLength < sizeof(LONG))
        {
            ntStatus = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        {
            KLOCK_QUEUE_HANDLE lockHandle;
            KeAcquireInStackQueuedSpinLock(&g_SpinLock, &lockHandle);
            *(PLONG)outBuf = g_SharedCounter;
            KeReleaseInStackQueuedSpinLock(&lockHandle);
        }
        Irp->IoStatus.Information = sizeof(LONG);
        break;

    case IOCTL_MYDRIVER_WRITE:
        if (inBufLength < sizeof(LONG))
        {
            ntStatus = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        {
            KLOCK_QUEUE_HANDLE lockHandle;
            KeAcquireInStackQueuedSpinLock(&g_SpinLock, &lockHandle);
            g_SharedCounter = *(PLONG)inBuf;
            KeReleaseInStackQueuedSpinLock(&lockHandle);
        }
        break;

    default:
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        MYDRIVER_KDPRINT(("ERROR: Unrecognized IOCTL %x\n",
            irpSp->Parameters.DeviceIoControl.IoControlCode));
        break;
    }

    Irp->IoStatus.Status = ntStatus;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return ntStatus;
}

// Timer DPC routine
VOID
TimerDpcRoutine(
    _In_ PKDPC Dpc,
    _In_opt_ PVOID DeferredContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
)
{
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    PDEVICE_OBJECT deviceObject = (PDEVICE_OBJECT)DeferredContext;
    if (deviceObject == NULL) {
        MYDRIVER_KDPRINT(("ERROR: Invalid device object in TimerDpcRoutine\n"));
        return;
    }

    MYDRIVER_KDPRINT(("Timer DPC Called\n"));

    // Increment the shared counter
    InterlockedIncrement(&g_SharedCounter);

    // Queue a work item
    PIO_WORKITEM workItem = IoAllocateWorkItem(deviceObject);
    if (workItem != NULL)
    {
        IoQueueWorkItem(workItem, WorkItemRoutine, DelayedWorkQueue, workItem);
    }
    else
    {
        MYDRIVER_KDPRINT(("ERROR: Failed to allocate work item in TimerDpcRoutine\n"));
    }
}

// Work item routine
VOID
WorkItemRoutine(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_opt_ PVOID Context
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    MYDRIVER_KDPRINT(("Work Item Routine Called\n"));

    // Perform some deferred processing here
    // NOTE: Ensure any added processing is safe to run at DISPATCH_LEVEL or lower
    // Avoid operations that require PASSIVE_LEVEL

    // Free the work item
    if (Context != NULL)
    {
        IoFreeWorkItem((PIO_WORKITEM)Context);
    }
    else
    {
        MYDRIVER_KDPRINT(("ERROR: Invalid Context in WorkItemRoutine\n"));
    }
}
