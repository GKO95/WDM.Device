# WDM.Device
Generic WDM driver designed to test Windows NT kernel-mode behavior.

## Test Machine
Setup the configurations listed below to install, run, and debug the driver on a target machine.

### BCDEdit
Open an elevated Command Prompt, then enter the following commands:

```
bcdedit /debug on
bcdedit /set testsigning on
```
* [BCDEdit /debug](https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/bcdedit--debug)
* [BCDEdit /set § Verification Settings](https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/bcdedit--set#verification-settings)

For more information on why this is necessary, visit *[learn.microsoft.com](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/test-signing)* and read about test signing.

### DebugView
[DebugView](https://learn.microsoft.com/en-us/sysinternals/downloads/debugview) is a Sysinternal utility that monitors debug output. It is highly recommend to run the program with the following options:

* Run as administrator
* Disable Capture Win32 (shortcut: CTRL+W)
* Enable Capture Kernel (shortcut: CTRL+K)

### Computer Certificates
While Visual Studio can generate test certificate for signing the driver, this is not necessay and requires additional steps to import the key. However, if you would like to import the key:

1. Open Run.exe (Win+R) and enter `certlm.msc`.
1. Select *Trusted Root Certification Authorities*, then click *Import...* option from its context menu.
1. Browse the WDMDriver.cer file located with the driver package under output directory.
1. Select *Trusted Publisher* and repeat the same process.

### Device Manager
Since there is no such hardware as "Generic WDM Driver Device," simply adding drivers should not work as the system cannot find appropriate device. Instead, select *Add legacy hardware* and browse an .inf file to add both the device and the driver.
