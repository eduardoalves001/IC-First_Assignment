#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sndfile.hh>
#include <fftw3.h>
#include "bit_stream.h"

using namespace std;

// DCT-based lossy audio decoder
// Reconstructs audio from DCT coefficients

int main(int argc, char *argv[]) {
    bool verbose = false;
    
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " [-v] input.dct output.wav\n";
        cerr << "DCT-based lossy audio codec decoder.\n";
        cerr << "\nOptions:\n";
        cerr << "  -v              Verbose output\n";
        cerr << "\nExample:\n";
        cerr << "  " << argv[0] << " compressed.dct output.wav\n";
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
    
    // Read header
    int samplerate = bs.read_n_bits(32);
    sf_count_t frames = bs.read_n_bits(64);
    size_t blockSize = bs.read_n_bits(16);
    size_t numCoeffs = bs.read_n_bits(16);
    int quantBits = bs.read_n_bits(8);
    
    // Validate header
    if (samplerate < 1000 || samplerate > 192000) {
        cerr << "Error: invalid sample rate (" << samplerate << ") in header\n";
        return 1;
    }
    
    if (blockSize < 64 || blockSize > 8192) {
        cerr << "Error: invalid block size (" << blockSize << ") in header\n";
        return 1;
    }
    
    if (numCoeffs < 1 || numCoeffs > blockSize) {
        cerr << "Error: invalid number of coefficients (" << numCoeffs << ") in header\n";
        return 1;
    }
    
    if (quantBits < 4 || quantBits > 16) {
        cerr << "Error: invalid quantization bits (" << quantBits << ") in header\n";
        return 1;
    }
    
    if (verbose) {
        cout << "=== DCT Audio Decoder ===\n";
        cout << "Input file: " << inputFile << "\n";
        cout << "Output file: " << outputFile << "\n";
        cout << "Sample rate: " << samplerate << " Hz\n";
        cout << "Total frames: " << frames << " (" << (double)frames / samplerate << " seconds)\n";
        cout << "Block size: " << blockSize << " samples\n";
        cout << "Coefficients per block: " << numCoeffs << "\n";
        cout << "Quantization bits: " << quantBits << "\n";
        
        double compressionRatio = (double)(blockSize * 16) / (numCoeffs * quantBits);
        cout << "Compression ratio: " << compressionRatio << ":1\n";
        cout << "\nDecoding...\n";
    }
    
    // Open output WAV file
    SndfileHandle sfhOut { outputFile, SFM_WRITE, 
                          SF_FORMAT_WAV | SF_FORMAT_PCM_16,
                          1, samplerate };  // Mono output
    
    if (sfhOut.error()) {
        cerr << "Error: cannot create output file '" << outputFile << "'\n";
        cerr << sfhOut.strError() << "\n";
        return 1;
    }
    
    // Allocate FFTW arrays
    double* dctCoeffs = fftw_alloc_real(blockSize);
    double* audioBlock = fftw_alloc_real(blockSize);
    
    // Create inverse DCT plan (REDFT01 = DCT-III, inverse of DCT-II)
    fftw_plan idctPlan = fftw_plan_r2r_1d(blockSize, dctCoeffs, audioBlock, 
                                          FFTW_REDFT01, FFTW_ESTIMATE);
    
    // Process blocks
    vector<short> samples(blockSize);
    size_t totalBlocks = 0;
    sf_count_t framesProcessed = 0;
    int maxLevel = (1 << quantBits) - 1;
    
    while (framesProcessed < frames) {
        // Read scaling factor
        uint32_t maxBits = bs.read_n_bits(32);
        float maxCoeffFloat;
        memcpy(&maxCoeffFloat, &maxBits, sizeof(float));
        double maxCoeff = (double)maxCoeffFloat;
        
        // Initialize coefficients to zero
        for (size_t i = 0; i < blockSize; i++) {
            dctCoeffs[i] = 0.0;
        }
        
        // Read and dequantize coefficients
        for (size_t i = 0; i < numCoeffs; i++) {
            int level = bs.read_n_bits(quantBits);
            
            // Map from [0, maxLevel] to [-1, 1]
            double normalized = (level * 2.0 / maxLevel) - 1.0;
            
            // Scale back
            dctCoeffs[i] = normalized * maxCoeff;
        }
        
        // Denormalize coefficients before inverse DCT
        // Undo the normalization that was applied after forward DCT
        dctCoeffs[0] /= sqrt(1.0 / blockSize);
        double norm = sqrt(2.0 / blockSize);
        for (size_t i = 1; i < numCoeffs; i++) {
            dctCoeffs[i] /= norm;
        }
        
        // Perform inverse DCT
        fftw_execute(idctPlan);
        
        // Scale the inverse DCT output
        // FFTW's REDFT01 needs to be scaled by 2*N
        double idctScale = 2.0 * blockSize;
        
        // Convert to 16-bit samples
        size_t framesToWrite = blockSize;
        if (framesProcessed + framesToWrite > frames) {
            framesToWrite = frames - framesProcessed;
        }
        
        for (size_t i = 0; i < framesToWrite; i++) {
            // Scale back, denormalize and clamp
            double sample = (audioBlock[i] / idctScale) * 32768.0;
            
            if (sample > 32767.0) sample = 32767.0;
            if (sample < -32768.0) sample = -32768.0;
            
            samples[i] = (short)round(sample);
        }
        
        // Write to output file
        sfhOut.writef(samples.data(), framesToWrite);
        
        framesProcessed += framesToWrite;
        totalBlocks++;
        
        if (verbose && totalBlocks % 100 == 0) {
            cout << "Decoded " << totalBlocks << " blocks (" 
                 << (double)framesProcessed / samplerate << " seconds)...\n";
        }
    }
    
    // Clean up
    fftw_destroy_plan(idctPlan);
    fftw_free(dctCoeffs);
    fftw_free(audioBlock);
    
    bs.close();
    fsIn.close();
    
    if (verbose) {
        cout << "\nDecoding complete!\n";
        cout << "Total blocks decoded: " << totalBlocks << "\n";
        cout << "Total frames written: " << framesProcessed << "\n";
        cout << "Output file created: " << outputFile << "\n";
    }
    
    return 0;
}
