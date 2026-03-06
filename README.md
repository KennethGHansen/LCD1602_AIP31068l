# LCD1602 (AIP31068L / DFR0555) – Practical Notes, Pitfalls, and Hardware Reality

This repository contains an STM32 driver and extensive investigation notes for the **LCD1602 I²C module based on the AIP31068L controller**, commonly sold under names such as **DFRobot DFR0555** or **“LCD1602 Module V1.1”**.

What follows is a **hard‑earned, practical README** documenting *what the datasheets say*, *what the boards actually do*, and *what you must do to make them usable in the real world*.

If you are here because:
- text works but is faint  
- backlight does not turn on at all  
- I²C addresses don’t match documentation  
- streaming text (`puts`) corrupts output  

…then this document is for you.

---

## 1. Module Overview

**What works consistently**
- LCD controller: **AIP31068L**
- Interface: **2‑wire serial (I²C‑like)**
- LCD address: **0x7C (8‑bit) / 0x3E (7‑bit)** ✅
- Character display, cursor positioning, CGRAM all work

**What is inconsistent**
- Backlight behavior
- Backlight control method
- Presence of PCA9633
- “V1.1” feature set

---

## 2. I²C Address Reality (Important)

### LCD Controller (AIP31068L)
- Datasheet address: `0x7C` (8‑bit)
- STM32 HAL expects: **`0x3E` (7‑bit)** ✅

This part is consistent across all boards tested.

### Backlight Controller (PCA9633?)
Documentation claims:
- V1.0 → `0x60`
- V1.1 → `0x6B`

**Reality discovered:**
- Many “V1.1” boards **do NOT contain a PCA9633 at all**
- No device responds at `0x60` or `0x6B`
- I²C scanning shows **only 0x3E**

➡️ **Conclusion:**  
You cannot assume PCA9633 exists just because documentation says so.

---

## 3. The Backlight Problem (The Big One)

### Observed behavior
- On power‑up: **NO backlight at all**
- Text visible only faintly via ambient light
- Changing contrast commands does nothing
- PCA9633 initialization code has no effect

### Critical hardware discovery
- Backlight LED cathode is routed through a **10‑pin chip** marked:
  x38353193

This is:
- ❌ NOT a PCA9633
- ✅ A simple transistor / load‑switch / LED driver

When:
- Backlight **cathode is tied directly to GND**
- Backlight **anode is tied to +5V**

➡️ **The backlight turns on immediately and fully**

### Final conclusion
**The backlight is hardware‑gated and OFF by default.**  
There is **no I²C, register, or software path** to enable it on this board variant.

This is **not a firmware bug**.

---

## 4. Why This Is Not Documented

Unfortunately, this is a classic low‑cost module issue:

- Same product name
- Same PCB
- Different BOMs
- Different populated components
- Same documentation reused

“LCD1602 Module V1.1” is a **marketing label**, not a guaranteed hardware feature set.

Some variants:
- have PCA9633
- some have footprints only
- some replace it with a dumb FET
- some leave the enable pin floating or disabled

---

## 5. The Practical Fix (What Actually Works)

### ✅ Confirmed working solution
**Short the backlight cathode to GND**

This:
- bypasses the transistor
- supplies current correctly
- restores full backlight brightness

No software changes required.

### Electrical safety
- LED current is already limited on the module
- Direct GND short is safe in this design
- Optionally add 100–220 Ω in series if desired

### Limitation
- Requires a physical connection
- No PWM / dimming without additional hardware
- Permanent mod requires soldering (not always available)

---

## 6. Why Text Streaming (`puts`) Was Corrupted

This is **separate from the backlight issue**, but worth documenting.

### Root cause
- AIP31068L **cannot read Busy Flag (BF)** in serial mode
- Datasheet explicitly requires **delays between writes**
- Streaming multiple characters in one I²C transaction overwhelms the controller

### Symptom
Strings like:
STM32F446RE

Become:
SM2F46E
…and vary each reset

### ✅ Correct solution
**Send characters one‑by‑one with a delay per character**

This is why:
- `putc()` works
- `puts()` did not

Streaming writes are **not reliable** on this controller.

---

## 7. Summary (TL;DR)

✅ LCD controller works reliably at `0x3E`  
✅ Your AIP31068L code is correct  
✅ PCA9633 initialization logic is datasheet‑correct  
❌ Your board does not contain PCA9633  
❌ Backlight is OFF by hardware default  
✅ Shorting cathode to GND is the correct and effective fix  

> **If there is zero backlight, it is not a contrast issue, and not a software issue.  
> It is a missing hardware enable path.**

---

## 8. Recommendation for Others

If you need:
- predictable behavior
- software‑controlled backlight
- documented hardware

👉 Use:
- a true **RGB LCD1602** with confirmed PCA9633
- or a classic HD44780 + I²C backpack with contrast pot

---

## 9. Final Note

This repository exists so the **next person does not lose days** chasing phantom I²C registers and conflicting documentation.

If this README saved you time: mission accomplished.


## Pictures, video

![20260305_142329](https://github.com/user-attachments/assets/38178f3e-9bdb-41cb-bd3a-3fada0298cdc)


![20260305_174158](https://github.com/user-attachments/assets/8562da80-13e3-440c-89dd-9837f3b6e27f)


https://github.com/user-attachments/assets/9ed52390-0737-49bc-bfe2-bda8aa6822ea

NOTE: White wire is GND connected directly to the LCD1602 backlight diode cathode to ensure backlight during power on!


