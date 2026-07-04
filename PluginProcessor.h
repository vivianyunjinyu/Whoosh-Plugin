#pragma once

#include <JuceHeader.h>

class WhooshAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    WhooshAudioProcessor();
    ~WhooshAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    juce::AudioProcessorValueTreeState parameters;
    
    //UI stuffs:
    static constexpr int scopeSize = 1024;
    std::array<float, scopeSize> scopeBuffer {};

private:
    
    //control parameters-------------------------------------------------------
    std::atomic<float>* rateParam = nullptr; //user selected modulation rate
    std::atomic<float>* floorParam = nullptr; //Point A at phase 0; 0 = full duck, 1 = no duck
    std::atomic<float>* recoveryParam = nullptr; //Point B at amplitude 1; moveable in phase cycle location
    std::atomic<float>* curveParam = nullptr; //curve type choice
    //--------------------------------------------------------------------------
    
    //lookup table setup--------------------------------------------------------
    static constexpr int tableSize = 1024;
    std::array<float, tableSize> curveTable; //array to store the lookup table
    
    //helper function to regenerate the table when change happens
    void regenerateCurveTable (float floorLevel, float recoveryPhase, int curveType);
    
    //impossible initial value to detect change
    float lastFloor    = -1.0f;
    float lastRecovery = -1.0f;
    int   lastCurve    = -1;
    //---------------------------------------------------------------------------
    
    double freeRunPhase = 0.0; //declares internal clock independent of the host time
    
    float lastScopePhase = 0.0f; //UI setup
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WhooshAudioProcessor)
};
