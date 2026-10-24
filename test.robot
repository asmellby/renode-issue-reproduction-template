*** Settings ***
Resource                      ${CURDIR}/common.robot
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Run Reproducer With Bus Errors
Test Teardown                 Test Teardown

*** Keywords ***
Run Reproducer With Bus Errors
    Execute Script            ${SCRIPT}
    # Unmapped addresses raise bus errors, as on hardware
    Execute Command           sysbus UnhandledAccessBehaviour ThrowException
    Execute Command           emulation RunFor "0.01"

*** Test Cases ***
Firmware Should Finish
    Firmware Should Have Finished

Secure CCR Write Should Keep Banked And Shared Fields
    ${before}=                Result    1
    ${expected}=              Evaluate    ${before} | 0x110
    Result Should Be          2    ${expected}    CCR after setting DIV_0_TRP and BFHFNMIGN

Non-secure CCR Write Should Keep The Secure CCR
    ${before}=                Result    3
    Result Should Be          4    ${before}    Secure CCR after writing CCR_NS

Non-secure CCR Write Of BFHFNMIGN Should Be Ignored Without BFHFNMINS
    Result Should Be          5    1             CCR.BFHFNMIGN after clearing it through CCR_NS
    Result Should Be          6    1             CCR_NS.BFHFNMIGN (read-only, not RAZ)

Non-secure CCR Write Of BFHFNMIGN Should Take Effect With BFHFNMINS
    Result Should Be          7    0             CCR.BFHFNMIGN after clearing it through CCR_NS

TRD Should Be RES0 On An Armv8.0-M Core
    Result Should Be          8    0             CCR.TRD after setting it

Bits 9 And 0 Should Be RES1
    Result Should Be          9    0x201         CCR bits [9] and [0]

BFHFNMIGN Should Ignore A BusFault At Negative Priority
    Result Should Be          10   1             Execution continued after the bus error
