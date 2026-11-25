# Keystone 280 manual

The jacks in this collection are monophonic. With a polyphonic cable, a module reads the first channel. Voltages and timing below describe the software. For more on how the models were fitted, see [Calibration](../CALIBRATION.md).

- [Susquehanna Oscillator](#susquehanna-oscillator)
- [Schuylkill VCA](#schuylkill-vca)
- [Lehigh Envelope](#lehigh-envelope)
- [Allegheny Low-Pass](#allegheny-low-pass)
- [Monongahela High-Pass](#monongahela-high-pass)
- [Lackawanna Filters & Noise](#lackawanna-filters--noise)
- [Brandywine Filter Bank](#brandywine-filter-bank)
- [Juniata Sequencer](#juniata-sequencer)
- [Conestoga Interface](#conestoga-interface)
- [Tioga Sequential Switch](#tioga-sequential-switch)
- [Clarion Mixer](#clarion-mixer)
- [Kiski Attenuators](#kiski-attenuators)
- [Loyalsock Function Generator](#loyalsock-function-generator)
- [Swatara Quantizer](#swatara-quantizer)

## Susquehanna Oscillator

Use Susquehanna as a pitched voice or slow modulation source. At the default `8'` range and centered `TUNE`, it starts at about 261.6 Hz with no pitch CV.

- **RANGE** steps by octaves from `32'` to `1'`. **TUNE** adjusts pitch within the selected **SPAN**, either ±6 octaves or ±12 semitones. **RATE** selects AUDIO or SUB. SUB divides the frequency by 100. SPAN starts at ±6 octaves and RATE starts at AUDIO.
- **PITCH 1**, **PITCH 2** and **PITCH 3** sum at 1 V/oct. **SINE**, **TRI**, **SAW** and **PULSE** are the four fixed waveform outputs. **PULSE WIDTH** and the summed **WIDTH 1** and **WIDTH 2** inputs set pulse duty. Each volt adds 15 percentage points, with the result limited to 10–90%. The knob starts at 50%.
- **AUX WAVE** selects sine, triangle, inverted saw, saw, pulse or inverted pulse. **AUX LEVEL** starts at zero and raises the level to a nominal 3.47 V peak. **AUX +** sends the selected wave and **AUX -** sends its inversion.
- **CLAMP PHASE** sets where in the cycle the oscillator jumps, from 10% to 90%. **V-TRIG** activates at +2.5 V and releases below +1.5 V. A patched **S-TRIG** activates at or below +1 V and releases above +2 V. The default holds the phase until the trigger releases. The **Clamp behaviour** menu can select a reset that keeps the wave running.

Patch a pitch source into **PITCH 1** and **SAW** into Allegheny. For slow pulse-width modulation, send a slow triangle from a second oscillator into **WIDTH 1**.

Bench recordings set the waveform levels and shapes, and checked pitch tracking and octave steps. The software centers the default tuning at C4. The **Oscillator calibration** menu starts at **Unit A**. **Unit B** lowers pitch by 1.85 semitones, based on the measured difference between the two units. The range controls, auxiliary routing and clamp controls follow the hardware documentation. The precise trigger thresholds and waveform shape away from the measured pitch are modeled.

## Schuylkill VCA

Schuylkill varies the gain of audio or CV. It subtracts **IN -** from **IN +**, then applies the gain. **OUT +** sends the result and **OUT -** sends its inverse.

- **BIAS** adds 0–6 V to the control signal. It starts at zero. **CV 1**, **CV 2** and **CV 3** add to that voltage.
- **RESPONSE** selects LIN or EXP, with EXP as the default. LIN rises roughly in proportion to control voltage above its mute region. EXP gives a response closer to a steady change in decibels. The model saturates at high gain and soft-clips the signal near ±10 V.

Send an oscillator to **IN +** and Lehigh's **OUT** to **CV 1**. With **BIAS** down, **OUT +** follows the envelope. Raise **BIAS** to leave some signal audible between notes.

Bench sweeps set the linear gain slope, mute region and ceiling, and the exponential gain curve. The summed control and differential signal routing follow the hardware documentation. The fade near the exponential noise floor and the output clipping curve are modeled.

## Lehigh Envelope

Lehigh makes a positive ADSR envelope. **OUT** rises toward 6 V, decays to the sustain level and releases to zero.

- **ATTACK** sets the time to peak, from 2 ms to 30 s in the model. **DECAY** and **RELEASE** range from 18 ms to 29 s. Those times describe how long it takes to cover 90% of the respective voltage change.
- **SUSTAIN** sets the held level from 0 to 6 V. It starts at 3 V. **S-TRIG** starts the envelope at or below +1 V while patched. It releases above +2 V or when unplugged. Raising the input early starts release from the current level.

Patch a regular positive gate to Conestoga's **1A 1**, then **S BANK 1** to Lehigh's **S-TRIG**. Send **OUT** to a VCA for note articulation.

Timed gate recordings set the decay, sustain and release landmarks. The ADSR sequence and active-low input follow the hardware documentation. Much of the attack taper and the 6 V peak are model estimates.

## Allegheny Low-Pass

Allegheny is a four-pole low-pass filter. **IN** is DC-coupled and **OUT** is AC-coupled.

- **CUTOFF** sets the base pole frequency. **RANGE** positions 1, 2 and 3 are two octaves apart. With **CUTOFF** centered and no CV, the nominal pole frequencies are about 355 Hz, 1.42 kHz and 5.68 kHz. Position 2 is the default.
- **CV 1**, **CV 2** and **CV 3** sum at 1 V/oct. Positive voltage raises cutoff. The DSP limits the pole frequency to 45% of the host sample rate.
- **RESONANCE** adds feedback. It starts at minimum. At around two thirds of the dial, the model can self-oscillate. The pitch of that tone depends on cutoff and feedback.

Patch a saw into **IN** and an envelope into **CV 1**. Lower **CUTOFF** and raise **RESONANCE** until the envelope sweep brings out the upper harmonics.

Bench sweeps set the cutoff taper, the onset of self-oscillation and the change in oscillation pitch with feedback. The range spacing and four-pole low-pass behavior follow the hardware documentation. The DSP approximates the ladder and its drive behavior.

## Monongahela High-Pass

Monongahela is a four-pole high-pass filter. **IN** and **OUT** are DC-coupled. A steady DC level is removed by the high-pass response.

- **CUTOFF** adds a control offset of −6 to +6 V. **RANGE** selects LOW or HIGH. With **CUTOFF** centered and no CV, the cutoff is about 283 Hz on LOW, the default, and 707 Hz on HIGH.
- **CV 1**, **CV 2** and **CV 3** sum with that offset at 1 V/oct. Positive voltage raises cutoff and removes more low end. The combined control is limited to ±7 V and cutoff to 45% of the sample rate.

Put this after an oscillator mix. Start in LOW and turn **CUTOFF** up until the bass clears. Patch slow CV into **CV 1** to move the low end in and out.

Noise recordings checked the cutoff at noon in both ranges. The summed cutoff control and high-pass topology follow the hardware documentation. The pole spacing and drive behavior are modeled.

## Lackawanna Filters & Noise

The two filters work independently, with 6 dB/octave slopes. Two noise sources run even with the filter inputs empty.

- **LOW-PASS** sets the cutoff for **LP IN** to **LP OUT**. It sweeps from 6 Hz to 6 kHz and starts at maximum. **HIGH-PASS** sets the cutoff for **HP IN** to **HP OUT**, from 4.7 Hz to 4.7 kHz. It starts at minimum.
- **WHITE 1** and **WHITE 2** are copies of the same white noise at about 0.73 V RMS. **PINK 1** and **PINK 2** copy the pink noise at about 1.14 V RMS. Pink noise has more energy at low frequencies than white noise.
- **HP OUT** removes the slowly changing part of its input. A sudden reversal from a settled DC level can briefly produce almost twice the input peak voltage.

For a band of noise, patch **WHITE 1** to **HP IN**, then **HP OUT** to **LP IN**. Listen to **LP OUT** and set **HIGH-PASS** below **LOW-PASS**.

Bench recordings set the noon cutoffs and the noise levels. The single-pole slopes, separate signal paths and noise types follow the hardware documentation. The rest of the cutoff taper and noise spectrum are modeled.

## Brandywine Filter Bank

Brandywine sums fourteen fixed filter branches. Each knob sets the contribution of one branch, from about −65 dB to unity. All branch knobs start at minimum.

- **LOW-PASS** controls the 100 Hz low branch. **HIGH-PASS** controls the 7.5 kHz high branch.
- **125 Hz**, **175 Hz**, **250 Hz**, **350 Hz**, **500 Hz**, **700 Hz**, **1000 Hz**, **1400 Hz**, **2000 Hz**, **2800 Hz**, **4000 Hz** and **5600 Hz** control the band-pass branches at those center frequencies.
- **IN** feeds the bank. **OUT** and **OUT 2** carry the same filtered sum. **THRU** copies the raw input before filtering.

Start with a saw or noise at **IN**. Bring up **500 Hz** and **1400 Hz** and listen to **OUT**. Use **THRU** for a dry path if you want to blend some of the original sound back in.

A bench sweep of the 1 kHz branch set the gain taper shared by the knobs. The branch frequencies follow the hardware documentation. Bandwidth, clipping and the response away from the measured branch are modeled.

## Juniata Sequencer

Juniata runs three CV rows across eight playable stages. Each row has its own voltage range. The internal clock starts stopped.

- **STAGE 1** through **STAGE 9** select a stage directly. Each of the first eight columns has three unlabelled voltage knobs, one per row, and a **SKIP / NORMAL / STOP** selector. NORMAL plays the stage. SKIP omits it when advancing. STOP selects the stage and stops the clock. Stage 9 has no voltage knobs. Its **SKIP / STOP** selector controls whether it is skipped at the end of the sequence. When selected, stage 9 stops the clock and all rows output 0 V.
- **ROW A**, **ROW B** and **ROW C** select X1 (0–2 V), X2 (0–4 V) or X4 (0–8 V). Each row starts at X1. **A 1** and **A 2**, **B 1** and **B 2**, and **C 1** and **C 2** are pairs of copies. **ROW C TIMING** lets the current C voltage change the internal clock rate. Below 1 V it slows the clock. Above 1 V it can speed the clock up, with a ceiling that depends on the base rate.
- **CLOCK RANGE** and **CLOCK RATE** set the base clock speed. At zero CV and with timing off, ranges 1 to 6 span 0.04–0.5 Hz, 2.75–30 Hz, 0.17–2 Hz, 11–130 Hz, 0.7–8 Hz and 44–500 Hz respectively. Range 5 is the default. **RATE CV** shifts the rate at 1 V/oct with an input limit of ±5 V.
- The **RUN** and **STOP** buttons start and stop the clock. The **RUN** and **STOP** jacks do the same on rising crossings of +3.5 V. **CLOCK** sends +5 V pulses, high for about 32% of a cycle at a steady rate.
- **ADVANCE** or a rising crossing of +0.35 V at **SHIFT** moves one stage. The **SET 1** through **SET 9** inputs select their stage at +3.5 V. **GATE 1** through **GATE 9** stay at +5 V while their stage is active. The stage lights show the selection.
- By default a STOP stage blocks manual and external advances. Use direct selection to leave it, or choose **STOP only stops the oscillator** under **Stop stage behaviour** to allow advances.

Patch **A 1** to Susquehanna's **PITCH 1** and tune the top row by ear, or run it through Swatara. Take **CLOCK** to Conestoga and use a timed bank to give Lehigh a short gate on each clock cycle.

The clock duty cycle, external advance from a stop stage and the effect of row C on timing were checked on the bench. The row and stage layout follows the hardware documentation. Row voltage spans and precise thresholds are model settings.

## Conestoga Interface

Conestoga makes gates from audio and converts between positive-going and active-low signals. The two trigger banks are independent. The S-to-V path in each bank works independently of its V-to-S path.

- **AUDIO** feeds a level detector with a 10 Hz high-pass input. **AUDIO SENSE** changes the detector gain. **AUDIO V1** and **AUDIO V2** send the same 0/+5 V gate. After patching, the detector waits for the level to drop before it can fire.
- **S 1** feeds **V 1A** and **V 1B**. **S 2** feeds **V 2A** and **V 2B**. Each pair goes to +5 V while the corresponding S input is active. A patched S input activates at or below +1 V and releases above +2 V.
- **1A 1** through **1A 6** and **2A 1** through **2A 6** hold their bank active as long as any of their gates is high. **1B 1** through **1B 6** and **2B 1** through **2B 6** start fixed-length pulses on rising edges. The A and B inputs turn on at +1.5 V and rearm below +1 V. B edges during an active A gate or an existing B pulse are ignored.
- **BANK 1 TIME** and **BANK 2 TIME** set the B pulse lengths from 45 ms to 4.2 s. Both start at the short end. **S BANK 1** and **S BANK 2** idle at +12 V and go to 0 V while active.

To give Lehigh the same gate length on every note, patch a positive clock to **1B 1**, set **BANK 1 TIME**, and take **S BANK 1** to Lehigh's **S-TRIG**. For a gate length that follows the source, use **1A 1**.

Bench checks set the timed bank lengths and confirmed that the lower and upper input jacks behave alike. The converter routing follows the hardware documentation. The audio detector is an approximation based on observations with noise.

## Tioga Sequential Switch

Tioga connects one of three sources to a pair of output jacks. It switches audio or CV.

- **SIG 1**, **SIG 2** and **SIG 3** feed the three stages. **OUT 1** and **OUT 2** carry the same selected signal, or 0 V if that input is empty.
- **SHIFT** advances on a rising crossing of +1.5 V. It loops through two stages with **SIG 3** empty and three with **SIG 3** patched.
- **SELECT 1**, **SELECT 2** and **SELECT 3** select their stage by hand. **SEL 1**, **SEL 2** and **SEL 3** do the same at +3.5 V. Direct selection takes priority over **SHIFT** and can select an empty stage.
- **GATE 1**, **GATE 2** and **GATE 3** are +5 V for the active stage and 0 V otherwise. The adjacent light shows the active stage.

Send Juniata's **A 1** and **B 1** to **SIG 1** and **SIG 2**, then patch **GATE 1** from Juniata to **SHIFT**. Take **OUT 1** as a pitch sequence that alternates rows each time Juniata enters stage 1.

The two-stage loop and manual selection of an empty third stage were checked on the rack. The stage gates and parallel outputs follow the hardware documentation. Trigger thresholds and instant switching are model settings.

## Clarion Mixer

Clarion sums four signals and also provides two independent multiples.

- **IN 1**, **IN 2**, **IN 3** and **IN 4** take audio or CV. **GAIN 1** through **GAIN 4** attenuate those inputs from zero to unity and start at zero.
- **MASTER** scales the sum from zero to unity and starts at half. **+ MIX 1** and **+ MIX 2** carry identical copies. **- MIX 1** and **- MIX 2** carry the inverse. The mix soft-clips near ±8 V.
- **MULT A** feeds **A COPY 1**, **A COPY 2** and **A COPY 3**. **MULT B** feeds **B COPY 1**, **B COPY 2** and **B COPY 3**. The copies bypass the gain controls.

Mix two oscillator waves through **IN 1** and **IN 2**, and send **+ MIX 1** to a filter. Use the negative mix to send an inverted version of a CV mix to a second destination.

Meter readings set the linear master taper. The four-channel sum, complementary outputs and multiples follow the hardware documentation. Channel gain and clipping curves are modeled.

## Kiski Attenuators

Kiski makes separately scaled copies of a signal and supplies fixed voltages.

- **ATT 1** through **ATT 4** scale their respective **IN 1** through **IN 4** to **OUT 1** through **OUT 4**. They go from mute to about 0.564× gain and start at the maximum. An empty input receives the raw signal from the last patched input above it. Each channel applies its own attenuation.
- **-6 V A** and **-6 V B** supply −6 V. **+6 V A** and **+6 V B** supply +6 V.
- **MULT A** feeds **A COPY 1**, **A COPY 2** and **A COPY 3**. **MULT B** feeds **B COPY 1**, **B COPY 2** and **B COPY 3**. The multiples copy their inputs at unity.

Patch an LFO to **IN 1** only. Use **ATT 1** and **ATT 2** to set different modulation depths for a filter and a pulse-width input. For an adjustable positive offset, patch **+6 V A** to an attenuator input.

An unloaded meter sweep of the first attenuator set the gain taper shared by all four. The cascaded input normals, multiples and reference voltages follow the hardware documentation.

## Loyalsock Function Generator

Channels 1 and 4 slew signals or generate rise/fall shapes. Channels 2 and 3 scale, invert or provide offsets. The module can also sum channels or pick the highest positive channel voltage.

- **IN 1** and **IN 4** feed the slew channels and accept up to ±10 V. **TRIG 1** and **TRIG 4** start a rise toward +10 V followed by a fall. Triggering needs a fast enough edge above +1.1 V. A trigger during the rise is ignored. **RAW 1** and **RAW 4** carry the signals before their attenuverters.
- The **RISE 1** and **RISE 4** knobs set rise time. The **FALL 1** and **FALL 4** knobs set fall time. **CURVE 1** and **CURVE 4** go from LOG through LIN to EXP. They change the envelope shape and time range. Centered at LIN, an unmodulated full-scale rise or fall spans 0.5 ms to 25 s.
- The **RISE 1**, **RISE 4**, **FALL 1** and **FALL 4** jacks lengthen their part of the cycle with positive CV. Positive CV at **BOTH 1** or **BOTH 4** halves both times per volt until the minimum is reached. All six time CV inputs are limited to ±8 V.
- The **CYCLE 1** and **CYCLE 4** buttons toggle looping. Their matching input jacks enable looping while a gate is high, with an onset threshold above +2.5 V. The cycle lights show whether looping is enabled.
- **ATT 1** through **ATT 4** turn from inverted unity through zero to positive unity, with zero as the default. **IN 2** and **IN 3** go directly to their attenuverters. With no input, channel 2 gives −10 to +10 V and channel 3 gives −5 to +6 V. **OUT 1** through **OUT 4** carry the four scaled signals.
- **END RISE** on channel 1 is about +8.7 V during the fall. **END CYCLE** on channel 4 is about +9.8 V at idle and briefly at cycle completion. Their lights follow the outputs.
- **SUM** adds the contributing channels and is limited to ±10 V. **INVERT** flips that sum. **OR** sends the highest contribution or zero if all are negative. By default, patching an **OUT 1** through **OUT 4** jack removes that channel from the shared outputs. Toggle **Exclude patched outputs from SUM and OR** in the context menu to keep those channels in the mix.

Turn on **CYCLE 1**, leave **IN 1** empty and take **RAW 1** to a filter cutoff input. Use **RISE 1** and **FALL 1** to set the sweep up and down independently. For a one-shot, turn cycling off and send a gate to **TRIG 1**.

Bench measurements set the linear timing taper, trigger response, rest levels and end-gate levels. The control layout and slew, cycle and bus functions follow the hardware documentation. The response shapes and extreme timing ranges use the documented limits and model assumptions.

## Swatara Quantizer

Swatara turns a continuous voltage into notes. It can send a three-note chord or pass successive notes down a three-output shift register.

- **MODE** selects CHORD or SCALE and starts in CHORD. **TYPE** selects one of 32 types in each mode and starts at the first. **TYPE CV** offsets the selection, with 5 V covering the full span at full **TYPE CV AMT**. **TYPE CV AMT** starts at full. The display uses 0–F for the slot, a trailing dot for the second bank and a leading dot for SCALE.
- **IN** takes 0–5 V at 1 V/oct. **TRANSPOSE** adds 0–11 semitones and starts at 6 semitones. **TRANS** adds another 0–5 V of transposition in semitone steps. **OUT 1**, **OUT 2** and **OUT 3** are limited to 0–5 V.
- With **TRIG** empty, pitch updates continuously. In SCALE, **OUT 1** and **OUT 2** then carry the same note and **OUT 3** is 0 V. When **TRIG** is patched, a rising crossing of +1.5 V updates the outputs. In SCALE each trigger puts the new note on **OUT 1** and moves older notes to **OUT 2** and **OUT 3**.
- **HOLD** engages at +1.5 V. Without **TRIG**, it freezes the outputs and the display shows H. With **TRIG** patched in SCALE, each trigger rotates the three stored notes. With **TRIG** patched in CHORD, each trigger reads the current pitch and controls and rotates the chord tones across the jacks.

Patch a slow ramp to **IN**, a clock to **TRIG**, and select SCALE. Send the three outputs to three oscillators to hear the notes move between voices. Raise **HOLD** to repeat the last three notes.

Bench pitch readings checked the first chord and chromatic scale, transpose span and where notes appeared at the outputs. The mode and hold functions follow the hardware documentation. Some of the scale and chord intervals are inferred.
