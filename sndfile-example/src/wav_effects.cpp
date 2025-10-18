#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>
#include <sstream>
#include <sndfile.hh>

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // Buffer for reading/writing frames
constexpr double PI = 3.14159265358979323846;

class AudioEffects {
private:
    vector<short> audioData;
    int channels;
    int sampleRate;
    size_t numFrames;
    
    // Helper function to clamp values to 16-bit range
    short clamp16(double value) {
        if (value > 32767.0) return 32767;
        if (value < -32768.0) return -32768;
        return static_cast<short>(round(value));
    }
    
    // Helper function to get sample at specific frame and channel
    short getSample(size_t frame, int channel) {
        if (frame >= numFrames || channel >= channels) return 0;
        return audioData[frame * channels + channel];
    }
    
    // Helper function to set sample at specific frame and channel
    void setSample(size_t frame, int channel, short value) {
        if (frame < numFrames && channel < channels) {
            audioData[frame * channels + channel] = value;
        }
    }

public:
    bool loadAudio(const string& filename) {
        SndfileHandle sfh(filename);
        if (sfh.error()) {
            cerr << "Error: cannot open input file '" << filename << "': " << sfh.strError() << "\n";
            return false;
        }
        
        if ((sfh.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) {
            cerr << "Error: input file is not in WAV format\n";
            return false;
        }
        
        if ((sfh.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
            cerr << "Error: input file is not in 16-bit PCM format\n";
            return false;
        }
        
        channels = sfh.channels();
        sampleRate = sfh.samplerate();
        numFrames = sfh.frames();
        
        audioData.resize(numFrames * channels);
        sfh.readf(audioData.data(), numFrames);
        
        return true;
    }
    
    bool saveAudio(const string& filename) {
        SndfileHandle sfh(filename, SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_PCM_16, channels, sampleRate);
        if (sfh.error()) {
            cerr << "Error: cannot create output file '" << filename << "': " << sfh.strError() << "\n";
            return false;
        }
        
        sfh.writef(audioData.data(), numFrames);
        return true;
    }
    
    // Effect 1: Single Echo
    void applySingleEcho(double delayMs, double feedback) {
        size_t delaySamples = static_cast<size_t>(delayMs * sampleRate / 1000.0);
        
        if (delaySamples >= numFrames) {
            cerr << "Warning: delay too long for audio duration\n";
            return;
        }
        
        vector<short> outputData = audioData; // Copy original
        
        for (size_t frame = delaySamples; frame < numFrames; frame++) {
            for (int ch = 0; ch < channels; ch++) {
                double original = getSample(frame, ch);
                double delayed = getSample(frame - delaySamples, ch);
                double mixed = original + feedback * delayed;
                outputData[frame * channels + ch] = clamp16(mixed);
            }
        }
        
        audioData = outputData;
    }
    
    // Effect 2: Multiple Echoes
    void applyMultipleEchoes(const vector<double>& delaysMs, const vector<double>& feedbacks) {
        if (delaysMs.size() != feedbacks.size()) {
            cerr << "Error: delays and feedbacks vectors must have same size\n";
            return;
        }
        
        vector<size_t> delaySamples;
        for (double delayMs : delaysMs) {
            size_t samples = static_cast<size_t>(delayMs * sampleRate / 1000.0);
            if (samples < numFrames) {
                delaySamples.push_back(samples);
            }
        }
        
        vector<short> outputData = audioData; // Copy original
        
        for (size_t frame = 0; frame < numFrames; frame++) {
            for (int ch = 0; ch < channels; ch++) {
                double mixed = getSample(frame, ch);
                
                // Add all echoes
                for (size_t i = 0; i < delaySamples.size() && i < feedbacks.size(); i++) {
                    if (frame >= delaySamples[i]) {
                        double delayed = getSample(frame - delaySamples[i], ch);
                        mixed += feedbacks[i] * delayed;
                    }
                }
                
                outputData[frame * channels + ch] = clamp16(mixed);
            }
        }
        
        audioData = outputData;
    }
    
    // Effect 3: Amplitude Modulation
    void applyAmplitudeModulation(double modFreqHz, double modDepth) {
        for (size_t frame = 0; frame < numFrames; frame++) {
            double time = static_cast<double>(frame) / sampleRate;
            double modulator = 1.0 + modDepth * sin(2.0 * PI * modFreqHz * time);
            
            for (int ch = 0; ch < channels; ch++) {
                double original = getSample(frame, ch);
                double modulated = original * modulator;
                setSample(frame, ch, clamp16(modulated));
            }
        }
    }
    
    // Effect 4: Time-Varying Delay (Chorus/Flanger)
    void applyTimeVaryingDelay(double baseDelayMs, double modulationFreqHz, double modulationDepthMs, double feedback, double wetMix) {
        size_t maxDelay = static_cast<size_t>((baseDelayMs + modulationDepthMs) * sampleRate / 1000.0);
        
        if (maxDelay >= numFrames) {
            cerr << "Warning: maximum delay too long for audio duration\n";
            return;
        }
        
        vector<short> outputData = audioData; // Copy original
        
        for (size_t frame = maxDelay; frame < numFrames; frame++) {
            double time = static_cast<double>(frame) / sampleRate;
            double modulation = modulationDepthMs * sin(2.0 * PI * modulationFreqHz * time);
            double currentDelayMs = baseDelayMs + modulation;
            double currentDelaySamples = currentDelayMs * sampleRate / 1000.0;
            
            // Linear interpolation for fractional delay
            size_t delaySamplesInt = static_cast<size_t>(currentDelaySamples);
            double fraction = currentDelaySamples - delaySamplesInt;
            
            for (int ch = 0; ch < channels; ch++) {
                double original = getSample(frame, ch);
                
                if (frame >= delaySamplesInt + 1) {
                    double delayed1 = getSample(frame - delaySamplesInt, ch);
                    double delayed2 = getSample(frame - delaySamplesInt - 1, ch);
                    double interpolatedDelay = delayed1 * (1.0 - fraction) + delayed2 * fraction;
                    
                    double processed = original + feedback * interpolatedDelay;
                    double mixed = (1.0 - wetMix) * original + wetMix * processed;
                    
                    outputData[frame * channels + ch] = clamp16(mixed);
                } else {
                    outputData[frame * channels + ch] = clamp16(original);
                }
            }
        }
        
        audioData = outputData;
    }
    
    // Effect 5: Ring Modulation
    void applyRingModulation(double carrierFreqHz) {
        for (size_t frame = 0; frame < numFrames; frame++) {
            double time = static_cast<double>(frame) / sampleRate;
            double carrier = sin(2.0 * PI * carrierFreqHz * time);
            
            for (int ch = 0; ch < channels; ch++) {
                double original = getSample(frame, ch);
                double modulated = original * carrier;
                setSample(frame, ch, clamp16(modulated));
            }
        }
    }

    // Effect 6: Distortion (Soft Clipping)
    void applyDistortion(double gain, double threshold) {
        double thresholdValue = threshold * 32767.0;
        
        for (size_t frame = 0; frame < numFrames; frame++) {
            for (int ch = 0; ch < channels; ch++) {
                double original = getSample(frame, ch) * gain;
                double distorted;
                
                if (abs(original) > thresholdValue) {
                    // Soft clipping using tanh
                    distorted = thresholdValue * tanh(original / thresholdValue);
                } else {
                    distorted = original;
                }
                
                setSample(frame, ch, clamp16(distorted));
            }
        }
    }
    
    // Effect 7: Reverse
    void applyReverse() {
        for (size_t frame = 0; frame < numFrames / 2; frame++) {
            size_t reverseFrame = numFrames - 1 - frame;
            
            for (int ch = 0; ch < channels; ch++) {
                short temp = getSample(frame, ch);
                setSample(frame, ch, getSample(reverseFrame, ch));
                setSample(reverseFrame, ch, temp);
            }
        }
    }
    
    // Effect 8: Fade In/Out
    void applyFade(bool fadeIn, double durationMs) {
        size_t fadeSamples = static_cast<size_t>(durationMs * sampleRate / 1000.0);
        fadeSamples = min(fadeSamples, numFrames);
        
        for (size_t frame = 0; frame < fadeSamples; frame++) {
            double fadeMultiplier;
            if (fadeIn) {
                fadeMultiplier = static_cast<double>(frame) / fadeSamples;
            } else {
                fadeMultiplier = static_cast<double>(fadeSamples - frame) / fadeSamples;
            }
            
            size_t targetFrame = fadeIn ? frame : (numFrames - fadeSamples + frame);
            
            for (int ch = 0; ch < channels; ch++) {
                double original = getSample(targetFrame, ch);
                double faded = original * fadeMultiplier;
                setSample(targetFrame, ch, clamp16(faded));
            }
        }
    }
    
    // Utility functions
    int getChannels() const { return channels; }
    int getSampleRate() const { return sampleRate; }
    size_t getNumFrames() const { return numFrames; }
    double getDurationSeconds() const { return static_cast<double>(numFrames) / sampleRate; }
};

void printUsage(const char* programName) {
    cout << "Usage: " << programName << " <effect> [parameters] input.wav output.wav\n\n";
    cout << "Available effects:\n";
    cout << "  echo <delay_ms> <feedback>                    - Single echo\n";
    cout << "  multiecho <delay1,delay2,...> <fb1,fb2,...>  - Multiple echoes\n";
    cout << "  ampmod <freq_hz> <depth>                      - Amplitude modulation\n";
    cout << "  chorus <base_delay_ms> <mod_freq_hz> <mod_depth_ms> <feedback> <wet_mix> - Chorus/Flanger\n";
    cout << "  ringmod <carrier_freq_hz>                     - Ring modulation\n";
    cout << "  distort <gain> <threshold>                    - Soft clipping distortion\n";
    cout << "  reverse                                       - Reverse audio\n";
    cout << "  fadein <duration_ms>                          - Fade in effect\n";
    cout << "  fadeout <duration_ms>                         - Fade out effect\n\n";
    cout << "Examples:\n";
    cout << "  " << programName << " echo 250 0.4 input.wav output.wav\n";
    cout << "  " << programName << " multiecho 100,200,300 0.3,0.2,0.1 input.wav output.wav\n";
    cout << "  " << programName << " ampmod 5.0 0.5 input.wav output.wav\n";
    cout << "  " << programName << " chorus 10 1.5 5 0.3 0.5 input.wav output.wav\n";
    cout << "  " << programName << " distort 2.0 0.7 input.wav output.wav\n";
}

vector<double> parseDoubleList(const string& str) {
    vector<double> result;
    stringstream ss(str);
    string item;
    
    while (getline(ss, item, ',')) {
        try {
            result.push_back(stod(item));
        } catch (const exception& e) {
            cerr << "Error parsing number: " << item << "\n";
        }
    }
    
    return result;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printUsage(argv[0]);
        return 1;
    }
    
    string effect = argv[1];
    string inputFile = argv[argc - 2];
    string outputFile = argv[argc - 1];
    
    AudioEffects processor;
    
    cout << "Loading audio file: " << inputFile << "\n";
    if (!processor.loadAudio(inputFile)) {
        return 1;
    }
    
    cout << "Audio info: " << processor.getChannels() << " channels, "
         << processor.getSampleRate() << " Hz, "
         << processor.getDurationSeconds() << " seconds\n";
    
    cout << "Applying effect: " << effect << "\n";
    
    // Process effects based on command line arguments
    if (effect == "echo" && argc >= 6) {
        double delay = stod(argv[2]);
        double feedback = stod(argv[3]);
        processor.applySingleEcho(delay, feedback);
        
    } else if (effect == "multiecho" && argc >= 6) {
        vector<double> delays = parseDoubleList(argv[2]);
        vector<double> feedbacks = parseDoubleList(argv[3]);
        processor.applyMultipleEchoes(delays, feedbacks);
        
    } else if (effect == "ampmod" && argc >= 6) {
        double freq = stod(argv[2]);
        double depth = stod(argv[3]);
        processor.applyAmplitudeModulation(freq, depth);
        
    } else if (effect == "chorus" && argc >= 9) {
        double baseDelay = stod(argv[2]);
        double modFreq = stod(argv[3]);
        double modDepth = stod(argv[4]);
        double feedback = stod(argv[5]);
        double wetMix = stod(argv[6]);
        processor.applyTimeVaryingDelay(baseDelay, modFreq, modDepth, feedback, wetMix);
        
    } else if (effect == "ringmod" && argc >= 5) {
        double carrierFreq = stod(argv[2]);
        processor.applyRingModulation(carrierFreq);
        
    } else if (effect == "distort" && argc >= 6) {
        double gain = stod(argv[2]);
        double threshold = stod(argv[3]);
        processor.applyDistortion(gain, threshold);
        
    } else if (effect == "reverse" && argc >= 4) {
        processor.applyReverse();
        
    } else if (effect == "fadein" && argc >= 5) {
        double duration = stod(argv[2]);
        processor.applyFade(true, duration);
        
    } else if (effect == "fadeout" && argc >= 5) {
        double duration = stod(argv[2]);
        processor.applyFade(false, duration);
        
    } else {
        cerr << "Error: unknown effect or incorrect number of parameters\n\n";
        printUsage(argv[0]);
        return 1;
    }
    
    cout << "Saving processed audio: " << outputFile << "\n";
    if (!processor.saveAudio(outputFile)) {
        return 1;
    }
    
    cout << "Effect applied successfully!\n";
    return 0;
}