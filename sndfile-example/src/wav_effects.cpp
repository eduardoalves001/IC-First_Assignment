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
#include <algorithm>
#include <iomanip>

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // Buffer for reading/writing frames
constexpr double PI = 3.14159265358979323846;

// Base class for audio effects
class AudioEffect {
public:
    virtual ~AudioEffect() = default;
    virtual void process(vector<short>& samples, int nChannels, int sampleRate) = 0;
    virtual string getName() const = 0;
    virtual string getDescription() const = 0;
};

// Echo effect implementation
class EchoEffect : public AudioEffect {
private:
    double delaySeconds;
    double attenuation;
    vector<vector<short>> delayBuffer;
    size_t delaySize;
    size_t writeIndex;
    
public:
    EchoEffect(double delay_sec, double atten) 
        : delaySeconds(delay_sec), attenuation(atten), writeIndex(0) {}
    
    void initialize(int sampleRate, int nChannels) {
        delaySize = static_cast<size_t>(delaySeconds * sampleRate);
        delayBuffer.resize(nChannels, vector<short>(delaySize, 0));
        writeIndex = 0;
    }
    
    void process(vector<short>& samples, int nChannels, int sampleRate) override {
        if (delayBuffer.empty()) {
            initialize(sampleRate, nChannels);
        }
        
        size_t nFrames = samples.size() / nChannels;
        
        for (size_t frame = 0; frame < nFrames; frame++) {
            for (int ch = 0; ch < nChannels; ch++) {
                size_t idx = frame * nChannels + ch;
                
                // Get delayed sample
                short delayedSample = delayBuffer[ch][writeIndex];
                
                // Mix original with delayed sample
                double mixed = static_cast<double>(samples[idx]) + 
                              static_cast<double>(delayedSample) * attenuation;
                
                // Clamp to prevent overflow
                mixed = max(-32768.0, min(32767.0, mixed));
                samples[idx] = static_cast<short>(mixed);
                
                // Store current sample in delay buffer
                delayBuffer[ch][writeIndex] = static_cast<short>(
                    static_cast<double>(samples[idx]) * (1.0 - attenuation));
            }
            
            writeIndex = (writeIndex + 1) % delaySize;
        }
    }
    
    string getName() const override { return "Echo"; }
    string getDescription() const override {
        return "Single echo with " + to_string(delaySeconds) + "s delay, " + 
               to_string(attenuation) + " attenuation";
    }
};

// Multiple echo effect
class MultipleEchoEffect : public AudioEffect {
private:
    vector<unique_ptr<EchoEffect>> echos;
    
public:
    MultipleEchoEffect(const vector<pair<double, double>>& echo_params) {
        for (const auto& params : echo_params) {
            echos.push_back(make_unique<EchoEffect>(params.first, params.second));
        }
    }
    
    void process(vector<short>& samples, int nChannels, int sampleRate) override {
        for (auto& echo : echos) {
            echo->process(samples, nChannels, sampleRate);
        }
    }
    
    string getName() const override { return "Multiple Echos"; }
    string getDescription() const override {
        return "Multiple echos with " + to_string(echos.size()) + " repetitions";
    }
};

// Amplitude modulation effect (Tremolo)
class AmplitudeModulationEffect : public AudioEffect {
private:
    double frequency;
    double depth;
    double phase;
    
public:
    AmplitudeModulationEffect(double freq, double mod_depth) 
        : frequency(freq), depth(mod_depth), phase(0.0) {}
    
    void process(vector<short>& samples, int nChannels, int sampleRate) override {
        size_t nFrames = samples.size() / nChannels;
        double phaseIncrement = 2.0 * PI * frequency / sampleRate;
        
        for (size_t frame = 0; frame < nFrames; frame++) {
            // Calculate modulation factor
            double modulation = 1.0 + depth * sin(phase);
            
            for (int ch = 0; ch < nChannels; ch++) {
                size_t idx = frame * nChannels + ch;
                
                // Apply amplitude modulation
                double modulated = static_cast<double>(samples[idx]) * modulation;
                
                // Clamp to prevent overflow
                modulated = max(-32768.0, min(32767.0, modulated));
                samples[idx] = static_cast<short>(modulated);
            }
            
            phase += phaseIncrement;
            if (phase >= 2.0 * PI) {
                phase -= 2.0 * PI;
            }
        }
    }
    
