#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sndfile.hh>
#include <fftw3.h>
#include "bit_stream.h"

using namespace std;

// DCT-based lossy audio encoder
// Uses block-based DCT transformation with quantization and bit-packing

int main(int argc, char *argv[]) {
    bool verbose = false;
    size_t blockSize = 1024;      // DCT block size
    double keepFraction = 0.2;     // Fraction of coefficients to keep (0.0-1.0)
    int quantBits = 8;             // Bits for quantizing coefficients
    
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " [-v] [-bs blockSize] [-frac fraction] [-qbits bits] input.wav output.dct\n";
        cerr << "DCT-based lossy audio codec encoder for mono audio.\n";
        cerr << "\nOptions:\n";
        cerr << "  -v              Verbose output\n";
        cerr << "  -bs blockSize   DCT block size (default: 1024)\n";
        cerr << "  -frac fraction  Fraction of DCT coefficients to keep (default: 0.2)\n";
        cerr << "  -qbits bits     Bits for coefficient quantization (default: 8)\n";
        cerr << "\nNote: Input must be mono (single channel) WAV file.\n";
        cerr << "\nExample:\n";
        cerr << "  " << argv[0] << " -bs 1024 -frac 0.15 -qbits 8 input.wav output.dct\n";
        return 1;
    }
    
    string inputFile, outputFile;
    
    // Parse command line arguments
    for (int n = 1; n < argc; n++) {
        if (string(argv[n]) == "-v") {
            verbose = true;
        } else if (string(argv[n]) == "-bs") {
            if (n + 1 < argc) {
                blockSize = atoi(argv[++n]);
                if (blockSize < 64 || blockSize > 8192) {
                    cerr << "Error: block size must be between 64 and 8192\n";
                    return 1;
                }
            }
        } else if (string(argv[n]) == "-frac") {
            if (n + 1 < argc) {
                keepFraction = atof(argv[++n]);
                if (keepFraction <= 0.0 || keepFraction > 1.0) {
                    cerr << "Error: fraction must be between 0 and 1\n";
                    return 1;
                }
            }
        } else if (string(argv[n]) == "-qbits") {
            if (n + 1 < argc) {
                quantBits = atoi(argv[++n]);
                if (quantBits < 4 || quantBits > 16) {
                    cerr << "Error: quantization bits must be between 4 and 16\n";
                    return 1;
                }
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
    
    if (sfhIn.channels() != 1) {
        cerr << "Error: input file must be mono (1 channel), found " << sfhIn.channels() << " channels\n";
        return 1;
    }
    
    int samplerate = sfhIn.samplerate();
    sf_count_t frames = sfhIn.frames();
    
    size_t numCoeffs = (size_t)(blockSize * keepFraction);
    if (numCoeffs < 1) numCoeffs = 1;
    if (numCoeffs > blockSize) numCoeffs = blockSize;
    
    if (verbose) {
        cout << "=== DCT Audio Encoder ===\n";
        cout << "Input file: " << inputFile << "\n";
        cout << "Output file: " << outputFile << "\n";
        cout << "Sample rate: " << samplerate << " Hz\n";
        cout << "Total frames: " << frames << " (" << (double)frames / samplerate << " seconds)\n";
        cout << "Block size: " << blockSize << " samples\n";
        cout << "Keep fraction: " << keepFraction << " (" << numCoeffs << "/" << blockSize << " coefficients)\n";
        cout << "Quantization bits: " << quantBits << "\n";
        
        double compressionRatio = (double)(blockSize * 16) / (numCoeffs * quantBits);
        cout << "Expected compression ratio: " << compressionRatio << ":1\n";
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
    
    // Write header
    // Format: samplerate(32), frames(64), blockSize(16), numCoeffs(16), quantBits(8)
    bs.write_n_bits(samplerate, 32);
    bs.write_n_bits(frames, 64);
    bs.write_n_bits(blockSize, 16);
    bs.write_n_bits(numCoeffs, 16);
    bs.write_n_bits(quantBits, 8);
    
    if (verbose) {
        cout << "Header written: " << (32 + 64 + 16 + 16 + 8) / 8 << " bytes\n";
    }
    
    // Allocate FFTW arrays
    double* audioBlock = fftw_alloc_real(blockSize);
    double* dctCoeffs = fftw_alloc_real(blockSize);
    
    // Create DCT plan (REDFT10 = DCT-II)
    fftw_plan dctPlan = fftw_plan_r2r_1d(blockSize, audioBlock, dctCoeffs, 
                                         FFTW_REDFT10, FFTW_ESTIMATE);
    
    // Read and process audio in blocks
    vector<short> samples(blockSize);
    size_t totalBlocks = 0;
    size_t totalCoeffsWritten = 0;
    
    while (true) {
        size_t nRead = sfhIn.readf(samples.data(), blockSize);
        if (nRead == 0) break;
        
        // Zero-pad if last block is incomplete
        if (nRead < blockSize) {
            for (size_t i = nRead; i < blockSize; i++) {
                samples[i] = 0;
            }
        }
        
        // Convert to double and normalize
        for (size_t i = 0; i < blockSize; i++) {
            audioBlock[i] = samples[i] / 32768.0;
        }
        
        // Perform DCT
        fftw_execute(dctPlan);
        
        // Normalize DCT output
        double norm = sqrt(2.0 / blockSize);
        dctCoeffs[0] *= sqrt(1.0 / blockSize);
        for (size_t i = 1; i < blockSize; i++) {
            dctCoeffs[i] *= norm;
        }
        
        // Find max absolute value for quantization scaling
        double maxCoeff = 0.0;
        for (size_t i = 0; i < numCoeffs; i++) {
            double absVal = fabs(dctCoeffs[i]);
            if (absVal > maxCoeff) maxCoeff = absVal;
        }
        
        // Avoid division by zero
        if (maxCoeff < 1e-10) maxCoeff = 1.0;
        
        // Write scaling factor as float (32 bits)
        uint32_t maxBits;
        float maxCoeffFloat = (float)maxCoeff;  // Convert to float explicitly
        memcpy(&maxBits, &maxCoeffFloat, sizeof(float));
        bs.write_n_bits(maxBits, 32);
        
        // Quantize and write coefficients
        int maxLevel = (1 << quantBits) - 1;
        for (size_t i = 0; i < numCoeffs; i++) {
            // Normalize to [-1, 1] and quantize
            double normalized = dctCoeffs[i] / maxCoeff;
            
            // Map to [0, maxLevel]
            int level = (int)round((normalized + 1.0) * maxLevel / 2.0);
            
            // Clamp
            if (level < 0) level = 0;
            if (level > maxLevel) level = maxLevel;
            
            bs.write_n_bits(level, quantBits);
        }
        
        totalBlocks++;
        totalCoeffsWritten += numCoeffs;
        
        if (verbose && totalBlocks % 100 == 0) {
            cout << "Processed " << totalBlocks << " blocks...\n";
        }
    }
    
    // Clean up
    fftw_destroy_plan(dctPlan);
    fftw_free(audioBlock);
    fftw_free(dctCoeffs);
    
    bs.close();
    
    if (verbose) {
        cout << "\nEncoding complete!\n";
        cout << "Total blocks processed: " << totalBlocks << "\n";
        cout << "Total coefficients written: " << totalCoeffsWritten << "\n";
        
        // Get actual file size
        fsOut.seekg(0, ios::end);
        streampos fileSize = fsOut.tellg();
        
        long long originalSize = frames * 2; // 16 bits = 2 bytes
        double actualCompressionRatio = (double)originalSize / fileSize;
        
        cout << "Original size: " << originalSize << " bytes\n";
        cout << "Compressed size: " << fileSize << " bytes\n";
        cout << "Actual compression ratio: " << actualCompressionRatio << ":1\n";
    }
    
    fsOut.close();
    
    return 0;
}
