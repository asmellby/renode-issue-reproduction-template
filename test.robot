*** Settings ***
Resource                      ${CURDIR}/common.robot

*** Test Cases ***
Zephyr fatal exception Test Should Catch The Stack Overflow
    # Host time: an emulation that hangs never reaches the virtual-time UART timeouts
    [Timeout]                 60 seconds
    Load Reproducer
    Register Failing Uart String    PROJECT EXECUTION FAILED
    Run Reproducer
    Wait For Line On Uart     PASS - test_fatal_stack_hw_overflow    timeout=10
    Wait For Line On Uart     PROJECT EXECUTION SUCCESSFUL    timeout=10
