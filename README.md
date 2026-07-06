# Whoosh

I built a tempo-synced volume LFO plugin with the JUCE framework in C++ inspired by Kickstart by Cableguys. It emulates the classic EDM sidechain "pumping" effect without a sidechain input by locking a draggable ducking curve to the host's tempo and restarting it every cycle.

## GUI:
<img width="717" height="400" alt="image" src="https://github.com/user-attachments/assets/dd0b3f16-0f9a-467d-b395-6877793c3017" />

## Controls:

| Control  | Description                                                                                          |
| -------- | ---------------------------------------------------------------------------------------------------- |
| Point A  | Draggable breakpoint at phase 0; sets the floor of the duck (drag down for deeper, up for shallower)  |
| Point B  | Draggable breakpoint at amplitude 1; sets how far into the cycle the volume fully recovers            |
| Rate     | Selects the sync division of one duck cycle: 1/1, 1/2, 1/4, 1/8, or 1/16                              |
| Curve    | Selects the recovery shape between the two points: Exponential, Logarithmic, or Linear                |

## Technical Details:

- Framework: JUCE
- Language: C++
- Curve engine: 1024-slot lookup table, regenerated on parameter change
- Curve read: linear interpolation
- Curve shapes: single power function (t^p) with p = 3 / 0.33 / 1
- Sync: host PPQ phase-locked, re-anchored every block, with a free-running fallback clock when the transport is stopped
