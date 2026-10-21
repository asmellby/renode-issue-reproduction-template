*** Settings ***
Resource                      ${CURDIR}/common.robot

*** Test Cases ***
Example Check Should Pass
    # Host time: an emulation that hangs never reaches the virtual-time UART timeouts
    [Timeout]                 60 seconds
    Load Reproducer
    Run Reproducer
    Wait For Line On Uart     PASS: the example check
    Wait For Line On Uart     REPRODUCER DONE
