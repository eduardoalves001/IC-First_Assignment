#include <iostream>
#include <vector>
#include <fstream>
#include <sndfile.hh>
#include "bit_stream.h"

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // Buffer for writing frames

// Reconstruct sample from quantization level
short levelToSample(int level, int bits) {
    if (bits >= 16) {
        // Map unsigned back to signed 16-bit
        return static_cast<short>(level - 32768);
    }
    
    if (bits <= 0) {
        return 0;
    }
    
    // Calculate quantization parameters
    int totalLevels = 1 << bits; // 2^bits quantization levels
    int maxValue = 32767;   // Max 16-bit signed
    int minValue = -32768;  // Min 16-bit signed
    
    // Map from quantization level to 16-bit range
    double step = (double)(maxValue - minValue) / (totalLevels - 1);
    
    // Reconstruct sample from level
    short reconstructed = (short)(minValue + level * step);
    
    return reconstructed;
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " [-v (verbose)] input.bin output.wav\n";
        cerr << "Decodes a packed binary file to a WAV file.\n";
        cerr << "\nOptions:\n";
        cerr << "  -v           Enable verbose output\n";
        cerr << "\nThe input file must be created by wav_quant_enc.\n";
        cerr << "The decoder reads the header and reconstructs the quantized WAV file.\n";
        cerr << "\nExample:\n";
        cerr << "  " << argv[0] << " compressed.bin output.wav\n";
        cerr << "  " << argv[0] << " -v encoded.bin decoded.wav\n";
        return 1;
    }
    
    string inputFile, outputFile;
    
    // Parse command line arguments
    for (int n = 1; n < argc; n++) {
        if (string(argv[n]) == "-v") {
            verbose = true;
        } else if (inputFile.empty()) {
            inputFile = argv[n];
        } else if (outputFile.empty()) {
            outputFile = argv[n];
        }
    }
    
    if (inputFile.empty() || outputFile.empty()) {
        cerr << "Error: both input and output files must be specified\n";
        return 1;
    }
    
    // Open input binary file
    fstream fsIn(inputFile, ios::in | ios::binary);
    if (!fsIn.is_open()) {
        cerr << "Error: cannot open input file '" << inputFile << "'\n";
        return 1;
    }
    
    // Create BitStream for reading
    BitStream bs(fsIn, STREAM_READ);
    
    // Read header information
    // Format: channels (16 bits), samplerate (32 bits), frames (64 bits), bits (8 bits)
    int channels = bs.read_n_bits(16);
    int samplerate = bs.read_n_bits(32);
    sf_count_t frames = bs.read_n_bits(64);
    int bits = bs.read_n_bits(8);
    
    // Validate header
    if (channels < 1 || channels > 16) {
        cerr << "Error: invalid number of channels (" << channels << ") in header\n";
        return 1;
    }
    
    if (samplerate < 1000 || samplerate > 192000) {
        cerr << "Error: invalid sample rate (" << samplerate << ") in header\n";
        return 1;
    }
    
    if (bits < 1 || bits > 16) {
        cerr << "Error: invalid bits per sample (" << bits << ") in header\n";
        return 1;
    }
    
    if (verbose) {
        cout << "=== WAV Quantization Decoder ===\n";
        cout << "Input file: " << inputFile << "\n";
        cout << "Output file: " << outputFile << "\n";
        cout << "Channels: " << channels << "\n";
        cout << "Sample rate: " << samplerate << " Hz\n";
        cout << "Frames: " << frames << " (" << (double)frames / samplerate << " seconds)\n";
        cout << "Quantization bits: " << bits << "\n";
        cout << "Quantization levels: " << (1 << bits) << "\n";
        
        long long outputSize = frames * channels * 2; // 16 bits = 2 bytes
        cout << "Output size: " << outputSize << " bytes\n";
        
        cout << "\nDecoding...\n";
    }
    
    // Open output WAV file
    SndfileHandle sfhOut { outputFile, SFM_WRITE, 
                          SF_FORMAT_WAV | SF_FORMAT_PCM_16,
                          channels, samplerate };
    
    if (sfhOut.error()) {
        cerr << "Error: cannot create output file '" << outputFile << "'\n";
        cerr << sfhOut.strError() << "\n";
        return 1;
    }
    
    // Process audio data
    vector<short> samples(FRAMES_BUFFER_SIZE * channels);
    sf_count_t totalFramesProcessed = 0;
    sf_count_t framesToRead = frames;
    
    while (framesToRead > 0) {
        size_t nFrames = min((sf_count_t)FRAMES_BUFFER_SIZE, framesToRead);
        
        // Read and decode each sample
        for (size_t i = 0; i < nFrames * channels; i++) {
            int level = bs.read_n_bits(bits);
            samples[i] = levelToSample(level, bits);
        }
        
        // Write decoded samples to WAV file
        sfhOut.writef(samples.data(), nFrames);
        
        totalFramesProcessed += nFrames;
        framesToRead -= nFrames;
        
        if (verbose && totalFramesProcessed % (samplerate * 5) == 0) {
            cout << "Processed " << totalFramesProcessed << " frames ("
                 << (double)totalFramesProcessed / samplerate << " seconds)...\n";
        }
    }
    
    // Close the bit stream
    bs.close();
    fsIn.close();
    
    if (verbose) {
        cout << "\nDecoding complete!\n";
        cout << "Total frames decoded: " << totalFramesProcessed << "\n";
        cout << "Output WAV file created: " << outputFile << "\n";
    }
    
    return 0;
}
