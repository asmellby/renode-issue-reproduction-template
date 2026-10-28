*** Settings ***
Resource                      ${CURDIR}/common.robot
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Run Reproducer
Test Teardown                 Test Teardown

*** Test Cases ***
Firmware Should Finish
    [Timeout]                 60 seconds
    Firmware Should Have Finished

Rejected Write Without The MPU Should Be Dropped
    [Timeout]                 60 seconds
    Result Should Be          1    0             Exception

Rejected Write To Device-nGnRE Should Be Dropped
    [Timeout]                 60 seconds
    Result Should Be          5    0             Exception

Rejected Write To Device-nGnRnE Should Be A Precise BusFault
    [Timeout]                 60 seconds
    Result Should Be          9    5             Exception
    Result Should Be          10   0x8200        CFSR (PRECISERR, BFARVALID)
    Result Should Be          11   0x40000000    BFAR
