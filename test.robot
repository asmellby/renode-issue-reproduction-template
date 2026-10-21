*** Settings ***
Resource                      ${CURDIR}/common.robot
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Run Reproducer
Test Teardown                 Test Teardown

*** Test Cases ***
Firmware Should Finish
    Firmware Should Have Finished

Handler Should Run With CONTROL.SPSEL Clear
    ${control}=               Result    1
    ${spsel}=                 Evaluate    (${control} >> 1) & 1
    Should Be Equal As Integers    ${spsel}    0    CONTROL.SPSEL in the SVCall handler    values=False

Exception Preempting A Handler Should Return To SP_main
    ${exc_return}=            Result    2
    ${hex}=                   Convert To Hex    ${exc_return}    prefix=0x
    ${mode_spsel}=            Evaluate    (${exc_return} >> 2) & 3
    Should Be Equal As Integers    ${mode_spsel}    0    EXC_RETURN ${hex} of PendSV: Mode and SPSEL should be 0 (Handler, SP_main)    values=False
    Result Should Be          3    1             SVCall handler resumed after PendSV
    Result Should Be          4    1             Thread resumed after SVCall

Nested Interrupts Should Run With CONTROL.SPSEL Clear
    ${outer}=                 Result    5
    ${inner}=                 Result    6
    ${spsel}=                 Evaluate    ((${outer} >> 1) & 1) | ((${inner} >> 1) & 1)
    Should Be Equal As Integers    ${spsel}    0    CONTROL.SPSEL in the IRQ handlers    values=False
    ${exc_return}=            Result    7
    ${hex}=                   Convert To Hex    ${exc_return}    prefix=0x
    ${mode_spsel}=            Evaluate    (${exc_return} >> 2) & 3
    Should Be Equal As Integers    ${mode_spsel}    0    EXC_RETURN ${hex} of the inner IRQ: Mode and SPSEL should be 0 (Handler, SP_main)    values=False
    Result Should Be          8    1             Outer IRQ handler resumed after the inner one
    Result Should Be          9    1             Thread resumed on PSP

Tail-Chained Interrupts Should Run With CONTROL.SPSEL Clear
    ${first}=                 Result    10
    ${second}=                Result    12
    ${spsel}=                 Evaluate    ((${first} >> 1) & 1) | ((${second} >> 1) & 1)
    Should Be Equal As Integers    ${spsel}    0    CONTROL.SPSEL in the IRQ handlers    values=False
    ${exc_return}=            Result    13
    ${hex}=                   Convert To Hex    ${exc_return}    prefix=0x
    ${mode_spsel}=            Evaluate    (${exc_return} >> 2) & 3
    Should Be Equal As Integers    ${mode_spsel}    3    EXC_RETURN ${hex} of the tail-chained IRQ: Mode and SPSEL should be 1 (Thread, SP_process)    values=False
    Result Should Be          14   1             Thread resumed on PSP
