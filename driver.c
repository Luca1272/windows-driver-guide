#include <wdm.h>

#define NT_DEVICE_NAME      L"\\Device\\SimpleWindowsDriver"
#define DOS_DEVICE_NAME     L"\\DosDevices\\SimpleWindowsDriver"

#define IOCTL_SWD_READ        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_SWD_WRITE       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_SWD_RESET       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_SWD_INCREMENT   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_WRITE_ACCESS)

#if DBG
#define SWD_KDPRINT(_x_) \
                DbgPrint("SimpleWindowsDriver: ");\
                DbgPrint _x_;
#else
#define SWD_KDPRINT(_x_)
#endif

#define SWD_TIMER_PERIOD_MS   1000
#define SWD_RELATIVE_SECOND   (-10000000LL)

typedef struct _DEVICE_EXTENSION {
    LONG       SharedCounter;
    KSPIN_LOCK SpinLock;

    KTIMER     Timer;
    KDPC       TimerDpc;

    // Set whenever DpcWorkItemCounter hits zero, so unload can wait for
    // queued work items to finish. Counter is touched only via Interlocked*.
    KEVENT     UnloadEvent;
    LONG       DpcWorkItemCounter;

    BOOLEAN    IsDeviceInitialized;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD SwdUnload;
_Dispatch_type_(IRP_MJ_CREATE)
_Dispatch_type_(IRP_MJ_CLOSE)
DRIVER_DISPATCH SwdCreateClose;
_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH SwdDeviceControl;
KDEFERRED_ROUTINE SwdTimerDpc;
IO_WORKITEM_ROUTINE SwdWorkItem;

static VOID
SwdSignalIfDrained(
    _In_ PDEVICE_EXTENSION DeviceExtension
)
{
    if (InterlockedCompareExchange(&DeviceExtension->DpcWorkItemCounter, 0, 0) == 0)
    {
        KeSetEvent(&DeviceExtension->UnloadEvent, IO_NO_INCREMENT, FALSE);
    }
}

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
    PDEVICE_EXTENSION deviceExtension = NULL;
    LARGE_INTEGER dueTime;

    UNREFERENCED_PARAMETER(RegistryPath);

    SWD_KDPRINT(("DriverEntry Called\n"));

    RtlInitUnicodeString(&ntUnicodeString, NT_DEVICE_NAME);

    ntStatus = IoCreateDevice(
        DriverObject,
        sizeof(DEVICE_EXTENSION),
        &ntUnicodeString,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject
    );

    if (!NT_SUCCESS(ntStatus))
    {
        SWD_KDPRINT(("Couldn't create the device object\n"));
        return ntStatus;
    }

    deviceExtension = (PDEVICE_EXTENSION)deviceObject->DeviceExtension;
    RtlZeroMemory(deviceExtension, sizeof(DEVICE_EXTENSION));

    KeInitializeSpinLock(&deviceExtension->SpinLock);
    KeInitializeEvent(&deviceExtension->UnloadEvent, SynchronizationEvent, FALSE);
    KeInitializeTimer(&deviceExtension->Timer);
    KeInitializeDpc(&deviceExtension->TimerDpc, SwdTimerDpc, deviceObject);

    DriverObject->MajorFunction[IRP_MJ_CREATE] = SwdCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = SwdCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SwdDeviceControl;
    DriverObject->DriverUnload = SwdUnload;

    RtlInitUnicodeString(&ntWin32NameString, DOS_DEVICE_NAME);

    ntStatus = IoCreateSymbolicLink(&ntWin32NameString, &ntUnicodeString);

    if (!NT_SUCCESS(ntStatus))
    {
        SWD_KDPRINT(("Couldn't create symbolic link\n"));
        IoDeleteDevice(deviceObject);
        return ntStatus;
    }

    deviceExtension->IsDeviceInitialized = TRUE;

    dueTime.QuadPart = SWD_RELATIVE_SECOND;
    KeSetTimerEx(&deviceExtension->Timer, dueTime, SWD_TIMER_PERIOD_MS, &deviceExtension->TimerDpc);

    return STATUS_SUCCESS;
}

NTSTATUS
SwdCreateClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    SWD_KDPRINT(("Create or Close request\n"));

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

VOID
SwdUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    PDEVICE_OBJECT deviceObject = DriverObject->DeviceObject;
    UNICODE_STRING uniWin32NameString;
    PDEVICE_EXTENSION deviceExtension;

    PAGED_CODE();

    SWD_KDPRINT(("Unload Called\n"));

    if (deviceObject != NULL)
    {
        deviceExtension = (PDEVICE_EXTENSION)deviceObject->DeviceExtension;

        if (deviceExtension->IsDeviceInitialized)
        {
            KeCancelTimer(&deviceExtension->Timer);
            KeFlushQueuedDpcs();

            while (InterlockedCompareExchange(&deviceExtension->DpcWorkItemCounter, 0, 0) > 0)
            {
                KeWaitForSingleObject(&deviceExtension->UnloadEvent, Executive, KernelMode, FALSE, NULL);
            }

            RtlInitUnicodeString(&uniWin32NameString, DOS_DEVICE_NAME);
            IoDeleteSymbolicLink(&uniWin32NameString);
        }

        IoDeleteDevice(deviceObject);
    }
}

