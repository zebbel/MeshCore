# Charging & Power — v1.0.14

The display order is Repeater, GPS, Battery/Uptime, Charging & Power, Neighbors.
The new page uses six rows at logical y=0,10,20,30,40,50; each line is at most
21 characters for the 128x64 logical display. Existing T114 ST7789 typography
and scaling are retained. No divider consumes an extra row.

- Battery: existing getBattMilliVolts(), cached up to ten seconds, two decimals.
  Invalid readings show -- V.
- USB: existing isExternalPowered(), which reads Nordic USB VBUS detection.
  Connected/disconnected does not assert charging.
- Solar: Not detectable.
- Trend: Collecting, Rising, Stable or Falling; never charging confirmation.
- Charging: Unknown.

Sampling is independent of the active page and continues with the display off.
One existing ADC reading is taken every ten seconds. No catch-up bursts occur.
Each three readings produce a median, rejecting single-sample outliers.
An eleven-entry ring compares filtered endpoints five minutes apart.
A 30 mV change enters Rising/Falling; returning within 10 mV releases to Stable.
Two consecutive filtered updates confirm a change. Initial collection takes
about 5 minutes 20 seconds (up to another 30 seconds for a directional result).
Values outside 2.0–5.0 V or a sampling gap over 20 seconds reset collection.
These bounds are plausibility checks, not estimates of battery capacity.
The original Battery/Uptime page is untouched, including its voltage refresh.

Changed files:
- examples/simple_repeater/UITask.cpp and UITask.h
- examples/simple_repeater/BatteryTrend.h (new, no dependencies or persistence)
- tests/t114_ui/test_ui.py
- tests/t114_power/test_trend.cpp (new)
- meshcoreStation.version and this document

Host tests pass for five-page rendering/cycling, two-decimal voltage, USB on/off,
truthful solar/charging labels, text length, background ADC sampling, all button
gestures, display timeout, GPS acquisition/retry, RGB feedback and green LED.
Trend tests cover rising/falling/stable, isolated spikes, small fluctuations,
direction reversal, insufficient/invalid data, sampling gaps and timer wrap.
Generic and no-GPS UI compilation checks pass. Existing position tests pass.

Real T114 firmware build was attempted but blocked during ARM toolchain
installation (network approval was cancelled). No binary was produced.
Hardware tests still required: USB-only, solar-only, combined input and battery
operation; actual display clipping and voltage accuracy; long-run trend behavior;
ADC overhead under LoRa traffic and power-saving operation. Solar-only and
battery-only deliberately show the same solar/charging uncertainty.
