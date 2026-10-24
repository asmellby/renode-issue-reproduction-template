*** Settings ***
Resource                      ${CURDIR}/common.robot

*** Test Cases ***
TT Should Report The Secure IDAU Region
    # Host time: an emulation that hangs never reaches the virtual-time UART timeouts
    [Timeout]                 60 seconds
    Load Reproducer           ${CURDIR}/nucleo_h533re_tz.repl
    Execute Command           cpu0 IDAUEnabled true
    # Region 0: 0x2003F000-0x2003F7FF, RLAR = limit | Secure (bit 2) | enabled (bit 0)
    Execute Command           cpu0 SetIDAURegion 0 0x2003F000 0x2003F7E5
    # Region 1: 0x2003F800-0x2003FFFF, RLAR = limit | enabled: Non-secure
    Execute Command           cpu0 SetIDAURegion 1 0x2003F800 0x2003FFE1
    Run Reproducer
    Wait For Line On Uart     PASS: TT reports the Secure IDAU region as Secure, in IDAU region 0
    Wait For Line On Uart     PASS: TT reports the Non-secure IDAU region as Non-secure, in IDAU region 1
    Wait For Line On Uart     PASS: TT reports an address in no IDAU region as Secure, in no IDAU region
    Wait For Line On Uart     REPRODUCER DONE
