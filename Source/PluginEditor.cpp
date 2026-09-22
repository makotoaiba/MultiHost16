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

#include "PluginEditor.h"

using namespace juce;

//==============================================================================
Colour SlotStrip::colourForSlot (int index)
{
    const float hue = (float) (index % mh::kNumSlots) / (float) mh::kNumSlots;
    return Colour::fromHSV (hue, 0.48f, 0.82f, 1.0f);
}

//==============================================================================
HostedPluginWindow::HostedPluginWindow (AudioPluginInstance& p, std::function<void()> onCloseCb)
    : DocumentWindow (p.getName(), Colour (0xff202020), DocumentWindow::closeButton),
      onClose (std::move (onCloseCb))
{
    setUsingNativeTitleBar (true);
    setAlwaysOnTop (true);

    if (auto* ed = p.createEditorIfNeeded())
        setContentOwned (ed, true);
    else
        setContentOwned (new GenericAudioProcessorEditor (p), true);

    setResizable (true, false);
    centreWithSize (jmax (400, getWidth()), jmax (200, getHeight()));
    setVisible (true);
}

void HostedPluginWindow::closeButtonPressed()
{
    if (onClose) MessageManager::callAsync (onClose);
}

//==============================================================================
// MultiHostLookAndFeel
//==============================================================================
const Colour MultiHostLookAndFeel::bg      { 0xff15171b };
const Colour MultiHostLookAndFeel::panel   { 0xff1e2229 };
const Colour MultiHostLookAndFeel::panelHi { 0xff262b34 };
const Colour MultiHostLookAndFeel::outline { 0xff2f3540 };
const Colour MultiHostLookAndFeel::text    { 0xffe4e8ef };
const Colour MultiHostLookAndFeel::textDim { 0xff8b94a5 };
const Colour MultiHostLookAndFeel::accent  { 0xff5aa9e6 };

MultiHostLookAndFeel::MultiHostLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, bg);
    setColour (Label::textColourId,                 text);
    setColour (TextButton::buttonColourId,          panelHi);
    setColour (TextButton::buttonOnColourId,        accent);
    setColour (TextButton::textColourOffId,         text);
    setColour (TextButton::textColourOnId,          Colours::black);
    setColour (ComboBox::backgroundColourId,        panelHi);
    setColour (ComboBox::textColourId,              text);
    setColour (ComboBox::outlineColourId,           outline);
    setColour (ComboBox::arrowColourId,             textDim);
    setColour (PopupMenu::backgroundColourId,       panel);
    setColour (PopupMenu::textColourId,             text);
    setColour (PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
    setColour (PopupMenu::highlightedTextColourId,  text);
    setColour (Slider::textBoxTextColourId,         text);
    setColour (Slider::textBoxBackgroundColourId,   Colours::transparentBlack);
    setColour (Slider::textBoxOutlineColourId,      Colours::transparentBlack);
    setColour (Slider::rotarySliderFillColourId,    accent);
    setColour (Slider::trackColourId,               accent);
    setColour (Slider::backgroundColourId,          outline);
    setColour (ToggleButton::textColourId,          text);
    setColour (ToggleButton::tickColourId,          accent);
    setColour (ScrollBar::thumbColourId,            outline.brighter (0.3f));
    setColour (TooltipWindow::backgroundColourId,   panel);
    setColour (TooltipWindow::textColourId,         text);
}

Font MultiHostLookAndFeel::getLabelFont (Label& l)            { return l.getFont(); }
Font MultiHostLookAndFeel::getComboBoxFont (ComboBox&)        { return Font (FontOptions (12.0f)); }
Font MultiHostLookAndFeel::getTextButtonFont (TextButton&, int h)
{
    return Font (FontOptions (jmin (13.0f, (float) h * 0.6f)));
}

