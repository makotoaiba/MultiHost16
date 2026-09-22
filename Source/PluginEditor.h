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

#include "PluginProcessor.h"

//==============================================================================
/** ホストしたプラグインの GUI を表示する別ウィンドウ。 */
class HostedPluginWindow : public juce::DocumentWindow
{
public:
    HostedPluginWindow (juce::AudioPluginInstance& p, std::function<void()> onCloseCb);
    void closeButtonPressed() override;

private:
    std::function<void()> onClose;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostedPluginWindow)
};

//==============================================================================
/** 全体の見た目をまとめた LookAndFeel。 */
class MultiHostLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MultiHostLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    static const juce::Colour bg, panel, panelHi, outline, text, textDim, accent;
};

//==============================================================================
/** スロットの出力ルーティングを一覧・編集するパネル（ポップアップ内に表示）。 */
class SlotRoutingPanel : public juce::Component
{
public:
    SlotRoutingPanel (MultiHostAudioProcessor&, int slotIndex, std::function<void()> onChange);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void rebuildRows();
    void applyConsecutive();
    juce::String destinationName (int busIndex) const;

    MultiHostAudioProcessor& proc;
    int index;
    std::function<void()> onChange;

    juce::Label       titleLabel, hintLabel, startLabel;
    juce::ToggleButton multiToggle { "Enable multi-output" };
    juce::OwnedArray<juce::Label>    pairLabels;
    juce::OwnedArray<juce::ComboBox> pairBoxes;
    juce::ComboBox    startBox;
    juce::TextButton  assignButton { "Assign" }, resetButton { "Reset all" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlotRoutingPanel)
};

//==============================================================================
/** 1 スロット分の操作パネル。 */
class SlotStrip : public juce::Component
{
public:
    SlotStrip (MultiHostAudioProcessor&, int slotIndex);
    ~SlotStrip() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();

    static juce::Colour colourForSlot (int index);
    static constexpr int kStripHeight = 64;

    /** 1 行分の列位置。見出しとストリップで必ず同じ関数を使うこと。 */
    struct Columns
    {
        juce::Rectangle<int> dot, number, plugin, editClear, muteSolo,
                             output, midi, keysRow, knobs;
    };
    static Columns layoutColumns (juce::Rectangle<int> stripBounds);

    /** 出力レベルを取り込み、必要なら点を描き直す。 */
    void updateActivity();

private:
    void showPluginMenu();
    void showRoutingPanel();
    void openPluginEditor();
    juce::String routingSummary() const;

    using SlAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BtAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CbAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    MultiHostAudioProcessor& proc;
    int index;

    juce::Label      numberLabel;
    juce::TextButton loadButton { "-- empty --" }, editButton { "Edit" }, clearButton { "X" };
    juce::TextButton muteButton { "M" }, soloButton { "S" };
    juce::Slider gain, pan, lowKey, highKey, lowVel, highVel, transpose;
    juce::Label  gainLabel, panLabel, keyLabel, velLabel, transLabel, outLabel, chLabel;
    juce::ComboBox outBox, chBox;
    juce::TextButton routeButton { "Outs" };

    std::unique_ptr<SlAtt> gainAtt, panAtt, loKeyAtt, hiKeyAtt, loVelAtt, hiVelAtt, transAtt;
    std::unique_ptr<BtAtt> muteAtt, soloAtt;
    std::unique_ptr<CbAtt> outAtt, chAtt;

    std::unique_ptr<HostedPluginWindow> window;

    juce::Rectangle<int> dotArea;
    float level = 0.0f, lastDrawnLevel = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlotStrip)
};

//==============================================================================
/** スロットを縦に並べてスクロールさせる中身。 */
class SlotListContent : public juce::Component,
                        private juce::Timer
{
public:
    SlotListContent (MultiHostAudioProcessor&);
    void resized() override;
    void refreshAll();

private:
    void timerCallback() override;
    juce::OwnedArray<SlotStrip> strips;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlotListContent)
};

//==============================================================================
/** バックグラウンドでプラグインをスキャンし、進捗を報告する。 */
class ScanSession : private juce::Timer
{
public:
    ScanSession (MultiHostAudioProcessor&);
    ~ScanSession() override;

    void start (const juce::FileSearchPath& paths);
    void cancel();
    bool isRunning() const noexcept   { return worker != nullptr && worker->isThreadRunning(); }

    double getProgress() const noexcept { return progress; }
    juce::String getStatusText() const;

    std::function<void()> onProgress;
    std::function<void (int numFound)> onFinished;

private:
    void timerCallback() override;

    struct Worker : juce::Thread
    {
        Worker (ScanSession& o) : juce::Thread ("MH16 scan"), owner (o) {}
        void run() override;
        ScanSession& owner;
    };

    MultiHostAudioProcessor& proc;
    std::unique_ptr<juce::PluginDirectoryScanner> scanner;
    std::unique_ptr<Worker> worker;

    std::atomic<double> progress { 0.0 };
    std::atomic<bool>   done { false };
    mutable juce::CriticalSection statusLock;
    juce::String statusText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScanSession)
};

//==============================================================================
class MultiHostAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::ChangeListener,
                                      private juce::Timer
{
public:
    explicit MultiHostAudioProcessorEditor (MultiHostAudioProcessor&);
    ~MultiHostAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void updateBusStatus();
    void buildColumnHeader();
    void startScan (const juce::FileSearchPath& paths);
    void showScanMenu();
    void showAdvancedScanner();
    void chooseFolderAndScan();
    void updateScanUI();
    void showSetupMenu();
    void applyMultitimbral();
    void applyLayerMode();
    void applyAutoAssignOutputs();

    MultiHostAudioProcessor& proc;

    juce::TextButton scanButton  { "Scan VSTi" };
    juce::TextButton scanMenuButton { "..." };
    juce::TextButton setupButton { "Setup" };
    juce::Label      scanStatus;
    std::unique_ptr<juce::ProgressBar> progressBar;
    double           progressValue = 0.0;
    std::unique_ptr<ScanSession> scanSession;
    std::unique_ptr<juce::FileChooser> folderChooser;
    juce::Label      title { {}, "MultiHost 16" }, masterLabel { {}, "Master" };
    juce::Label      busStatus;
    juce::Slider     masterGain;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAtt;

    juce::Component  columnHeader;
    juce::OwnedArray<juce::Label> headerLabels;

    MultiHostLookAndFeel lnf;

    juce::Viewport   viewport;
    SlotListContent  slotList;

    juce::Component::SafePointer<juce::DialogWindow> scannerWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiHostAudioProcessorEditor)
};
