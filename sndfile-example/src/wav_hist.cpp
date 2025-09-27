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

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // Buffer for reading frames

int main(int argc, char *argv[]) {

	if(argc < 3) {
		cerr << "Usage: " << argv[0] << " [-b binSize] <input file> <channel|MID|SIDE>\n";
		cerr << "  channel: 0, 1, 2, ... for individual channels\n";
		cerr << "  MID: for MID channel (stereo only)\n";
		cerr << "  SIDE: for SIDE channel (stereo only)\n";
		cerr << "  binSize: 1, 2, 4, 8, ... (power of 2, default=1)\n";
		return 1;
	}

	int binSize = 1;
	int argOffset = 0;
	
	// Check for bin size option
	if(argc >= 4 && string(argv[1]) == "-b") {
		binSize = stoi(argv[2]);
		argOffset = 2;
		
		// Validate bin size is power of 2
		if(binSize <= 0 || (binSize & (binSize - 1)) != 0) {
			cerr << "Error: bin size must be a positive power of 2\n";
			return 1;
		}
	}

	SndfileHandle sndFile { argv[argc-2] };
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

	string channelArg { argv[argc-1] };
	
	// Process the audio file
	size_t nFrames;
	vector<short> samples(FRAMES_BUFFER_SIZE * sndFile.channels());
	WAVHist hist { sndFile, binSize };
	while((nFrames = sndFile.readf(samples.data(), FRAMES_BUFFER_SIZE))) {
		samples.resize(nFrames * sndFile.channels());
		hist.update(samples);
	}

	// Output the requested histogram
	if(channelArg == "MID") {
		if(!hist.hasMidSide()) {
			cerr << "Error: MID channel only available for stereo audio\n";
			return 1;
		}
		hist.dumpMid();
	}
	else if(channelArg == "SIDE") {
		if(!hist.hasMidSide()) {
			cerr << "Error: SIDE channel only available for stereo audio\n";
			return 1;
		}
		hist.dumpSide();
	}
	else {
		int channel = stoi(channelArg);
		if(channel >= sndFile.channels()) {
			cerr << "Error: invalid channel requested\n";
			return 1;
		}
		hist.dump(channel);
	}

	return 0;
}