void MultiHostLookAndFeel::drawButtonBackground (Graphics& g, Button& b,
                                                 const Colour& backgroundColour,
                                                 bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float corner = 4.0f;

    Colour fill = backgroundColour;

    if (! b.isEnabled())      fill = panel.withAlpha (0.6f);
    else if (down)            fill = fill.darker (0.25f);
    else if (highlighted)     fill = fill.brighter (0.18f);

    g.setColour (fill);
    g.fillRoundedRectangle (r, corner);

    g.setColour (outline.withAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

void MultiHostLookAndFeel::drawComboBox (Graphics& g, int width, int height, bool,
                                         int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

    g.setColour (box.isEnabled() ? panelHi : panel.withAlpha (0.6f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    Path arrow;
    const float cx = (float) width - 12.0f, cy = (float) height * 0.5f;
    arrow.startNewSubPath (cx - 3.5f, cy - 2.0f);
    arrow.lineTo (cx, cy + 2.5f);
    arrow.lineTo (cx + 3.5f, cy - 2.0f);

    g.setColour (textDim.withAlpha (box.isEnabled() ? 1.0f : 0.4f));
    g.strokePath (arrow, PathStrokeType (1.4f, PathStrokeType::curved, PathStrokeType::rounded));
}

void MultiHostLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                                             float pos, float startAngle, float endAngle,
                                             Slider& sl)
{
    auto area = Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float radius = jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const auto centre = area.getCentre();
    const float thickness = jmax (2.5f, radius * 0.22f);
    const float angle = startAngle + pos * (endAngle - startAngle);

    Path back;
    back.addCentredArc (centre.x, centre.y, radius - thickness * 0.5f, radius - thickness * 0.5f,
                        0.0f, startAngle, endAngle, true);
    g.setColour (outline);
    g.strokePath (back, PathStrokeType (thickness, PathStrokeType::curved, PathStrokeType::rounded));

    if (pos > 0.001f)
    {
        Path front;
        front.addCentredArc (centre.x, centre.y, radius - thickness * 0.5f, radius - thickness * 0.5f,
                             0.0f, startAngle, angle, true);
        g.setColour (sl.findColour (Slider::rotarySliderFillColourId)
                        .withAlpha (sl.isEnabled() ? 1.0f : 0.4f));
        g.strokePath (front, PathStrokeType (thickness, PathStrokeType::curved, PathStrokeType::rounded));
    }

    Path pointer;
    const float len = radius - thickness * 1.1f;
    pointer.startNewSubPath (centre.x, centre.y - len * 0.45f);
    pointer.lineTo (centre.x, centre.y - len);
    pointer.applyTransform (AffineTransform::rotation (angle, centre.x, centre.y));
    g.setColour (text.withAlpha (sl.isEnabled() ? 0.9f : 0.35f));
    g.strokePath (pointer, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
}

void MultiHostLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h,
                                             float sliderPos, float, float,
                                             Slider::SliderStyle style, Slider& sl)
{
    if (style != Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0.0f, 0.0f, style, sl);
        return;
    }

    const float cy = (float) y + (float) h * 0.5f;
    const float trackH = 4.0f;

    g.setColour (outline);
    g.fillRoundedRectangle ((float) x, cy - trackH * 0.5f, (float) w, trackH, trackH * 0.5f);

    g.setColour (accent.withAlpha (sl.isEnabled() ? 0.9f : 0.4f));
    g.fillRoundedRectangle ((float) x, cy - trackH * 0.5f, sliderPos - (float) x, trackH, trackH * 0.5f);

    g.setColour (text);
    g.fillEllipse (sliderPos - 5.0f, cy - 5.0f, 10.0f, 10.0f);
}

//==============================================================================
SlotStrip::SlotStrip (MultiHostAudioProcessor& p, int slotIndex)
    : proc (p), index (slotIndex)
{
    auto addRotary = [this] (Slider& s, Label& l, const String& name)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::TextBoxBelow, false, 56, 14);
        s.setRotaryParameters (MathConstants<float>::pi * 1.25f,
                               MathConstants<float>::pi * 2.75f, true);
        s.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        s.setColour (Slider::textBoxOutlineColourId,    Colours::transparentBlack);
        s.setColour (Slider::textBoxTextColourId,       MultiHostLookAndFeel::text);
        addAndMakeVisible (s);
        l.setText (name, dontSendNotification);
        l.setFont (FontOptions (10.0f));
        l.setColour (Label::textColourId, MultiHostLookAndFeel::textDim);
        l.setJustificationType (Justification::centred);
        addAndMakeVisible (l);
    };

    auto addInc = [this] (Slider& s)
    {
        s.setSliderStyle (Slider::IncDecButtons);
        s.setTextBoxStyle (Slider::TextBoxLeft, false, 46, 17);
        s.setIncDecButtonsMode (Slider::incDecButtonsDraggable_Vertical);
        addAndMakeVisible (s);
    };

    auto addSmallLabel = [this] (Label& l, const String& t)
    {
        l.setText (t, dontSendNotification);
        l.setFont (FontOptions (10.0f));
        l.setColour (Label::textColourId, MultiHostLookAndFeel::textDim);
        addAndMakeVisible (l);
    };

    numberLabel.setText (String (index + 1), dontSendNotification);
    numberLabel.setFont (FontOptions (13.0f, Font::bold));
    numberLabel.setColour (Label::textColourId, colourForSlot (index));
    numberLabel.setJustificationType (Justification::centred);
    addAndMakeVisible (numberLabel);

    addAndMakeVisible (loadButton);
    loadButton.onClick = [this] { showPluginMenu(); };

    addAndMakeVisible (editButton);
    editButton.onClick = [this] { openPluginEditor(); };

    addAndMakeVisible (clearButton);
    clearButton.onClick = [this] { window.reset(); proc.clearSlot (index); refresh(); };

    muteButton.setClickingTogglesState (true);
    muteButton.setColour (TextButton::buttonOnColourId, Colours::orangered);
    addAndMakeVisible (muteButton);

    soloButton.setClickingTogglesState (true);
    soloButton.setColour (TextButton::buttonOnColourId, Colours::gold);
    addAndMakeVisible (soloButton);

    addRotary (gain, gainLabel, "Gain");
    addRotary (pan,  panLabel,  "Pan");

    addSmallLabel (keyLabel,   "Key");
    addSmallLabel (velLabel,   "Vel");
    addSmallLabel (transLabel, "Tr");
    addSmallLabel (outLabel,   "Out");
    addSmallLabel (chLabel,    "Ch");

    addInc (lowKey);  addInc (highKey);
    addInc (lowVel);  addInc (highVel);
    addInc (transpose);

    // ComboBoxAttachment は選択肢を流し込んでくれないので、
    // アタッチする前に自分で項目を入れておく必要がある。
    // 項目数はパラメータ側の選択肢数と一致していなければならない。
    outBox.addItemList (mh::outputBusChoices(), 1);
    chBox .addItemList (mh::midiChannelChoices(), 1);

    jassert (outBox.getNumItems() == mh::outputBusChoices().size());
    jassert (chBox .getNumItems() == mh::midiChannelChoices().size());

    addAndMakeVisible (outBox);
    addAndMakeVisible (chBox);

    routeButton.onClick = [this] { showRoutingPanel(); };
    routeButton.setTooltip ("Open the output routing panel for this slot");
    addAndMakeVisible (routeButton);

    auto& s = proc.apvts;
    gainAtt  = std::make_unique<SlAtt> (s, mh::slotParamID (index, "gain"),  gain);
    panAtt   = std::make_unique<SlAtt> (s, mh::slotParamID (index, "pan"),   pan);
    loKeyAtt = std::make_unique<SlAtt> (s, mh::slotParamID (index, "lokey"), lowKey);
    hiKeyAtt = std::make_unique<SlAtt> (s, mh::slotParamID (index, "hikey"), highKey);
    loVelAtt = std::make_unique<SlAtt> (s, mh::slotParamID (index, "lovel"), lowVel);
    hiVelAtt = std::make_unique<SlAtt> (s, mh::slotParamID (index, "hivel"), highVel);
    transAtt = std::make_unique<SlAtt> (s, mh::slotParamID (index, "trans"), transpose);
    muteAtt  = std::make_unique<BtAtt> (s, mh::slotParamID (index, "mute"),  muteButton);
    soloAtt  = std::make_unique<BtAtt> (s, mh::slotParamID (index, "solo"),  soloButton);
    outAtt   = std::make_unique<CbAtt> (s, mh::slotParamID (index, "out"),   outBox);
    chAtt    = std::make_unique<CbAtt> (s, mh::slotParamID (index, "mch"),   chBox);

    refresh();
}

