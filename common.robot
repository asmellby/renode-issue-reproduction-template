*** Variables ***
${SCRIPT}                     ${CURDIR}/test.resc
${RESULTS}                    0x2000F000
${RESULTS_DONE}               0xD0D0D0D0

*** Keywords ***
Run Reproducer
    Execute Script            ${SCRIPT}
    Execute Command           emulation RunFor "0.01"

Firmware Should Have Finished
    ${unexpected}=            Result    60
    ${cfsr}=                  Result    61
    ${pc}=                    Result    63
    Should Be Equal As Integers    ${unexpected}    0
    ...    Unexpected exception ${unexpected} (CFSR ${cfsr}, PC ${pc})    values=False
    ${done}=                  Result    0
    Should Be Equal As Integers    ${done}    ${RESULTS_DONE}    The firmware did not finish its steps    values=False

Result
    [Arguments]               ${index}
    ${address}=               Evaluate    ${RESULTS} + 4 * ${index}
    ${value}=                 Execute Command    sysbus ReadDoubleWord ${address}
    ${value}=                 Convert To Integer    ${value.strip()}
    RETURN                    ${value}

Result Should Be
    [Arguments]               ${index}    ${expected}    ${what}
    ${value}=                 Result    ${index}
    ${hex}=                   Convert To Hex    ${value}    prefix=0x
    ${expected_hex}=          Convert To Hex    ${expected}    prefix=0x
    Should Be Equal As Integers    ${value}    ${expected}    ${what}: got ${hex}, expected ${expected_hex}    values=False
