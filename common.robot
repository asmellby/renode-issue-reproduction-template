*** Variables ***
${SCRIPT}                     ${CURDIR}/test.resc
${UART}                       sysbus.usart2

*** Keywords ***
Load Reproducer
    [Arguments]               ${repl}=${CURDIR}/nucleo_h533re.repl
    Execute Command           $repl=@${repl}
    Execute Script            ${SCRIPT}
    Create Terminal Tester    ${UART}    timeout=2
    # The app prints "PASS: ..." or "FAIL: ..." for every check
    Register Failing Uart String    FAIL:

Run Reproducer
    Start Emulation
    Wait For Line On Uart     *** Booting Zephyr OS build