SlotStrip::~SlotStrip()
{
    window.reset();
}

void SlotStrip::refresh()
{
    auto& slot = proc.getSlot (index);
    const bool loaded = (slot.plugin != nullptr);

    loadButton.setButtonText (loaded ? slot.plugin->getName()
                                     : (slot.lastError.isNotEmpty() ? "! " + slot.lastError
                                                                    : String ("-- empty --")));
    editButton.setEnabled (loaded);
    clearButton.setEnabled (loaded);

    routeButton.setEnabled (loaded);
    routeButton.setButtonText (routingSummary());

    if (! loaded)
        window.reset();

    repaint();
}

void SlotStrip::showPluginMenu()
{
    // インストゥルメントのみを、メーカーごとのサブメニューにまとめる
    const auto types = proc.getKnownPluginList().getTypes();

    Array<int> instrumentIndices;
    for (int i = 0; i < types.size(); ++i)
        if (types.getReference (i).isInstrument)
            instrumentIndices.add (i);

    StringArray makers;
    for (auto i : instrumentIndices)
    {
        auto m = types.getReference (i).manufacturerName.trim();
        if (m.isEmpty()) m = "(Unknown)";
        makers.addIfNotAlreadyThere (m);
    }
    makers.sortNatural();

    PopupMenu menu;
    menu.addItem (1, "Remove plugin", proc.getSlot (index).plugin != nullptr);
    menu.addSeparator();

    if (instrumentIndices.isEmpty())
    {
        menu.addItem (2, "(no instruments found - run Scan Plugins first)", false);
    }
    else
    {
        for (const auto& maker : makers)
        {
            PopupMenu sub;

            for (auto i : instrumentIndices)
            {
                const auto& d = types.getReference (i);
                auto m = d.manufacturerName.trim();
                if (m.isEmpty()) m = "(Unknown)";
                if (m != maker) continue;

                sub.addItem (100 + i, d.name + "  (" + d.pluginFormatName + ")");
            }

            menu.addSubMenu (maker, sub);
        }
    }

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (loadButton),
                        [this, types] (int result)
    {
        if (result <= 1)
        {
            if (result == 1)
            {
                window.reset();
                proc.clearSlot (index);
                refresh();
            }
            return;
        }

        const int i = result - 100;
        if (isPositiveAndBelow (i, types.size()))
        {
            window.reset();
            proc.loadPluginIntoSlot (index, types.getReference (i));
            refresh();
        }
    });
}

//==============================================================================
/** 指定バスを使っている「他の」スロット番号を返す（衝突確認用）。 */
static String busUsageSuffix (MultiHostAudioProcessor& proc, int busIdx, int excludeSlot)
{
    StringArray users;

    for (int s = 0; s < mh::kNumSlots; ++s)
    {
        if (s == excludeSlot) continue;

        auto& sl = proc.getSlot (s);
        if (sl.plugin == nullptr) continue;

        const int pairs = sl.multiOut ? jlimit (1, mh::kMaxOutPairs, sl.numOutPairs) : 1;
        bool used = false;

        for (int p = 0; p < pairs && ! used; ++p)
        {
            const int r = sl.pairRouting[(size_t) p].load();
            if (r == mh::kRouteOff) continue;
            const int b = (r == mh::kRouteFollow) ? sl.outBus->getIndex() : r;
            if (b == busIdx) used = true;
        }

        if (used) users.add (String (s + 1));
    }

    return users.isEmpty() ? String() : ("  - used by slot " + users.joinIntoString (", "));
}

//==============================================================================
// SlotRoutingPanel
//==============================================================================
SlotRoutingPanel::SlotRoutingPanel (MultiHostAudioProcessor& p, int slotIndex,
                                    std::function<void()> onChangeCb)
    : proc (p), index (slotIndex), onChange (std::move (onChangeCb))
{
    auto& slot = proc.getSlot (index);

    titleLabel.setText ("Slot " + String (index + 1) + "  -  "
                            + (slot.plugin != nullptr ? slot.plugin->getName() : String ("empty")),
                        dontSendNotification);
    titleLabel.setFont (FontOptions (14.0f, Font::bold));
    addAndMakeVisible (titleLabel);

    hintLabel.setFont (FontOptions (11.0f));
    hintLabel.setColour (Label::textColourId, Colours::lightgrey);
    addAndMakeVisible (hintLabel);

    multiToggle.setToggleState (slot.multiOut, dontSendNotification);
    multiToggle.setEnabled (slot.plugin != nullptr && slot.availableOutBuses > 1);
    multiToggle.onClick = [this]
    {
        proc.setSlotMultiOut (index, multiToggle.getToggleState());
        rebuildRows();
        if (onChange) onChange();
    };
    addAndMakeVisible (multiToggle);

    startLabel.setText ("Start at", dontSendNotification);
    startLabel.setFont (FontOptions (11.0f));
    addAndMakeVisible (startLabel);

    for (int i = 1; i <= mh::kNumAuxOut; ++i)
        startBox.addItem ("Out " + String (i), i);
    startBox.setSelectedId (1, dontSendNotification);
    addAndMakeVisible (startBox);

    assignButton.onClick = [this] { applyConsecutive(); };
    addAndMakeVisible (assignButton);

    resetButton.onClick = [this]
    {
        proc.getSlot (index).resetRouting();
        rebuildRows();
        if (onChange) onChange();
    };
    addAndMakeVisible (resetButton);

    rebuildRows();
}

