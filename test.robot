*** Settings ***
Resource                      ${CURDIR}/common.robot

*** Test Cases ***
Zephyr arm_thread_swap Test Should Pend An Interrupt From User Mode
    # Host time: an emulation that hangs never reaches the virtual-time UART timeouts
    [Timeout]                 60 seconds
    Load Reproducer
    Register Failing Uart String    PROJECT EXECUTION FAILED
    Run Reproducer
    Wait For Line On Uart     PASS - test_arm_syscalls    timeout=10
    Wait For Line On Uart     PROJECT EXECUTION SUCCESSFUL    timeout=10
