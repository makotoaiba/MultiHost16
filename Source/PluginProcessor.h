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

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>
#include <memory>

namespace mh
{
    static constexpr int kNumSlots    = 16;  // 登録できる音源の数
    static constexpr int kNumAuxOut   = 16;  // パラアウト用ステレオバス数
    static constexpr int kMaxOutPairs = 16;  // 子プラグイン1台から受け取るステレオペア数の上限

    // 子プラグインの各出力ペアの送り先
    static constexpr int kRouteFollow = -1;  // スロットの Out 設定に従う（既定）
    static constexpr int kRouteOff    = -2;  // このペアは出力しない

    inline juce::String slotParamID (int slot, const char* name)
    {
        return "s" + juce::String (slot + 1) + "_" + name;
    }

    /** 出力先バスの選択肢。
        パラメータ定義と ComboBox の中身は必ずこの関数から作ること。
        （両者の項目数が食い違うと ComboBoxAttachment が正しく動かない） */
    inline juce::StringArray outputBusChoices()
    {
        juce::StringArray a { "Main Mix" };
        for (int i = 1; i <= kNumAuxOut; ++i)
            a.add ("Out " + juce::String (i));
        return a;
    }

    /** 受信 MIDI チャンネルの選択肢。 */
    inline juce::StringArray midiChannelChoices()
    {
        juce::StringArray a { "Omni" };
        for (int i = 1; i <= 16; ++i)
            a.add ("Ch " + juce::String (i));
        return a;
    }
}

//==============================================================================
/** レイテンシ補正用の単純な整数ディレイ。 */
struct SimpleDelay
{
    void prepare (int numChannels, int maxDelaySamples)
    {
        maxDelay = juce::jmax (1, maxDelaySamples);
        buffer.setSize (juce::jmax (1, numChannels), maxDelay + 1);
        buffer.clear();
        writePos = 0;
    }

    void setDelay (int samples)
    {
        const int d = juce::jlimit (0, maxDelay, samples);
        if (d != delay) { delay = d; buffer.clear(); }
    }

    void process (juce::AudioBuffer<float>& io, int numSamples)
    {
        const int len = buffer.getNumSamples();
        if (len <= 1) return;
        const int chans = juce::jmin (io.getNumChannels(), buffer.getNumChannels());

        for (int ch = 0; ch < chans; ++ch)
        {
            auto* d = io.getWritePointer (ch);
            auto* b = buffer.getWritePointer (ch);
            int wp = writePos;

            for (int i = 0; i < numSamples; ++i)
            {
                int rp = wp - delay;
                if (rp < 0) rp += len;
                b[wp] = d[i];
                d[i]  = b[rp];
                if (++wp >= len) wp = 0;
            }
        }

        writePos = (writePos + numSamples) % len;
    }

    juce::AudioBuffer<float> buffer;
    int writePos = 0, delay = 0, maxDelay = 1;
};

//==============================================================================
/** 1 スロット = ホストされた VSTi 1 台 + キーレンジ / ミキサー設定。 */
struct HostedSlot
{
    HostedSlot (int idx, juce::AudioProcessorValueTreeState& state);

    //--- パラメータ（Cubase からオートメーション可能） -----------------------
    juce::AudioParameterBool*   mute      = nullptr;
    juce::AudioParameterBool*   solo      = nullptr;
    juce::AudioParameterFloat*  gainDb    = nullptr;
    juce::AudioParameterFloat*  pan       = nullptr;
    juce::AudioParameterChoice* outBus    = nullptr;   // 0 = Main Mix, 1..4 = Out 1..4
    juce::AudioParameterInt*    lowKey    = nullptr;
    juce::AudioParameterInt*    highKey   = nullptr;
    juce::AudioParameterInt*    lowVel    = nullptr;
    juce::AudioParameterInt*    highVel   = nullptr;
    juce::AudioParameterInt*    transpose = nullptr;
    juce::AudioParameterChoice* midiChan  = nullptr;   // 0 = Omni, 1..16

    //--- ホストされたプラグイン ---------------------------------------------
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::PluginDescription                    description;
    juce::String                               lastError;

    //--- DSP ワーク ----------------------------------------------------------
    juce::AudioBuffer<float>   work;
    juce::AudioBuffer<float>   gainRamp;        // [0]=L [1]=R のゲインカーブ
    juce::MidiBuffer           midi;
    SimpleDelay                pdc;

