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
#include <iomanip>
#include <limits>
#include <sndfile.hh>

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // Buffer for reading frames

struct ErrorMetrics {
    double mse;           // Mean Squared Error (L2 norm squared)
    double maxAbsError;   // Maximum absolute error (L∞ norm)
    double snr_db;        // Signal-to-Noise Ratio in dB
    size_t numSamples;    // Number of samples processed
    
    ErrorMetrics() : mse(0.0), maxAbsError(0.0), snr_db(0.0), numSamples(0) {}
};

class WAVComparator {
private:
    vector<ErrorMetrics> channelMetrics;
    ErrorMetrics averageMetrics;
    int numChannels;
    
    // Statistics for SNR calculation
    vector<double> channelSignalPower;
    double averageSignalPower;
    
public:
    WAVComparator(int channels) : numChannels(channels) {
        channelMetrics.resize(channels);
        channelSignalPower.resize(channels, 0.0);
        averageSignalPower = 0.0;
    }
    
    void processFrame(const vector<short>& original, const vector<short>& processed) {
        if (original.size() != processed.size()) {
            throw runtime_error("Frame sizes don't match");
        }
        
        // Process each channel
        for (size_t i = 0; i < original.size(); i++) {
            int channel = i % numChannels;
            
            short orig = original[i];
            short proc = processed[i];
            double error = static_cast<double>(orig - proc);
            double absError = abs(error);
            
            // Update channel metrics
            channelMetrics[channel].mse += error * error;
            channelMetrics[channel].numSamples++;
            
            if (absError > channelMetrics[channel].maxAbsError) {
                channelMetrics[channel].maxAbsError = absError;
            }
            
            // Signal power for SNR calculation (using original signal)
            double originalSample = static_cast<double>(orig);
            channelSignalPower[channel] += originalSample * originalSample;
        }
        
        // Process average of channels (if stereo)
        if (numChannels == 2 && original.size() >= 2) {
            for (size_t i = 0; i < original.size(); i += 2) {
                if (i + 1 < original.size()) {
                    // Calculate average of left and right channels
                    double origAvg = (static_cast<double>(original[i]) + static_cast<double>(original[i + 1])) / 2.0;
                    double procAvg = (static_cast<double>(processed[i]) + static_cast<double>(processed[i + 1])) / 2.0;
                    
                    double error = origAvg - procAvg;
                    double absError = abs(error);
                    
                    // Update average metrics
                    averageMetrics.mse += error * error;
                    averageMetrics.numSamples++;
                    
                    if (absError > averageMetrics.maxAbsError) {
                        averageMetrics.maxAbsError = absError;
                    }
                    
                    // Signal power for average
                    averageSignalPower += origAvg * origAvg;
                }
            }
        }
    }
    
    void finalize() {
        // Finalize channel metrics
        for (int ch = 0; ch < numChannels; ch++) {
            if (channelMetrics[ch].numSamples > 0) {
                channelMetrics[ch].mse /= channelMetrics[ch].numSamples;
                
                // Calculate SNR
                double avgSignalPower = channelSignalPower[ch] / channelMetrics[ch].numSamples;
                double noisePower = channelMetrics[ch].mse;
                
                if (noisePower > 0.0) {
                    channelMetrics[ch].snr_db = 10.0 * log10(avgSignalPower / noisePower);
                } else {
                    channelMetrics[ch].snr_db = numeric_limits<double>::infinity();
                }
            }
        }
        
        // Finalize average metrics (for stereo)
        if (numChannels == 2 && averageMetrics.numSamples > 0) {
            averageMetrics.mse /= averageMetrics.numSamples;
            
            // Calculate SNR for average
            double avgSignalPower = averageSignalPower / averageMetrics.numSamples;
            double noisePower = averageMetrics.mse;
            
            if (noisePower > 0.0) {
                averageMetrics.snr_db = 10.0 * log10(avgSignalPower / noisePower);
            } else {
                averageMetrics.snr_db = numeric_limits<double>::infinity();
            }
        }
    }
    
