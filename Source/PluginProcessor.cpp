/*
    MultiHost 16 - a VST3 instrument that hosts up to 16 VSTi plugins.
    Copyright (C) 2026 Takasono Makoto

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    SPDX-License-Identifier: AGPL-3.0-or-later

    This program uses the JUCE framework (AGPLv3 / commercial dual licence)
    and the Steinberg VST 3 SDK.
    VST is a trademark of Steinberg Media Technologies GmbH.
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// HostedSlot
//==============================================================================
HostedSlot::HostedSlot (int idx, AudioProcessorValueTreeState& state)
    : index (idx)
{
    auto get = [&] (const char* n) { return state.getParameter (mh::slotParamID (idx, n)); };

    mute      = dynamic_cast<AudioParameterBool*>   (get ("mute"));
    solo      = dynamic_cast<AudioParameterBool*>   (get ("solo"));
    gainDb    = dynamic_cast<AudioParameterFloat*>  (get ("gain"));
    pan       = dynamic_cast<AudioParameterFloat*>  (get ("pan"));
    outBus    = dynamic_cast<AudioParameterChoice*> (get ("out"));
    lowKey    = dynamic_cast<AudioParameterInt*>    (get ("lokey"));
    highKey   = dynamic_cast<AudioParameterInt*>    (get ("hikey"));
    lowVel    = dynamic_cast<AudioParameterInt*>    (get ("lovel"));
    highVel   = dynamic_cast<AudioParameterInt*>    (get ("hivel"));
    transpose = dynamic_cast<AudioParameterInt*>    (get ("trans"));
    midiChan  = dynamic_cast<AudioParameterChoice*> (get ("mch"));

    jassert (mute && solo && gainDb && pan && outBus && lowKey && highKey
             && lowVel && highVel && transpose && midiChan);

    resetNoteMap();
}

void HostedSlot::resetRouting()
{
    for (auto& r : pairRouting)
        r.store (mh::kRouteFollow, std::memory_order_relaxed);
}

void HostedSlot::resetNoteMap()
{
    for (auto& ch : noteMap)
        ch.fill ((int8_t) -1);
}

void HostedSlot::prepare (double sampleRate, int blockSize)
{
    // PDC の最大遅延。子のチャンネル数ぶん確保するので上限を設けておく
    const int maxPdc = juce::jmin (16384, (int) sampleRate);

    gainRamp.setSize (2, juce::jmax (1, blockSize), false, true, true);
    gainRamp.clear();

    skipAfterBlocks = juce::jmax (8, (int) (0.5 * sampleRate / juce::jmax (1, blockSize)));
    silentBlocks = 0;

    smoothedL.reset (sampleRate, 0.02);
    smoothedR.reset (sampleRate, 0.02);
    smoothedL.setCurrentAndTargetValue (0.0f);
    smoothedR.setCurrentAndTargetValue (0.0f);

    midi.ensureSize (2048);
    resetNoteMap();
    wasAudible = false;

    if (plugin != nullptr)
    {
        // マルチアウトは明示的に有効にしたときだけ。既定はメイン出力のみで軽い。
        if (multiOut)
            plugin->enableAllBuses();
        else
            plugin->disableNonMainBuses();

        const int ins  = plugin->getTotalNumInputChannels();
        const int outs = plugin->getTotalNumOutputChannels();

        plugin->setRateAndBufferSizeDetails (sampleRate, blockSize);
        plugin->prepareToPlay (sampleRate, blockSize);

        const int ch = jmax (2, jmax (ins, outs));
        work.setSize (ch, jmax (1, blockSize), false, true, true);
        numOutPairs = multiOut ? jlimit (1, mh::kMaxOutPairs, (jmax (2, outs) + 1) / 2) : 1;
        pdc.prepare (ch, maxPdc);
    }
    else
    {
        work.setSize (2, jmax (1, blockSize), false, true, true);
        numOutPairs = 1;
        pdc.prepare (2, maxPdc);
    }

    work.clear();
}

void HostedSlot::release()
{
    if (plugin != nullptr)
        plugin->releaseResources();
}

bool HostedSlot::isAudible (bool anySolo) const
{
    if (plugin == nullptr)        return false;
    if (mute->get())              return false;
    if (anySolo && ! solo->get()) return false;
    return true;
}

void HostedSlot::routeMidiMessage (const MidiMessage& m, int samplePos)
{
    const int chIndex = midiChan->getIndex();          // 0 = Omni
    const int ch      = m.getChannel();                // 1..16（0 = チャンネル非依存）

    if (ch <= 0)
    {
        midi.addEvent (m, samplePos);                  // システムメッセージ等
        return;
    }

    if (chIndex != 0 && ch != chIndex)
        return;

    const int lo = jmin (lowKey->get(), highKey->get());
    const int hi = jmax (lowKey->get(), highKey->get());
    const int lv = jmin (lowVel->get(), highVel->get());
    const int hv = jmax (lowVel->get(), highVel->get());

    if (m.isNoteOn())
    {
        const int n = m.getNoteNumber();
        const int v = m.getVelocity();
        if (n < lo || n > hi || v < lv || v > hv) return;

        const int tn = jlimit (0, 127, n + transpose->get());
        noteMap[(size_t) (ch - 1)][(size_t) n] = (int8_t) tn;
        midi.addEvent (MidiMessage::noteOn (ch, tn, (uint8) v), samplePos);
    }
    else if (m.isNoteOff())
    {
        const int n = m.getNoteNumber();
        const auto tn = noteMap[(size_t) (ch - 1)][(size_t) n];
        if (tn < 0) return;
        noteMap[(size_t) (ch - 1)][(size_t) n] = (int8_t) -1;
        midi.addEvent (MidiMessage::noteOff (ch, (int) tn, m.getVelocity()), samplePos);
    }
    else if (m.isAftertouch())
    {
        const int n = m.getNoteNumber();
        const auto tn = noteMap[(size_t) (ch - 1)][(size_t) n];
        if (tn < 0) return;
        midi.addEvent (MidiMessage::aftertouchChange (ch, (int) tn, m.getAfterTouchValue()), samplePos);
    }
    else
    {
        // CC / ピッチベンド / チャンネルプレッシャー / プログラムチェンジは透過
        midi.addEvent (m, samplePos);
    }
}

void HostedSlot::flushAllNotes (int samplePos)
{
    for (int c = 0; c < 16; ++c)
    {
        for (int n = 0; n < 128; ++n)
        {
            const auto tn = noteMap[(size_t) c][(size_t) n];
            if (tn >= 0)
            {
                midi.addEvent (MidiMessage::noteOff (c + 1, (int) tn), samplePos);
                noteMap[(size_t) c][(size_t) n] = (int8_t) -1;
            }
        }

        midi.addEvent (MidiMessage::allNotesOff (c + 1), samplePos);
    }
}

//==============================================================================
// 別プロセスでのプラグインスキャン
//
// プラグインのロードは別プロセス (MultiHost16Scanner) にやらせる。
// 相性の悪い音源で落ちても、死ぬのはその子プロセスだけで DAW は無傷。
//==============================================================================
namespace
{
    class OutOfProcessScanner : public KnownPluginList::CustomScanner
    {
    public:
        bool findPluginTypesFor (AudioPluginFormat& format,
                                 OwnedArray<PluginDescription>& result,
                                 const String& fileOrIdentifier) override
        {
            const auto exe = MultiHostAudioProcessor::findScannerExecutable();

            if (! exe.existsAsFile())
                return true;      // スキャナが無い場合は何も追加せず続行

            auto tempFile = File::createTempFile (".mh4scan.xml");

            StringArray args;
            args.add (exe.getFullPathName());
            args.add (format.getName());
            args.add (fileOrIdentifier);
            args.add (tempFile.getFullPathName());

            ChildProcess child;

            if (child.start (args, 0))
            {
                if (! child.waitForProcessToFinish (kTimeoutMs))
                {
                    child.kill();
                }
                else if (child.getExitCode() == 0)
                {
                    if (auto xml = parseXML (tempFile))
                    {
                        for (auto* e : xml->getChildIterator())
                        {
                            PluginDescription desc;
                            if (desc.loadFromXml (*e))
                                result.add (new PluginDescription (desc));
                        }
                    }
                }
            }

            tempFile.deleteFile();
            return true;
        }

    private:
        static constexpr int kTimeoutMs = 60000;   // 1 音源あたり最大 60 秒
    };
}

File MultiHostAudioProcessor::findScannerExecutable()
{
   #if JUCE_WINDOWS
    const String exeName ("MultiHost16Scanner.exe");
   #else
    const String exeName ("MultiHost16Scanner");
   #endif

    // 1) プラグイン本体と同じ場所（バンドルに同梱した場合）
    const auto beside = File::getSpecialLocation (File::currentExecutableFile)
                            .getParentDirectory().getChildFile (exeName);
    if (beside.existsAsFile())
        return beside;

    // 2) ユーザー設定フォルダ（ビルド時 / インストール時にコピーされる場所）
    const auto inAppData = File::getSpecialLocation (File::userApplicationDataDirectory)
                               .getChildFile ("MultiHost16").getChildFile (exeName);
    if (inAppData.existsAsFile())
        return inAppData;

    return {};
}

//==============================================================================
// パラメータ定義
//==============================================================================
AudioProcessorValueTreeState::ParameterLayout MultiHostAudioProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "master_gain", 1 }, "Master Gain",
        NormalisableRange<float> (-60.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    const auto outChoices = mh::outputBusChoices();
    const auto chChoices   = mh::midiChannelChoices();

    auto noteText = [] (int v, int) { return MidiMessage::getMidiNoteName (v, true, true, 3); };

    for (int s = 0; s < mh::kNumSlots; ++s)
    {
        const String pre = "Slot " + String (s + 1) + " ";

        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { mh::slotParamID (s, "mute"), 1 }, pre + "Mute", false));

        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { mh::slotParamID (s, "solo"), 1 }, pre + "Solo", false));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { mh::slotParamID (s, "gain"), 1 }, pre + "Gain",
            NormalisableRange<float> (-60.0f, 12.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("dB")));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { mh::slotParamID (s, "pan"), 1 }, pre + "Pan",
            NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { mh::slotParamID (s, "out"), 1 }, pre + "Output",
            outChoices, jmin (s + 1, mh::kNumAuxOut)));

        layout.add (std::make_unique<AudioParameterInt> (
            ParameterID { mh::slotParamID (s, "lokey"), 1 }, pre + "Low Key", 0, 127, 0,
            AudioParameterIntAttributes().withStringFromValueFunction (noteText)));

        layout.add (std::make_unique<AudioParameterInt> (
            ParameterID { mh::slotParamID (s, "hikey"), 1 }, pre + "High Key", 0, 127, 127,
            AudioParameterIntAttributes().withStringFromValueFunction (noteText)));

        layout.add (std::make_unique<AudioParameterInt> (
            ParameterID { mh::slotParamID (s, "lovel"), 1 }, pre + "Low Vel", 1, 127, 1));

        layout.add (std::make_unique<AudioParameterInt> (
            ParameterID { mh::slotParamID (s, "hivel"), 1 }, pre + "High Vel", 1, 127, 127));

        layout.add (std::make_unique<AudioParameterInt> (
            ParameterID { mh::slotParamID (s, "trans"), 1 }, pre + "Transpose", -48, 48, 0));

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { mh::slotParamID (s, "mch"), 1 }, pre + "MIDI Ch", chChoices, 0));
    }

    return layout;
}

//==============================================================================
// バス構成: Main Mix (stereo) + Out 1..4 (stereo)
//==============================================================================
AudioProcessor::BusesProperties MultiHostAudioProcessor::createBusLayout()
{
    auto b = BusesProperties().withOutput ("Main Mix", AudioChannelSet::stereo(), true);

    for (int i = 0; i < mh::kNumAuxOut; ++i)
        b = b.withOutput ("Out " + String (i + 1), AudioChannelSet::stereo(), false);

    return b;
}

//==============================================================================
MultiHostAudioProcessor::MultiHostAudioProcessor()
    : AudioProcessor (createBusLayout()),
      apvts (*this, nullptr, "MultiHost16State", createLayout())
{
    masterGainDb = dynamic_cast<AudioParameterFloat*> (apvts.getParameter ("master_gain"));

    formatManager.addDefaultFormats();

    // スキャンは必ず別プロセスに任せる（DAW を巻き添えにしないため）
    knownPlugins.setCustomScanner (std::make_unique<OutOfProcessScanner>());

    for (int i = 0; i < mh::kNumSlots; ++i)
        slots[(size_t) i] = std::make_unique<HostedSlot> (i, apvts);

    if (auto* props = getAppProperties())
        if (auto xml = props->getXmlValue ("pluginList"))
            knownPlugins.recreateFromXml (*xml);
}

MultiHostAudioProcessor::~MultiHostAudioProcessor()
{
    const ScopedLock sl (pluginLock);

    for (auto& s : slots)
    {
        if (s != nullptr && s->plugin != nullptr)
        {
            s->plugin->releaseResources();
            s->plugin.reset();
        }
    }
}

PropertiesFile* MultiHostAudioProcessor::getAppProperties()
{
    if (appProperties == nullptr)
    {
        PropertiesFile::Options o;
        o.applicationName     = "MultiHost16";
        o.filenameSuffix      = "settings";
        o.osxLibrarySubFolder = "Application Support";
        o.folderName          = "MultiHost16";

        appProperties = std::make_unique<ApplicationProperties>();
        appProperties->setStorageParameters (o);
    }

    return appProperties->getUserSettings();
}

//==============================================================================
// スキャン関連
//==============================================================================
void MultiHostAudioProcessor::savePluginList()
{
    if (auto* props = getAppProperties())
    {
        if (auto xml = knownPlugins.createXml())
            props->setValue ("pluginList", xml.get());

        props->saveIfNeeded();
    }
}

int MultiHostAudioProcessor::getNumKnownInstruments() const
{
    int n = 0;
    for (const auto& d : knownPlugins.getTypes())
        if (d.isInstrument)
            ++n;
    return n;
}

AudioPluginFormat* MultiHostAudioProcessor::getVST3Format()
{
    for (auto* f : formatManager.getFormats())
        if (f != nullptr && f->getName() == "VST3")
            return f;

    return nullptr;
}

File MultiHostAudioProcessor::getDeadMansPedalFile() const
{
    auto f = File::getSpecialLocation (File::userApplicationDataDirectory)
                 .getChildFile ("MultiHost16").getChildFile ("deadMansPedal");
    f.getParentDirectory().createDirectory();
    return f;
}

FileSearchPath MultiHostAudioProcessor::getExtraScanPaths() const
{
    if (auto* props = const_cast<MultiHostAudioProcessor*> (this)->getAppProperties())
        return FileSearchPath (props->getValue ("extraScanPaths"));

    return {};
}

void MultiHostAudioProcessor::addExtraScanPath (const File& folder)
{
    if (! folder.isDirectory())
        return;

    auto paths = getExtraScanPaths();
    paths.addIfNotAlreadyThere (folder);

    if (auto* props = getAppProperties())
    {
        props->setValue ("extraScanPaths", paths.toString());
        props->saveIfNeeded();
    }
}

FileSearchPath MultiHostAudioProcessor::getScanPaths() const
{
    FileSearchPath paths;

    if (auto* fmt = const_cast<MultiHostAudioProcessor*> (this)->getVST3Format())
        paths = fmt->getDefaultLocationsToSearch();

    const auto extra = getExtraScanPaths();
    for (int i = 0; i < extra.getNumPaths(); ++i)
        paths.addIfNotAlreadyThere (extra[i]);

    return paths;
}

//==============================================================================
bool MultiHostAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.inputBuses.size() > 0
        && layouts.getMainInputChannelSet() != AudioChannelSet::disabled())
        return false;

    if (layouts.getChannelSet (false, 0) != AudioChannelSet::stereo())
        return false;

    for (int i = 1; i < layouts.outputBuses.size(); ++i)
    {
        const auto set = layouts.getChannelSet (false, i);
        if (set != AudioChannelSet::stereo() && set != AudioChannelSet::disabled())
            return false;
    }

    return true;
}

//==============================================================================
void MultiHostAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    const ScopedLock sl (pluginLock);

    for (auto& s : slots)
        s->prepare (sampleRate, samplesPerBlock);

    updateLatency();
}

void MultiHostAudioProcessor::releaseResources()
{
    const ScopedLock sl (pluginLock);

    for (auto& s : slots)
        s->release();
}

void MultiHostAudioProcessor::updateLatency()
{
    int maxLat = 0;

    for (auto& s : slots)
        if (s->plugin != nullptr)
            maxLat = jmax (maxLat, s->plugin->getLatencySamples());

    for (auto& s : slots)
    {
        const int own = (s->plugin != nullptr) ? s->plugin->getLatencySamples() : 0;
        s->pdc.setDelay (maxLat - own);
    }

    setLatencySamples (maxLat);
}

//==============================================================================
void MultiHostAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    const ScopedTryLock stl (pluginLock);
    if (! stl.isLocked())
        return;                       // プラグイン差し替え中は無音

    //--- ソロ状態 -----------------------------------------------------------
    bool anySolo = false;
    for (auto& s : slots)
        if (s->plugin != nullptr && s->solo->get())
            anySolo = true;

    //--- 可聴状態の更新と MIDI の振り分け -----------------------------------
    for (auto& s : slots)
    {
        s->midi.clear();

        const bool audible = s->isAudible (anySolo);
        if (audible != s->wasAudible)
        {
            if (! audible)
                s->flushAllNotes (0);
            s->wasAudible = audible;
        }
    }

    for (const auto meta : midiMessages)
    {
        const auto msg = meta.getMessage();
        const int  pos = jlimit (0, jmax (0, numSamples - 1), meta.samplePosition);

        for (auto& s : slots)
            if (s->wasAudible)
                s->routeMidiMessage (msg, pos);
    }

    //--- 各スロットをレンダリングしてミックス --------------------------------
    const float masterGain = Decibels::decibelsToGain (masterGainDb->get(), -60.0f);

    for (auto& s : slots)
    {
        if (s->plugin == nullptr)
            continue;

        // 無音が続いているスロットは子の処理ごと止めて CPU を節約する
        if (! s->wasAudible)
        {
            if (s->silentBlocks > s->skipAfterBlocks)
            {
                s->activity.store (0.0f, std::memory_order_relaxed);
                continue;
            }
            ++s->silentBlocks;
        }
        else
        {
            s->silentBlocks = 0;
        }

        if (s->work.getNumSamples() < numSamples)
            s->work.setSize (jmax (2, s->work.getNumChannels()), numSamples, false, true, true);

        s->work.clear();

        {
            AudioBuffer<float> sub (s->work.getArrayOfWritePointers(),
                                    s->work.getNumChannels(), 0, numSamples);
            s->plugin->processBlock (sub, s->midi);
        }

        // レイテンシ補正は全チャンネルに適用する
        {
            AudioBuffer<float> all (s->work.getArrayOfWritePointers(),
                                    s->work.getNumChannels(), 0, numSamples);
            s->pdc.process (all, numSamples);
        }

        //--- ゲイン / パン（コンスタントパワー）を 1 回だけ計算し、
        //    全ての出力ペアに同じカーブを使う ----------------------------------
        const float g   = s->wasAudible ? Decibels::decibelsToGain (s->gainDb->get(), -60.0f) : 0.0f;
        const float p   = jlimit (-1.0f, 1.0f, s->pan->get());
        const float ang = (p + 1.0f) * 0.25f * MathConstants<float>::pi;

        s->smoothedL.setTargetValue (g * std::cos (ang) * MathConstants<float>::sqrt2);
        s->smoothedR.setTargetValue (g * std::sin (ang) * MathConstants<float>::sqrt2);

        // UI 表示用のピーク（1 パスで済む SIMD 版を使う）
        {
            const auto range = FloatVectorOperations::findMinAndMax (s->work.getReadPointer (0),
                                                                     numSamples);
            const float peak = jmax (std::abs (range.getStart()), std::abs (range.getEnd())) * g;
            s->activity.store (jmin (1.0f, peak), std::memory_order_relaxed);
        }

        if (s->gainRamp.getNumSamples() < numSamples)
            s->gainRamp.setSize (2, numSamples, false, true, true);

        float* rampL = s->gainRamp.getWritePointer (0);
        float* rampR = s->gainRamp.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            rampL[i] = s->smoothedL.getNextValue() * masterGain;
            rampR[i] = s->smoothedR.getNextValue() * masterGain;
        }

        //--- 出力ペアごとに送り先バスへ加算 ----------------------------------
        const int childCh   = s->work.getNumChannels();
        const int usedPairs = jlimit (1, mh::kMaxOutPairs, jmin (s->numOutPairs, (childCh + 1) / 2));
        const int slotBus   = s->outBus->getIndex();

        for (int pair = 0; pair < usedPairs; ++pair)
        {
            const int route = s->pairRouting[(size_t) pair].load (std::memory_order_relaxed);

            if (route == mh::kRouteOff)
                continue;

            int busIdx = (route == mh::kRouteFollow) ? slotBus : route;

            if (busIdx < 0 || busIdx >= getBusCount (false))
                busIdx = 0;

            if (auto* bus = getBus (false, busIdx))
                if (! bus->isEnabled())
                    busIdx = 0;                    // 無効なバスは Main へ回す

            auto dest = getBusBuffer (buffer, false, busIdx);
            if (dest.getNumChannels() < 1)
                continue;

            const int chL = pair * 2;
            const int chR = jmin (chL + 1, childCh - 1);
            if (chL >= childCh)
                break;

            const float* srcL = s->work.getReadPointer (chL);
            const float* srcR = s->work.getReadPointer (chR);

            float* dstL = dest.getWritePointer (0);
            float* dstR = dest.getNumChannels() > 1 ? dest.getWritePointer (1) : nullptr;

            for (int i = 0; i < numSamples; ++i)
            {
                dstL[i] += srcL[i] * rampL[i];
                if (dstR != nullptr)
                    dstR[i] += srcR[i] * rampR[i];
            }
        }
    }
}

//==============================================================================
// スロットのロード / アンロード（メッセージスレッド）
//==============================================================================
bool MultiHostAudioProcessor::loadPluginIntoSlot (int slotIndex, const PluginDescription& desc)
{
    jassert (MessageManager::getInstance()->isThisTheMessageThread());

    if (! isPositiveAndBelow (slotIndex, mh::kNumSlots))
        return false;

    auto& slot = *slots[(size_t) slotIndex];

    // エフェクトなど、インストゥルメント以外は受け付けない
    if (! desc.isInstrument)
    {
        slot.lastError = "Not an instrument";
        sendChangeMessage();
        return false;
    }

    String err;
    auto instance = formatManager.createPluginInstance (desc, currentSampleRate,
                                                        currentBlockSize, err);

    if (instance == nullptr)
    {
        slot.lastError = err;
        sendChangeMessage();
        return false;
    }

    // 読み込み直後はメイン出力のみ。マルチアウトは後から任意で有効化する。
    const int availableBuses = instance->getBusCount (false);
    instance->disableNonMainBuses();

    const int ins  = instance->getTotalNumInputChannels();
    const int outs = instance->getTotalNumOutputChannels();
    instance->setRateAndBufferSizeDetails (currentSampleRate, currentBlockSize);
    instance->prepareToPlay (currentSampleRate, currentBlockSize);

    {
        suspendProcessing (true);
        const ScopedLock sl (pluginLock);

        if (slot.plugin != nullptr)
        {
            slot.plugin->releaseResources();
            slot.plugin.reset();
        }

        slot.plugin      = std::move (instance);
        slot.description = desc;
        slot.lastError   = {};
        const int ch = jmax (2, jmax (ins, outs));
        slot.work.setSize (ch, jmax (1, currentBlockSize), false, true, true);
        slot.work.clear();
        slot.numOutPairs = 1;
        slot.availableOutBuses = availableBuses;
        slot.multiOut = false;
        slot.pdc.prepare (ch, jmin (16384, (int) currentSampleRate));
        slot.resetRouting();
        slot.resetNoteMap();
        slot.wasAudible = false;
        slot.silentBlocks = 0;

        updateLatency();
        suspendProcessing (false);
    }

    sendChangeMessage();
    return true;
}

void MultiHostAudioProcessor::clearSlot (int slotIndex)
{
    jassert (MessageManager::getInstance()->isThisTheMessageThread());

    if (! isPositiveAndBelow (slotIndex, mh::kNumSlots))
        return;

    std::unique_ptr<AudioPluginInstance> doomed;

    {
        suspendProcessing (true);
        const ScopedLock sl (pluginLock);

        auto& slot = *slots[(size_t) slotIndex];
        doomed = std::move (slot.plugin);
        slot.description = {};
        slot.numOutPairs = 1;
        slot.availableOutBuses = 1;
        slot.multiOut = false;
        slot.resetRouting();
        slot.resetNoteMap();
        slot.wasAudible = false;
        slot.silentBlocks = 0;

        updateLatency();
        suspendProcessing (false);
    }

    if (doomed != nullptr)
        doomed->releaseResources();

    doomed.reset();
    sendChangeMessage();
}

void MultiHostAudioProcessor::setSlotMultiOut (int slotIndex, bool shouldBeMultiOut)
{
    jassert (MessageManager::getInstance()->isThisTheMessageThread());

    if (! isPositiveAndBelow (slotIndex, mh::kNumSlots))
        return;

    auto& slot = *slots[(size_t) slotIndex];

    if (slot.plugin == nullptr || slot.multiOut == shouldBeMultiOut)
        return;

    {
        suspendProcessing (true);
        const ScopedLock sl (pluginLock);

        slot.plugin->releaseResources();
        slot.multiOut = shouldBeMultiOut;
        slot.prepare (currentSampleRate, currentBlockSize);

        updateLatency();
        suspendProcessing (false);
    }

    sendChangeMessage();
}

//==============================================================================
// 状態の保存 / 復元
//==============================================================================
void MultiHostAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    XmlElement root ("MULTIHOST16");

    if (auto state = apvts.copyState().createXml())
        root.addChildElement (new XmlElement (*state));

    const ScopedLock sl (pluginLock);

    for (int i = 0; i < mh::kNumSlots; ++i)
    {
        auto& slot = *slots[(size_t) i];
        auto* e = root.createNewChildElement ("SLOT");
        e->setAttribute ("index", i);

        if (slot.plugin != nullptr)
        {
            e->addChildElement (slot.description.createXml().release());

            MemoryBlock mb;
            slot.plugin->getStateInformation (mb);
            e->setAttribute ("state", mb.toBase64Encoding());

            StringArray routes;
            for (int pr = 0; pr < mh::kMaxOutPairs; ++pr)
                routes.add (String (slot.pairRouting[(size_t) pr].load()));
            e->setAttribute ("routing", routes.joinIntoString (","));
            e->setAttribute ("multiOut", slot.multiOut ? 1 : 0);
        }
    }

    copyXmlToBinary (root, destData);
}

void MultiHostAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName ("MULTIHOST16"))
        return;

    if (auto* p = xml->getChildByName (apvts.state.getType().toString()))
        apvts.replaceState (ValueTree::fromXml (*p));

    pendingState = std::move (xml);

    WeakReference<MultiHostAudioProcessor> weak (this);
    auto job = [weak]
    {
        if (auto* self = weak.get())
            self->applyPendingState();
    };

    if (MessageManager::getInstance()->isThisTheMessageThread())
        job();
    else
        MessageManager::callAsync (job);
}

void MultiHostAudioProcessor::applyPendingState()
{
    if (pendingState == nullptr)
        return;

    auto xml = std::move (pendingState);

    for (auto* e : xml->getChildWithTagNameIterator ("SLOT"))
    {
        const int idx = e->getIntAttribute ("index", -1);
        if (! isPositiveAndBelow (idx, mh::kNumSlots))
            continue;

        auto* descXml = e->getFirstChildElement();

        if (descXml == nullptr)
        {
            clearSlot (idx);
            continue;
        }

        PluginDescription desc;
        if (! desc.loadFromXml (*descXml))
        {
            clearSlot (idx);
            continue;
        }

        if (loadPluginIntoSlot (idx, desc))
        {
            MemoryBlock mb;
            if (mb.fromBase64Encoding (e->getStringAttribute ("state")) && mb.getSize() > 0)
            {
                const ScopedLock sl (pluginLock);
                if (auto* p = slots[(size_t) idx]->plugin.get())
                    p->setStateInformation (mb.getData(), (int) mb.getSize());
            }

            if (e->getIntAttribute ("multiOut", 0) != 0)
                setSlotMultiOut (idx, true);

            const auto routes = StringArray::fromTokens (e->getStringAttribute ("routing"), ",", "");
            auto& slot = *slots[(size_t) idx];
            for (int pr = 0; pr < jmin (routes.size(), mh::kMaxOutPairs); ++pr)
            {
                const int v = routes[pr].getIntValue();
                if (v >= mh::kRouteOff && v <= mh::kNumAuxOut)
                    slot.pairRouting[(size_t) pr].store (v);
            }
        }
    }

    sendChangeMessage();
}

//==============================================================================
AudioProcessorEditor* MultiHostAudioProcessor::createEditor()
{
    return new MultiHostAudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MultiHostAudioProcessor();
}
