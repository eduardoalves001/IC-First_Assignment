## Implementation

The `wav_quant` program performs uniform scalar quantization on 16-bit PCM WAV audio files, reducing the effective number of bits per sample while maintaining the WAV file format. This tool is essential for studying quantization effects, simulating compression algorithms, and analyzing the trade-offs between bit depth and audio quality.


#### Key Components

1. **Quantization Function**

   ```cpp
   short quantizeUniform(short sample, int bits)
   ```

   - **Input**: 16-bit sample and target bit depth
   - **Process**:
     - Calculates quantization step size based on target bits
     - Maps the 16-bit range (-32768 to 32767) to quantization levels
     - Rounds to nearest quantization level
     - Maps back to 16-bit range
   - **Output**: Quantized 16-bit sample

2. **Command Line Interface**

   Supports:
   - `-v`: Verbose output with statistics
   - `-b bits`: Target bit depth (1-16)
   - Input and output WAV file paths

3. **File Processing**

   - Uses libsndfile for reading/writing WAV files
   - Processes audio in chunks (65,536 frames buffer)
   - Maintains original file format parameters

4. **Error Analysis**

   Calculates quality metrics:
   - MSE (Mean Squared Error)
   - RMSE (Root Mean Square Error)
   - Maximum absolute error
   - Approximate SNR in dB

### How Uniform Scalar Quantization Works

**Step Size Calculation:**

```
totalLevels = 2^bits
step = (32767 - (-32768)) / (totalLevels - 1)
```

**Quantization Process:**

1. Forward mapping: `level = round((sample - minValue) / step)`
2. Clamping: Ensure level stays within valid range
3. Reverse mapping: `quantized = minValue + level × step`


## Basic Usage
```bash
# 8-bit uniform quantization (default)
./wav_quant input.wav output.wav

# 4-bit uniform quantization  
./wav_quant -b 4 input.wav output.wav

# Verbose 12-bit quantization
./wav_quant -v -b 12 input.wav output.wav
```

## Results

The following histograms were generated using wav_hist and Gnuplot to illustrate the distribution of sample values in a quantized audio file at different bit depths:

![Figure 1](8.png)
*Figure 1: Gnuplot histogram of a audio file processed with 8-bit uniform scalar quantization*

![Figure 2](4.png)
*Figure 2: Gnuplot histogram of a audio file processed with 4-bit uniform scalar quantization*

![Figure 3](2.png)
*Figure 2: Gnuplot histogram of a audio file processed with 2-bit uniform scalar quantization*

The histograms confirm that quantization has been performed correctly, as the number of discrete amplitude levels corresponds to 2ⁿ for each bit depth. Specifically, Figure 1 shows 256 levels (2⁸), Figure 2 shows 16 levels (2⁴), and Figure 3 shows 4 levels (2²).