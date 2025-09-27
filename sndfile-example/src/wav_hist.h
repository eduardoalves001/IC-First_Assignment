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
#ifndef WAVHIST_H
#define WAVHIST_H

#include <iostream>
#include <vector>
#include <map>
#include <sndfile.hh>
#include <cmath>

class WAVHist {
  private:
	std::vector<std::map<short, size_t>> counts;
	std::map<short, size_t> midCounts;  // MID channel histogram
	std::map<short, size_t> sideCounts; // SIDE channel histogram
	int binSize;
	int numChannels;

  public:
	WAVHist(const SndfileHandle& sfh, int binSize = 1) : binSize(binSize), numChannels(sfh.channels()) {
		counts.resize(sfh.channels());
	}

	void update(const std::vector<short>& samples) {
		size_t n { };
		for(auto s : samples) {
			// Original channel histograms with binning
			short binValue = (binSize == 1) ? s : s / binSize;
			counts[n++ % counts.size()][binValue]++;
		}
		
		// Calculate MID and SIDE for stereo audio
		if(numChannels == 2) {
			for(size_t i = 0; i < samples.size(); i += 2) {
				if(i + 1 < samples.size()) {
					short left = samples[i];
					short right = samples[i + 1];
					
					// MID = (L + R) / 2, SIDE = (L - R) / 2
					short mid = (left + right) / 2;
					short side = (left - right) / 2;
					
					// Apply binning
					short midBin = (binSize == 1) ? mid : mid / binSize;
					short sideBin = (binSize == 1) ? side : side / binSize;
					
					midCounts[midBin]++;
					sideCounts[sideBin]++;
				}
			}
		}
	}

	void dump(const size_t channel) const {
		if(channel < counts.size()) {
			for(auto [value, counter] : counts[channel])
				std::cout << value << '\t' << counter << '\n';
		}
	}
	
	void dumpMid() const {
		for(auto [value, counter] : midCounts)
			std::cout << value << '\t' << counter << '\n';
	}
	
	void dumpSide() const {
		for(auto [value, counter] : sideCounts)
			std::cout << value << '\t' << counter << '\n';
	}
	
	// Get number of channels for validation
	int getChannels() const { return numChannels; }
	
	// Check if MID/SIDE data is available
	bool hasMidSide() const { return numChannels == 2 && !midCounts.empty(); }
};

#endif

