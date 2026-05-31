# SimpleWindowsDriver

A small educational Windows kernel-mode (WDM) software driver. It shows the core
pieces of a driver: creating a device, handling IOCTLs, protecting shared state
with a spin lock, and running periodic work from a timer/DPC and a work item.

Run it only on a test machine or virtual machine with test signing enabled. A bug
in kernel code can crash the whole system.

## Contents

- [What it does](#what-it-does)
- [Repository layout](#repository-layout)
- [Prerequisites](#prerequisites)
- [Building](#building)
- [Signing](#signing)
- [Running](#running)
- [Test app](#test-app)
- [Uninstalling](#uninstalling)

## What it does

The driver keeps a single shared counter:

- A kernel timer fires roughly once a second, increments the counter in a DPC, and
  queues a work item that runs at `PASSIVE_LEVEL`.
- A spin lock guards every access to the counter so the timer, work item, and
  IOCTL handlers serialize on it.
- Four IOCTLs let a user-mode app operate on the counter:
  - `IOCTL_SWD_READ` - read the value
  - `IOCTL_SWD_WRITE` - set the value
  - `IOCTL_SWD_RESET` - set it to 0
  - `IOCTL_SWD_INCREMENT` - increment and return the new value
- On unload the driver cancels the timer, flushes queued DPCs, and waits for any
  in-flight work items before deleting the device.

This is raw WDM so the primitives are visible. For new production drivers, KMDF is
the framework to learn next.

## Repository layout

| File | Purpose |
| --- | --- |
| `driver.c` | Kernel-mode driver source. |
| `test_app.c` | User-mode console app that drives the IOCTLs. |
| `SimpleWindowsDriver.inf` | Reference INF. `sc.exe` is the preferred install path. |
| `Simple Windows Driver.vcxproj` / `.sln` | Visual Studio / WDK project. |
| `build.ps1` | Build / install / start / stop / uninstall helper. |

## Prerequisites

- Windows 10 or 11 (a test machine or VM).
- Visual Studio 2022 or later with "Desktop development with C++".
- The Windows Driver Kit (WDK) matching your Visual Studio version, plus the
  Windows SDK and the WDK Visual Studio extension. See
  [Download the WDK](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk).
- Administrator privileges to install and load the driver.

Build for x64 or ARM64.

## Building

### Visual Studio

Open `Simple Windows Driver.sln`, pick a configuration (e.g. Release | x64), and
build. The output is `SimpleWindowsDriver.sys`.

### Command line

From the "x64 Native Tools Command Prompt for VS" (the project file name has
spaces, so quote it):

```cmd
msbuild "Simple Windows Driver.vcxproj" /p:Configuration=Release /p:Platform=x64
```

### Helper script

From an elevated PowerShell on the test machine:

```powershell
.\build.ps1 build
.\build.ps1 install
.\build.ps1 start
.\build.ps1 stop
.\build.ps1 uninstall
```

## Signing

Windows will not load an unsigned kernel driver. For development, use a
self-signed certificate plus test signing mode.

1. Create a code-signing certificate:

   ```powershell
   $cert = New-SelfSignedCertificate `
       -Subject "CN=SimpleWindowsDriver Test Cert" `
       -Type CodeSigningCert `
       -KeyUsage DigitalSignature `
       -CertStoreLocation "Cert:\CurrentUser\My" `
       -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3")
   ```

   Import it into Trusted Root Certification Authorities and Trusted Publishers
   on the test machine (e.g. via `certmgr.msc`).

2. Sign the driver:

   ```cmd
   signtool sign /v /fd sha256 ^
       /s My /n "SimpleWindowsDriver Test Cert" ^
       /tr http://timestamp.digicert.com /td sha256 ^
       x64\Release\SimpleWindowsDriver\SimpleWindowsDriver.sys
   ```

3. Enable test signing and reboot:

   ```cmd
   bcdedit /set testsigning on
   ```

For distribution, a kernel driver must be signed by Microsoft through
[Partner Center](https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/),
which requires a registered hardware developer account and an EV code-signing
certificate.

## Running

On the test machine, from an elevated command prompt (the spaces after `type=`
and `binPath=` are required by `sc.exe`):

```cmd
sc create SimpleWindowsDriver type= kernel binPath= C:\path\to\SimpleWindowsDriver.sys
sc start  SimpleWindowsDriver
```

Debug-build output from `SWD_KDPRINT` shows up in a kernel debugger or in DebugView
with "Capture Kernel" enabled.

## Test app

Build `test_app.c` from a Developer Command Prompt:

```cmd
cl /W4 /Fe:test_app.exe test_app.c
```

With the driver running:

```cmd
test_app.exe            :: demo sequence
test_app.exe read       :: print the counter
test_app.exe write 42   :: set the counter
test_app.exe reset      :: zero the counter
test_app.exe inc        :: increment and print the new value
```

Run `test_app.exe read` a few times to watch the timer increment the counter.

## Uninstalling

```cmd
sc stop   SimpleWindowsDriver
sc delete SimpleWindowsDriver
```

To turn off test signing afterward:

```cmd
bcdedit /set testsigning off
```
