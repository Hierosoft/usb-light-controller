# usb_light_controller
Control your 4-wire RGB lights.

## Wiring

## Requirements
The wiring can be completed without soldering using:
1. Arduino UNO (others may work, but wiring may differ).
2. Elegoo Dupont wires kit (use male to male for GND to breadboard, and male to female for others)
3. Breadboard (single rail for ground is all that is needed unless using SSRs for higher voltage LEDs).
3. All of the following with Dupont connections, such as from Elegoo 37 Sensor Kit:
  - 4-wire LED
  - (optional) momentary switch: any NO (normally open) switch that is triggered only while pressed.
  - (optional) piezoelectric ("piezo") tweeter

## Assembly
- Use a project board (a.k.a. breadboard) so you can have more ground ports during testing.
- Plug GND into left "-" rail on project board.
- Plug R & G into 9 & 10, and blue into 5
  - This odd arrangement is necessary since the `tone` function keeps the timer busy and disables PWM (anything actually) on pins 3 & 11.
- Plug ground (or white cable from pre-wired RGB) into "-" on breadboard or into GND.
- (optional) Plug momentary a.k.a. NO (normally open) switch (first and last pin) into ground and pin 2.
  - Toggles between 3 modes
- (optional) Connect piezo tweeter:
  - "S" (signal) into pin 8
  - middle pin (power) into 5V
  - "-" into "-" on project board (or into GND)
  - The tone indicates mode.
    - low is white or off
    - high is externally-set color (controlled by PC/other device via USB using serial connection)

Color can be get or set reliably using ulightcontroller-get.py and ulightcontroller-set.py.

The GUI example is WIP.

This can be made to work with higher voltage lights such as for modded Replicator 2X if using (3) DC to DC SSRs (Solid State Relays) accepting 5V (logic level on Arduino UNO) and a power supply matching their required voltage connected to the appropriate input on each SSR.
