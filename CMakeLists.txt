cmake_minimum_required(VERSION 3.22)

project(ArtilodyTracker VERSION 1.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    enable_language(OBJCXX)
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.10
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(ArtilodyTracker
    PRODUCT_NAME "Artilody Tracker"
    COMPANY_NAME "Artilody"
    BUNDLE_ID "com.artilody.tracker"
    VERSION ${PROJECT_VERSION}
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Artilody Tracker uses audio input for plug-ins that process live audio.")

juce_add_binary_data(ArtilodyData SOURCES
    resources/logo_full.png
    resources/logo_mark.png
    resources/inter-400.ttf
    resources/inter-600.ttf
    resources/inter-700.ttf
    resources/mono-500.ttf
    resources/mono-700.ttf)

set(ARTILODY_SOURCES
    src/Main.cpp
    src/Song.cpp
    src/Theme.cpp
    src/BuiltinFX.cpp
    src/SoundFontInstrument.cpp
    src/SampleInstrument.cpp
    src/StripProcessor.cpp
    src/AudioEngine.cpp
    src/PluginHost.cpp
    src/ClapHost.cpp
    src/ProjectIO.cpp
    src/ui/TopBar.cpp
    src/ui/PatternGrid.cpp
    src/ui/TrackArea.cpp
    src/ui/MixerStrip.cpp
    src/ui/SidePanels.cpp
    src/ui/MainComponent.cpp)

if(APPLE)
    list(APPEND ARTILODY_SOURCES src/ClapMacView.mm)
endif()

target_sources(ArtilodyTracker PRIVATE ${ARTILODY_SOURCES})

target_include_directories(ArtilodyTracker PRIVATE
    src
    thirdparty/tsf
    thirdparty/clap/include)

target_compile_definitions(ArtilodyTracker PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_PLUGINHOST_LV2=0
    JUCE_USE_FLAC=1
    JUCE_USE_OGGVORBIS=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:ArtilodyTracker,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:ArtilodyTracker,JUCE_VERSION>"
    ARTILODY_VERSION="${PROJECT_VERSION}")

target_link_libraries(ArtilodyTracker
    PRIVATE
        ArtilodyData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(UNIX AND NOT APPLE)
    target_link_libraries(ArtilodyTracker PRIVATE ${CMAKE_DL_LIBS})
endif()

# ---------------------------------------------------------------------------
# Installation + Debian/Ubuntu packages (CPack)
# ---------------------------------------------------------------------------
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(PROGRAMS "$<TARGET_FILE:ArtilodyTracker>"
            DESTINATION lib/artilody-tracker
            RENAME artilody-tracker COMPONENT app)
    install(PROGRAMS packaging/linux/artilody-tracker.sh DESTINATION bin RENAME artilody-tracker COMPONENT app)
    install(FILES packaging/linux/artilody-tracker.desktop DESTINATION share/applications COMPONENT app)
    install(FILES packaging/linux/icon-256.png DESTINATION share/icons/hicolor/256x256/apps RENAME artilody-tracker.png COMPONENT app)
    install(FILES packaging/linux/icon-512.png DESTINATION share/icons/hicolor/512x512/apps RENAME artilody-tracker.png COMPONENT app)
    install(FILES LICENSE THIRD_PARTY_LICENSES.md DESTINATION share/doc/artilody-tracker COMPONENT app)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/soundfonts/GeneralUser-GS.sf2")
        install(FILES soundfonts/GeneralUser-GS.sf2 DESTINATION share/artilody-tracker/soundfonts COMPONENT app)
    endif()

    set(ARTILODY_DISTRO_SUFFIX "" CACHE STRING "Suffix for the package file name, e.g. debian12 or ubuntu22.04")
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "artilody-tracker")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_VENDOR "Artilody")
    set(CPACK_PACKAGE_CONTACT "Artilody <noreply@github.com>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Music tracker with a full mixer, SoundFont instruments and VST3/CLAP hosting")
    set(CPACK_PACKAGE_DESCRIPTION "Artilody Tracker is a pattern based music tracker. Every track gets its own\nchannel strip with instrument selection (built-in Pop, Rock, Electro and Jazz\nSoundFont instruments or VST3/CLAP plug-ins), any number of plug-in effects,\nbuilt-in EQ, compressor and reverb, panning and volume.")
    set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/shenphenchoedron-hue/Artilody-Tracker")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libx11-6, libxext6, libxinerama1, libxrandr2, libxcursor1, libxrender1, libxcomposite1")
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "pulseaudio | pipewire-pulse")
    set(CPACK_STRIP_FILES ON)
    if(ARTILODY_DISTRO_SUFFIX)
        set(CPACK_DEBIAN_FILE_NAME "artilody-tracker_${PROJECT_VERSION}_${ARTILODY_DISTRO_SUFFIX}_amd64.deb")
    else()
        set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    endif()
    set(CPACK_DEB_COMPONENT_INSTALL ON)
    set(CPACK_COMPONENTS_ALL app)
    set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
    include(CPack)
endif()
