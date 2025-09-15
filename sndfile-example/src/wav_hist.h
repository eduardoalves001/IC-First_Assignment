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
#include <fstream> // Include fstream for std::ofstream

class WAVHist {
  private:
    int nChannels;
    std::vector<std::map<int, size_t>> counts; // per-channel histograms
    std::map<int, size_t> midHist;             // (L+R)/2
    std::map<int, size_t> sideHist;            // (L-R)/2

  public:
    WAVHist(const SndfileHandle& sfh) {
        nChannels = sfh.channels();
        counts.resize(nChannels);
    }

    void update(const std::vector<short>& samples) {
        size_t nFrames = samples.size() / nChannels;
        for(size_t i = 0; i < nFrames; i++) {
            // Update each channel histogram
            for(int ch = 0; ch < nChannels; ch++) {
                short value = samples[i * nChannels + ch];
                counts[ch][value]++;
            }

            // If stereo, also update MID and SIDE
            if(nChannels == 2) {
                int L = samples[i * 2];
                int R = samples[i * 2 + 1];
                int MID  = (L + R) / 2;
                int SIDE = (L - R) / 2;
                midHist[MID]++;
                sideHist[SIDE]++;
            }
        }
    }

    // Dump a histogram
    // channel: 0..N-1 = original channels, N = MID, N+1 = SIDE (if stereo)
    // binSize: grouping factor (1 = original resolution, 2/4/8... = coarser bins)
    void dump(size_t channel, int binSize = 1) const {
        const std::map<int, size_t>* histPtr = nullptr;

        if(channel < (size_t)nChannels) {
            histPtr = &counts[channel];
        } else if(channel == (size_t)nChannels && nChannels == 2) {
            histPtr = &midHist;
        } else if(channel == (size_t)nChannels+1 && nChannels == 2) {
            histPtr = &sideHist;
        } else {
            std::cerr << "Error: invalid channel index\n";
            return;
        }

        std::map<int, size_t> rebinned;
        if(binSize > 1) {
            for(const auto& [value, counter] : *histPtr) {
                int bin = (value / binSize) * binSize;
                rebinned[bin] += counter;
            }
        }

        const auto& finalHist = (binSize > 1) ? rebinned : *histPtr;

        for(const auto& [value, counter] : finalHist)
            std::cout << value << '\t' << counter << '\n';
    }

    // Method to save histogram data to a file
    void dumpToFile(const std::string& filename, size_t channel, int binSize) const;
};

#endif
