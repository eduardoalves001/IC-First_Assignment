# WAV Comparison Tool (wav_cmp) - Implementation Documentation

## Overview

The `wav_cmp` program performs comprehensive error analysis between two WAV audio files, calculating various error metrics that are essential for audio quality assessment. This tool is particularly valuable for evaluating the quality degradation caused by compression, quantization, or other audio processing operations.

## Features

### Comprehensive Error Metrics

**1. Mean Squared Error (MSE) - L2² Norm**
- Calculates average squared difference between corresponding samples
- Provides measure of overall error magnitude
- Formula: `MSE = (1/N) * Σ(original[i] - processed[i])²`

**2. Maximum Absolute Error - L∞ Norm**
- Largest single-sample absolute difference
- Indicates worst-case error scenario
- Formula: `L∞ = max(|original[i] - processed[i]|)`

**3. Signal-to-Noise Ratio (SNR)**
- Ratio of signal power to noise (error) power in dB
- Standard measure of audio quality preservation
- Formula: `SNR = 10 * log₁₀(signal_power / noise_power)`

### Analysis Scope

- **Individual Channels**: Separate analysis for each audio channel
- **Channel Average**: Analysis of (L+R)/2 for stereo files
- **Overall Summary**: Combined statistics across all channels
- **Quality Assessment**: Automatic quality rating based on RMSE thresholds

## Technical Implementation

### Error Metric Calculations

```cpp
class WAVComparator {
    struct ErrorMetrics {
        double mse;           // Mean Squared Error
        double maxAbsError;   // Maximum absolute error
        double snr_db;        // Signal-to-Noise Ratio in dB
        size_t numSamples;    // Number of samples processed
    };
    
    void processFrame(const vector<short>& original, const vector<short>& processed) {
        for (size_t i = 0; i < original.size(); i++) {
            double error = static_cast<double>(original[i] - processed[i]);
            
            // Accumulate MSE
            mse += error * error;
            
            // Track maximum absolute error
            if (abs(error) > maxAbsError) {
                maxAbsError = abs(error);
            }
            
            // Accumulate signal power for SNR
            signalPower += original[i] * original[i];
        }
    }
};
```

### SNR Calculation Method

The SNR calculation uses the power-based formula:
```cpp
double avgSignalPower = signalPower / numSamples;
double noisePower = mse;  // MSE represents noise power
snr_db = 10.0 * log10(avgSignalPower / noisePower);
```

## Usage Examples

### Basic Usage
```bash
# Compare original with quantized version
./wav_cmp original.wav quantized.wav

# Compare with verbose output
./wav_cmp -v reference.wav compressed.wav

# Check if files are identical
./wav_cmp file1.wav file2.wav
```

### Integration with Other Tools
```bash
# Complete quality assessment workflow
./wav_quant -b 8 original.wav quantized.wav     # Apply quantization
./wav_cmp original.wav quantized.wav            # Analyze quality loss
./wav_hist quantized.wav 0 | wc -l              # Verify quantization levels
```

## Experimental Results Analysis

### Test Results (1-second stereo sine wave, 48kHz)

#### 8-bit Uniform Quantization Comparison
```
Channel 0 (Left):   SNR: 49.96 dB, MSE: 5416.64, Max Error: 128
Channel 1 (Right):  SNR: 49.80 dB, MSE: 5616.04, Max Error: 128  
Average (L+R)/2:    SNR: 49.87 dB, MSE: 2768.66, Max Error: 128
Overall Quality:    Good (MSE < 10000.0)
```

#### 4-bit Uniform Quantization Comparison  
```
Channel 0 (Left):   SNR: 25.68 dB, MSE: 1452651.68, Max Error: 2184
Channel 1 (Right):  SNR: 25.68 dB, MSE: 1452627.25, Max Error: 2184
Average (L+R)/2:    SNR: 25.67 dB, MSE: 728307.69,  Max Error: 2184
Overall Quality:    Poor (MSE >= 1000000.0)
```

#### Quantization Method Comparison (8-bit Uniform vs Bit-Shift)
```
Method Difference Analysis:
Channel 0: MSE: 29487.76, Max Error: 382 (uniform is better)
Channel 1: MSE: 29039.22, Max Error: 380 (uniform is better)
Quality: Good (demonstrates method differences)
```

### Key Observations

1. **SNR Validation**: Measured SNR values closely match theoretical predictions
2. **Channel Consistency**: Similar error patterns across stereo channels for symmetric signals
3. **Method Differences**: Uniform quantization consistently outperforms bit-shift method
4. **Error Scaling**: MSE increases quadratically with quantization step size
5. **Average Benefits**: Channel averaging reduces error variance in MSE calculations

## Error Metric Interpretation

### SNR Quality Thresholds (Approximate)
- **>60 dB**: Excellent quality (transparent)
- **48-60 dB**: Very good quality (high-end audio)
- **40-48 dB**: Good quality (broadcast quality)
- **30-40 dB**: Fair quality (noticeable but acceptable)
- **<30 dB**: Poor quality (significant degradation)

