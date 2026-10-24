*** Settings ***
Resource                      ${CURDIR}/common.robot
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Run Reproducer
Test Teardown                 Test Teardown

*** Test Cases ***
Firmware Should Finish
    Firmware Should Have Finished

Unprivileged Stacking Into Privileged Memory Should Raise MSTKERR
    Result Should Be          1    0x00000010    CFSR after stacking an unprivileged frame into privileged-only memory
    Result Should Be          2    0             SVCall entered with its frame in privileged-only memory

Privileged Stacking Into Privileged Memory Should Succeed
    Result Should Be          3    1             SVCall entered with the frame of a privileged thread below BOUNDARY
    Result Should Be          4    0             CFSR in the SVCall handler

Tail-Chain To Non-secure Should Push The Callee Frame To The Thread Stack
    Result Should Be          7    1             Secure SVCall entered from a Secure thread on PSP_S
    Result Should Be          8    1             Non-secure interrupt tail-chained from the SVCall handler
    Result Should Be          9    1             R4-R11 restored from the callee frame on return to the thread

Lazy FP Preservation Of An Unprivileged Thread Should Raise MLSPERR
    Result Should Be          6    1             SVCall entered with the basic frame in unprivileged memory
    Result Should Be          5    0x00000020    CFSR after lazily preserving into privileged-only memory
