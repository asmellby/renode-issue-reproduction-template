*** Settings ***
Resource                      ${CURDIR}/common.robot
Suite Setup                   Setup
Suite Teardown                Teardown
Test Teardown                 Test Teardown

*** Keywords ***
Run With IDAU
    Execute Script            ${SCRIPT}
    Execute Command           cpu IDAUEnabled true
    # Region 0: 0x20008000-0x200087FF, RLAR = limit | Secure (bit 2) | enabled (bit 0)
    Execute Command           cpu SetIDAURegion 0 0x20008000 0x200087E5
    # Region 1: 0x20008800-0x20008FFF, RLAR = limit | enabled: Non-secure
    Execute Command           cpu SetIDAURegion 1 0x20008800 0x20008FE1
    # Region 2: 0x20009000-0x200097FF, RLAR = limit | Secure | Non-secure callable (bit 1) | enabled
    Execute Command           cpu SetIDAURegion 2 0x20009000 0x200097E7
    Execute Command           emulation RunFor "0.01"
    Firmware Should Have Finished

*** Test Cases ***
TT Should Report A Secure IDAU Region
    Run With IDAU
    Result Should Be          1    1             TT_RESP.S in the Secure IDAU region
    Result Should Be          2    1             TT_RESP.IRVALID in the Secure IDAU region
    Result Should Be          3    0             TT_RESP.IREGION in the Secure IDAU region

TT Should Report A Non-secure IDAU Region
    Run With IDAU
    Result Should Be          4    0             TT_RESP.S in the Non-secure IDAU region
    Result Should Be          5    1             TT_RESP.IRVALID in the Non-secure IDAU region
    Result Should Be          6    1             TT_RESP.IREGION in the Non-secure IDAU region

TT Should Report A Non-secure Callable And Secure IDAU Region As Non-secure Callable
    Create Log Tester         0
    Run With IDAU
    Wait For Log Entry        Region 2 is both Non-secure callable and Secure    timeout=0
    Result Should Be          7    1             TT_RESP.S in the Non-secure callable IDAU region
    Result Should Be          8    1             TT_RESP.IRVALID in the Non-secure callable IDAU region
    Result Should Be          9    2             TT_RESP.IREGION in the Non-secure callable IDAU region

TT Should Report No IDAU Region Outside All Regions
    Run With IDAU
    Result Should Be          10   1             TT_RESP.S outside all IDAU regions
    Result Should Be          11   0             TT_RESP.IRVALID outside all IDAU regions
