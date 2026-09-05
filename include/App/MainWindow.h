#pragma once

#include <QByteArray>
#include <QMainWindow>
#include <memory>

#include "Audio/AudioEngine.h"
#include "Audio/PlaybackController.h"
#include "Core/Project.h"
#include "Editor/PianoRollEditor.h"
#include "Renderer/Renderer.h"
#include "Singer/SingerManager.h"
#include "Undo/UndoManager.h"

namespace myvocal {

class ArrangementEditor;
class MainToolBar;
class PhonemeStripEditor;
class PianoKeyboard;
class TrackPanel;
class TransportBar;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void newProject();
    void openProject();
    void saveProject();
    void saveProjectAs();
    void importAudio();
    void exportWav();
    void addTrack();
    void removeTrack();
    void selectTrack(int index);
    void setTool(EditTool tool);
    void togglePlay();
    void stopPlayback();
    void renderProject();
    void showPreferences();
    void showDiagnostics();
    void rescanVoiceBanks();
    void seekFromTimeline(qint64 ms);

private:
    void buildUi();
    void buildMenus();
    void connectUi();
    void setProject(std::unique_ptr<Project> project);
    void applyDefaults(Project& project);
    void refreshVoiceBanks();
    void updateTitle();
    void refreshUndoSnapshot();
    void restoreProjectSnapshot(const QByteArray& snapshot);
    qint64 currentTick() const;
    qint64 currentMs() const;

    std::unique_ptr<Project> m_project;
    AudioEngine m_audio;
    SingerManager m_singers;
    Renderer m_renderer;
    UndoManager m_undo;
    std::unique_ptr<PlaybackController> m_playback;

    PianoRollEditor* m_editor{nullptr};
    ArrangementEditor* m_arrangement{nullptr};
    PhonemeStripEditor* m_phonemeStrip{nullptr};
    PianoKeyboard* m_keyboard{nullptr};
    TrackPanel* m_trackPanel{nullptr};
    MainToolBar* m_toolbar{nullptr};
    TransportBar* m_transport{nullptr};

    QString m_resampler;
    QByteArray m_undoSnapshot;
    bool m_restoringSnapshot{false};
};

}