*** Settings ***
Resource                      ${CURDIR}/common.robot
Suite Setup                   Setup
Suite Teardown                Teardown
Test Teardown                 Test Teardown

*** Keywords ***
Load With IDAU Regions
    Execute Script            ${SCRIPT}
    Execute Command           cpu IDAUEnabled true
    # Region 0: 0x20008000-0x20008FFF, enabled, Non-secure callable
    Execute Command           cpu SetIDAURegion 0 0x20008000 0x20008FFF true true

Load With Custom IDAU
    Execute Command           include @${CURDIR}/AddressBitIdau.cs
    Execute Script            ${SCRIPT}
    # A fixed-function IDAU model: 0x20008000-0x20008FFF is Non-secure callable
    Execute Command           machine LoadPlatformDescriptionFromString "idau: Miscellaneous.AddressBitIdau @ none { cpu: cpu; nscStart: 0x20008000; nscEnd: 0x20008FFF }"
    Execute Command           cpu IDAUEnabled true

Load With Exemption
    Execute Script            ${SCRIPT}
    Execute Command           cpu TryAddImplementationDefinedExemptionRegion 0x20009000 0x20009FFF

Run And Check
    [Arguments]               ${when}
    Execute Command           emulation RunFor "0.01"
    Firmware Should Have Finished
    Result Should Be          2    1             TT_RESP.S for the IDAU NSC region ${when}

Run And Check Exemption
    [Arguments]               ${when}
    Execute Command           emulation RunFor "0.01"
    Firmware Should Have Finished
    Result Should Be          3    1             TT_RESP.S for the exempt region ${when}

*** Test Cases ***
IDAU Regions Should Attribute The Region After A CPU Reset
    Load With IDAU Regions
    Run And Check             before the reset
    Execute Command           cpu Reset
    Run And Check             after cpu Reset

IDAU Regions Should Attribute The Region After A System Reset Request
    Load With IDAU Regions
    Run And Check             before the reset
    # AIRCR.SYSRESETREQ, as firmware requests a reset
    Execute Command           sysbus WriteDoubleWord 0xE000ED0C 0x05FA0004
    Run And Check             after SYSRESETREQ

Custom IDAU Should Attribute The Region After A CPU Reset
    Load With Custom IDAU
    Run And Check             before the reset
    Execute Command           cpu Reset
    Run And Check             after cpu Reset

Custom IDAU Should Attribute The Region After A System Reset Request
    Load With Custom IDAU
    Run And Check             before the reset
    Execute Command           sysbus WriteDoubleWord 0xE000ED0C 0x05FA0004
    Run And Check             after SYSRESETREQ

Attribution Exemption Should Apply After A CPU Reset
    Load With Exemption
    Run And Check Exemption   before the reset
    Execute Command           cpu Reset
    Run And Check Exemption   after cpu Reset

Attribution Exemption Should Apply After A System Reset Request
    Load With Exemption
    Run And Check Exemption   before the reset
    Execute Command           sysbus WriteDoubleWord 0xE000ED0C 0x05FA0004
    Run And Check Exemption   after SYSRESETREQ
