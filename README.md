# IN DEVELOPMENT MIGHT NOT WORK AS INTENDED



# MyDriver

This repository contains the source code for `MyDriver`, a Windows kernel-mode driver. This guide provides detailed instructions on how to compile, test, and run your own drivers in test mode, both with and without Visual Studio.

## Table of Contents
- [Introduction](#introduction)
- [Prerequisites](#prerequisites)
- [Compiling the Driver](#compiling-the-driver)
  - [Using Visual Studio](#using-visual-studio)
  - [Without Visual Studio](#without-visual-studio)
- [Running the Driver in Test Mode](#running-the-driver-in-test-mode)
- [Uninstalling the Driver](#uninstalling-the-driver)
- [Licenses](#licenses)

## Introduction

`MyDriver` is a simple Windows kernel-mode driver example. The driver demonstrates basic driver operations, including creating and closing a device, handling device control requests, and unloading the driver.

## Prerequisites

To compile and run this driver, you need the following tools and dependencies:

- Windows operating system
- Windows Driver Kit (WDK)
- Visual Studio (optional but recommended for ease of use)
- Administrator privileges to install and run the driver

## Compiling the Driver

### Using Visual Studio

1. **Install Visual Studio**:
   - Download and install Visual Studio from the [official website](https://visualstudio.microsoft.com/).

2. **Install the Windows Driver Kit (WDK)**:
   - Download and install the WDK that matches your version of Visual Studio from the [official Microsoft website](https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk).

3. **Open the Driver Project**:
   - Open Visual Studio and create a new project using the "Kernel Mode Driver, Empty (KMDF)" template.
   - Add the `driver.c` file to the project.

4. **Configure Project Properties**:
   - Set the target OS version and platform.
   - Configure the signing properties to use a test certificate or disable signing for testing purposes.

5. **Build the Driver**:
   - Click on "Build" > "Build Solution" to compile the driver.

### Without Visual Studio

1. **Install the Windows Driver Kit (WDK)**:
   - Download and install the WDK from the [official Microsoft website](https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk).

2. **Open a WDK Command Prompt**:
   - Open the "x64 Native Tools Command Prompt for VS" (or the appropriate one for your architecture).

3. **Compile the Driver**:
   - Navigate to the directory containing `driver.c`.
   - Use the `msbuild` command to compile the driver:
     ```sh
     msbuild driver.vcxproj /p:Configuration=Release
     ```

## Running the Driver in Test Mode

1. **Enable Test Signing Mode**:
   - Open a Command Prompt with administrative privileges.
   - Enable test signing mode by running:
     ```sh
     bcdedit /set testsigning on
     ```
   - Restart your computer to apply the changes.

2. **Install the Driver**:
   - Use the `sc` command to create a service for your driver:
     ```sh
     sc create MyDriver type= kernel binPath= C:\path\to\MyDriver.sys
     ```
   - Start the driver service:
     ```sh
     sc start MyDriver
     ```

3. **Verify Driver Installation**:
   - Open the Device Manager and verify that the driver is loaded.

## Uninstalling the Driver

1. **Stop the Driver Service**:
   - Use the `sc` command to stop the driver service:
     ```sh
     sc stop MyDriver
     ```

2. **Delete the Driver Service**:
   - Use the `sc` command to delete the service:
     ```sh
     sc delete MyDriver
     ```
 
3. **Disable Test Signing Mode**:
   - Disable test signing mode by running:
     ```sh
     bcdedit /set testsigning off
     ```
   - Restart your computer to apply the changes.

## Additional Resources

- [Microsoft Docs: Windows Driver Kit](https://docs.microsoft.com/en-us/windows-hardware/drivers/)
- [Visual Studio Documentation](https://docs.microsoft.com/en-us/visualstudio/)

For any questions or issues, please open an issue on the GitHub repository.
