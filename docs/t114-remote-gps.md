# T114 remote GPS update — meshcoreStation v1.0.17

An authenticated repeater administrator can enter `gps update` in the Repeater Management Console. USB serial uses the same command. The immediate reply is one of:

- `GPS update started`
- `GPS update already running`
- `ERR: GPS unavailable`
- `ERR: GPS start failed`

Started means acquisition is running, not that position/time has been acquired. Use `gps status` to read the existing acquisition status (SEARCHING, FIX / SYNCING, FIX SAVED - GPS OFF, TIMEOUT, SAVE FAILED, UNAVAILABLE or OFF, with the existing GPS prefixes).

Both commands are intercepted in the T114 acquisition-enabled repeater handler, after whitespace/companion-prefix handling and before the generic GPS fallback. Region-load sessions retain their original parsing. Existing GPS on/off/sync/diag/advert commands continue through CommonCLI unchanged.

`startGPSAcquisition()` now returns the immediate result. It checks for an active acquisition before changing any state, checks the provider and exposed GPS control, checks the power-control result and provider enabled state, requests fresh time synchronization, and then marks the attempt active. Startup and triple-button retry still call this same function and may ignore its return value. Failed startup reports GPS OFF or UNAVAILABLE; no saved coordinates are changed.

The success/timeout/save-failure loop is unchanged: fresh position plus valid date/time and RTC readback, persistent save, one normal flooded advertisement under the configured location policy, then transient GPS power-off. Automatic advertisements remain independent of button RGB feedback. Persistent GPS-enabled settings are unchanged.

## Access and replay handling

No new authentication or radio protocol was added. The existing `onPeerDataRecv()` known-client and administrator checks guard remote CLI execution. Old timestamps are rejected and equal-timestamp retries do not execute the command again, including after acquisition completes. A new valid command timestamp during acquisition returns already-running. A new command after completion intentionally starts a new update. USB serial retains its existing local access model.

## Changed files

- `examples/simple_repeater/GPSAcquisition.cpp`: reliable start-result return and enabled-state check.
- `examples/simple_repeater/MyMesh.h`: return type declaration.
- `examples/simple_repeater/MyMesh.cpp`: two target-guarded command branches.
- `tests/t114_gps/test_acquisition.py`: result, unavailable-control and failed-enable coverage.
- `tests/t114_gps/test_remote_cli.py`: production peer-gate and dispatcher execution with host stand-ins.
- `meshcoreStation.version` and this report.

## Verification

Passed: GPS acquisition/save/time/timeout/retry and one-advertisement regression; all four start replies; production remote peer handler with admin, guest, unknown peer, invalid flags, older timestamp, equal timestamp, duplicate after completion, new retry; USB-style dispatch and companion prefix; every acquisition status; existing GPS commands falling through; production advert location policies and button tagging; RGB feedback; three-page UI and gestures; GPS freshness; green TX LED. `git diff --check` passed.

The remote test executes the production `onPeerDataRecv()` body and command-dispatch section with mocked transport, not encrypted radio packets or a live management application. It does not replace physical security/integration testing.

The full PlatformIO T114 build was attempted but remained blocked at downloading `platformio/toolchain-gccarmnoneeabi`; no successful ARM compilation or flashable firmware artifact is claimed. Hardware checks outstanding: authenticated console and USB interaction, rejected guest access over radio, GPS power-on/fix/RTC/save/power-off, actual automatic advert reception and location, absence of automatic RGB flashes, concurrent routing and battery behavior.

Source publication follows the user's standing instruction to publish handovers.