String SlotRoutingPanel::destinationName (int busIndex) const
{
    const auto dests = mh::outputBusChoices();
    String name = dests[busIndex];

    if (auto* bus = proc.getBus (false, busIndex))
        if (! bus->isEnabled())
            name += "  (not activated in host)";

    if (busIndex > 0)
        name += busUsageSuffix (proc, busIndex, index);

    return name;
}

void SlotRoutingPanel::rebuildRows()
{
    pairLabels.clear();
    pairBoxes.clear();

    auto& slot = proc.getSlot (index);
    const bool multi = slot.multiOut;
    const int  pairs = multi ? jlimit (1, mh::kMaxOutPairs, slot.numOutPairs) : 1;

    if (slot.plugin == nullptr)
        hintLabel.setText ("No plugin loaded.", dontSendNotification);
    else if (slot.availableOutBuses <= 1)
        hintLabel.setText ("This plugin has a single stereo output.", dontSendNotification);
    else if (! multi)
        hintLabel.setText ("This plugin offers " + String (slot.availableOutBuses)
                               + " output buses. Turn on multi-output to use them.",
                           dontSendNotification);
    else
        hintLabel.setText ("Set where each of the plugin's outputs should go.",
                           dontSendNotification);

    for (int p = 0; p < pairs; ++p)
    {
        auto* lab = pairLabels.add (new Label());
        lab->setText ("Output " + String (p * 2 + 1) + "/" + String (p * 2 + 2),
                      dontSendNotification);
        lab->setFont (FontOptions (12.0f));
        addAndMakeVisible (lab);

        auto* box = pairBoxes.add (new ComboBox());
        box->addItem ("Follow slot Out", 1);
        box->addItem ("Off (do not output)", 2);

        for (int d = 0; d <= mh::kNumAuxOut; ++d)
            box->addItem (destinationName (d), 3 + d);

        const int cur = slot.pairRouting[(size_t) p].load();
        box->setSelectedId (cur == mh::kRouteFollow ? 1
                            : cur == mh::kRouteOff  ? 2
                                                    : 3 + cur, dontSendNotification);
        box->setEnabled (multi || pairs == 1);

        box->onChange = [this, p, box]
        {
            const int id = box->getSelectedId();
            int value = mh::kRouteFollow;
            if (id == 2)      value = mh::kRouteOff;
            else if (id >= 3) value = id - 3;

            proc.getSlot (index).pairRouting[(size_t) p].store (value);
            if (onChange) onChange();
        };

        addAndMakeVisible (box);
    }

    const bool canAssign = multi && pairs > 1;
    startBox.setEnabled (canAssign);
    assignButton.setEnabled (canAssign);
    resetButton.setEnabled (slot.plugin != nullptr);

    setSize (440, 116 + pairs * 26 + 44);
    resized();
}

void SlotRoutingPanel::applyConsecutive()
{
    auto& slot = proc.getSlot (index);
    const int pairs = jlimit (1, mh::kMaxOutPairs, slot.numOutPairs);
    const int start = jmax (1, startBox.getSelectedId());

    for (int p = 0; p < pairs; ++p)
        slot.pairRouting[(size_t) p].store (jmin (start + p, mh::kNumAuxOut));

    rebuildRows();
    if (onChange) onChange();
}

void SlotRoutingPanel::paint (Graphics& g)
{
    g.fillAll (Colour (0xff232323));
}

void SlotRoutingPanel::resized()
{
    auto area = getLocalBounds().reduced (12, 10);

    titleLabel.setBounds (area.removeFromTop (20));
    hintLabel.setBounds (area.removeFromTop (30));
    area.removeFromTop (4);
    multiToggle.setBounds (area.removeFromTop (22));
    area.removeFromTop (8);

    for (int i = 0; i < pairBoxes.size(); ++i)
    {
        auto row = area.removeFromTop (26).reduced (0, 2);
        pairLabels[i]->setBounds (row.removeFromLeft (100));
        pairBoxes[i]->setBounds (row);
    }

    area.removeFromTop (8);
    auto bottom = area.removeFromTop (26);
    startLabel.setBounds (bottom.removeFromLeft (48));
    startBox.setBounds (bottom.removeFromLeft (90));
    bottom.removeFromLeft (6);
    assignButton.setBounds (bottom.removeFromLeft (80));
    bottom.removeFromLeft (6);
    resetButton.setBounds (bottom.removeFromLeft (80));
}

//==============================================================================
String SlotStrip::routingSummary() const
{
    auto& slot = proc.getSlot (index);

    if (slot.plugin == nullptr || ! slot.multiOut)
        return "Outs";

    const int pairs = jlimit (1, mh::kMaxOutPairs, slot.numOutPairs);
    Array<int> buses;

    for (int p = 0; p < pairs; ++p)
    {
        const int r = slot.pairRouting[(size_t) p].load();
        if (r == mh::kRouteOff) continue;
        const int b = (r == mh::kRouteFollow) ? slot.outBus->getIndex() : r;
        buses.addIfNotAlreadyThere (b);
    }

    if (buses.isEmpty())
        return "Outs: off";

    buses.sort();

    const bool consecutive = (buses.getLast() - buses.getFirst() + 1) == buses.size();
    const auto dests = mh::outputBusChoices();

    if (buses.size() == 1)
        return dests[buses[0]];

    if (consecutive)
        return dests[buses.getFirst()] + "-" + String (buses.getLast());

    return "Outs x" + String (buses.size());
}