    string getName() const override { return "Amplitude Modulation"; }
    string getDescription() const override {
        return "Tremolo at " + to_string(frequency) + "Hz, depth " + to_string(depth);
    }
};

// Time-varying delay effect (Chorus/Flanger)
class TimeVaryingDelayEffect : public AudioEffect {
private:
    double baseDelay;
    double modDepth;
    double modFrequency;
    double phase;
    vector<vector<short>> delayBuffer;
    size_t maxDelaySize;
    size_t writeIndex;
    
public:
    TimeVaryingDelayEffect(double base_delay, double mod_depth, double mod_freq)
        : baseDelay(base_delay), modDepth(mod_depth), modFrequency(mod_freq), 
          phase(0.0), writeIndex(0) {}
    
    void initialize(int sampleRate, int nChannels) {
        maxDelaySize = static_cast<size_t>((baseDelay + modDepth) * sampleRate);
        delayBuffer.resize(nChannels, vector<short>(maxDelaySize, 0));
        writeIndex = 0;
    }
    
    void process(vector<short>& samples, int nChannels, int sampleRate) override {
        if (delayBuffer.empty()) {
            initialize(sampleRate, nChannels);
        }
        
        size_t nFrames = samples.size() / nChannels;
        double phaseIncrement = 2.0 * PI * modFrequency / sampleRate;
        
        for (size_t frame = 0; frame < nFrames; frame++) {
            // Calculate current delay
            double currentDelay = baseDelay + modDepth * sin(phase);
            size_t delaySamples = static_cast<size_t>(currentDelay * sampleRate);
            
            for (int ch = 0; ch < nChannels; ch++) {
                size_t idx = frame * nChannels + ch;
                
                // Calculate read position
                size_t readIndex = (writeIndex + maxDelaySize - delaySamples) % maxDelaySize;
                
                // Get delayed sample
                short delayedSample = delayBuffer[ch][readIndex];
                
                // Mix original with delayed sample
                double mixed = static_cast<double>(samples[idx]) * 0.7 + 
                              static_cast<double>(delayedSample) * 0.3;
                
                // Clamp to prevent overflow
                mixed = max(-32768.0, min(32767.0, mixed));
                samples[idx] = static_cast<short>(mixed);
                
                // Store current sample in delay buffer
                delayBuffer[ch][writeIndex] = samples[idx];
            }
            
            writeIndex = (writeIndex + 1) % maxDelaySize;
            phase += phaseIncrement;
            if (phase >= 2.0 * PI) {
                phase -= 2.0 * PI;
            }
        }
    }
    
    string getName() const override { return "Time-Varying Delay"; }
    string getDescription() const override {
        return "Chorus/Flanger with " + to_string(baseDelay) + "s base delay, " +
               to_string(modDepth) + "s modulation depth at " + to_string(modFrequency) + "Hz";
    }
};

// Function to show available effects
void showEffects() {
    cout << "Available effects:\n";
    cout << "  echo <delay_sec> <attenuation>           - Single echo\n";
    cout << "  multi_echo <n> <delay1> <atten1> ...     - Multiple echos\n";
    cout << "  tremolo <freq_hz> <depth>                - Amplitude modulation\n";
    cout << "  chorus <base_delay> <mod_depth> <mod_freq> - Time-varying delay\n";
    cout << "\nExamples:\n";
    cout << "  " << "echo 0.3 0.4" << "                        - Echo with 300ms delay, 40% attenuation\n";
    cout << "  " << "multi_echo 2 0.2 0.3 0.4 0.2" << "        - Two echos\n";
    cout << "  " << "tremolo 4.0 0.5" << "                     - 4Hz tremolo, 50% depth\n";
    cout << "  " << "chorus 0.02 0.005 0.8" << "               - Chorus effect\n";
}

