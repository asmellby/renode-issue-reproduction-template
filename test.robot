*** Settings ***
Resource                      ${CURDIR}/common.robot

*** Test Cases ***
CCR Writes Should Keep The Fields Of The Other Security State
    # Host time: an emulation that hangs never reaches the virtual-time UART timeouts
    [Timeout]                 60 seconds
    Load Reproducer           ${CURDIR}/nucleo_h533re_tz.repl
    # Reads of PROBE_ADDRESS end with a bus error, which the CPU reports as a precise BusFault
    Execute Command           sysbus Tag <0x60000000 0x100> "probe" 0 false false false false true
    Run Reproducer
    Wait For Line On Uart     PASS: the BusFault at negative priority was ignored
    Wait For Line On Uart     PASS: writing CCR_NS kept the Secure CCR
    Wait For Line On Uart     REPRODUCER DONE
