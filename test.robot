*** Settings ***
Resource                      ${CURDIR}/common.robot

*** Test Cases ***
BFAR Writes Should Read Back
    # Host time: an emulation that hangs never reaches the virtual-time UART timeouts
    [Timeout]                 60 seconds
    Load Reproducer
    Run Reproducer
    Wait For Line On Uart     PASS: BFAR reads back 0x12345678
    Wait For Line On Uart     PASS: BFAR reads back 0x00000000
    Wait For Line On Uart     REPRODUCER DONE
