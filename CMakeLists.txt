cmake_minimum_required(VERSION 3.21)

project(MyVocalSynth LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)

set(SOURCES
    src/main.cpp
    src/App/MainWindow.cpp
    src/Audio/AudioEngine.cpp
    src/Audio/PlaybackController.cpp
    src/Core/Note.cpp
    src/Core/PitchCurve.cpp
    src/Core/SongTime.cpp
    src/Core/Project.cpp
    src/Core/TempoMap.cpp
    src/Core/Track.cpp
    src/DSP/PitchProcessor.cpp
    src/DSP/VibratoProcessor.cpp
    src/DSP/WaveMixer.cpp
    src/Editor/PianoRollEditor.cpp
    src/Phonemizer/DefaultCVPhonemizer.cpp
    src/Phonemizer/JapaneseVCVPhonemizer.cpp
    src/Phonemizer/JapaneseCVVCPhonemizer.cpp
    src/Phonemizer/HybridJapanesePhonemizer.cpp
    src/Phonemizer/KoreanVCVPhonemizer.cpp
    src/Phonemizer/KoreanCBNNPhonemizer.cpp
    src/Phonemizer/AliasResolver.cpp
    src/Renderer/MoresamplerProcess.cpp
    src/Renderer/WaveAssembler.cpp
    src/Renderer/RenderWorker.cpp
    src/Renderer/Renderer.cpp
    src/Singer/OtoParser.cpp
    src/Singer/CharacterParser.cpp
    src/Singer/Singer.cpp
    src/Singer/SingerManager.cpp
    src/UI/MainToolBar.cpp
    src/UI/TrackPanel.cpp
    src/UI/ParameterPanel.cpp
    src/UI/TransportBar.cpp
    src/UI/PianoKeyboard.cpp
    src/UI/LyricEditor.cpp
    src/UI/PreferencesDialog.cpp
    src/UI/DiagnosticsDialog.cpp
    src/UI/ArrangementEditor.cpp
    src/Undo/UndoManager.cpp
    src/Utils/Logger.cpp
    src/Utils/ShortcutManager.cpp
)

set(HEADERS
    include/App/MainWindow.h
    include/Audio/AudioEngine.h
    include/Audio/PlaybackController.h
    include/Core/AudioClip.h
    include/Core/Note.h
    include/Core/Phoneme.h
    include/Core/PitchCurve.h
    include/Core/Project.h
    include/Core/SongTime.h
    include/Core/TempoMap.h
    include/Core/Track.h
    include/DSP/PitchProcessor.h
    include/DSP/VibratoProcessor.h
    include/DSP/WaveMixer.h
    include/Editor/PianoRollEditor.h
    include/Phonemizer/IPhonemizer.h
    include/Phonemizer/AliasResolver.h
    include/Phonemizer/DefaultCVPhonemizer.h
    include/Phonemizer/JapaneseVCVPhonemizer.h
    include/Phonemizer/JapaneseCVVCPhonemizer.h
    include/Phonemizer/HybridJapanesePhonemizer.h
    include/Phonemizer/KoreanVCVPhonemizer.h
    include/Phonemizer/KoreanCBNNPhonemizer.h
    include/Renderer/IAudioAssembler.h
    include/Renderer/MoresamplerProcess.h
    include/Renderer/RenderWorker.h
    include/Renderer/Renderer.h
    include/Renderer/WaveAssembler.h
    include/Singer/CharacterParser.h
    include/Singer/OtoEntry.h
    include/Singer/OtoParser.h
    include/Singer/Singer.h
    include/Singer/SingerInfo.h
    include/Singer/SingerManager.h
    include/UI/ArrangementEditor.h
    include/UI/DiagnosticsDialog.h
    include/UI/LyricEditor.h
    include/UI/MainToolBar.h
    include/UI/ParameterPanel.h
    include/UI/PianoKeyboard.h
    include/UI/PreferencesDialog.h
    include/UI/TrackPanel.h
    include/UI/TransportBar.h
    include/Undo/Command.h
    include/Undo/Commands.h
    include/Undo/UndoManager.h
    include/Utils/Logger.h
    include/Utils/ShortcutManager.h
)

set(MOC_HEADERS
    include/App/MainWindow.h
    include/Audio/AudioEngine.h
    include/Audio/PlaybackController.h
    include/Editor/PianoRollEditor.h
    include/Renderer/MoresamplerProcess.h
    include/Renderer/Renderer.h
    include/Renderer/RenderWorker.h
    include/UI/ArrangementEditor.h
    include/UI/DiagnosticsDialog.h
    include/UI/LyricEditor.h
    include/UI/MainToolBar.h
    include/UI/ParameterPanel.h
    include/UI/PianoKeyboard.h
    include/UI/PreferencesDialog.h
    include/UI/TrackPanel.h
    include/UI/TransportBar.h
    include/Utils/ShortcutManager.h
)

qt_wrap_cpp(MYVOCAL_MOC_SOURCES ${MOC_HEADERS})

add_executable(MyVocalSynth WIN32
    ${SOURCES}
    ${HEADERS}
    ${MYVOCAL_MOC_SOURCES}
)

target_include_directories(MyVocalSynth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(MyVocalSynth PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
)

if(MSVC)
    target_compile_options(MyVocalSynth PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(MyVocalSynth PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_command(TARGET MyVocalSynth POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:MyVocalSynth>/resampler"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:MyVocalSynth>/VoiceBanks"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:MyVocalSynth>/presets"
)

enable_testing()

set(TEST_SOURCES
    tests/TestMain.cpp
    tests/NoteTest.cpp
    tests/OtoParserTest.cpp
    tests/SongTimeTest.cpp
    src/Core/Note.cpp
    src/Core/PitchCurve.cpp
    src/Core/SongTime.cpp
    src/Core/TempoMap.cpp
    src/Singer/OtoParser.cpp
)

add_executable(MyVocalSynthTests ${TEST_SOURCES})
target_include_directories(MyVocalSynthTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(MyVocalSynthTests PRIVATE Qt6::Core Qt6::Gui)
add_test(NAME MyVocalSynthTests COMMAND MyVocalSynthTests)

include(GNUInstallDirs)
install(TARGETS MyVocalSynth RUNTIME DESTINATION .)
install(DIRECTORY resources DESTINATION .)
install(DIRECTORY presets DESTINATION . OPTIONAL)
