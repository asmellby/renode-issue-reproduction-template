*** Settings ***
Resource                      ${CURDIR}/common.robot

*** Keywords ***
Should Attribute The Region Across A Reboot
    Run Reproducer
    Wait For Line On Uart     PASS: TT reports the IDAU NSC region as Secure
    Wait For Line On Uart     Rebooting
    Wait For Line On Uart     *** Booting Zephyr OS build
    Wait For Line On Uart     PASS: TT reports the IDAU NSC region as Secure

*** Test Cases ***
IDAU Regions Should Survive A Reboot
    # Host time: an emulation that hangs never reaches the virtual-time UART timeouts
    [Timeout]                 60 seconds
    Load Reproducer           ${CURDIR}/nucleo_h533re_tz.repl
    Execute Command           cpu0 IDAUEnabled true
    # Region 0: 0x2003F000-0x2003FFFF, enabled, Non-secure callable
    Execute Command           cpu0 SetIDAURegion 0 0x2003F000 0x2003FFFF true true
    Should Attribute The Region Across A Reboot

Custom IDAU Should Survive A Reboot
    # Host time: an emulation that hangs never reaches the virtual-time UART timeouts
    [Timeout]                 60 seconds
    Execute Command           include @${CURDIR}/AddressBitIdau.cs
    Load Reproducer           ${CURDIR}/nucleo_h533re_tz.repl
    # A fixed-function IDAU model: 0x2003F000-0x2003FFFF is Non-secure callable
    Execute Command           machine LoadPlatformDescriptionFromString "idau: Miscellaneous.AddressBitIdau @ none { cpu: cpu0; nscStart: 0x2003F000; nscEnd: 0x2003FFFF }"
    Execute Command           cpu0 IDAUEnabled true
    Should Attribute The Region Across A Reboot
