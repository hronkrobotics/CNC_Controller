# Instructions

Build the box from the 1515 subframe through first motion. Electrical reference: [hardware](docs.md/hardware.md). Firmware: [software](docs.md/software.md). Wiring: [system diagram](docs.md/images/diagram-system.jpg). CAD: [cad/](cad/).

**Do not apply 36 V until the wiring check is done.** Mount **all rear-panel connectors (M12s, IEC, switch, fuse) on the panel before** you bolt that panel to the extrusion — it is much harder after.

CAD stills are orthographic / isometric renders of the same parts. Use them together with the photos. If extrusion lengths or printed brackets are unclear, open the STEP files in Fusion or SolidWorks.

---

## 1. Frame base

Lay out the **1515** lengths and PETG corner / divider pieces. Note how the printed brackets sit in the T-slots and how the cage is split (driver bay vs PSU / logic bay). Geometry: [`cad/Extrusions.step`](cad/Extrusions.step), [`cad/Hardware.step`](cad/Hardware.step).

![Base — top](docs.md/images/01-base-top.jpg)

![Base — isometric](docs.md/images/01-base-iso.jpg)

---

## 2. Drivers, Arduino holder, power supply

Mount the four **TB6600** drivers, the **Uno** holder, and the **36 V PSU** to the base. Set DIP switches on the drivers now, before wiring. The CNC shield plugs onto the Uno later — do not bury the headers. Electronics placement: [`cad/Electronics.step`](cad/Electronics.step).

![CAD — drivers and PSU](docs.md/images/02-drivers-psu-iso.jpg)

![CAD — Arduino mount from above](docs.md/images/02-drivers-arduino-top.jpg)

![Photo — drivers, Uno plate, PSU on the cage](docs.md/images/02-mounts-photo.jpg)

![CAD — driver rails](docs.md/images/02-driver-rails.jpg)

---

## 3. Right inner, left inner, rear panels

Fit the inner side panels (4010 fan cutouts) and the rear panel (M12 grid, IEC, switch, fuse). Leave the fan openings empty for now — the grills are thermoformed last, [step 9](#9-thermoform-the-fan-grills).

![Right inner panel](docs.md/images/03-right-inner.jpg)

![Left inner panel](docs.md/images/03-left-inner.jpg)

![Rear panel on the cage](docs.md/images/03-rear.jpg)

![Fan-side enclosure](docs.md/images/03-enclosure-fan-side.jpg)

![Closed side profile](docs.md/images/03-enclosure-closed-side.jpg)

---

## 4. Rear panel — M12s, IEC, switch (do this off the frame)

Cut perfboard to the **vertical pairs** of through-hole M12s. Solder the connectors. Braid the wires through the board and solder. Fit the **IEC inlet**, **on/off switch**, and fuse on the same panel. Wire those to the schematic **before** the panel goes on the extrusion.

![Soldering and dressing the rear panel](docs.md/images/04-m12-solder.gif)

---

## 5. Press-fit M3 nuts

Press **M3 nuts** into the traps on the **top edge of the left and right inner panels**. Those nuts take the display / top panel later. Do this while the traps are still easy to reach.

![M3 nut traps on the inner-panel lip](docs.md/images/04-m3-nut-traps.jpg)

---

## 6. Top panel

Mount the top / bezel panel to those M3s. Leave access until wiring is done if you need it.

![Top panel](docs.md/images/05-top-panel-iso.jpg)

![Interior with top open](docs.md/images/05-top-open.jpg)

---

## 7. Wire power, drivers, connectors

Follow the [wiring diagram](docs.md/images/diagram-system.jpg) and the [as-built notes](docs.md/hardware.md) (each driver has its own 36 V pair; three M12s are X/Y/Z endstops; one M12 is 5 V / GND / probe). Set the **LM2596** to 5 V. Put the **NC e-stop** in series with driver 36 V.

- [ ] Each TB6600 has its own 36 V pair from the Wagos
- [ ] NC e-stop opens driver 36 V
- [ ] Buck output is 5 V before it hits the Uno
- [ ] Four motor M12s = driver phases
- [ ] Three M12s = endstops X/Y/Z
- [ ] One M12 = 5 V / GND / probe
- [ ] UART through the BSS138 shifter
- [ ] Earth, fused IEC, no 36 V on logic pins

![Wiring — PSU, Wagos, drivers, Uno](docs.md/images/06-wiring-a.jpg)

![Wiring — fans and buck in place](docs.md/images/06-wiring-b.jpg)

![Wiring — close on drivers and shield](docs.md/images/06-wiring-c.jpg)

---

## 8. Flash firmware and check

1. Pendant **unplugged** from Uno TX/RX. Flash **GRBL 1.1** to the Uno over USB (`$10=1`, `$13=0`).
2. Reconnect UART. Flash the pendant — [software](docs.md/software.md). Native USB port.
3. Logic only first (USB). Screen should boot; Alarm until Unlock is normal.
4. Then 36 V. Jog **1 mm**. Hit the **NC e-stop** if anything is wrong.

Double-check every screw terminal and M12 before you call it done.

![Firmware up — pendant, web UI, open box](docs.md/images/07-firmware.jpg)

First-boot WiFi: AP `CNC-Pendant` — change `cnc12345` before a public demo.

---

## 9. Thermoform the fan grills

The four 4010 openings take PETG mesh grills that are **printed flat, then formed over a mold with a heat gun**. They are not CNC-cut and not pressed in a heated platen. Mold and grill geometry: [`cad/Fan_Grill_Parts.step`](cad/Fan_Grill_Parts.step).

This process is much easier to get right from a video than from a paragraph. **[Uncle Jessy](https://www.youtube.com/@UncleJessy)** walked the same method — print a mesh flat, heat it slowly, press it into a printed mold — in *[I Printed This Flat Then Bent It Into 3D!](https://www.youtube.com/watch?v=TsG89NgZJO8)*. That tutorial is excellent; follow it for heat, distance, and how hard to press. Credit to him for documenting the technique so clearly.

A few notes specific to this box:

- Print the grill meshes and the mold from [`Fan_Grill_Parts.step`](cad/Fan_Grill_Parts.step). PETG needs more heat than PLA; keep the gun on **low**, keep it moving, and wear gloves.
- The goal is stretch, not melt. If an edge tears or the mesh shrinks off the mold, you are too hot or too close.
- Let each grill cool **on the mold** so the curve sets, then trim flash and seat it in the side-panel opening.

![Fan-side panel openings](docs.md/images/03-panel-fan-side-a.jpg)

![Grill openings, other side](docs.md/images/03-panel-fan-side-b.jpg)
