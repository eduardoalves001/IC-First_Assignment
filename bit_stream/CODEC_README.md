# WAV Quantization Codec - Quick Start Guide

## Overview

A pair of audio compression tools that use uniform scalar quantization with efficient bit-packing:
- **`wav_quant_enc`**: Compress WAV files by reducing bit depth and packing samples
- **`wav_quant_dec`**: Decompress packed files back to WAV format

## Quick Start

### 1. Build the Tools
```bash
cd src/build
cmake ..
make
```

Executables will be in `bin/`:
- `bin/wav_quant_enc`
- `bin/wav_quant_dec`

### 2. Basic Usage

**Encode (compress):**
```bash
./bin/wav_quant_enc -b 8 input.wav output.bin
```

**Decode (decompress):**
```bash
./bin/wav_quant_dec output.bin recovered.wav
```

### 3. Common Examples

**High quality, moderate compression (12-bit):**
```bash
./bin/wav_quant_enc -b 12 music.wav music_hq.bin      # 25% smaller
./bin/wav_quant_dec music_hq.bin music_hq.wav
```

**Good quality, better compression (8-bit):**
```bash
./bin/wav_quant_enc -b 8 podcast.wav podcast.bin      # 50% smaller
./bin/wav_quant_dec podcast.bin podcast.wav
```

**Maximum compression (4-bit):**
```bash
./bin/wav_quant_enc -b 4 speech.wav speech.bin        # 75% smaller
./bin/wav_quant_dec speech.bin speech.wav
```

### 4. Verify Quality
```bash
./bin/wav_cmp input.wav recovered.wav
```

## Command Line Options

### Encoder
```
wav_quant_enc [-v] [-b bits] input.wav output.bin

Options:
  -v         Verbose output with statistics
  -b bits    Quantization bits (1-16, default: 8)
```

### Decoder
```
wav_quant_dec [-v] input.bin output.wav

Options:
  -v         Verbose output with progress
```

## Compression vs Quality

| Bits | Compression | Quality | Use Case |
|------|-------------|---------|----------|
| 12   | 1.33:1      | Excellent (74 dB SNR) | High-quality music |
| 10   | 1.6:1       | Very good (62 dB SNR) | General music |
| 8    | 2.0:1       | Good (50 dB SNR) | Podcasts, general audio |
| 6    | 2.67:1      | Fair (38 dB SNR) | Voice, simple audio |
| 4    | 4.0:1       | Poor (26 dB SNR) | Highly compressed voice |

## File Format

**Input:** 16-bit PCM WAV files (mono or stereo)  
**Output:** Custom binary format with bit-packed samples

## Requirements

- libsndfile (for WAV I/O)
- C++20 compiler
- CMake 3.16+

## Documentation

For detailed information, see:
- **WAV_QUANT_CODEC_DOCUMENTATION.md** - Full technical documentation
- **CODEC_IMPLEMENTATION_SUMMARY.md** - Implementation details and test results

## Example Workflow

```bash
# 1. Encode with verbose output
./bin/wav_quant_enc -v -b 8 original.wav compressed.bin

# Output:
# Channels: 2
# Sample rate: 48000 Hz
# Frames: 48000 (1 seconds)
# Quantization bits: 8
# Compression ratio: 2:1

# 2. Check file size
ls -lh compressed.bin

# 3. Decode
./bin/wav_quant_dec compressed.bin recovered.wav

# 4. Verify quality
./bin/wav_cmp original.wav recovered.wav

# Output:
# Overall MSE: 5908.34
# Overall Maximum Error: 128.00
# Quality: Good quality (MSE < 10000.0)
```

## Tips

1. **Start with 8 bits** for general use (2:1 compression, good quality)
2. **Use 12 bits** for archival or music (excellent quality, 25% savings)
3. **Try 6 bits** for voice-only content (acceptable quality, 62% savings)
4. **Always verify** quality with wav_cmp after encoding
5. **Keep originals** as backup for critical content

## Troubleshooting

**"Error: input file is not in WAV format"**
- Input must be WAV format (not MP3, FLAC, etc.)
- Use conversion tools if needed

**"Error: input file is not in 16-bit PCM format"**
- Only 16-bit PCM WAV supported
- Convert 24-bit or float WAV to 16-bit first

**"Error: bits must be between 1 and 16"**
- Check -b parameter value
- Valid range: 1-16

## License

Copyright 2025 University of Aveiro, Portugal, All Rights Reserved.
Supplied free of charge for research purposes only.

## Authors

Implementation by Bruno Marujo for IC course, MECT, University of Aveiro
Based on BitStream library by Armando J. Pinho (ap@ua.pt)