void SlotStrip::showRoutingPanel()
{
    auto& slot = proc.getSlot (index);
    if (slot.plugin == nullptr) return;

    Component::SafePointer<SlotStrip> safe (this);
    auto panel = std::make_unique<SlotRoutingPanel> (proc, index,
                                                     [safe]
                                                     {
                                                         if (auto* s = safe.getComponent())
                                                             s->refresh();
                                                     });

    if (auto* top = getTopLevelComponent())
    {
        const auto area = top->getLocalArea (&routeButton, routeButton.getLocalBounds());
        CallOutBox::launchAsynchronously (std::move (panel), area, top);
    }
}

void SlotStrip::openPluginEditor()
{
    auto& slot = proc.getSlot (index);
    if (slot.plugin == nullptr) return;

    if (window != nullptr) { window->toFront (true); return; }

    Component::SafePointer<SlotStrip> safe (this);
    window = std::make_unique<HostedPluginWindow> (*slot.plugin,
                                                   [safe]
                                                   {
                                                       if (auto* s = safe.getComponent())
                                                           s->window.reset();
                                                   });
}

void SlotStrip::paint (Graphics& g)
{
    auto& slot = proc.getSlot (index);
    const bool loaded = slot.plugin != nullptr;

    auto r = getLocalBounds().toFloat().reduced (3.0f, 2.0f);

    g.setColour (loaded ? MultiHostLookAndFeel::panel
                        : MultiHostLookAndFeel::panel.withAlpha (0.55f));
    g.fillRoundedRectangle (r, 6.0f);

    g.setColour (MultiHostLookAndFeel::outline.withAlpha (loaded ? 1.0f : 0.5f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    // 左端のアクセントライン
    auto accentArea = r.reduced (1.0f).removeFromLeft (3.0f);
    g.setColour (colourForSlot (index).withAlpha (loaded ? 0.9f : 0.25f));
    g.fillRoundedRectangle (accentArea, 1.5f);

    // 発音インジケーター
    if (! dotArea.isEmpty())
    {
        auto d = dotArea.toFloat().reduced (1.0f);
        const auto col = colourForSlot (index);

        g.setColour (MultiHostLookAndFeel::outline);
        g.drawEllipse (d, 1.0f);

        if (loaded && level > 0.002f)
        {
            const float a = jlimit (0.0f, 1.0f, std::sqrt (level) * 1.4f);

            g.setColour (col.withAlpha (a * 0.25f));
            g.fillEllipse (d.expanded (2.5f * a));

            g.setColour (col.withAlpha (0.25f + a * 0.75f));
            g.fillEllipse (d.reduced (1.0f));
        }
        else if (loaded)
        {
            g.setColour (MultiHostLookAndFeel::outline.brighter (0.15f));
            g.fillEllipse (d.reduced (2.5f));
        }
    }
}

void SlotStrip::updateActivity()
{
    const float target = proc.getSlot (index).activity.load (std::memory_order_relaxed);

    level = jmax (target, level * 0.70f);       // 立ち上がり即時・減衰はゆっくり
    if (level < 0.002f) level = 0.0f;

    if (std::abs (level - lastDrawnLevel) > 0.004f)
    {
        lastDrawnLevel = level;
        if (! dotArea.isEmpty())
            repaint (dotArea.expanded (4));
    }
}

SlotStrip::Columns SlotStrip::layoutColumns (Rectangle<int> stripBounds)
{
    Columns c;

    auto full = stripBounds.reduced (3, 2);

    full.removeFromLeft (6);
    c.dot    = full.removeFromLeft (14);
    c.number = full.removeFromLeft (22);

    c.knobs = full.removeFromRight (136);

    auto body = full.reduced (6, 4);
    auto row1 = body.removeFromTop (24);
    body.removeFromTop (3);
    c.keysRow = body.removeFromTop (22);

    c.plugin    = row1.removeFromLeft (188);  row1.removeFromLeft (4);
    c.editClear = row1.removeFromLeft (72);   row1.removeFromLeft (10);
    c.muteSolo  = row1.removeFromLeft (52);   row1.removeFromLeft (12);
    c.output    = row1.removeFromLeft (186);  row1.removeFromLeft (10);
    c.midi      = row1.removeFromLeft (98);

    return c;
}

void SlotStrip::resized()
{
    const auto c = layoutColumns (getLocalBounds());

    dotArea = c.dot.withSizeKeepingCentre (10, 10);
    numberLabel.setBounds (c.number);

    auto knobs = c.knobs.reduced (4, 2);
    auto gArea = knobs.removeFromLeft (64);
    gainLabel.setBounds (gArea.removeFromTop (12));
    gain.setBounds (gArea.reduced (2, 0));
    panLabel.setBounds (knobs.removeFromTop (12));
    pan.setBounds (knobs.reduced (2, 0));

    loadButton.setBounds (c.plugin);

    auto ec = c.editClear;
    editButton.setBounds (ec.removeFromLeft (46));
    ec.removeFromLeft (2);
    clearButton.setBounds (ec.removeFromLeft (24));

    auto ms = c.muteSolo;
    muteButton.setBounds (ms.removeFromLeft (25));
    ms.removeFromLeft (2);
    soloButton.setBounds (ms.removeFromLeft (25));

    auto out = c.output;
    outLabel.setBounds (out.removeFromLeft (26));
    outBox.setBounds (out.removeFromLeft (76));
    out.removeFromLeft (4);
    routeButton.setBounds (out.removeFromLeft (76));

    auto mid = c.midi;
    chLabel.setBounds (mid.removeFromLeft (24));
    chBox.setBounds (mid.removeFromLeft (72));

    auto row2 = c.keysRow;
    keyLabel.setBounds (row2.removeFromLeft (26));
    lowKey.setBounds   (row2.removeFromLeft (92));
    highKey.setBounds  (row2.removeFromLeft (92));
    row2.removeFromLeft (10);
    velLabel.setBounds (row2.removeFromLeft (26));
    lowVel.setBounds   (row2.removeFromLeft (82));
    highVel.setBounds  (row2.removeFromLeft (82));
    row2.removeFromLeft (10);
    transLabel.setBounds (row2.removeFromLeft (22));
    transpose.setBounds  (row2.removeFromLeft (82));
}

//==============================================================================
SlotListContent::SlotListContent (MultiHostAudioProcessor& p)
{
    for (int i = 0; i < mh::kNumSlots; ++i)
        addAndMakeVisible (strips.add (new SlotStrip (p, i)));

    setSize (900, mh::kNumSlots * SlotStrip::kStripHeight);
    startTimerHz (24);
}

void SlotListContent::timerCallback()
{
    for (auto* s : strips)
        s->updateActivity();
}

void SlotListContent::resized()
{
    auto area = getLocalBounds();
    for (auto* s : strips)
        s->setBounds (area.removeFromTop (SlotStrip::kStripHeight));
}

void SlotListContent::refreshAll()
{
    for (auto* s : strips)
        s->refresh();
}

//==============================================================================
// ScanSession
//==============================================================================
ScanSession::ScanSession (MultiHostAudioProcessor& p) : proc (p) {}

ScanSession::~ScanSession()
{
    cancel();
    stopTimer();
}

void ScanSession::start (const FileSearchPath& paths)
{
    cancel();

    auto* fmt = proc.getVST3Format();
    if (fmt == nullptr)
    {
        const ScopedLock sl (statusLock);
        statusText = "VST3 format unavailable";
        return;
    }

    progress = 0.0;
    done = false;
    {
        const ScopedLock sl (statusLock);
        statusText = "Starting...";
    }

    scanner = std::make_unique<PluginDirectoryScanner> (proc.getKnownPluginList(),
                                                        *fmt,
                                                        paths,
                                                        true,                       // recursive
                                                        proc.getDeadMansPedalFile(),
                                                        false);                     // async 不要
    worker = std::make_unique<Worker> (*this);
    worker->startThread();
    startTimerHz (12);
}

void ScanSession::cancel()
{
    if (worker != nullptr)
    {
        worker->signalThreadShouldExit();
        worker->stopThread (8000);
        worker.reset();
    }
    scanner.reset();
    stopTimer();
}

juce::String ScanSession::getStatusText() const
{
    const ScopedLock sl (statusLock);
    return statusText;
}

void ScanSession::Worker::run()
{
    String name;

    while (! threadShouldExit())
    {
        if (owner.scanner == nullptr)
            break;

        if (! owner.scanner->scanNextFile (true, name))
            break;

        owner.progress = (double) owner.scanner->getProgress();
        const ScopedLock sl (owner.statusLock);
        owner.statusText = File::createFileWithoutCheckingPath (name).getFileNameWithoutExtension();
    }

    owner.done = true;
}

void ScanSession::timerCallback()
{
    if (onProgress) onProgress();

    if (done.load())
    {
        stopTimer();

        const int found = proc.getNumKnownInstruments();
        proc.savePluginList();

        if (worker != nullptr)
        {
            worker->stopThread (4000);
            worker.reset();
        }
        scanner.reset();

        {
            const ScopedLock sl (statusLock);
            statusText = {};
        }

        if (onFinished) onFinished (found);
    }
}

//==============================================================================
MultiHostAudioProcessorEditor::MultiHostAudioProcessorEditor (MultiHostAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), slotList (p)
{
    setLookAndFeel (&lnf);

    title.setFont (FontOptions (20.0f, Font::bold));
    addAndMakeVisible (title);

    scanSession = std::make_unique<ScanSession> (proc);
    scanSession->onProgress = [this] { updateScanUI(); };
    scanSession->onFinished = [this] (int found)
    {
        updateScanUI();
        slotList.refreshAll();
        scanStatus.setText (String (found) + " instruments found",
                            dontSendNotification);
    };

    addAndMakeVisible (scanButton);
    scanButton.onClick = [this]
    {
        if (scanSession->isRunning())
            scanSession->cancel();
        else
            startScan (proc.getScanPaths());

        updateScanUI();
    };

    addAndMakeVisible (scanMenuButton);
    scanMenuButton.onClick = [this] { showScanMenu(); };

    scanStatus.setFont (FontOptions (11.0f));
    scanStatus.setColour (Label::textColourId, Colours::grey);
    addAndMakeVisible (scanStatus);

    progressBar = std::make_unique<ProgressBar> (progressValue);
    progressBar->setPercentageDisplay (false);
    addChildComponent (progressBar.get());

    if (MultiHostAudioProcessor::findScannerExecutable() == File())
    {
        scanButton.setEnabled (false);
        scanButton.setButtonText ("Scanner missing");
        scanButton.setTooltip ("MultiHost16Scanner.exe not found. "
                               "Run install_for_test.bat (or build_windows.bat) again.");
    }

    addAndMakeVisible (setupButton);
    setupButton.onClick = [this] { showSetupMenu(); };

    busStatus.setFont (FontOptions (11.0f));
    busStatus.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (busStatus);

    masterLabel.setFont (FontOptions (11.0f));
    masterLabel.setJustificationType (Justification::centredRight);
    addAndMakeVisible (masterLabel);

    masterGain.setSliderStyle (Slider::LinearHorizontal);
    masterGain.setTextBoxStyle (Slider::TextBoxRight, false, 60, 18);
    addAndMakeVisible (masterGain);
    masterAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
                    proc.apvts, "master_gain", masterGain);

    buildColumnHeader();

    viewport.setViewedComponent (&slotList, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    proc.addChangeListener (this);
    updateBusStatus();
    startTimer (1000);
    setResizable (true, true);
    setResizeLimits (900, 420, 1600, 1100);
    setSize (980, 700);

    updateScanUI();
}

MultiHostAudioProcessorEditor::~MultiHostAudioProcessorEditor()
{
    proc.removeChangeListener (this);
    if (scannerWindow != nullptr)
        scannerWindow.deleteAndZero();
    setLookAndFeel (nullptr);
}

void MultiHostAudioProcessorEditor::changeListenerCallback (ChangeBroadcaster*)
{
    slotList.refreshAll();
}

void MultiHostAudioProcessorEditor::buildColumnHeader()
{
    const char* names[] = { "Plugin", "Editor", "Mute/Solo", "Output routing",
                            "MIDI ch", "Gain / Pan" };

    for (auto* n : names)
    {
        auto* l = headerLabels.add (new Label ({}, n));
        l->setFont (FontOptions (10.5f));
        l->setColour (Label::textColourId, MultiHostLookAndFeel::textDim);
        columnHeader.addAndMakeVisible (l);
    }

    headerLabels.getLast()->setJustificationType (Justification::centred);

    columnHeader.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (columnHeader);
}

void MultiHostAudioProcessorEditor::timerCallback()
{
    updateBusStatus();
}

void MultiHostAudioProcessorEditor::updateBusStatus()
{
    StringArray active;

    for (int i = 1; i < proc.getBusCount (false); ++i)
        if (auto* bus = proc.getBus (false, i))
            if (bus->isEnabled())
                active.add (String (i));

    String text;

    if (active.isEmpty())
    {
        text = "Host outputs: Main only  -  activate Out 1..16 in your DAW to use them";
        busStatus.setColour (Label::textColourId, Colours::orange.withAlpha (0.9f));
    }
    else
    {
        text = "Host outputs: Main + Out " + active.joinIntoString (", ");
        busStatus.setColour (Label::textColourId, Colours::lightgreen.withAlpha (0.9f));
    }

    if (busStatus.getText() != text)
        busStatus.setText (text, dontSendNotification);
}

void MultiHostAudioProcessorEditor::showSetupMenu()
{
    PopupMenu m;
    m.addSectionHeader ("Quick setup");
    m.addItem (1, "Multitimbral  (Slot N -> MIDI Ch N / Out N)");
    m.addItem (2, "Layer  (all Omni -> Main Mix)");
    m.addSeparator();
    m.addItem (3, "Auto-assign outputs  (slot order: Out 1, 2, 3 ...)");

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (setupButton),
                     [this] (int r)
    {
        if (r == 1) applyMultitimbral();
        else if (r == 2) applyLayerMode();
        else if (r == 3) applyAutoAssignOutputs();
    });
}

/** パラメータを 0..1 の正規化値でセットする小さなヘルパ。 */
static void setParam (RangedAudioParameter* p, float newValue)
{
    if (p == nullptr) return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (newValue));
    p->endChangeGesture();
}