### MSE Quality Assessment
- **MSE < 1.0**: Excellent quality
- **MSE < 100.0**: Very good quality  
- **MSE < 10000.0**: Good quality
- **MSE < 1000000.0**: Fair quality
- **MSE ≥ 1000000.0**: Poor quality

### Maximum Error Significance
- **Max Error = 0**: Perfect match (identical files)
- **Max Error < quantization_step**: Expected quantization bounds
- **Max Error >> √MSE**: Isolated artifacts or clipping
- **Max Error ≈ √MSE**: Uniform error distribution

## Integration with Audio Processing Pipeline

### Quality Assessment Workflow

```bash
# 1. Baseline analysis
./wav_hist original.wav 0 > original_hist.txt

# 2. Apply processing (quantization example)
./wav_quant -v -b 6 original.wav processed.wav

# 3. Comprehensive comparison
./wav_cmp -v original.wav processed.wav

# 4. Verify processing effects
./wav_hist processed.wav 0 > processed_hist.txt
diff original_hist.txt processed_hist.txt
```

### Batch Quality Testing

```bash
#!/bin/bash
# Test multiple bit depths
for bits in 8 6 4 3 2; do
    echo "=== ${bits}-bit Quantization ==="
    ./wav_quant -b $bits original.wav temp_${bits}bit.wav
    ./wav_cmp original.wav temp_${bits}bit.wav | grep "SNR\|MSE"
    echo ""
done
```

## File Format Requirements

### Input Specifications
- **Format**: WAV (RIFF/WAVE container)
- **Encoding**: 16-bit signed integer PCM
- **Channels**: Any (mono, stereo, multi-channel)
- **Sample Rate**: Any supported by libsndfile
- **Compatibility**: Both files must have identical format parameters

### Validation and Error Handling
- **Format Verification**: Ensures both files are WAV format
- **Parameter Matching**: Validates channels, sample rate, bit depth
- **Length Handling**: Processes shorter file length if files differ
- **Error Reporting**: Comprehensive error messages for format issues

## Mathematical Foundations

### Error Norms

**L2² Norm (Mean Squared Error)**:
```
||x - y||₂² = (1/N) * Σᵢ(xᵢ - yᵢ)²
```
- Represents mean squared error between signals
- Penalizes large errors more than small ones
- Foundation for signal quality assessment

**L∞ Norm (Maximum Norm)**:
```
||x - y||∞ = max|xᵢ - yᵢ|
```
- Represents worst-case error scenario
- Important for clipping and artifact detection
- Critical for dynamic range assessment

### Signal-to-Noise Ratio

**Power-based SNR**:
```
SNR = 10 * log₁₀(Psignal / Pnoise)
```
Where:
- `Psignal = (1/N) * Σᵢ(original[i])²`
- `Pnoise = MSE = (1/N) * Σᵢ(original[i] - processed[i])²`

## Applications and Use Cases

### Audio Codec Development
- **Quality Benchmarking**: Compare different compression algorithms
- **Parameter Optimization**: Fine-tune codec settings for quality/bitrate trade-offs
- **Artifact Analysis**: Identify specific types of processing artifacts
- **Regression Testing**: Ensure codec modifications don't degrade quality

### Academic Research
- **Perceptual Studies**: Correlate objective metrics with subjective quality
- **Algorithm Evaluation**: Compare novel processing methods
- **Quantization Research**: Study effects of different quantization strategies
- **Error Analysis**: Understand error distribution patterns

### Quality Assurance
- **Production Validation**: Verify audio processing pipeline quality
- **Format Conversion**: Assess quality loss in format changes
- **Real-time Processing**: Monitor live audio processing quality
- **Archive Verification**: Validate audio restoration and digitization

### Educational Applications
- **Signal Processing Courses**: Demonstrate quantization and error concepts
- **Audio Engineering**: Show practical effects of processing parameters
- **Quality Metrics**: Teach objective audio quality assessment
- **Algorithm Comparison**: Compare different processing approaches

## Performance Characteristics

### Processing Efficiency
- **Memory Usage**: Constant memory regardless of file size
- **Processing Speed**: Real-time for typical audio files
- **Scalability**: Linear time complexity with file length
- **Precision**: Double-precision floating-point calculations

### Accuracy Validation
- **Theoretical Consistency**: Results match expected quantization theory
- **Numerical Stability**: Robust handling of edge cases
- **Range Handling**: Proper handling of full 16-bit range
- **Precision Limits**: Appropriate for 16-bit PCM analysis

## Limitations and Future Enhancements

### Current Limitations
- **16-bit PCM Only**: Limited to 16-bit signed integer WAV files
- **Identical Format Required**: Both files must have same parameters
- **Basic Metrics**: Limited to fundamental error measures

### Potential Enhancements
- **Extended Format Support**: 24-bit, 32-bit, and floating-point audio
- **Perceptual Metrics**: PESQ, STOI, and other perceptual quality measures
- **Frequency Analysis**: Spectral error analysis and frequency-dependent metrics
- **Statistical Analysis**: Histogram of errors, distribution analysis
- **Batch Processing**: Automated testing of multiple file pairs
- **Output Formats**: CSV, JSON, and XML output for automated analysis

This implementation provides a solid foundation for audio quality assessment and serves as an essential tool for audio processing research, development, and quality assurance applications.