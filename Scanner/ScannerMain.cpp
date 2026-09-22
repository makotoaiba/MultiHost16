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

/*
    MultiHost16Scanner
    ------------------------------------------------------------------
    プラグインのスキャンを別プロセスで行うための小さなコンソールアプリ。

    使い方:
        MultiHost16Scanner <formatName> <fileOrIdentifier> <outputXmlPath>

    見つかった PluginDescription を XML で書き出して終了する。
    対象のプラグインがクラッシュしてもこのプロセスが死ぬだけで、
    ホスト (DAW) には影響しない。
*/

#include <juce_audio_processors/juce_audio_processors.h>

int main (int argc, char* argv[])
{
    if (argc < 4)
    {
        std::fprintf (stderr,
                      "usage: MultiHost16Scanner <format> <fileOrIdentifier> <outputXml>\n");
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI juceInit;

    // 括弧だと関数宣言に解釈される (most vexing parse) のでブレース初期化にする
    const juce::String formatName { juce::CharPointer_UTF8 (argv[1]) };
    const juce::String fileOrId   { juce::CharPointer_UTF8 (argv[2]) };
    const juce::File   outputFile { juce::String { juce::CharPointer_UTF8 (argv[3]) } };

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    juce::OwnedArray<juce::PluginDescription> found;

    for (auto* format : formatManager.getFormats())
    {
        if (format == nullptr || format->getName() != formatName)
            continue;

        // ここで実際にプラグインがロードされる。落ちるならこのプロセスだけ。
        format->findAllTypesForFile (found, fileOrId);
        break;
    }

    juce::XmlElement root ("PLUGINS");

    for (auto* desc : found)
        if (desc != nullptr)
            root.addChildElement (desc->createXml().release());

    outputFile.getParentDirectory().createDirectory();

    if (! outputFile.replaceWithText (root.toString()))
        return 3;

    return 0;
}
