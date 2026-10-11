# Tail engine and control contract

The native C++17 core copies a ready Galaxy at init, preallocates buffers and runs
4 or 8 live voices. Every voice has independent stereo history, epoch identity,
filter state, input ramp and output attenuation. Room identity is separate from
voice identity. Retired history is ignored through epoch stamps, so replacement
does not clear thousands of buffer samples inside an audio callback.

On every jump all departing voices age, including a same-room source. Absolute
targets relative to the voice's original unity reference are age 1 −6 dB,
ages 2–6 linearly spaced in dB, age 7+ −80 dB. These targets multiply natural
decay at output; they are not compounded decrements. Ramps start from current
gain. An old tail never gets the destination's fresh unity reference.

Default departure ramp is 5 ms, arrival delay 10 ms, arrival fade 5 ms and tail
target ramp 20 ms. Arrival delay is separate from room reflection distance.
At capacity, select the smallest tracked peak-level × current gain, fade it to
zero for the departure period, then change epoch and activate the destination.
Its arrival delay starts after that fade. Input during an arrival gap is not
buffered for later replay: dry is uninterrupted, but the first wet onset can be
lost. This is an explicit prototype tradeoff, not onset-preserving capture.

A jump during pending eviction retargets the pending room and retains the existing
fade countdown. A jump during an input/tail ramp retargets from the current value.
Cancelled arrivals never restore their input gain. This avoids growing the engine
count or repeatedly postponing fade completion during rapid leaps.

Reflection spacing determines the fixed feedback tap (right channel 1.037×).
Per-room Spread narrows each voice's wet output using mid/side mixing after its
filters and before summing contributions; feedback and dry stereo are unchanged.
Spread 0 centers the wet reflection, Spread 1 preserves the original stereo
samples, and departing tails retain their own room's width. See
[STEREO.md](STEREO.md) for mapping, migration and mono measurements.
Global Decay maps to feedback from an RT60 formula, with a .98 safety cap; damping
and tanh saturation shorten actual decay, so this is RT60 intent, not a calibrated
measurement. Feedback stays unpitched. Output uses two overlapping 80 ms delay
grains for fractional pitch and reverse reading. Normal speed bypasses that grain
reader. Grain spacing adds 0–80 ms latency on top of room spacing; pitch shifts
produce grain sidebands. Half speed shifts down roughly an octave but does not
double the entire decay duration. Reverse grains differ from reversing a complete
captured tail or producing a defined rising reverse-swell envelope.

Maximum read delay is below the allocated ring: 200 ms for neutral scenes, 320 ms when pre-delay is used. Fractional reads interpolate
linearly, with ring wrapping calculated in double to keep tiny negative offsets
from rounding to one past the ring. Complementary raised-cosine grain weights sum
to one and have zero slope at the read wraps. Overlapping reads can still cancel
and reduce RMS. Each channel tracks
the two read energies and their cross-product with a 20 ms exponential average.
For weight w, target energy is w·EA+(1−w)·EB and overlap energy is
w²·EA+(1−w)²·EB+2w(1−w)·cross. The square-root ratio is floored for silence, clamped
to 1–3 and smoothed over 1 ms. This compensation applies to pitched/reverse grain
output before voice-age attenuation and final wet saturation. Normal 1× forward
reading bypasses it. It is local overlap
compensation, not whole-output loudness matching or unbounded gain control.

Every room also has wet-only High cut (800–16,000 Hz) and Low cut (10–1,000 Hz).
Two-pole Butterworth filters soften the grain output and remove low-frequency
rumble. High cut also filters the bounded input/feedback written into history,
with its frequency divided by max(1, speed), reducing bandwidth before accelerated
reads. Output High cut and Low cut are applied after grain normalization. The
cutoffs are capped at 45% of sample rate. Coefficients are prepared at init;
each visit has independent double-precision filter state, reset on epoch activation.
Low cut must be below High cut. These gentle filters reduce high-frequency folding;
they are not a bandlimited resampler and do not remove every grain sideband.
The Studio default mapping uses High cut 2,500–8,000 Hz and Low cut 20–40 Hz.
Older four-field C++ descriptors receive 6,000/20 Hz defaults. Filters affect
actual decay as well as tone. Dry never passes through them.

Per-voice tanh
input/feedback protection and bounded final wet summing provide internal wet
headroom. Dry remains exact and additive, so output is not brickwall-limited;
a full-scale dry signal plus wet can exceed ±1. The host must preserve headroom.
Mix is wet-send amount, not an equal-power dry/wet crossfade; there is no output
volume control.

Sensitivity maps threshold 0.5→0.01. Dry-only peak follower has 1 ms attack and
25 ms release. Rearm requires 12 ms below half threshold; cooldown is 35 ms.
Held notes cannot retrigger until rearmed. Sensitivity and knob values are clamped;
Mix and Sensitivity move with 10 ms time constants; Decay uses 20 ms. Mix snaps to
its target within 1e-7 so settled Mix=0 leaves exact dry, including silent samples.
Auto/manual
triggering and Next/Leap traversal are independently selectable. Live synthetic
control sweeps are tested; per-note glitch behavior across real instruments still
needs listening.

Rates 8–96 kHz are accepted; broad tests use 44.1, 48 and 96 kHz, and filter corner
tests also use 8 kHz. All times/filter
coefficients derive from rate; frame length is supplied by the host. Controls and
snapshot initialization are not threadsafe and must be staged by host adapters.
Memory is fixed after init, process work is O(frames×budget), and there are no
locks, I/O, Python calls or callback allocation. A live immutable-snapshot handoff,
The WASM adapter reuses this core. Caller-provided embedded memory remains future work.
The local native host stages controls with a bounded queue; see [NATIVE.md](NATIVE.md).

## Room character extension

Scene v4 adds diffusion, early reflections, pre-delay, bloom, modulation, ducking,
pan, shimmer, drive and hold. Their neutral settings preserve this original signal
path; fresh scenes use restrained mappings. See [ROOM-CHARACTER.md](ROOM-CHARACTER.md)
for ranges, exact placement, feedback safeguards and migration.
