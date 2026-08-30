#pragma once

#include <QMainWindow>
#include <memory>
#include "Core/Project.h"
#include "Audio/AudioEngine.h"
#include "Singer/SingerManager.h"
#include "Renderer/Renderer.h"
#include "Editor/PianoRollEditor.h"
#include "Undo/UndoManager.h"

namespace myvocal {

class TrackPanel;
class MainToolBar;
class ParameterPanel;
class TransportBar;
class PianoKeyboard;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void newProject();
    void openProject();
    void saveProject();
    void saveProjectAs();
    void importMidi();
    void exportWav();
    void addTrack();
    void removeTrack();
    void selectTrack(int index);
    void setTool(EditTool tool);
    void togglePlay();
    void stopPlayback();
    void renderProject();
    void renderSelected();
    void showPreferences();
    void showDiagnostics();
    void rescanVoiceBanks();

private:
    void buildUi();
    void buildMenus();
    void connectUi();
    void setProject(std::unique_ptr<Project> project);
    void applyDefaults(Project& project);
    void refreshVoiceBanks();
    void updateTitle();
    QString defaultProjectPath() const;

    std::unique_ptr<Project> m_project;
    AudioEngine m_audio;
    SingerManager m_singers;
    Renderer m_renderer;
    UndoManager m_undo;
    PianoRollEditor* m_editor{nullptr};
    PianoKeyboard* m_keyboard{nullptr};
    TrackPanel* m_trackPanel{nullptr};
    MainToolBar* m_toolbar{nullptr};
    ParameterPanel* m_params{nullptr};
    TransportBar* m_transport{nullptr};
    QString m_resampler;
};

}
