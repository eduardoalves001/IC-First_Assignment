//------------------------------------------------------------------------------
//
// Copyright 2025 University of Aveiro, Portugal, All Rights Reserved.
//
// These programs are supplied free of charge for research purposes only,
// and may not be sold or incorporated into any commercial product. There is
// ABSOLUTELY NO WARRANTY of any sort, nor any undertaking that they are
// fit for ANY PURPOSE WHATSOEVER. Use them at your own risk. If you do
// happen to find a bug, or have modifications to suggest, please report
// the same to Armando J. Pinho, ap@ua.pt. The copyright notice above
// and this statement of conditions must remain an integral part of each
// and every copy made of these files.
//
// Armando J. Pinho (ap@ua.pt)
// IEETA / DETI / University of Aveiro
//
#include <iostream>
#include <vector>
#include <cmath>
#include <sndfile.hh>

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // Buffer for reading/writing frames

// Perform uniform scalar quantization
short quantizeUniform(short sample, int bits) {
    if (bits >= 16) {
        return sample; // No quantization needed
    }
    
    if (bits <= 0) {
        return 0; // Complete quantization to zero
    }
    
    // Calculate quantization step
    int totalLevels = 1 << bits; // 2^bits quantization levels
    int maxValue = (1 << 15) - 1; // 32767 for 16-bit
    int minValue = -(1 << 15);    // -32768 for 16-bit
    
    // Map from 16-bit range to quantization levels
    double step = (double)(maxValue - minValue) / (totalLevels - 1);
    
    // Quantize: map sample to quantization level and back
    int level = (int)round((double)(sample - minValue) / step);
    
    // Clamp level to valid range
    if (level < 0) level = 0;
    if (level >= totalLevels) level = totalLevels - 1;
    
    // Map back to 16-bit range
    short quantized = (short)(minValue + level * step);
    
    return quantized;
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    int targetBits = 8;
    
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " [-v (verbose)] [-b bits] input.wav output.wav\n";
        cerr << "  -v: verbose output\n";
        cerr << "  -b bits: target bits per sample (1-16, default: 8)\n";
        cerr << "Examples:\n";
        cerr << "  " << argv[0] << " input.wav output.wav          # 8-bit uniform quantization\n";
        cerr << "  " << argv[0] << " -b 4 input.wav output.wav     # 4-bit uniform quantization\n";
        return 1;
    }
    
    string inputFile, outputFile;
    
    // Parse command line arguments
    for (int n = 1; n < argc; n++) {
        if (string(argv[n]) == "-v") {
            verbose = true;
        }
        else if (string(argv[n]) == "-b" && n + 1 < argc) {
            targetBits = stoi(argv[n + 1]);
            n++; // Skip next argument
        }
        else if (inputFile.empty()) {
            inputFile = argv[n];
        }
        else if (outputFile.empty()) {
            outputFile = argv[n];
        }
    }
    
    if (inputFile.empty() || outputFile.empty()) {
        cerr << "Error: input and output files must be specified\n";
        return 1;
    }
    
    if (targetBits < 1 || targetBits > 16) {
        cerr << "Error: target bits must be between 1 and 16\n";
        return 1;
    }
    
    // Open input file
    SndfileHandle sfhIn { inputFile };
    if (sfhIn.error()) {
        cerr << "Error: cannot open input file '" << inputFile << "'\n";
        return 1;
    }
    
    if ((sfhIn.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) {
        cerr << "Error: input file is not in WAV format\n";
        return 1;
    }
    
    if ((sfhIn.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        cerr << "Error: input file is not in 16-bit PCM format\n";
        return 1;
    }
    
    // Create output file with same parameters as input
    SndfileHandle sfhOut { outputFile, SFM_WRITE, sfhIn.format(), sfhIn.channels(), sfhIn.samplerate() };
    if (sfhOut.error()) {
        cerr << "Error: cannot create output file '" << outputFile << "'\n";
        return 1;
    }
    
    if (verbose) {
        cout << "Input file: " << inputFile << "\n";
        cout << "Output file: " << outputFile << "\n";
        cout << "Channels: " << sfhIn.channels() << "\n";
        cout << "Sample rate: " << sfhIn.samplerate() << " Hz\n";
        cout << "Frames: " << sfhIn.frames() << "\n";
        cout << "Duration: " << (double)sfhIn.frames() / sfhIn.samplerate() << " seconds\n";
        cout << "Target bits: " << targetBits << " (from 16-bit)\n";
        cout << "Quantization method: Uniform Scalar"  << "\n";
        cout << "Quantization levels: " << (1 << targetBits) << "\n";
        cout << "Theoretical compression ratio: " << (16.0 / targetBits) << ":1\n";
    }
    
    // Process audio data
    size_t nFrames;
    vector<short> samples(FRAMES_BUFFER_SIZE * sfhIn.channels());
    size_t totalFrames = 0;
    size_t totalSamples = 0;
    
    // Statistics for analysis
    long long sumSquaredError = 0;
    long long maxSquaredError = 0;
    
    while ((nFrames = sfhIn.readf(samples.data(), FRAMES_BUFFER_SIZE))) {
        samples.resize(nFrames * sfhIn.channels());
        
        // Quantize each sample
        for (size_t i = 0; i < samples.size(); i++) {
            short original = samples[i];
            short quantized;
            
            quantized = quantizeUniform(original, targetBits);

            samples[i] = quantized;
            
            // Calculate error statistics
            long long error = (long long)(original - quantized);
            long long squaredError = error * error;
            sumSquaredError += squaredError;
            if (squaredError > maxSquaredError) {
                maxSquaredError = squaredError;
            }
            totalSamples++;
        }
        
        // Write quantized samples to output file
        if (sfhOut.writef(samples.data(), nFrames) != nFrames) {
            cerr << "Error: failed to write to output file\n";
            return 1;
        }
        
        totalFrames += nFrames;
    }
    
    if (verbose) {
        cout << "\nProcessing completed:\n";
        cout << "Total frames processed: " << totalFrames << "\n";
        cout << "Total samples processed: " << totalSamples << "\n";
        
        // Calculate and display error metrics
        double mse = (double)sumSquaredError / totalSamples;
        double rmse = sqrt(mse);
        double maxError = sqrt((double)maxSquaredError);
        
        cout << "\nQuantization Error Analysis:\n";
        cout << "Mean Squared Error (MSE): " << mse << "\n";
        cout << "Root Mean Square Error (RMSE): " << rmse << "\n";
        cout << "Maximum absolute error: " << maxError << "\n";
        
        // Calculate Signal-to-Noise Ratio (approximation)
        double maxSignal = 32767.0; // Maximum 16-bit value
        double snr_db = 20.0 * log10(maxSignal / rmse);
        cout << "Approximate SNR: " << snr_db << " dB\n";
        
        cout << "\nFile sizes:\n";
        cout << "Both files have the same size (16-bit PCM output)\n";
        cout << "Actual compression would require different encoding\n";
    }
    
    return 0;
}