# Tectonic Delay Module Implementation

## Overview

The Tectonic Delay is a multi-band textural synthesis delay engine that transforms delay effects from simple repeaters into powerful sound generators. It implements the Module Blueprint specifications using triode and matrix tube emulation instead of the originally proposed FractureTubeProcessor complexity.

## Architecture

### Signal Flow
```
Input ? 3-Band Crossover ? [Low/Mid/High Bands] ? Delay + Synthesis ? Sum ? Wet/Dry Mix ? Output
```

Each band contains:
- **Delay Line**: InterpolatedCircularBuffer with up to 4 seconds delay
- **Synthesis Engine**: FractureTubeProcessor with tube emulation
- **Independent Controls**: Delay time and synthesis parameters

### Key Components

#### 1. TectonicDelayProcessor
Main processor class implementing the multi-band architecture:
- **CrossoverNetwork**: 3-band Linkwitz-Riley crossover (4th order)
- **DelayBand[3]**: Low, Mid, High frequency processing chains
- **Parameters**: Per-band delay times, global synthesis controls

#### 2. FractureTubeProcessor
Hybrid tube decay synthesis engine with three layers:

**CrackleLayer** - Granular Processing:
- 32 concurrent grains with pitch shifting
- Triode saturation per grain
- Density and pitch controls

**HissLayer** - Noise Generation:
- Pink noise with tube-like filtering
- Triode saturation and spectral shaping
- Intensity control

**SustainLayer** - Micro-looping:
- 100ms micro-loops with matrix tube processing
- Multi-stage tube emulation (3 stages)
- Level-triggered recording

#### 3. Tube Emulation Models

**TriodeModel**:
- Asymmetric clipping characteristic
- Simple saturation with drive control
- Fast tanh approximation for efficiency

**MatrixTubeModel**:
- 3-stage cascaded tube processing
- Inter-stage filtering and DC blocking
- Progressive asymmetry per stage

## Parameters

| Parameter | Range | Description |
|-----------|-------|-------------|
| Low Time | 1-4000ms | Low band delay time |
| Mid Time | 1-4000ms | Mid band delay time |
| High Time | 1-4000ms | High band delay time |
| Feedback | 0-110% | Global feedback amount |
| Low/Mid X-Over | 50Hz-1kHz | Crossover frequency |
| Mid/High X-Over | 1kHz-10kHz | Crossover frequency |
| Decay Drive | 0-24dB | Synthesis engine drive |
| Decay Texture | 0-100% | Saturation vs synthesis mix |
| Decay Density | 0-100% | Granular density |
| Decay Pitch | ±24st | Pitch shift amount |
| Link | On/Off | Link synthesis parameters |
| Mix | 0-100% | Wet/dry balance |

## Implementation Features

### Performance Optimizations
- Smoothed parameter changes to avoid clicks
- Efficient grain management with circular indexing
- Fast math approximations (fastTanh, fastSinCycle)
- Minimal memory allocations during processing

### Audio Quality
- High-quality Linkwitz-Riley crossover for phase coherence
- Cubic interpolation in delay lines
- Proper feedback handling to prevent runaway
- Latency compensation between processing paths

### Integration
- Compatible with existing TESSERA parameter system
- Modular design allows easy modification
- Standard JUCE AudioProcessor interface
- Thread-safe parameter updates

## Usage

The Tectonic Delay excels at:
- **Textural delays**: Creating evolving, synthesized echoes
- **Rhythmic patterns**: Cross-band polyrhythms with different delay times
- **Ambient textures**: Dense, granular soundscapes
- **Creative effects**: Pitch-shifted delays with tube character

### Typical Settings

**Subtle Enhancement**:
- All times: 50-200ms
- Feedback: 20-40%
- Drive: 3-6dB
- Texture: 30-50%

**Rhythmic Polyrhythm**:
- Low: 250ms, Mid: 375ms, High: 125ms
- Feedback: 60-80%
- Density: 60-80%
- Different pitch per band

**Ambient Texture**:
- Long delays: 1-3 seconds
- High feedback: 80-100%
- High texture: 70-90%
- Dense granular processing

## Technical Notes

- Sample rate independent (tested 44.1kHz - 192kHz)
- Maximum delay time: 4 seconds per band
- Latency: ~64 samples (implementation dependent)
- CPU usage: Moderate (3 synthesis engines + crossover)

The implementation successfully creates a powerful multi-band delay with rich textural synthesis capabilities while maintaining efficient performance and audio quality.