    void printResults(const string& originalFile, const string& processedFile) const {
        cout << "=== WAV Comparison Results ===\n";
        cout << "Original file: " << originalFile << "\n";
        cout << "Processed file: " << processedFile << "\n";
        cout << "Channels: " << numChannels << "\n\n";
        
        cout << fixed << setprecision(6);
        
        // Print individual channel results
        for (int ch = 0; ch < numChannels; ch++) {
            cout << "Channel " << ch;
            if (numChannels == 2) {
                cout << (ch == 0 ? " (Left)" : " (Right)");
            }
            cout << ":\n";
            
            cout << "  Mean Squared Error (L2²): " << channelMetrics[ch].mse << "\n";
            cout << "  Maximum Absolute Error (L∞): " << channelMetrics[ch].maxAbsError << "\n";
            
            if (isfinite(channelMetrics[ch].snr_db)) {
                cout << "  Signal-to-Noise Ratio: " << setprecision(2) << channelMetrics[ch].snr_db << " dB\n";
            } else {
                cout << "  Signal-to-Noise Ratio: ∞ dB (identical signals)\n";
            }
            
            cout << "  Samples processed: " << channelMetrics[ch].numSamples << "\n\n";
        }
        
        // Print average results (for stereo)
        if (numChannels == 2 && averageMetrics.numSamples > 0) {
            cout << "Average of Channels (L+R)/2:\n";
            cout << "  Mean Squared Error (L2²): " << averageMetrics.mse << "\n";
            cout << "  Maximum Absolute Error (L∞): " << averageMetrics.maxAbsError << "\n";
            
            if (isfinite(averageMetrics.snr_db)) {
                cout << "  Signal-to-Noise Ratio: " << setprecision(2) << averageMetrics.snr_db << " dB\n";
            } else {
                cout << "  Signal-to-Noise Ratio: ∞ dB (identical signals)\n";
            }
            
            cout << "  Samples processed: " << averageMetrics.numSamples << "\n\n";
        }
        
        // Summary statistics
        cout << "=== Summary ===\n";
        double overallMSE = 0.0;
        double overallMaxError = 0.0;
        
        for (int ch = 0; ch < numChannels; ch++) {
            overallMSE += channelMetrics[ch].mse;
            if (channelMetrics[ch].maxAbsError > overallMaxError) {
                overallMaxError = channelMetrics[ch].maxAbsError;
            }
        }
        overallMSE /= numChannels;
        
        cout << "Overall MSE (average across channels): " << setprecision(6) << overallMSE << "\n";
        cout << "Overall Maximum Error: " << overallMaxError << "\n";
        
        // Provide quality assessment
        cout << "\n=== Quality Assessment ===\n";
        if (overallMaxError == 0.0) {
            cout << "Files are identical (perfect match)\n";
        } else if (overallMSE < 1.0) {
            cout << "Excellent quality (MSE < 1.0)\n";
        } else if (overallMSE < 100.0) {
            cout << "Very good quality (MSE < 100.0)\n";
        } else if (overallMSE < 10000.0) {
            cout << "Good quality (MSE < 10000.0)\n";
        } else if (overallMSE < 1000000.0) {
            cout << "Fair quality (MSE < 1000000.0)\n";
        } else {
            cout << "Poor quality (MSE >= 1000000.0)\n";
        }
    }
};

