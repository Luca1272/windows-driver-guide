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

`MyDriver` is a Windows kernel-mode driver example that demonstrates several basic features commonly used in drivers. It includes device creation, IOCTL handling, timer usage, work item implementation, and basic synchronization.

## Prerequisites

To compile and run this driver, you need the following tools and dependencies:

* Windows 10 or later operating system
* Windows Driver Kit (WDK) 10 or later
* Visual Studio 2019 or later (optional but recommended)
* Windows SDK
* Administrator privileges to install and run the driver

## Driver Features

The driver demonstrates the following features:

1. Device creation and symbolic link creation
2. Handling of create/close requests
3. IOCTL processing for read and write operations
4. Use of a global variable shared across the driver
5. Basic synchronization using a spin lock
6. Timer and DPC (Deferred Procedure Call) usage
7. Work item for deferred processing

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

### Test Signing

1. Generate a test certificate:
makecert -r -pe -ss PrivateCertStore -n CN=MyDriverTestCert MyDriverTestCert.cer

2. Sign the driver:
signtool sign /v /s PrivateCertStore /n MyDriverTestCert /t http://timestamp.digicert.com MyDriver.sys

### Production Signing

For production signing, you need an EV Code Signing Certificate from a trusted Certificate Authority.

1. Obtain an EV Code Signing Certificate from a trusted CA.
2. Install the certificate on your development machine.
3. Sign the driver using the production certificate:
signtool sign /v /fd sha256 /s MY /n "Your Company Name" /t http://timestamp.digicert.com MyDriver.sys

4. Submit the driver to the Windows Hardware Developer Center for WHQL certification.

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
This updated README includes:

A new section on Driver Features, explaining what the expanded driver demonstrates.
More detailed information on driver signing, including both test signing and production signing processes.
Instructions for running the driver in both test mode and production mode.
Additional resources for further learning about Windows driver development and signing.

The steps for making and signing a driver have been expanded to include:

Generating a test certificate for development and testing.
Signing the driver with a test certificate.
Information on obtaining an EV Code Signing Certificate for production signing.
Steps for signing a driver with a production certificate.
Mention of the WHQL certification process for production drivers.

These additions provide a more comprehensive guide to the entire process of developing, signing, and deploying a Windows kernel-mode driver, from initial development through to production release.
