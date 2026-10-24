*** Settings ***
Resource                      ${CURDIR}/common.robot

*** Test Cases ***
Interrupt Stacking Below A User Stack Should Fault
    # Host time: an emulation that hangs never reaches the virtual-time UART timeouts
    [Timeout]                 60 seconds
    Load Reproducer
    Run Reproducer
    Wait For Line On Uart     PASS: stacking below the user stack was reported as a stack overflow
    Wait For Line On Uart     REPRODUCER DONE
