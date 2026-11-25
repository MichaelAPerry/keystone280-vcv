# Native tests

Run `make -C tests` with a C++17 compiler, GNU Make and Python 3. The DSP unit
tests exercise the engine directly. The robustness test also runs each plugin's
audio-side Rack module class against a small port, parameter and light shim.
The build extracts the classes from `src/` so the tests use the same parameter
ranges, connection flags, voltage reads, engine calls and output writes as Rack.

The robustness test processes ten seconds of audio at 44.1, 48 and 96 kHz. It
runs one instance, then eight independent instances interleaved on one thread.
Inputs get silence, independent uniform noise from −10 to +10 V, both DC rails
at ±12 V, square waves, subnormal samples and a silent tail. Each input gets
one sample each of NaN, +infinity and −infinity. The production wrapper maps
these to 0 V before the engine processes them. All output voltages must remain
finite during the injections and afterward. Parameters, including buttons and
switches, are randomized every 64 frames. The test checks output voltages and
light values, and limits average CPU time per instance to 5% of real time. Each one-second
window in the quiet tail, including the subnormal burst, has the same limit.
These budgets include input generation and output checks.

The ±12 V check applies to the test input range. Lackawanna's high-pass output
has a ±24 V limit in this test because a fast reversal from one DC rail to the
other can briefly leave nearly the full difference at the output. Direct copies
and switches also pass voltages above 12 V if fed a larger signal.

The shim covers audio processing, displays' audio-side state updates and cable
connection flags. Rack's scheduler, cable propagation, parameter smoothing,
widget rendering, context menus, and JSON persistence are outside this test.
The multichannel fixture provides a second input channel and checks that the
wrappers only write a single output channel. The Windows SDK build checks that
the wrappers compile and link against Rack.