NTSTATUS
SwdDeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    PIO_STACK_LOCATION irpSp;
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG inBufLength, outBufLength;
    PVOID inBuf, outBuf;
    PDEVICE_EXTENSION deviceExtension;
    KLOCK_QUEUE_HANDLE lockHandle;

    PAGED_CODE();

    irpSp = IoGetCurrentIrpStackLocation(Irp);
    inBufLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    outBufLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

    // METHOD_BUFFERED shares one system buffer for input and output.
    inBuf = Irp->AssociatedIrp.SystemBuffer;
    outBuf = Irp->AssociatedIrp.SystemBuffer;

    deviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    Irp->IoStatus.Information = 0;

    switch (irpSp->Parameters.DeviceIoControl.IoControlCode)
    {
    case IOCTL_SWD_READ:
        if (outBufLength < sizeof(LONG))
        {
            ntStatus = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        KeAcquireInStackQueuedSpinLock(&deviceExtension->SpinLock, &lockHandle);
        *(PLONG)outBuf = deviceExtension->SharedCounter;
        KeReleaseInStackQueuedSpinLock(&lockHandle);
        Irp->IoStatus.Information = sizeof(LONG);
        break;

    case IOCTL_SWD_WRITE:
        if (inBufLength < sizeof(LONG))
        {
            ntStatus = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        KeAcquireInStackQueuedSpinLock(&deviceExtension->SpinLock, &lockHandle);
        deviceExtension->SharedCounter = *(PLONG)inBuf;
        KeReleaseInStackQueuedSpinLock(&lockHandle);
        break;

    case IOCTL_SWD_RESET:
        KeAcquireInStackQueuedSpinLock(&deviceExtension->SpinLock, &lockHandle);
        deviceExtension->SharedCounter = 0;
        KeReleaseInStackQueuedSpinLock(&lockHandle);
        break;

    case IOCTL_SWD_INCREMENT:
        if (outBufLength < sizeof(LONG))
        {
            ntStatus = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        KeAcquireInStackQueuedSpinLock(&deviceExtension->SpinLock, &lockHandle);
        deviceExtension->SharedCounter += 1;
        *(PLONG)outBuf = deviceExtension->SharedCounter;
        KeReleaseInStackQueuedSpinLock(&lockHandle);
        Irp->IoStatus.Information = sizeof(LONG);
        break;

    default:
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        SWD_KDPRINT(("ERROR: Unrecognized IOCTL %x\n",
            irpSp->Parameters.DeviceIoControl.IoControlCode));
        break;
    }

    Irp->IoStatus.Status = ntStatus;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return ntStatus;
}

VOID
SwdTimerDpc(
    _In_ PKDPC Dpc,
    _In_opt_ PVOID DeferredContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
)
{
    PDEVICE_OBJECT deviceObject = (PDEVICE_OBJECT)DeferredContext;
    PDEVICE_EXTENSION deviceExtension;
    PIO_WORKITEM workItem;
    KLOCK_QUEUE_HANDLE lockHandle;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    if (deviceObject == NULL) {
        return;
    }

    deviceExtension = (PDEVICE_EXTENSION)deviceObject->DeviceExtension;

    KeAcquireInStackQueuedSpinLock(&deviceExtension->SpinLock, &lockHandle);
    deviceExtension->SharedCounter += 1;
    KeReleaseInStackQueuedSpinLock(&lockHandle);

    // Count the work item before queuing so unload can't miss it.
    InterlockedIncrement(&deviceExtension->DpcWorkItemCounter);

    workItem = IoAllocateWorkItem(deviceObject);
    if (workItem != NULL)
    {
        IoQueueWorkItem(workItem, SwdWorkItem, DelayedWorkQueue, workItem);
    }
    else
    {
        InterlockedDecrement(&deviceExtension->DpcWorkItemCounter);
        SwdSignalIfDrained(deviceExtension);
    }
}

VOID
SwdWorkItem(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_opt_ PVOID Context
)
{
    PDEVICE_EXTENSION deviceExtension;
    LONG currentValue;
    KLOCK_QUEUE_HANDLE lockHandle;

    if (DeviceObject == NULL)
    {
        return;
    }

    deviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    KeAcquireInStackQueuedSpinLock(&deviceExtension->SpinLock, &lockHandle);
    currentValue = deviceExtension->SharedCounter;
    KeReleaseInStackQueuedSpinLock(&lockHandle);

    SWD_KDPRINT(("Current counter value: %d\n", currentValue));

    if (Context != NULL)
    {
        IoFreeWorkItem((PIO_WORKITEM)Context);
    }

    InterlockedDecrement(&deviceExtension->DpcWorkItemCounter);
    SwdSignalIfDrained(deviceExtension);
}
