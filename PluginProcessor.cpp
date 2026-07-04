#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
WhooshAudioProcessor::WhooshAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       parameters (*this, nullptr, juce::Identifier ("params"),
{
         std::make_unique<juce::AudioParameterChoice>("rate", "Rate",
                                                     juce::StringArray{"1/1", "1/2", "1/4", "1/8", "1/16"}, //rate choices
                                                     2), //default to 1/4
         std::make_unique<juce::AudioParameterFloat>("floor", "Floor", 0.0f, 1.0f, 0.0f), //point A height
         std::make_unique<juce::AudioParameterFloat>("recovery", "Recovery", 0.0f, 1.0f, 0.5f), // point B phase
         std::make_unique<juce::AudioParameterChoice>("curve", "Curve",
                                                     juce::StringArray{"Exponential", "Logarithmic", "Linear"}, //curve choices
                                                     0), // default to exponential
    })
     {
         //access the value of each parameter
         rateParam = parameters.getRawParameterValue("rate");
         floorParam = parameters.getRawParameterValue("floor");
         recoveryParam = parameters.getRawParameterValue("recovery");
         curveParam = parameters.getRawParameterValue("curve");
     }
                       
WhooshAudioProcessor::~WhooshAudioProcessor()
{
}

//==============================================================================
const juce::String WhooshAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool WhooshAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool WhooshAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool WhooshAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double WhooshAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int WhooshAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int WhooshAudioProcessor::getCurrentProgram()
{
    return 0;
}

void WhooshAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String WhooshAudioProcessor::getProgramName (int index)
{
    return {};
}

void WhooshAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void WhooshAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    freeRunPhase = 0.0; //reset internal clock
    
    // waveform scope setup (UI)
    scopeBuffer.fill (0.0f);
    lastScopePhase = 0.0f;
}

void WhooshAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool WhooshAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

//take in new values and regenerate the lookup table==================================================
void WhooshAudioProcessor::regenerateCurveTable (float floorLevel, float recoveryPhase, int curveType){
    //using one power formula but different p values to create the curve
    float p;
    switch (curveType)
    {
        case 0:  p = 3.0f;  break; // exponential: start slow then fast
        case 1:  p = 0.33f; break; // logarithmic: start fast then slow
        default: p = 1.0f;  break; // linear: straight line
    }

    //calculate the index of point B
    const int recoveryEnd = juce::jlimit (1, tableSize - 1, (int) (recoveryPhase * tableSize));

    //loop through the table and recalculate
    for (int i = 0; i < tableSize; ++i)
    {
        if (i < recoveryEnd) //if the index is before point B
        {
            const float t = (float) i / (float) recoveryEnd; //calculate current position in the curve
            const float shaped = std::pow (t, p); //calculate the current amplitude based on selected curve type
            curveTable[(size_t) i] = floorLevel + (1.0f - floorLevel) * shaped; //vertical shift up to floor level
        }
        else
        {
            curveTable[(size_t) i] = 1.0f; //after point B, just hold still
        }
    }
}
//=====================================================================================================

void WhooshAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals; //CPU safety; treat tiny numbers as plain 0's
    auto* const* channelData = buffer.getArrayOfWritePointers(); //get write pointers of all channels
    const int numSamples = buffer.getNumSamples();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    
    //declaring default values and assigning them for fallback safety--------------------------
    double bpm = 120.0;
    double ppq = 0.0; //Pulses Per Quarter note, aka how many quarter notes have passed
    bool playing = false; //track whether playback is active
    //------------------------------------------------------------------------------------------
    
    //Access & Assign information from current project------------------------------------------
    if (auto* playHead = getPlayHead()){ //does the host have a playhead?
        if (auto pos = playHead -> getPosition()){ //get the current position of playhead
            if (auto b = pos -> getBpm())
                bpm = *b; //access & store BPM
            if (auto p = pos -> getPpqPosition())
                ppq = *p; //access & store ppq
            playing = pos -> getIsPlaying(); //is playback active right now?
        }
    }
    //-------------------------------------------------------------------------------------------
            
    //Cycle calculations-------------------------------------------------------------------------
    //access user choice of cycle
    const int rateIndex = (int) *rateParam;
    const double cycleTable[] = {4.0, 2.0, 1.0, 0.5, 0.25}; //1/1, 1/2, 1/4, 1/8, 1/16
    const double cycleQuarters = cycleTable[rateIndex];
    
    //calculate per sample info
    const double ppqPerSample = (bpm / 60.0) / getSampleRate(); //calculate beats per sample
    const double phasePerSample = ppqPerSample / cycleQuarters; //calculate cycle per sample
    //-------------------------------------------------------------------------------------------
            
    //Clock setup--------------------------------------------------------------------------------
    double basePhase; //phase according to host
    
    if (playing) //follow basePhase if playback is active
        basePhase = (float) std::fmod (ppq / cycleQuarters, 1.0); //calculate the cycle we're in
                                                                  //and take the division remainder to find our position in the cycle
    else
        basePhase = freeRunPhase; //follow internal clock if playback is not active to prevent clicks
    
    if (basePhase < 0.0)
        basePhase += 1.0; //safety line
    //-------------------------------------------------------------------------------------------
    
    //Check & rebuild lookup table if change happens---------------------------------------------
    const float floorLevel = *floorParam;
    const float recoveryPhase = *recoveryParam;
    const int curveType = (int) *curveParam;
    
    //check if any changes have occured
    if (std::abs (floorLevel - lastFloor) > 1.0e-4f
         || std::abs (recoveryPhase - lastRecovery) > 1.0e-4f
         || curveType != lastCurve){
        regenerateCurveTable (floorLevel, recoveryPhase, curveType);
        lastFloor = floorLevel; lastRecovery = recoveryPhase; lastCurve = curveType; //update info
    }
    //-------------------------------------------------------------------------------------------
    
    //Processing and whooshing magic-------------------------------------------------------------
    for (int sample = 0; sample < numSamples; ++sample){
        //calculate current phase of LFO, and wrap around if it passes 1.0
        float phase = (float) std::fmod (basePhase + sample * phasePerSample, 1.0);

        //read lookup table
        float tablePos = phase * (float) tableSize; //calculate corresponding index in table
        //linear interpolation
            int index0 = (int) tablePos; //round tablePos down
            int index1 = (index0 + 1) % tableSize; //next index above tablePos, wrap around if necessary
            float frac = tablePos - (float) index0; //how far tablePos is from index0
        
        float inc = (curveTable[(size_t) index1] - curveTable[(size_t) index0]) * frac; //calculate the increase
        float gain = curveTable[(size_t) index0] + inc; //add increase to index0 value
        //--------------------------------------------------------------------

        //apply the gain
        for (int channel = 0; channel < totalNumOutputChannels; ++channel)
            channelData[channel][sample] *= gain;
        
        //UI setup---------------------------------------------------------------------------
        if (totalNumOutputChannels > 0){
            const int slot = juce::jlimit (0, scopeSize - 1, (int) (phase * scopeSize));
            scopeBuffer[(size_t) slot] = juce::jmax (scopeBuffer[(size_t) slot],
                                                     std::abs (channelData[0][sample]));
        }
        
        //--------------------------------------------------------------------
        }
        const float endPhase = (float) std::fmod (basePhase + numSamples * phasePerSample, 1.0);
        if (endPhase < (float) basePhase)              // cycle boundary crossed this block
            for (auto& s : scopeBuffer) s *= 0.9f;
        freeRunPhase = std::fmod (basePhase + numSamples * phasePerSample, 1.0);
    //--------------------------------------------------------------------------------------------
}

//==============================================================================
bool WhooshAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* WhooshAudioProcessor::createEditor()
{
    return new WhooshAudioProcessorEditor (*this);
}

//==============================================================================
void WhooshAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    //get the same control settings as when you closed the application
    if (auto xml = parameters.copyState().createXml())
            copyXmlToBinary (*xml, destData);
}

void WhooshAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    //store the control settings before closing
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WhooshAudioProcessor();
}