void MultiHostAudioProcessorEditor::applyMultitimbral()
{
    for (int i = 0; i < mh::kNumSlots; ++i)
    {
        auto& s = proc.getSlot (i);
        setParam (s.midiChan, (float) jmin (i + 1, 16));                  // Ch 1..16
        setParam (s.outBus,   (float) jmin (i + 1, mh::kNumAuxOut));      // Out 1..16
        setParam (s.lowKey,   0.0f);
        setParam (s.highKey,  127.0f);
        setParam (s.lowVel,   1.0f);
        setParam (s.highVel,  127.0f);
    }
    slotList.refreshAll();
}

void MultiHostAudioProcessorEditor::applyLayerMode()
{
    for (int i = 0; i < mh::kNumSlots; ++i)
    {
        auto& s = proc.getSlot (i);
        setParam (s.midiChan, 0.0f);   // Omni
        setParam (s.outBus,   0.0f);   // Main Mix
    }
    slotList.refreshAll();
}

void MultiHostAudioProcessorEditor::applyAutoAssignOutputs()
{
    // スロット順に、使う本数ぶんだけ Out を連番で割り当てる。
    // 例) スロット1 が 4 ペア -> Out 1-4、スロット2 が 2 ペア -> Out 5-6
    int next = 1;

    for (int i = 0; i < mh::kNumSlots; ++i)
    {
        auto& s = proc.getSlot (i);
        if (s.plugin == nullptr) continue;

        if (next > mh::kNumAuxOut) break;          // バスを使い切ったら以降は触らない

        const int pairs = s.multiOut ? jlimit (1, mh::kMaxOutPairs, s.numOutPairs) : 1;

        if (pairs <= 1)
        {
            setParam (s.outBus, (float) next);
            s.resetRouting();
        }
        else
        {
            for (int p = 0; p < pairs; ++p)
                s.pairRouting[(size_t) p].store (jmin (next + p, mh::kNumAuxOut));
        }

        next += pairs;
    }

    slotList.refreshAll();
}

