# T114 display redesign — review build

meshcoreStation v1.0.15 source update, based on v1.0.14. Layout reviewed and source publication authorized by the user.

## Behavior

Three normal pages: Repeater (including uptime), GPS, Neighbors. Every page has an estimated battery level and GPS satellite/text banner. Green follows the existing RAM-only successful-acquisition flag, orange the active acquisition flag, red otherwise. Automatic GPS power-off after success stays green. Four most recent neighbors fit below the banner.

USB VBUS (`isExternalPowered`) keeps the display on. After 20 seconds the large percentage page replaces the normal page. A single press returns to the selected page without advancing; subsequent presses advance. Other button events reset inactivity and can return to the selected page. Triple retry is ignored while the USB idle page is visible. Unplugging after inactivity switches the display off; unplugging during activity preserves the remaining timeout. Solar is not inferred. Splash, shutdown, double advertisement and normal GPS retry are preserved.

Single-cell Li-ion voltage estimate: piecewise curve from 3300 mV/0% to 4200 mV/100%, clamped. Every ten seconds one existing ADC reading is taken, even with the display off. Median-of-three and a fixed-point EMA (1/4 new sample) plus a two-point deadband reduce jitter. Initial/invalid readings show `--%`; readings outside 2500–4500 mV invalidate the estimate. A gap over 30 seconds resets the filter. Every valid percentage has `~`. Voltage under load or charge, temperature, battery age and chemistry affect accuracy; USB can bias it upward. No charging status or current is asserted.

The ST7789 framebuffer is monochrome. A T114-only SPI output tint colors the GPS region without a second framebuffer; color-only changes invalidate that region. Other targets retain existing color behavior.

## Modified files

- `examples/simple_repeater/UITask.cpp`, `UITask.h`: layouts, banner, three-page navigation and USB idle mode.
- `examples/simple_repeater/MyMesh.h`: read-only active-acquisition accessor.
- `examples/simple_repeater/BatteryEstimate.h`: new filtered voltage estimate.
- `examples/simple_repeater/BatteryTrend.h`: removed obsolete trend history.
- `src/helpers/ui/ST7789Display.cpp`, `ST7789Spi.h`: T114 banner tint and repaint handling.
- `tests/t114_power/test_estimate.cpp`: replaces obsolete `test_trend.cpp`.
- `tests/t114_ui/test_ui.py`, `test_status_color.py`, `render_preview.py`: behavior, font widths, SPI tint and preview generation.
- `docs/t114-display-preview/`: actual UI draw-call captures and rendered previews.

## Verification

Passed host tests: production UITask with board/sensor mocks (splash, three pages, live/saved GPS, green/orange/red, empty/populated neighbor table, buttons, USB connect/idle/disconnect, shutdown, display-off sampling); generic and no-GPS compilation checks; actual neighbor copy/sort; battery curve bounds/monotonicity/filter/spikes/invalid/gaps/timer rollover; SPI tint selection/byte order/color-only refresh; GPS acquisition and date validation; freshness; RGB successful-packet feedback; TX LED and NodePrefs access; position-button regression; mocked three-target build/checksum/cleanup and release preflight; Nordic layout checks. `git diff --check` passes.

The real PlatformIO `Heltec_t114_repeater` build was attempted but stalled installing `platformio/toolchain-gccarmnoneeabi`. Full ARM compilation is NOT verified and no flashable firmware was generated. Mock build/release tests do not publish or produce usable firmware.

## Previews and hardware limits

Run `TMPDIR="$PWD/.test-tmp" python3 tests/t114_ui/test_ui.py`, then `python3 tests/t114_ui/render_preview.py` (Pillow required on host only). Captures come from production UITask; rendering uses the repository's Arial16/24 bitmap data and the driver's 128x64-to-240x135 mapping. Each PNG is native 240x135; the contact sheet doubles pixels. Example readings are simulated. All lit glyph pixels and rectangles are checked for bounds.

These are software previews, not photographs. Physical ST7789 color order/orientation, USB detection, battery calibration under USB/solar/load, ADC noise, buttons, GPS antenna acquisition, RGB timing, shutdown/low-voltage behavior and concurrent LoRa routing require hardware testing. No changes were made to GPS acquisition, radio, LED or low-voltage protection implementations.

The satellite-icon layout was presented for review. The user subsequently explicitly authorized publication. No compiled firmware release is included.
