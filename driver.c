#include <wdm.h>

#define NT_DEVICE_NAME      L"\\Device\\MyDriver"
#define DOS_DEVICE_NAME     L"\\DosDevices\\MyDriver"

#define IOCTL_MYDRIVER_READ    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_MYDRIVER_WRITE   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_WRITE_ACCESS)

#if DBG
#define MYDRIVER_KDPRINT(_x_) \
                DbgPrint("MYDRIVER.SYS: ");\
                DbgPrint _x_;
#else
#define MYDRIVER_KDPRINT(_x_)
#endif

typedef struct _DEVICE_EXTENSION {
    LONG SharedCounter;
    KSPIN_LOCK SpinLock;
    KTIMER Timer;
    KDPC TimerDpc;
    KEVENT UnloadEvent;
    LONG DpcWorkItemCounter;
    BOOLEAN IsDeviceInitialized;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD MyDriverUnload;
_Dispatch_type_(IRP_MJ_CREATE)
_Dispatch_type_(IRP_MJ_CLOSE)
DRIVER_DISPATCH MyDriverCreateClose;
_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH MyDriverDeviceControl;
KDEFERRED_ROUTINE TimerDpcRoutine;
IO_WORKITEM_ROUTINE WorkItemRoutine;

// New function prototypes
NTSTATUS MyDriverAddDevice(_In_ PDRIVER_OBJECT DriverObject, _In_ PDEVICE_OBJECT PhysicalDeviceObject);
NTSTATUS MyDriverPnP(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
NTSTATUS MyDriverPower(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);

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

    UNREFERENCED_PARAMETER(RegistryPath);

    MYDRIVER_KDPRINT(("DriverEntry Called\n"));

    RtlInitUnicodeString(&ntUnicodeString, NT_DEVICE_NAME);

    ntStatus = IoCreateDevice(
        DriverObject,
        sizeof(DEVICE_EXTENSION),
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

    deviceExtension = (PDEVICE_EXTENSION)deviceObject->DeviceExtension;
    RtlZeroMemory(deviceExtension, sizeof(DEVICE_EXTENSION));

    KeInitializeSpinLock(&deviceExtension->SpinLock);
    KeInitializeEvent(&deviceExtension->UnloadEvent, SynchronizationEvent, FALSE);
    KeInitializeTimer(&deviceExtension->Timer);
    KeInitializeDpc(&deviceExtension->TimerDpc, TimerDpcRoutine, deviceObject);

    DriverObject->MajorFunction[IRP_MJ_CREATE] = MyDriverCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = MyDriverCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MyDriverDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_PNP] = MyDriverPnP;
    DriverObject->MajorFunction[IRP_MJ_POWER] = MyDriverPower;
    DriverObject->DriverExtension->AddDevice = MyDriverAddDevice;
    DriverObject->DriverUnload = MyDriverUnload;

    RtlInitUnicodeString(&ntWin32NameString, DOS_DEVICE_NAME);

    ntStatus = IoCreateSymbolicLink(&ntWin32NameString, &ntUnicodeString);

    if (!NT_SUCCESS(ntStatus))
    {
        MYDRIVER_KDPRINT(("Couldn't create symbolic link\n"));
        IoDeleteDevice(deviceObject);
        return ntStatus;
    }

    LARGE_INTEGER dueTime;
    dueTime.QuadPart = -10000000LL; // 1 second (negative for relative time)
    KeSetTimerEx(&deviceExtension->Timer, dueTime, 1000, &deviceExtension->TimerDpc); // 1 second periodic

    deviceExtension->IsDeviceInitialized = TRUE;

    return STATUS_SUCCESS;
}

NTSTATUS
MyDriverAddDevice(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PDEVICE_OBJECT PhysicalDeviceObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(PhysicalDeviceObject);

    // This function would be implemented for PnP drivers
    // For this example, we'll just return success
    return STATUS_SUCCESS;
}

NTSTATUS
MyDriverPnP(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    // For simplicity, we're just passing PnP IRPs down the stack
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(((PDEVICE_EXTENSION)DeviceObject->DeviceExtension)->LowerDeviceObject, Irp);
}

NTSTATUS
MyDriverPower(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    // For simplicity, we're just passing Power IRPs down the stack
    PoStartNextPowerIrp(Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    return PoCallDriver(((PDEVICE_EXTENSION)DeviceObject->DeviceExtension)->LowerDeviceObject, Irp);
}

NTSTATUS
MyDriverCreateClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    MYDRIVER_KDPRINT(("Create or Close request\n"));

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

VOID
MyDriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    PDEVICE_OBJECT deviceObject = DriverObject->DeviceObject;
    UNICODE_STRING uniWin32NameString;
    PDEVICE_EXTENSION deviceExtension;

    MYDRIVER_KDPRINT(("Unload Called\n"));

    if (deviceObject != NULL)
    {
        deviceExtension = (PDEVICE_EXTENSION)deviceObject->DeviceExtension;

        if (deviceExtension->IsDeviceInitialized)
        {
            KeCancelTimer(&deviceExtension->Timer);
            KeFlushQueuedDpcs();

            // Wait for all DPCs and work items to complete
            while (InterlockedCompareExchange(&deviceExtension->DpcWorkItemCounter, 0, 0) > 0)
            {
                MYDRIVER_KDPRINT(("Waiting for DPCs and work items to finish...\n"));
                KeWaitForSingleObject(&deviceExtension->UnloadEvent, Executive, KernelMode, FALSE, NULL);
            }

            RtlInitUnicodeString(&uniWin32NameString, DOS_DEVICE_NAME);
            IoDeleteSymbolicLink(&uniWin32NameString);
        }

        IoDeleteDevice(deviceObject);
    }
}

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
    PDEVICE_EXTENSION deviceExtension;

    PAGED_CODE();

    irpSp = IoGetCurrentIrpStackLocation(Irp);
    inBufLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    outBufLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    inBuf = Irp->AssociatedIrp.SystemBuffer;
    outBuf = Irp->AssociatedIrp.SystemBuffer;

    deviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    MYDRIVER_KDPRINT(("Device Control Request\n"));

    Irp->IoStatus.Information = 0;

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
            KeAcquireInStackQueuedSpinLock(&deviceExtension->SpinLock, &lockHandle);
            *(PLONG)outBuf = deviceExtension->SharedCounter;
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
            KeAcquireInStackQueuedSpinLock(&deviceExtension->SpinLock, &lockHandle);
            deviceExtension->SharedCounter = *(PLONG)inBuf;
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
    PDEVICE_EXTENSION deviceExtension;

    // Validate DeferredContext
    if (deviceObject == NULL) {
        MYDRIVER_KDPRINT(("ERROR: Invalid device object in TimerDpcRoutine\n"));
        return;
    }

    deviceExtension = (PDEVICE_EXTENSION)deviceObject->DeviceExtension;

    MYDRIVER_KDPRINT(("Timer DPC Called\n"));

    InterlockedIncrement(&deviceExtension->SharedCounter);
    InterlockedIncrement(&deviceExtension->DpcWorkItemCounter);

    PIO_WORKITEM workItem = IoAllocateWorkItem(deviceObject);
    int retryCount = 0;

    // Retry allocation if it fails, up to 3 times
    while (workItem == NULL && retryCount < 3)
    {
        MYDRIVER_KDPRINT(("WARNING: Work item allocation failed, retrying...\n"));
        retryCount++;
        LARGE_INTEGER interval;
        interval.QuadPart = -1000000; // 100ms
        KeDelayExecutionThread(KernelMode, FALSE, &interval);
        workItem = IoAllocateWorkItem(deviceObject);
    }

    if (workItem != NULL)
    {
        IoQueueWorkItem(workItem, WorkItemRoutine, DelayedWorkQueue, workItem);
    }
    else
    {
        MYDRIVER_KDPRINT(("ERROR: Failed to allocate work item after retries, skipping DPC work item\n"));
        InterlockedDecrement(&deviceExtension->DpcWorkItemCounter);
        if (InterlockedCompareExchange(&deviceExtension->DpcWorkItemCounter, 0, 0) == 0)
        {
            KeSetEvent(&deviceExtension->UnloadEvent, IO_NO_INCREMENT, FALSE);
        }
    }
}

VOID
WorkItemRoutine(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_opt_ PVOID Context
)
{
    PDEVICE_EXTENSION deviceExtension;

    if (DeviceObject == NULL)
    {
        MYDRIVER_KDPRINT(("ERROR: Invalid DeviceObject in WorkItemRoutine\n"));
        return;
    }

    deviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    MYDRIVER_KDPRINT(("Work Item Routine Called\n"));

    LONG currentValue = InterlockedAdd(&deviceExtension->SharedCounter, 0);
    MYDRIVER_KDPRINT(("Current counter value: %d\n", currentValue));

    if (Context != NULL)
    {
        IoFreeWorkItem((PIO_WORKITEM)Context);
    }
    else
    {
        MYDRIVER_KDPRINT(("ERROR: Invalid Context in WorkItemRoutine\n"));
    }

    InterlockedDecrement(&deviceExtension->DpcWorkItemCounter);
    if (InterlockedCompareExchange(&deviceExtension->DpcWorkItemCounter, 0, 0) == 0)
    {
        KeSetEvent(&deviceExtension->UnloadEvent, IO_NO_INCREMENT, FALSE);
    }
}