void MultiHostAudioProcessorEditor::startScan (const FileSearchPath& paths)
{
    if (MultiHostAudioProcessor::findScannerExecutable() == File())
    {
        scanStatus.setText ("Scanner executable missing", dontSendNotification);
        return;
    }

    if (paths.getNumPaths() == 0)
    {
        scanStatus.setText ("No folders to scan - use ... > Scan a folder",
                            dontSendNotification);
        return;
    }

    scanSession->start (paths);
    updateScanUI();
}

void MultiHostAudioProcessorEditor::updateScanUI()
{
    const bool running = scanSession != nullptr && scanSession->isRunning();

    if (running)
    {
        progressValue = scanSession->getProgress();
        scanButton.setButtonText ("Cancel");
        scanStatus.setText (scanSession->getStatusText(), dontSendNotification);
    }
    else if (MultiHostAudioProcessor::findScannerExecutable() != File())
    {
        const int n = proc.getNumKnownInstruments();
        scanButton.setButtonText (n > 0 ? "Rescan VSTi" : "Scan VSTi");
        if (scanStatus.getText().isEmpty())
            scanStatus.setText (String (n) + " instruments", dontSendNotification);
    }

    if (progressBar != nullptr)
        progressBar->setVisible (running);

    scanMenuButton.setEnabled (! running);
}

