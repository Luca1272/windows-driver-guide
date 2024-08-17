# MyDriver

This repository contains the source code for `MyDriver`, a Windows kernel-mode driver. This guide provides detailed instructions on how to compile, test, and run your own drivers, including information on driver signing for both test and production environments.

## Table of Contents
* Introduction
* Prerequisites
* Driver Features
* Compiling the Driver
   * Using Visual Studio
   * Without Visual Studio
* Signing the Driver
   * Test Signing
   * Production Signing
* Running the Driver
   * In Test Mode
   * In Production Mode
* Uninstalling the Driver
* Additional Resources

## Introduction

`MyDriver` is a Windows kernel-mode driver example that demonstrates several basic features commonly used in drivers. It includes device creation, IOCTL handling, timer usage, work item implementation, and basic synchronization. This driver is a simple example of how to handle IOCTL commands, manage shared data in a thread-safe manner, and perform periodic tasks within a Windows kernel-mode environment. It serves as a foundation for more complex drivers that might involve additional hardware interaction or more sophisticated data processing.

## Prerequisites

To compile and run this driver, you need the following tools and dependencies:

* Windows 10 or later operating system
* Windows Driver Kit (WDK) 10 or later
* Visual Studio 2019 or later (optional but recommended)
* Windows SDK
* Administrator privileges to install and run the driver

## Driver Features and How It Works

The driver demonstrates the following features:

1. **Device Creation**: The driver creates a device object and a symbolic link, allowing user-mode applications to interact with the driver.

2. **IOCTL Handling**: The driver processes Input/Output Control (IOCTL) requests for read and write operations on a shared counter.

3. **Shared Counter**: A global variable (`g_SharedCounter`) is used across the driver to demonstrate shared state.

4. **Synchronization**: A spin lock (`g_SpinLock`) is used to protect access to the shared counter, ensuring thread-safe operations.

5. **Timer and DPC**: The driver uses a kernel timer (`g_Timer`) and a Deferred Procedure Call (DPC) to periodically increment the shared counter.

6. **Work Item**: A work item is queued from the DPC routine to demonstrate deferred processing at a lower IRQL.

Here's a brief explanation of how the driver works:

1. In `DriverEntry`, the driver initializes its components, creates the device, and starts the timer.
2. The timer periodically triggers the DPC routine (`TimerDpcRoutine`).
3. The DPC routine increments the shared counter and queues a work item.
4. The work item (`WorkItemRoutine`) performs some processing at a lower IRQL.
5. User-mode applications can interact with the driver through IOCTLs to read or write the shared counter.
6. When the driver is unloaded, it cleans up all resources in the `MyDriverUnload` routine.

## Compiling the Driver

### Using Visual Studio

1. Install Visual Studio and the WDK.
2. Open Visual Studio and create a new project using the "Kernel Mode Driver, Empty (KMDF)" template.
3. Add the `driver.c` file to the project.
4. Configure project properties:
   - Set the target OS version and platform.
   - Configure the signing properties (see Signing the Driver section).
5. Build the solution.

### Without Visual Studio

1. Install the WDK.
2. Open the "x64 Native Tools Command Prompt for VS".
3. Navigate to the directory containing `driver.c`.
4. Compile using `msbuild`:
msbuild driver.vcxproj /p:Configuration=Release /p:Platform=x64

## Signing the Driver

Driver signing is crucial for both testing and production environments. Here's a more detailed guide:

### Test Signing

1. Generate a test certificate:
makecert -r -pe -ss PrivateCertStore -n CN=MyDriverTestCert MyDriverTestCert.cer
CopyThis creates a self-signed certificate in the PrivateCertStore.

2. Sign the driver:
signtool sign /v /s PrivateCertStore /n MyDriverTestCert /t http://timestamp.digicert.com MyDriver.sys
CopyThis signs the driver with the test certificate and adds a timestamp.

3. Enable test signing mode on your development machine:
bcdedit /set testsigning on
CopyRestart your computer for this change to take effect.

### Production Signing

1. Obtain an Extended Validation (EV) Code Signing Certificate from a trusted Certificate Authority (CA) like DigiCert, GlobalSign, or Sectigo.

2. Install the certificate on your development machine:
- Double-click the certificate file (.pfx)
- Follow the Certificate Import Wizard
- Ensure you select "Place all certificates in the following store" and choose "Personal"

3. Sign the driver using the production certificate:
signtool sign /v /fd sha256 /s MY /n "Your Company Name" /t http://timestamp.digicert.com MyDriver.sys
CopyReplace "Your Company Name" with the exact name on your certificate.

4. Submit the driver to the Windows Hardware Developer Center for WHQL certification:
- Go to the [Windows Hardware Dev Center Dashboard](https://partner.microsoft.com/en-us/dashboard/hardware/)
- Create a new submission for your driver
- Upload your signed driver and complete the submission process
- Microsoft will test your driver and, if it passes, provide you with a WHQL-signed version

## Running the Driver

### In Test Mode

1. Enable test signing mode:
bcdedit /set testsigning on
2. Restart your computer.
3. Install the driver:
sc create MyDriver type= kernel binPath= C:\path\to\MyDriver.sys
sc start MyDriver

### In Production Mode

1. Ensure you have a WHQL certified driver.
2. Install the driver using the same method as in test mode, but without enabling test signing.

## Uninstalling the Driver

1. Stop the driver:
sc stop MyDriver
2. Delete the driver service:
sc delete MyDriver
3. If in test mode, disable it:
bcdedit /set testsigning off

## Additional Resources

* [Microsoft Docs: Windows Driver Kit](https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)
* [Microsoft Docs: Driver Signing](https://docs.microsoft.com/en-us/windows-hardware/drivers/install/driver-signing)
* [Windows Hardware Dev Center Dashboard](https://partner.microsoft.com/en-us/dashboard/hardware/)

For any questions or issues, please open an issue on the GitHub repository.
