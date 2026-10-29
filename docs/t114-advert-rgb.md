# Button advertisement RGB feedback — v1.0.12

Only Heltec_t114_repeater defines T114_BUTTON_RGB. A double click still calls
sendSelfAdvertisement(1500, true), via a wrapper marking that specific newly
created packet. Its internal flag is never serialized and is reset at every
pool allocation. Ordinary, relayed, automatic and CLI advertisements are unmarked.

The existing Dispatcher calls logTx after isSendComplete and onSendFinished.
Only this successful callback consumes the mark and requests feedback.
logTxFail clears it without flashing. Allocation failure and queue drops cannot
flash. GPS hasFreshFix(5000) is read at completion, not at button press.

Both GRB SK6812 LEDs on GPIO 14 use an 800 kHz PWM/EasyDMA waveform. Blue means a
fresh GPS fix; purple means no current fix, disabled or absent GPS. Brightness
is limited to 32/255. The main loop starts an off frame after 300 ms.
A fixed buffer includes 100 us reset-low padding. No delays, busy-waits,
heap allocations, interrupt handlers or interrupt masking are introduced.
Only an unused, disconnected PWM peripheral is selected; if none is available,
feedback is skipped rather than disrupting another peripheral. The normal T114
configuration must be checked on hardware for PWM availability and LED order.

Green GPIO 35, its saved preference, GPS power/configuration, four UI screens,
routing and transmission scheduling remain unchanged.

Host verification: tests/t114_rgb/test_feedback.py compiles production RGB driver
and send/completion/failure code with device stand-ins. Tests cover both LEDs,
blue/purple, 300 ms expiry, timer wrap, GPS state at completion, ordinary traffic,
allocation failure, TX failure, and duplicate callback suppression.
UI/button, GPS freshness, position and green TX LED regressions pass.

Full T114 compilation was attempted but timed out installing the ARM toolchain.
No firmware binary is provided. Hardware verification is required for actual
SK6812 polarity/order/timing, idle-off, RF failure paths, busy radio reception,
and coexistence with display/backlight PWM. No received-by-peer guarantee is
implied by a successful local radio transmission.
