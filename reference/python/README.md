# Phase A exact reconstruction experiment

Read Phase_A_protocol.md first. No camera recovery or application code is included.

Requires Python 3.10+ and NumPy (tested with the version recorded in phase_a_results.json).

Run the main correctness suite:

    python phase_a.py --output results --all-a0 --all-a1 --all-a2 --samples 128
    python adversarial_fixtures.py

Fast smoke run (world enumeration remains complete):

    python phase_a.py --output smoke --samples 4

Generic A1 search comparison:

    python phase_a.py --output baseline --a1-mode search --samples 128

Resolution control:

    python phase_a.py --output resolution16 --resolution 16 --a1-mode search --samples 32

The supplied resolution control predates the separate A2 palette and contains A0/A1/Amono.
The main supplied suite includes all four palettes.

CSV root_domains are bitsets: state s is present exactly when mask & (1 << s) is nonzero.
State names/order are recorded in each phase's JSON palette. Emission color is separately
recorded: in particular, a slab does not have a special geometry-identifying pixel color.

results/cases.csv contains main per-case results; results/results.json contains its
summary. Baseline and resolution summaries are also included. All timings distinguish
unaudited inference from the oracle/audit. Peak RSS includes the complete benchmark.

The oracle only enters inference through a read-only audit callback. Timing runs disable
that callback. No timeout or unsuccessful heuristic is interpreted as a proof.
