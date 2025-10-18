#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <sndfile.hh>
#include "bit_stream.h"

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // Buffer for reading frames

// Perform uniform scalar quantization and return quantized level (0 to 2^bits-1)
int quantizeToLevel(short sample, int bits) {
    if (bits >= 16) {
        // Map 16-bit signed to unsigned
        return static_cast<int>(sample) + 32768;
    }
    
    if (bits <= 0) {
        return 0;
    }
    
    // Calculate quantization parameters
    int totalLevels = 1 << bits; // 2^bits quantization levels
    int maxValue = 32767;   // Max 16-bit signed
    int minValue = -32768;  // Min 16-bit signed
    
    // Map from 16-bit range to quantization levels
    double step = (double)(maxValue - minValue) / (totalLevels - 1);
    
    // Quantize: map sample to quantization level
    int level = (int)round((double)(sample - minValue) / step);
    
    // Clamp level to valid range
    if (level < 0) level = 0;
    if (level >= totalLevels) level = totalLevels - 1;
    
    return level;
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    int bits = 8; // Default quantization bits
    
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " [-v (verbose)] [-b bits] input.wav output.bin\n";
        cerr << "Encodes a WAV file using uniform scalar quantization and bit packing.\n";
        cerr << "\nOptions:\n";
        cerr << "  -v           Enable verbose output\n";
        cerr << "  -b bits      Number of bits per sample (1-16, default: 8)\n";
        cerr << "\nThe output is a packed binary file containing:\n";
        cerr << "  - Header: channels, samplerate, frames, bits\n";
        cerr << "  - Packed quantized samples using exactly 'bits' per sample\n";
        cerr << "\nExample:\n";
        cerr << "  " << argv[0] << " -b 8 input.wav output.bin\n";
        cerr << "  " << argv[0] << " -v -b 4 audio.wav compressed.bin\n";
        return 1;
    }
    
    string inputFile, outputFile;
    
    // Parse command line arguments
    for (int n = 1; n < argc; n++) {
        if (string(argv[n]) == "-v") {
            verbose = true;
        } else if (string(argv[n]) == "-b") {
            if (n + 1 < argc) {
                bits = atoi(argv[++n]);
                if (bits < 1 || bits > 16) {
                    cerr << "Error: bits must be between 1 and 16\n";
                    return 1;
                }
            } else {
                cerr << "Error: -b option requires a value\n";
                return 1;
            }
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
    
    // Open input WAV file
    SndfileHandle sfhIn { inputFile };
    if (sfhIn.error()) {
        cerr << "Error: cannot open input file '" << inputFile << "'\n";
        cerr << sfhIn.strError() << "\n";
        return 1;
    }
    
    // Validate input format
    if ((sfhIn.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) {
        cerr << "Error: input file is not in WAV format\n";
        return 1;
    }
    
    if ((sfhIn.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        cerr << "Error: input file is not in 16-bit PCM format\n";
        return 1;
    }
    
    int channels = sfhIn.channels();
    int samplerate = sfhIn.samplerate();
    sf_count_t frames = sfhIn.frames();
    
    if (verbose) {
        cout << "=== WAV Quantization Encoder ===\n";
        cout << "Input file: " << inputFile << "\n";
        cout << "Output file: " << outputFile << "\n";
        cout << "Channels: " << channels << "\n";
        cout << "Sample rate: " << samplerate << " Hz\n";
        cout << "Frames: " << frames << " (" << (double)frames / samplerate << " seconds)\n";
        cout << "Quantization bits: " << bits << "\n";
        cout << "Quantization levels: " << (1 << bits) << "\n";
        
        // Calculate compression ratio
        long long originalBits = frames * channels * 16;
        long long compressedBits = frames * channels * bits;
        double compressionRatio = (double)originalBits / compressedBits;
        
        cout << "Original size: " << originalBits / 8 << " bytes (" << originalBits << " bits)\n";
        cout << "Compressed size (data only): " << compressedBits / 8 << " bytes (" << compressedBits << " bits)\n";
        cout << "Compression ratio: " << compressionRatio << ":1\n";
        cout << "\nEncoding...\n";
    }
    
    // Open output binary file
    fstream fsOut(outputFile, ios::out | ios::binary);
    if (!fsOut.is_open()) {
        cerr << "Error: cannot create output file '" << outputFile << "'\n";
        return 1;
    }
    
    // Create BitStream for writing
    BitStream bs(fsOut, STREAM_WRITE);
    
    // Write header information
    // Format: channels (16 bits), samplerate (32 bits), frames (64 bits), bits (8 bits)
    bs.write_n_bits(channels, 16);
    bs.write_n_bits(samplerate, 32);
    bs.write_n_bits(frames, 64);
    bs.write_n_bits(bits, 8);
    
    if (verbose) {
        cout << "Header written: " << (16 + 32 + 64 + 8) / 8 << " bytes\n";
    }
    
    // Process audio data
    size_t nFrames;
    vector<short> samples(FRAMES_BUFFER_SIZE * channels);
    sf_count_t totalFramesProcessed = 0;
    
    while ((nFrames = sfhIn.readf(samples.data(), FRAMES_BUFFER_SIZE)) > 0) {
        // Quantize and write each sample
        for (size_t i = 0; i < nFrames * channels; i++) {
            int level = quantizeToLevel(samples[i], bits);
            bs.write_n_bits(level, bits);
        }
        
        totalFramesProcessed += nFrames;
        
        if (verbose && totalFramesProcessed % (samplerate * 5) == 0) {
            cout << "Processed " << totalFramesProcessed << " frames ("
                 << (double)totalFramesProcessed / samplerate << " seconds)...\n";
        }
    }
    
    // Close the bit stream (flushes any remaining bits)
    bs.close();
    
    if (verbose) {
        cout << "\nEncoding complete!\n";
        cout << "Total frames encoded: " << totalFramesProcessed << "\n";
        
        // Get actual file size
        fsOut.seekg(0, ios::end);
        streampos fileSize = fsOut.tellg();
        
        cout << "Output file size: " << fileSize << " bytes\n";
        
        long long originalSize = frames * channels * 2; // 16 bits = 2 bytes
        double actualCompressionRatio = (double)originalSize / fileSize;
        cout << "Actual compression ratio (including header): " 
             << actualCompressionRatio << ":1\n";
    }
    
    fsOut.close();
    
    return 0;
}
