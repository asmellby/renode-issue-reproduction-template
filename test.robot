*** Settings ***
Resource                      ${CURDIR}/common.robot
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Run Reproducer With A Debugger Translation
Test Teardown                 Test Teardown

*** Keywords ***
Run Reproducer With A Debugger Translation
    Run Reproducer
    ${waiting}=               Result    88
    IF    ${waiting} == 1
        # Translate ICSR through the CPU, as the GDB stub does, while it runs unprivileged
        Execute Command           cpu TranslateAddress 0xE000ED04 Read
        Execute Command           sysbus WriteDoubleWord 0x2000F164 1
        Execute Command           emulation RunFor "0.01"
    END

*** Test Cases ***
Firmware Should Finish
    Firmware Should Have Finished

Unprivileged STIR Write With USERSETMPEND Should Pend The Interrupt
    Result Should Be          2    0             Fault exception number
    Result Should Be          3    0             CFSR
    Result Should Be          1    1             IRQ 0 taken

Unprivileged ICSR Read Should Raise A Precise BusFault
    Result Should Be          6    5             Fault exception number (5 = BusFault, 4 = MemManage)
    Result Should Be          7    0x00008200    CFSR: PRECISERR and BFARVALID
    Result Should Be          8    0xE000ED04    BFAR

Unprivileged NVIC_IPR Byte Write Should Raise A Precise BusFault
    Result Should Be          10   5             Fault exception number (5 = BusFault, 4 = MemManage)
    Result Should Be          11   0x00008200    CFSR: PRECISERR and BFARVALID
    Result Should Be          12   0xE000E401    BFAR: the byte address
    Result Should Be          50   1             NVIC_IPR0 not updated

Unprivileged DWT Read Should Raise A Precise BusFault
    Result Should Be          14   5             Fault exception number (5 = BusFault, 4 = MemManage)
    Result Should Be          15   0x00008200    CFSR: PRECISERR and BFARVALID
    Result Should Be          16   0xE0001000    BFAR

Unprivileged STIR Write Without USERSETMPEND Should Raise A Precise BusFault
    Result Should Be          18   5             Fault exception number (5 = BusFault, 4 = MemManage)
    Result Should Be          19   0x00008200    CFSR: PRECISERR and BFARVALID
    Result Should Be          20   0xE000EF00    BFAR
    Result Should Be          17   0             IRQ 0 taken

Privileged SCS Write Under A Read-Only MPU Region Should Succeed
    Result Should Be          22   0             Fault exception number
    Result Should Be          52   1             Step finished

Privileged SCS Read Without PRIVDEFENA Should Succeed
    Result Should Be          26   0             Fault exception number
    Result Should Be          51   1             VTOR read back

Unprivileged ITM Stimulus Write Should Succeed
    Result Should Be          30   0             Fault exception number

Unprivileged Stacking Into The PPB Should Not Fault
    Result Should Be          33   1             IRQ 0 taken
    Result Should Be          54   0             CFSR after stacking into the PPB

Unprivileged SCS Write With The MPU Disabled Should Raise A Precise BusFault
    Result Should Be          38   5             Fault exception number (5 = BusFault, 4 = MemManage)
    Result Should Be          39   0x00008200    CFSR: PRECISERR and BFARVALID
    Result Should Be          40   0xE000ED20    BFAR
    Result Should Be          53   1             SHPR3 not updated

Unprivileged STIR Read With USERSETMPEND Should Read As Zero
    Result Should Be          65   0             Fault exception number
    Result Should Be          87   1             Read completed
    Result Should Be          84   0             STIR read value

Unprivileged VLSTM To The PPB Should Raise A Precise BusFault
    Result Should Be          69   5             Fault exception number (5 = BusFault, 4 = MemManage)
    Result Should Be          70   0x00008200    CFSR: PRECISERR and BFARVALID
    Result Should Be          71   0xE0001000    BFAR

Unprivileged VLLDM From The PPB Should Raise A Precise BusFault
    Result Should Be          73   5             Fault exception number (5 = BusFault, 4 = MemManage)
    Result Should Be          74   0x00008200    CFSR: PRECISERR and BFARVALID
    Result Should Be          75   0xE0001000    BFAR

Non-secure Unprivileged Read Of A Secure PPB Address Should Raise A SecureFault
    Result Should Be          77   7             Fault exception number (7 = SecureFault, 5 = BusFault)
    Result Should Be          78   0             CFSR
    Result Should Be          85   0x00000048    SFSR: AUVIOL and SFARVALID
    Result Should Be          86   0xE0004000    SFAR

Unprivileged ICSR Read After A Debugger Translation Should Raise A Precise BusFault
    Result Should Be          88   1             Thread waited for the translation
    Result Should Be          81   5             Fault exception number (5 = BusFault)
    Result Should Be          82   0x00008200    CFSR: PRECISERR and BFARVALID
    Result Should Be          83   0xE000ED04    BFAR
