cmake_minimum_required(VERSION 3.22)

project(MultiHost16 VERSION 1.6.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# MSVC ランタイムを静的リンクする。
# これで配布先に「Visual C++ 再頒布可能パッケージ」が不要になる。
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# JUCE 8
#   -DJUCE_PATH=/path/to/JUCE を指定するか、未指定なら GitHub から取得する
# ---------------------------------------------------------------------------
if (JUCE_PATH)
    add_subdirectory("${JUCE_PATH}" JUCE_build)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.6
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# ビルド後のコピー先
#   Windows: 管理者権限が不要なユーザー個人の VST3 フォルダへ入れる。
#            システム全体 (C:\Program Files\Common Files\VST3) に入れたい場合は
#            -DMH_USER_VST3_DIR= と空を渡すか、この if ブロックを消す。
# ---------------------------------------------------------------------------
set(MH_COPY_ARGS "")

if (WIN32 AND DEFINED ENV{LOCALAPPDATA})
    set(MH_USER_VST3_DIR "$ENV{LOCALAPPDATA}/Programs/Common/VST3"
        CACHE PATH "ビルド後に VST3 をコピーする先")
    if (MH_USER_VST3_DIR)
        set(MH_COPY_ARGS VST3_COPY_DIR "${MH_USER_VST3_DIR}")
        message(STATUS "MultiHost16: VST3 copy dir = ${MH_USER_VST3_DIR}")
    endif()
endif()

juce_add_plugin(MultiHost16
    ${MH_COPY_ARGS}
    COMPANY_NAME                "Takazono"
    BUNDLE_ID                   "com.takazono.multihost4"
    PLUGIN_MANUFACTURER_CODE    Tkzn          # 4文字・先頭大文字
    PLUGIN_CODE                 Mh16          # 4文字・先頭大文字（他と衝突させない）
    FORMATS                     VST3
    PRODUCT_NAME                "MultiHost 16"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    VST3_CATEGORIES             "Instrument" "Synth")

target_sources(MultiHost16 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(MultiHost16 PUBLIC
    # 自分自身が VST3 プラグインであり、かつ VST3 ホストでもある
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=0            # VST2 を読むなら 1 + VST2 SDK が必要
    JUCE_PLUGINHOST_AU=0             # macOS で AU も読むなら 1
    JUCE_PLUGINHOST_LV2=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0)

target_link_libraries(MultiHost16
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_devices
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# MultiHost16Scanner
#   プラグインのスキャンを別プロセスで行うためのコンソールアプリ。
#   これが無いとプラグイン内でスキャンすることになり、
#   相性の悪い音源で DAW ごと落ちる。
# ---------------------------------------------------------------------------
juce_add_console_app(MultiHost16Scanner
    PRODUCT_NAME "MultiHost16Scanner")

target_sources(MultiHost16Scanner PRIVATE
    Scanner/ScannerMain.cpp)

target_compile_definitions(MultiHost16Scanner PUBLIC
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_AU=0
    JUCE_PLUGINHOST_LV2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1)

target_link_libraries(MultiHost16Scanner
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# プラグインをビルドしたらスキャナも一緒に作る
add_dependencies(MultiHost16 MultiHost16Scanner)
