*** Settings ***
Resource                      ${CURDIR}/common.robot
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Run Reproducer
Test Teardown                 Test Teardown

*** Test Cases ***
Firmware Should Finish
    Firmware Should Have Finished

SP Write Below PSPLIM Should Raise STKOF
    Result Should Be          1    0x00100000    CFSR after MOV SP below PSPLIM
    Result Should Be          2    1             SP left unchanged

Push Below PSPLIM Should Raise STKOF
    Result Should Be          3    0x00100000    CFSR after STMDB below PSPLIM
    Result Should Be          4    1             SP left unchanged
    Result Should Be          5    1             No write below PSPLIM

Exception Stacking Below PSPLIM Should Raise STKOF
    Result Should Be          6    0x00100000    CFSR after stacking below PSPLIM
    Result Should Be          7    1             PSP stopped at PSPLIM
    Result Should Be          12   0             PendSV not taken with the overflowing frame

STRD Below PSPLIM Should Raise STKOF
    Result Should Be          21   0x00100000    CFSR after STRD below PSPLIM
    Result Should Be          22   1             No write below PSPLIM

STKOF In An IT Block Should Keep ITSTATE
    Result Should Be          23   0x00100000    CFSR after MOV SP below PSPLIM in an IT block
    Result Should Be          24   1             Stacked xPSR has ITSTATE

NMI Stacking Below PSPLIM Should Raise STKOF With STKOFHFNMIGN
    Result Should Be          25   0x00100000    CFSR in the NMI handler
    Result Should Be          26   1             PSP stopped at PSPLIM

Unprivileged MSR PSPLIM Should Be Ignored
    Result Should Be          27   1             PSPLIM unchanged by an unprivileged write

PSPLIM Bits 2 To 0 Should Be RES0
    Result Should Be          28   1             PSPLIM reads LIMIT after writing LIMIT + 5

VPUSH Below PSPLIM Should Raise STKOF
    Result Should Be          29   0x00100000    CFSR after VPUSH below PSPLIM
    Result Should Be          30   1             SP left unchanged
    Result Should Be          31   1             No write below PSPLIM

Lazy FP Context Below PSPLIM Should Not Be Written
    Result Should Be          32   0x00100000    CFSR after stacking below PSPLIM
    Result Should Be          33   1             FPCCR.SPLIMVIOL
    Result Should Be          34   1             No lazy FP write below PSPLIM

SUBS SP Below PSPLIM Should Leave The Flags
    Result Should Be          35   0x00100000    CFSR after SUBS SP below PSPLIM
    Result Should Be          36   1             NZCV unchanged
    Result Should Be          37   1             SP left unchanged

ADDS SP Below PSPLIM Should Leave The Flags
    Result Should Be          38   0x00100000    CFSR after ADDS SP below PSPLIM
    Result Should Be          39   1             NZCV unchanged
    Result Should Be          40   1             SP left unchanged

Stacking Fault Above PSPLIM Should Still Raise STKOF
    Result Should Be          41   0x00100010    CFSR in the MemManage handler
    Result Should Be          42   1             PSP stopped at PSPLIM
    Result Should Be          43   1             STKOF UsageFault pending
    Result Should Be          44   0x00100010    CFSR in the UsageFault handler
    Result Should Be          45   1             No write below PSPLIM
    Result Should Be          52   0             PendSV not taken with the overflowing frame

Undefined Load Store With SP Writeback Should Raise UNDEFINSTR
    Result Should Be          46   0x00010000    CFSR after the UNDEFINED store
    Result Should Be          47   1             SP left unchanged

Unprivileged MRS MSP Should Read Zero
    Result Should Be          49   1             MRS executed
    Result Should Be          48   0             MSP read unprivileged
