# T1000-E charging LED — meshcoreStation v1.0.19

The existing `UITask::userLedHandler()` now gives the charging indication priority over ordinary status flashes on the BLE, USB and BLE position-button companions only. No settings are added or rewritten.

- Existing LED-disabled preference: controls ordinary status indication only; charging indication always takes priority.
- External power HIGH and charging-status LOW: green LED eases from off to full brightness and back over two seconds.
- External power HIGH and charging status inactive: off. This does not prove the battery is full; protection/fault-related charge stops are also dark.
- External power absent: original heartbeat and unread-message timings resume if enabled; otherwise the LED remains off. The saved preference is never changed.

The two inputs are sampled every 100 ms. A new combined state must persist for 300 ms before it is accepted; the LED remains dark during initial qualification. The animation uses integer smoothstep easing with at most one PWM update per 20 ms, no waiting, heap allocation or new library. BLE connection state does not gate this handler.

PWM uses the existing Adafruit nRF52 Arduino `analogWrite()` hardware allocation. Its default eight-bit range is retained; no global PWM resolution or buzzer settings are changed. A board helper detaches only the status LED pin and stops the peripheral only if it has no remaining pins. This prevents a residual PWM level from overriding digital status flashes or shutdown. Shutdown detaches PWM before the existing shutdown sequence; the board power-off path also detaches it before its existing button-release indication. No animation runs after system-off.

## Hardware evidence and outstanding checks

The repository's T1000-E variant already initializes P1.03 and P0.05 as plain INPUT, without internal pulls; this is preserved. The Meshtastic T1000-E board definition declares charging active LOW; its external-power default is HIGH with INPUT mode. These are source-level corroboration, not measurements on the user's board:

- https://github.com/meshtastic/firmware/blob/develop/variants/nrf52840/tracker-t1000-e/variant.h
- https://github.com/meshtastic/firmware/blob/develop/src/Power.cpp
- https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/master/cores/nRF5/wiring_analog.cpp
- https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/master/cores/nRF5/HardwarePWM.cpp

**Hardware validation is pending; this source update is not hardware-finalized.** Measure power-detect and charge-status levels while unplugged, actively charging and no longer charging. Check for floating inputs on the actual board revision. Confirm a smooth two-second pulse, complete darkness when charging stops or when unplugged with status LED disabled, reconnect and unplug behavior, and no residual activity after power-off. Check BLE/USB, radio RX/TX, GPS, buttons, private-channel position/beep, battery monitoring and low-voltage operation while PWM is active. Test charge-stop conditions safely; no full-charge assertion is made from the signal alone.

## Modified files

- `examples/companion_radio/ui-orig/UITask.cpp`: charging priority, input qualification, eased PWM and shutdown cleanup inside the existing handler.
- `variants/t1000-e/T1000eBoard.h/.cpp`: PWM detachment helper and power-off cleanup.
- `variants/t1000-e/platformio.ini`: feature flag on BLE/USB companion; position build inherits BLE flags. Repeater, room-server, modem and other boards are excluded.
- `tests/t1000_charging/test_led.py`: production handler/PWM-release execution with hardware stand-ins.
- `meshcoreStation.version` and this report.

## Verification

Host tests passed for two-second monotonic rise/fall with intermediate brightness levels, sampling/update limits, unstable inputs, charging-stop/reconnect/unplug, disabled preference, original heartbeat and unread pulse restoration, shared PWM preservation, millis rollover and build-target guards. The ordinary single-color path also compiles without the feature flag. Position-button state/firmware integration, T114 remote-GPS and RGB regression checks passed. No real BLE/USB/radio/GPS hardware test was performed.

PlatformIO builds were attempted for all three T1000-E targets. The ARM toolchain package download remains unavailable; full firmware compilation and firmware artifact generation are not verified. No flashable binary is included. Source publication is authorized by the user's corrected instruction and standing handover-publication preference.

## v1.0.19 priority correction

Moved the existing status-preference check below the charging override and PWM-release transition. This is the only production behavior change. Charging-input polarity, debounce, PWM waveform, shutdown and persistent settings are unchanged. The BLE/USB targets and position variant retain their existing feature flags.

Modified files for this correction: `examples/companion_radio/ui-orig/UITask.cpp`, `tests/t1000_charging/test_led.py`, `meshcoreStation.version` and this document.

Host tests passed: begin charging with status disabled; toggle on/off during a complete waveform without phase restart; charging stopped stays dark with either preference; reconnect while disabled; detach PWM and remain dark when unplugged with status disabled; enable normal heartbeat afterward; unread-message pulse restoration; debounce, shared-PWM cleanup and timer rollover. Position-button state/firmware integration tests passed. The preference API and persistence code were not modified. These are simulated hardware tests, not physical BLE/USB/LoRa/GPS validation.

All three PlatformIO target builds were attempted again but remain blocked at the ARM toolchain download. No successful full compilation or new firmware artifacts are claimed. Hardware follow-up: repeat the supplied /led off/on charging sequence, charging-stop and unplug/reconnect transitions; verify no disruption to BLE/USB, GPS, radio traffic, position button, shutdown and power management. The user's report confirms charging already works with /led on; no polarity changes were made.
