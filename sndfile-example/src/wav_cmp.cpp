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
#include <string>
#include <sndfile.hh>
#include <cmath>
#include <iomanip>

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // Buffer for reading frames

struct AudioMetrics {
    double mse;           // Mean Squared Error (L2 norm)
    double maxError;      // Maximum absolute error (L∞ norm)
    double snr;           // Signal-to-Noise Ratio
    size_t sampleCount;   // Number of samples processed
};

// Calculate metrics for a single channel
AudioMetrics calculateChannelMetrics(const vector<short>& original, const vector<short>& modified, int channel, int nChannels) {
    AudioMetrics metrics = {0.0, 0.0, 0.0, 0};
    
    double sumSquaredError = 0.0;
    double sumSquaredOriginal = 0.0;
    double maxAbsError = 0.0;
    
    size_t nFrames = original.size() / nChannels;
    
    for (size_t frame = 0; frame < nFrames; frame++) {
        size_t idx = frame * nChannels + channel;
        
        double orig = static_cast<double>(original[idx]);
        double mod = static_cast<double>(modified[idx]);
        double error = orig - mod;
        double absError = abs(error);
        
        sumSquaredError += error * error;
        sumSquaredOriginal += orig * orig;
        
        if (absError > maxAbsError) {
            maxAbsError = absError;
        }
    }
    
    metrics.sampleCount = nFrames;
    metrics.mse = sumSquaredError / nFrames;
    metrics.maxError = maxAbsError;
    
    // Calculate SNR: 10 * log10(signal_power / noise_power)
    if (sumSquaredError > 0.0) {
        metrics.snr = 10.0 * log10(sumSquaredOriginal / sumSquaredError);
    } else {
        metrics.snr = INFINITY; // Perfect match
    }
    
    return metrics;
}

// Calculate metrics for the average of channels (mono version)
AudioMetrics calculateAverageMetrics(const vector<short>& original, const vector<short>& modified, int nChannels) {
    AudioMetrics metrics = {0.0, 0.0, 0.0, 0};
    
    if (nChannels == 1) {
        // If mono, just return the single channel metrics
        return calculateChannelMetrics(original, modified, 0, 1);
    }
    
    double sumSquaredError = 0.0;
    double sumSquaredOriginal = 0.0;
    double maxAbsError = 0.0;
    
    size_t nFrames = original.size() / nChannels;
    
    for (size_t frame = 0; frame < nFrames; frame++) {
        // Calculate average of all channels for this frame
        double origAvg = 0.0;
        double modAvg = 0.0;
        
        for (int ch = 0; ch < nChannels; ch++) {
            size_t idx = frame * nChannels + ch;
            origAvg += static_cast<double>(original[idx]);
            modAvg += static_cast<double>(modified[idx]);
        }
        
        origAvg /= nChannels;
        modAvg /= nChannels;
        
        double error = origAvg - modAvg;
        double absError = abs(error);
        
        sumSquaredError += error * error;
        sumSquaredOriginal += origAvg * origAvg;
        
        if (absError > maxAbsError) {
            maxAbsError = absError;
        }
    }
    
    metrics.sampleCount = nFrames;
    metrics.mse = sumSquaredError / nFrames;
    metrics.maxError = maxAbsError;
    
    // Calculate SNR
    if (sumSquaredError > 0.0) {
        metrics.snr = 10.0 * log10(sumSquaredOriginal / sumSquaredError);
    } else {
        metrics.snr = INFINITY; // Perfect match
    }
    
    return metrics;
}

