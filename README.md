# Whoosh - LFO Volume Shaper

> work in progress

A host-synced LFO plugin for rhythmic volume shaping and sidechain-style ducking. Inspired by plugins like Kickstart and LFOTool.

## Features

- **Host-synced timing** — the LFO locks to your DAW's tempo so the shape stays in time at any BPM.
- **Draggable curve editor** — a two-breakpoint shaper lets you design the volume curve (exponential vs. logarithmic vs. linear)
- **Efficient real-time playback** — 1024-sample lookup table that regenerates when the shape changes

## UI prototype: 
<img width="720" height="400" alt="image" src="https://github.com/user-attachments/assets/d6894eac-6c8b-4c0b-a69c-7ba97a3005fb" />

## Built With

- C++
- JUCE framework
