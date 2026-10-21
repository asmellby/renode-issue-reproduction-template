*** Settings ***
Resource                      ${CURDIR}/common.robot
Suite Setup                   Setup
Suite Teardown                Teardown
Test Setup                    Run Reproducer
Test Teardown                 Test Teardown

*** Test Cases ***
Firmware Should Finish
    Firmware Should Have Finished

BFAR Should Read Back The Written Value
    Result Should Be          1    0x12345678    BFAR after writing 0x12345678
    Result Should Be          2    0             BFAR after writing 0

Non-secure BFAR Write Should Be Ignored Without BFHFNMINS
    Result Should Be          3    0             BFAR after writing BFAR_NS
    Result Should Be          4    0             BFAR_NS (RAZ)

Non-secure BFAR Write Should Take Effect With BFHFNMINS
    Result Should Be          5    0x55550000    BFAR after writing BFAR_NS

MMFAR Should Read Back The Written Value Of Each Bank
    Result Should Be          6    0x11112222    MMFAR after writing 0x11112222
    Result Should Be          7    0x33334444    MMFAR_NS after writing 0x33334444

SFAR Should Read Back The Written Value
    Result Should Be          8    0x77778888    SFAR after writing 0x77778888 and then 0 to SFAR_NS
    Result Should Be          9    0             SFAR_NS (RAZ)