void printMetrics(const string& label, const AudioMetrics& metrics) {
    cout << label << ":\n";
    cout << "  Mean Squared Error (L2 norm): " << fixed << setprecision(6) << metrics.mse << "\n";
    cout << "  Maximum Absolute Error (L∞ norm): " << fixed << setprecision(6) << metrics.maxError << "\n";
    if (isinf(metrics.snr)) {
        cout << "  Signal-to-Noise Ratio (SNR): ∞ dB (perfect match)\n";
    } else {
        cout << "  Signal-to-Noise Ratio (SNR): " << fixed << setprecision(2) << metrics.snr << " dB\n";
    }
    cout << "  Samples processed: " << metrics.sampleCount << "\n\n";
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    
    if(argc < 3) {
        cerr << "Usage: " << argv[0] << " [ -v (verbose) ] originalFile modifiedFile\n";
        cerr << "       Compare two WAV files and compute error metrics\n";
        cerr << "       Example: " << argv[0] << " original.wav quantized.wav\n";
        return 1;
    }
    
    // Check for verbose flag and parse arguments
    vector<string> args;
    for(int n = 1; n < argc; n++) {
        if(string(argv[n]) == "-v") {
            verbose = true;
        } else {
            args.push_back(string(argv[n]));
        }
    }
    
    if(args.size() < 2) {
        cerr << "Error: insufficient arguments\n";
        return 1;
    }
    
    string originalFile = args[0];
    string modifiedFile = args[1];
    
    // Open original file
    SndfileHandle sfhOrig { originalFile };
    if(sfhOrig.error()) {
        cerr << "Error: invalid original file\n";
        return 1;
    }
    
    // Open modified file
    SndfileHandle sfhMod { modifiedFile };
    if(sfhMod.error()) {
        cerr << "Error: invalid modified file\n";
        return 1;
    }
    
    // Check if files have compatible formats
    if(sfhOrig.samplerate() != sfhMod.samplerate()) {
        cerr << "Error: files have different sample rates\n";
        return 1;
    }
    
    if(sfhOrig.channels() != sfhMod.channels()) {
        cerr << "Error: files have different number of channels\n";
        return 1;
    }
    
    if(sfhOrig.frames() != sfhMod.frames()) {
        cerr << "Error: files have different number of frames\n";
        return 1;
    }
    
    if((sfhOrig.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV ||
       (sfhMod.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) {
        cerr << "Error: both files must be in WAV format\n";
        return 1;
    }
    
    if((sfhOrig.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16 ||
       (sfhMod.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        cerr << "Error: both files must be in PCM_16 format\n";
        return 1;
    }
    
    int nChannels = sfhOrig.channels();
    size_t totalFrames = sfhOrig.frames();
    
    if(verbose) {
        cout << "Comparing files:\n";
        cout << "  Original: " << originalFile << "\n";
        cout << "  Modified: " << modifiedFile << "\n";
        cout << "  Frames: " << totalFrames << "\n";
        cout << "  Sample rate: " << sfhOrig.samplerate() << " Hz\n";
        cout << "  Channels: " << nChannels << "\n\n";
    }
    
    // Read all samples from both files
    vector<short> originalSamples(totalFrames * nChannels);
    vector<short> modifiedSamples(totalFrames * nChannels);
    
    size_t framesRead1 = sfhOrig.readf(originalSamples.data(), totalFrames);
    size_t framesRead2 = sfhMod.readf(modifiedSamples.data(), totalFrames);
    
    if(framesRead1 != totalFrames || framesRead2 != totalFrames) {
        cerr << "Error: could not read all frames from files\n";
        return 1;
    }
    
    cout << "=== AUDIO COMPARISON METRICS ===\n\n";
    
    // Calculate metrics for each individual channel
    for(int ch = 0; ch < nChannels; ch++) {
        string channelName;
        if(nChannels == 1) {
            channelName = "Mono Channel";
        } else if(nChannels == 2) {
            channelName = (ch == 0) ? "Left Channel (L)" : "Right Channel (R)";
        } else {
            channelName = "Channel " + to_string(ch);
        }
        
        AudioMetrics metrics = calculateChannelMetrics(originalSamples, modifiedSamples, ch, nChannels);
        printMetrics(channelName, metrics);
    }
    
    // Calculate metrics for the average of channels (if more than one channel)
    if(nChannels > 1) {
        AudioMetrics avgMetrics = calculateAverageMetrics(originalSamples, modifiedSamples, nChannels);
        printMetrics("Average of all channels", avgMetrics);
    }
    
    return 0;
}