void MultiHostAudioProcessorEditor::showScanMenu()
{
    PopupMenu m;
    m.addSectionHeader ("Scan");
    m.addItem (1, "Scan a folder...");
    m.addSeparator();
    m.addItem (2, "Show scan folders");
    m.addItem (3, "Clear plugin list", proc.getKnownPluginList().getNumTypes() > 0);
    m.addSeparator();
    m.addItem (4, "Advanced list window...");

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (scanMenuButton),
                     [this] (int r)
    {
        switch (r)
        {
            case 1:
                chooseFolderAndScan();
                break;

            case 2:
            {
                auto paths = proc.getScanPaths();
                String text;
                for (int i = 0; i < paths.getNumPaths(); ++i)
                    text << paths[i].getFullPathName() << newLine;

                if (text.isEmpty()) text = "(none)";

                AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon,
                                                  "Scan folders", text);
                break;
            }

            case 3:
                proc.getKnownPluginList().clear();
                proc.savePluginList();
                scanStatus.setText ("List cleared", dontSendNotification);
                updateScanUI();
                slotList.refreshAll();
                break;

            case 4:
                showAdvancedScanner();
                break;

            default:
                break;
        }
    });
}

void MultiHostAudioProcessorEditor::chooseFolderAndScan()
{
    folderChooser = std::make_unique<FileChooser> ("Choose a folder containing VST3 plug-ins",
                                                   File::getSpecialLocation (File::userHomeDirectory),
                                                   String(), true);

    folderChooser->launchAsync (FileBrowserComponent::openMode
                                    | FileBrowserComponent::canSelectDirectories,
                                [this] (const FileChooser& fc)
    {
        const auto folder = fc.getResult();
        if (! folder.isDirectory())
            return;

        proc.addExtraScanPath (folder);

        FileSearchPath just;
        just.add (folder);
        startScan (just);
    });
}

void MultiHostAudioProcessorEditor::showAdvancedScanner()
{
    if (scannerWindow != nullptr) { scannerWindow->toFront (true); return; }

    auto* list = new PluginListComponent (proc.getFormatManager(),
                                          proc.getKnownPluginList(),
                                          proc.getDeadMansPedalFile(),
                                          proc.getAppProperties(),
                                          false);
    list->setNumberOfThreadsForScanning (1);
    list->setSize (760, 500);

    DialogWindow::LaunchOptions o;
    o.content.setOwned (list);
    o.dialogTitle                  = "Plugin List";
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar            = true;
    o.resizable                    = true;
    o.dialogBackgroundColour       = Colour (0xff202020);

    scannerWindow = o.launchAsync();
}

void MultiHostAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (MultiHostLookAndFeel::bg);

    // ヘッダの下に細い区切り線
    auto y = 8 + 34 + 4;
    g.setColour (MultiHostLookAndFeel::outline);
    g.drawHorizontalLine (y, 8.0f, (float) getWidth() - 8.0f);
}

void MultiHostAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (8);

    auto top = area.removeFromTop (34);
    title.setBounds (top.removeFromLeft (140));
    scanButton.setBounds (top.removeFromLeft (110).reduced (0, 3));
    scanMenuButton.setBounds (top.removeFromLeft (30).reduced (0, 3));
    top.removeFromLeft (6);
    setupButton.setBounds (top.removeFromLeft (70).reduced (0, 3));
    top.removeFromLeft (10);

    masterGain.setBounds (top.removeFromRight (230).reduced (0, 5));
    masterLabel.setBounds (top.removeFromRight (60));

    // 残りの帯を進捗表示に使う
    auto statusArea = top.reduced (0, 6);
    if (progressBar != nullptr)
        progressBar->setBounds (statusArea.removeFromLeft (jmin (160, statusArea.getWidth())));
    statusArea.removeFromLeft (8);
    scanStatus.setBounds (statusArea);

    busStatus.setBounds (area.removeFromTop (16));
    area.removeFromTop (2);

    {
        auto hdr = area.removeFromTop (14);
        columnHeader.setBounds (hdr);

        // スロット 1 行と同じ幅で列位置を計算する（縦スクロールバー分だけ内側）
        auto ref = columnHeader.getLocalBounds().withTrimmedRight (10)
                       .withHeight (SlotStrip::kStripHeight);
        const auto c = SlotStrip::layoutColumns (ref);

        const Rectangle<int> cols[] = { c.plugin, c.editClear, c.muteSolo,
                                        c.output, c.midi, c.knobs };

        for (int i = 0; i < headerLabels.size() && i < 6; ++i)
            headerLabels[i]->setBounds (cols[i].withY (0).withHeight (14));
    }

    area.removeFromTop (2);

    viewport.setBounds (area);

    const int contentWidth = jmax (860, viewport.getMaximumVisibleWidth());
    slotList.setSize (contentWidth, mh::kNumSlots * SlotStrip::kStripHeight);
}
