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
#include <sndfile.hh>
#include "wav_hist.h"
#include <cmath>
#include <fstream>

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // Buffer for reading frames

int main(int argc, char *argv[]) {

    if(argc < 3) {
        cerr << "Usage: " << argv[0] << " <input file> <channel> [binSize]\n";
        cerr << "channel: 0..N-1 = original channels, N = MID, N+1 = SIDE (if stereo)\n";
        cerr << "binSize: optional, default=1 (use 2,4,8,... for coarser bins)\n";
        return 1;
    }

    SndfileHandle sndFile { argv[1] };
    if(sndFile.error()) {
        cerr << "Error: invalid input file\n";
        return 1;
    }

    if((sndFile.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) {
        cerr << "Error: file is not in WAV format\n";
        return 1;
    }

    if((sndFile.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        cerr << "Error: file is not in PCM_16 format\n";
        return 1;
    }

    int channel { stoi(argv[2]) };
    int binSize = (argc >= 4) ? stoi(argv[3]) : 1;

    int nChannels = sndFile.channels();
    int maxChannel = nChannels - 1;
    if(nChannels == 2) {
        maxChannel = nChannels + 1; // allow MID and SIDE
    }

    if(channel > maxChannel) {
        cerr << "Error: invalid channel requested\n";
        return 1;
    }

    size_t nFrames;
    vector<short> samples(FRAMES_BUFFER_SIZE * nChannels);
    WAVHist hist { sndFile };

    while((nFrames = sndFile.readf(samples.data(), FRAMES_BUFFER_SIZE))) {
        samples.resize(nFrames * nChannels);
        hist.update(samples);
    }

    hist.dump(channel, binSize);
    return 0;
}