    /** 子プラグインが持つステレオ出力ペアの数（1 = 普通のステレオ音源）。 */
    int numOutPairs = 1;

    /** 子側の出力バス総数。2 以上ならマルチアウトに対応している。 */
    int availableOutBuses = 1;

    /** マルチアウトを有効にしているか。既定 false（メイン出力のみ＝軽い）。 */
    bool multiOut = false;

    /** ペアごとの送り先。kRouteFollow / kRouteOff / 0=Main / 1..16=Out N */
    std::array<std::atomic<int>, mh::kMaxOutPairs> pairRouting;

    /** UI 表示用の出力レベル（0..1 目安）。オーディオスレッドから書く。 */
    std::atomic<float> activity { 0.0f };

    /** 無音が続いたブロック数。一定を超えたら子の処理自体を止める。 */
    int  silentBlocks     = 0;
    int  skipAfterBlocks  = 64;
    juce::SmoothedValue<float> smoothedL { 0.0f }, smoothedR { 0.0f };
    std::array<std::array<int8_t, 128>, 16> noteMap {};   // 実発音ノートの記録
    bool wasAudible = false;
    int  index = 0;

    void resetNoteMap();
    void prepare (double sampleRate, int blockSize);
    void release();
    bool isAudible (bool anySolo) const;
    void routeMidiMessage (const juce::MidiMessage& m, int samplePos);
    void flushAllNotes (int samplePos);
    void resetRouting();
};

//==============================================================================
class MultiHostAudioProcessor : public juce::AudioProcessor,
                                public juce::ChangeBroadcaster
{
public:
    MultiHostAudioProcessor();
    ~MultiHostAudioProcessor() override;

    //--- AudioProcessor ------------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                  { return true; }
    const juce::String getName() const override      { return "MultiHost 16"; }
    bool acceptsMidi() const override                { return true; }
    bool producesMidi() const override               { return false; }
    bool isMidiEffect() const override               { return false; }
    double getTailLengthSeconds() const override     { return 0.0; }
    int getNumPrograms() override                    { return 1; }
    int getCurrentProgram() override                 { return 0; }
    void setCurrentProgram (int) override            {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //--- スロット操作（メッセージスレッドから呼ぶこと） ----------------------
    bool loadPluginIntoSlot (int slotIndex, const juce::PluginDescription&);
    void clearSlot (int slotIndex);

    /** スロットのマルチアウトを切り替える（子を prepare し直す）。 */
    void setSlotMultiOut (int slotIndex, bool shouldBeMultiOut);

    HostedSlot& getSlot (int i) noexcept                          { return *slots[(size_t) i]; }
    juce::AudioPluginFormatManager& getFormatManager() noexcept   { return formatManager; }
    juce::KnownPluginList&          getKnownPluginList() noexcept { return knownPlugins; }
    juce::PropertiesFile*           getAppProperties();

    /** 別プロセス用スキャナ実行ファイルを探す。見つからなければ空の File。 */
    static juce::File findScannerExecutable();

    //--- プラグインのスキャン関連 -------------------------------------------
    /** 既知リストを設定ファイルへ保存する。 */
    void savePluginList();

    /** 既知リスト内のインストゥルメントの数。 */
    int getNumKnownInstruments() const;

    /** スキャン対象のフォルダ（VST3 の標準パス + ユーザー追加分）。 */
    juce::FileSearchPath getScanPaths() const;

    /** ユーザーが追加したスキャン対象フォルダを保存する。 */
    void addExtraScanPath (const juce::File& folder);
    juce::FileSearchPath getExtraScanPaths() const;

    /** クラッシュ記録ファイル（一度落ちた音源を次回スキップするために使う）。 */
    juce::File getDeadMansPedalFile() const;

    /** VST3 フォーマットを返す。見つからなければ nullptr。 */
    juce::AudioPluginFormat* getVST3Format();

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static BusesProperties createBusLayout();

    void updateLatency();
    void applyPendingState();

    std::array<std::unique_ptr<HostedSlot>, mh::kNumSlots> slots;

    juce::AudioPluginFormatManager               formatManager;
    juce::KnownPluginList                        knownPlugins;
    std::unique_ptr<juce::ApplicationProperties> appProperties;

    juce::CriticalSection      pluginLock;
    juce::AudioParameterFloat* masterGainDb = nullptr;

    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;

    std::unique_ptr<juce::XmlElement> pendingState;

    JUCE_DECLARE_WEAK_REFERENCEABLE (MultiHostAudioProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiHostAudioProcessor)
};
