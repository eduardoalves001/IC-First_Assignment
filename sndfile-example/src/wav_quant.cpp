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

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // Buffer for reading/writing frames

// Function to perform uniform scalar quantization
short quantize(short sample, int targetBits) {
    if (targetBits <= 0 || targetBits >= 16) {
        return sample; // No quantization if invalid target bits
    }
    
    // Calculate the number of bits to reduce
    int bitsToReduce = 16 - targetBits;
    
    // Quantize by shifting right and then left to remove lower bits
    short quantized = sample >> bitsToReduce;
    quantized = quantized << bitsToReduce;
    
    return quantized;
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    
    if(argc < 4) {
        cerr << "Usage: " << argv[0] << " [ -v (verbose) ] inputFile outputFile targetBits\n";
        cerr << "       targetBits: number of bits per sample (1-15)\n";
        cerr << "       Example: " << argv[0] << " input.wav output.wav 8\n";
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
    
    if(args.size() < 3) {
        cerr << "Error: insufficient arguments\n";
        return 1;
    }
    
    string inputFile = args[0];
    string outputFile = args[1];
    int targetBits = stoi(args[2]);
    
    // Validate target bits
    if(targetBits <= 0 || targetBits >= 16) {
        cerr << "Error: targetBits must be between 1 and 15\n";
        return 1;
    }
    
    // Open input file
    SndfileHandle sfhIn { inputFile };
    if(sfhIn.error()) {
        cerr << "Error: invalid input file\n";
        return 1;
    }
    
    if((sfhIn.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) {
        cerr << "Error: file is not in WAV format\n";
        return 1;
    }
    
    if((sfhIn.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        cerr << "Error: file is not in PCM_16 format\n";
        return 1;
    }
    
    if(verbose) {
        cout << "Input file has:\n";
        cout << '\t' << sfhIn.frames() << " frames\n";
        cout << '\t' << sfhIn.samplerate() << " samples per second\n";
        cout << '\t' << sfhIn.channels() << " channels\n";
        cout << "Quantizing from 16 bits to " << targetBits << " bits\n";
    }
    
    // Open output file with same format as input
    SndfileHandle sfhOut { outputFile, SFM_WRITE, sfhIn.format(),
                          sfhIn.channels(), sfhIn.samplerate() };
    if(sfhOut.error()) {
        cerr << "Error: invalid output file\n";
        return 1;
    }
    
    // Process audio in chunks
    size_t nFrames;
    vector<short> samples(FRAMES_BUFFER_SIZE * sfhIn.channels());
    size_t totalFramesProcessed = 0;
    
    while((nFrames = sfhIn.readf(samples.data(), FRAMES_BUFFER_SIZE))) {
        // Quantize each sample
        for(size_t i = 0; i < nFrames * sfhIn.channels(); i++) {
            samples[i] = quantize(samples[i], targetBits);
        }
        
        // Write quantized samples to output file
        sfhOut.writef(samples.data(), nFrames);
        totalFramesProcessed += nFrames;
        
        if(verbose && totalFramesProcessed % (sfhIn.samplerate() * 5) == 0) {
            cout << "Processed " << totalFramesProcessed << " frames...\n";
        }
    }
    
    if(verbose) {
        cout << "Quantization completed. Total frames processed: " << totalFramesProcessed << "\n";
        cout << "Output saved to: " << outputFile << "\n";
        
        // Calculate compression ratio information
        double originalRange = pow(2, 16);
        double quantizedRange = pow(2, targetBits);
        double compressionRatio = originalRange / quantizedRange;
        cout << "Theoretical compression ratio: " << compressionRatio << ":1\n";
        cout << "Bits saved per sample: " << (16 - targetBits) << " bits\n";
    }
    
    return 0;
}