unique_ptr<AudioEffect> parseEffect(const vector<string>& args, size_t& argIndex) {
    if (argIndex >= args.size()) {
        return nullptr;
    }
    
    string effectName = args[argIndex++];
    
    if (effectName == "echo") {
        if (argIndex + 1 >= args.size()) {
            cerr << "Error: echo requires 2 parameters (delay_sec, attenuation)\n";
            return nullptr;
        }
        double delay = stod(args[argIndex++]);
        double attenuation = stod(args[argIndex++]);
        return make_unique<EchoEffect>(delay, attenuation);
        
    } else if (effectName == "multi_echo") {
        if (argIndex >= args.size()) {
            cerr << "Error: multi_echo requires number of echos parameter\n";
            return nullptr;
        }
        int numEchos = stoi(args[argIndex++]);
        if (argIndex + numEchos * 2 > args.size()) {
            cerr << "Error: multi_echo requires " << numEchos * 2 << " more parameters\n";
            return nullptr;
        }
        
        vector<pair<double, double>> echoParams;
        for (int i = 0; i < numEchos; i++) {
            double delay = stod(args[argIndex++]);
            double attenuation = stod(args[argIndex++]);
            echoParams.push_back({delay, attenuation});
        }
        return make_unique<MultipleEchoEffect>(echoParams);
        
    } else if (effectName == "tremolo") {
        if (argIndex + 1 >= args.size()) {
            cerr << "Error: tremolo requires 2 parameters (frequency, depth)\n";
            return nullptr;
        }
        double frequency = stod(args[argIndex++]);
        double depth = stod(args[argIndex++]);
        return make_unique<AmplitudeModulationEffect>(frequency, depth);
        
    } else if (effectName == "chorus") {
        if (argIndex + 2 >= args.size()) {
            cerr << "Error: chorus requires 3 parameters (base_delay, mod_depth, mod_freq)\n";
            return nullptr;
        }
        double baseDelay = stod(args[argIndex++]);
        double modDepth = stod(args[argIndex++]);
        double modFreq = stod(args[argIndex++]);
        return make_unique<TimeVaryingDelayEffect>(baseDelay, modDepth, modFreq);
        
    } else {
        cerr << "Error: unknown effect '" << effectName << "'\n";
        return nullptr;
    }
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    
    if(argc < 4) {
        cerr << "Usage: " << argv[0] << " [ -v (verbose) ] inputFile outputFile effect [params...]\n";
        cerr << "       Apply audio effects to a WAV file\n";
        showEffects();
        return 1;
    }
    
    // Parse arguments
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
    
    // Parse effect
    size_t effectArgIndex = 2;
    auto effect = parseEffect(args, effectArgIndex);
    if (!effect) {
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
    
    int nChannels = sfhIn.channels();
    int sampleRate = sfhIn.samplerate();
    size_t totalFrames = sfhIn.frames();
    
    if(verbose) {
        cout << "Processing audio file:\n";
        cout << "  Input: " << inputFile << "\n";
        cout << "  Output: " << outputFile << "\n";
        cout << "  Frames: " << totalFrames << "\n";
        cout << "  Sample rate: " << sampleRate << " Hz\n";
        cout << "  Channels: " << nChannels << "\n";
        cout << "  Effect: " << effect->getName() << "\n";
        cout << "  Description: " << effect->getDescription() << "\n\n";
    }
    
    // Open output file
    SndfileHandle sfhOut { outputFile, SFM_WRITE, sfhIn.format(),
                          nChannels, sampleRate };
    if(sfhOut.error()) {
        cerr << "Error: invalid output file\n";
        return 1;
    }
    
    // Process audio in chunks
    size_t nFrames;
    vector<short> samples(FRAMES_BUFFER_SIZE * nChannels);
    size_t totalFramesProcessed = 0;
    
    while((nFrames = sfhIn.readf(samples.data(), FRAMES_BUFFER_SIZE))) {
        samples.resize(nFrames * nChannels);
        
        // Apply effect
        effect->process(samples, nChannels, sampleRate);
        
        // Write processed samples to output file
        sfhOut.writef(samples.data(), nFrames);
        totalFramesProcessed += nFrames;
        
        if(verbose && totalFramesProcessed % (sampleRate * 2) == 0) {
            cout << "Processed " << totalFramesProcessed << " frames...\n";
        }
        
        samples.resize(FRAMES_BUFFER_SIZE * nChannels);
    }
    
    if(verbose) {
        cout << "Effect processing completed. Total frames processed: " << totalFramesProcessed << "\n";
        cout << "Output saved to: " << outputFile << "\n";
    }
    
    return 0;
}