int main(int argc, char *argv[]) {
    bool verbose = false;
    
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " [-v (verbose)] original.wav processed.wav\n";
        cerr << "Compares two WAV files and calculates error metrics:\n";
        cerr << "  • Mean Squared Error (L2 norm squared)\n";
        cerr << "  • Maximum Absolute Error (L∞ norm)\n";
        cerr << "  • Signal-to-Noise Ratio (SNR)\n";
        cerr << "\nFor each channel and average of channels (stereo only)\n";
        cerr << "\nExamples:\n";
        cerr << "  " << argv[0] << " original.wav quantized.wav\n";
        cerr << "  " << argv[0] << " -v reference.wav compressed.wav\n";
        return 1;
    }
    
    string originalFile, processedFile;
    
    // Parse command line arguments
    for (int n = 1; n < argc; n++) {
        if (string(argv[n]) == "-v") {
            verbose = true;
        } else if (originalFile.empty()) {
            originalFile = argv[n];
        } else if (processedFile.empty()) {
            processedFile = argv[n];
        }
    }
    
    if (originalFile.empty() || processedFile.empty()) {
        cerr << "Error: both original and processed files must be specified\n";
        return 1;
    }
    
    // Open original file
    SndfileHandle sfhOrig { originalFile };
    if (sfhOrig.error()) {
        cerr << "Error: cannot open original file '" << originalFile << "': " << sfhOrig.strError() << "\n";
        return 1;
    }
    
    // Open processed file
    SndfileHandle sfhProc { processedFile };
    if (sfhProc.error()) {
        cerr << "Error: cannot open processed file '" << processedFile << "': " << sfhProc.strError() << "\n";
        return 1;
    }
    
    // Validate file compatibility
    if ((sfhOrig.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) {
        cerr << "Error: original file is not in WAV format\n";
        return 1;
    }
    
    if ((sfhProc.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) {
        cerr << "Error: processed file is not in WAV format\n";
        return 1;
    }
    
    if ((sfhOrig.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        cerr << "Error: original file is not in 16-bit PCM format\n";
        return 1;
    }
    
    if ((sfhProc.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        cerr << "Error: processed file is not in 16-bit PCM format\n";
        return 1;
    }
    
    // Check compatibility
    if (sfhOrig.channels() != sfhProc.channels()) {
        cerr << "Error: files have different number of channels ("
             << sfhOrig.channels() << " vs " << sfhProc.channels() << ")\n";
        return 1;
    }
    
    if (sfhOrig.samplerate() != sfhProc.samplerate()) {
        cerr << "Error: files have different sample rates ("
             << sfhOrig.samplerate() << " vs " << sfhProc.samplerate() << ")\n";
        return 1;
    }
    
    if (sfhOrig.frames() != sfhProc.frames()) {
        cerr << "Warning: files have different lengths ("
             << sfhOrig.frames() << " vs " << sfhProc.frames() << " frames)\n";
        cerr << "Comparison will use the shorter length.\n\n";
    }
    
    if (verbose) {
        cout << "File Information:\n";
        cout << "  Channels: " << sfhOrig.channels() << "\n";
        cout << "  Sample Rate: " << sfhOrig.samplerate() << " Hz\n";
        cout << "  Original Length: " << sfhOrig.frames() << " frames ("
             << (double)sfhOrig.frames() / sfhOrig.samplerate() << " seconds)\n";
        cout << "  Processed Length: " << sfhProc.frames() << " frames ("
             << (double)sfhProc.frames() / sfhProc.samplerate() << " seconds)\n\n";
    }
    
    // Create comparator
    WAVComparator comparator(sfhOrig.channels());
    
    // Process audio data
    size_t nFramesOrig, nFramesProc;
    vector<short> samplesOrig(FRAMES_BUFFER_SIZE * sfhOrig.channels());
    vector<short> samplesProc(FRAMES_BUFFER_SIZE * sfhProc.channels());
    size_t totalFramesProcessed = 0;
    
    while (true) {
        nFramesOrig = sfhOrig.readf(samplesOrig.data(), FRAMES_BUFFER_SIZE);
        nFramesProc = sfhProc.readf(samplesProc.data(), FRAMES_BUFFER_SIZE);
        
        if (nFramesOrig == 0 || nFramesProc == 0) {
            break; // End of one or both files
        }
        
        // Use the minimum of the two frame counts
        size_t nFrames = min(nFramesOrig, nFramesProc);
        
        // Resize vectors to actual number of samples
        samplesOrig.resize(nFrames * sfhOrig.channels());
        samplesProc.resize(nFrames * sfhProc.channels());
        
        // Process this frame
        comparator.processFrame(samplesOrig, samplesProc);
        
        totalFramesProcessed += nFrames;
        
        if (verbose && totalFramesProcessed % (48000 * 5) == 0) { // Every 5 seconds at 48kHz
            cout << "Processed " << totalFramesProcessed << " frames...\n";
        }
    }
    
    if (verbose) {
        cout << "Total frames processed: " << totalFramesProcessed << "\n\n";
    }
    
    // Finalize calculations and print results
    comparator.finalize();
    comparator.printResults(originalFile, processedFile);
    
    return 0;
}