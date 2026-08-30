#include "App/MainWindow.h"

#include "UI/MainToolBar.h"
#include "UI/TrackPanel.h"
#include "UI/ParameterPanel.h"
#include "UI/TransportBar.h"
#include "UI/PianoKeyboard.h"
#include "UI/PreferencesDialog.h"
#include "UI/DiagnosticsDialog.h"

#include <QApplication>
#include <QDockWidget>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>
#include <QStatusBar>

namespace myvocal {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_project(std::make_unique<Project>())
    , m_renderer(&m_singers, this)
{
    buildUi();
    buildMenus();
    connectUi();
    refreshVoiceBanks();
    applyDefaults(*m_project);
    m_trackPanel->refresh();
    m_editor->setActiveTrack(0);
    m_renderer.setResampler(
        QSettings().value("renderer/moresampler").toString());
    updateTitle();
    resize(1440, 900);
}

void MainWindow::buildUi()
{
    m_toolbar = new MainToolBar(this);
    addToolBar(Qt::TopToolBarArea, m_toolbar);

    m_trackPanel = new TrackPanel(m_project.get(), &m_singers, this);
    auto* trackDock = new QDockWidget(QStringLiteral("Tracks"), this);
    trackDock->setObjectName(QStringLiteral("TrackDock"));
    trackDock->setMinimumWidth(300);
    trackDock->setWidget(m_trackPanel);
    addDockWidget(Qt::LeftDockWidgetArea, trackDock);

    m_editor = new PianoRollEditor(m_project.get(), this);
    m_keyboard = new PianoKeyboard(this);
    m_keyboard->setRowHeight(22);
    m_keyboard->setScrollPitch(108);

    auto* editorContainer = new QWidget(this);
    auto* editorLayout = new QHBoxLayout(editorContainer);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);
    editorLayout->addWidget(m_keyboard);
    editorLayout->addWidget(m_editor, 1);
    setCentralWidget(editorContainer);

    m_params = new ParameterPanel(m_project.get(), this);
    auto* parameterDock = new QDockWidget(QStringLiteral("Parameters"), this);
    parameterDock->setObjectName(QStringLiteral("ParameterDock"));
    parameterDock->setMinimumHeight(150);
    parameterDock->setWidget(m_params);
    addDockWidget(Qt::BottomDockWidgetArea, parameterDock);

    m_transport = new TransportBar(&m_audio, this);
    statusBar()->addPermanentWidget(m_transport, 1);
}

void MainWindow::buildMenus()
{
    auto* file = menuBar()->addMenu(QStringLiteral("File"));
    file->addAction(QStringLiteral("New"), this, &MainWindow::newProject, QKeySequence::New);
    file->addAction(QStringLiteral("Open"), this, &MainWindow::openProject, QKeySequence::Open);
    file->addAction(QStringLiteral("Save"), this, &MainWindow::saveProject, QKeySequence::Save);
    file->addAction(QStringLiteral("Save As"), this, &MainWindow::saveProjectAs, QKeySequence::SaveAs);
    file->addSeparator();
    file->addAction(QStringLiteral("Import MIDI"), this, &MainWindow::importMidi);
    file->addAction(QStringLiteral("Export WAV"), this, &MainWindow::exportWav);
    file->addSeparator();
    file->addAction(QStringLiteral("Exit"), qApp, &QApplication::quit);

    auto* edit = menuBar()->addMenu(QStringLiteral("Edit"));
    edit->addAction(QStringLiteral("Undo"), this, [this] {
        m_undo.undo();
        m_editor->update();
    }, QKeySequence::Undo);
    edit->addAction(QStringLiteral("Redo"), this, [this] {
        m_undo.redo();
        m_editor->update();
    }, QKeySequence::Redo);
    edit->addAction(QStringLiteral("Delete"), this, [this] {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
        QApplication::sendEvent(m_editor, &event);
    }, QKeySequence::Delete);

    auto* track = menuBar()->addMenu(QStringLiteral("Track"));
    track->addAction(QStringLiteral("Add Track"), this, &MainWindow::addTrack);
    track->addAction(QStringLiteral("Delete Track"), this, &MainWindow::removeTrack);

    auto* singer = menuBar()->addMenu(QStringLiteral("Singer"));
    singer->addAction(QStringLiteral("Rescan VoiceBanks"), this,
                      &MainWindow::rescanVoiceBanks);
    singer->addAction(QStringLiteral("Singer Diagnostics"), this,
                      &MainWindow::showDiagnostics);

    auto* tools = menuBar()->addMenu(QStringLiteral("Tools"));
    tools->addAction(QStringLiteral("Preferences"), this,
                     &MainWindow::showPreferences);
    tools->addAction(QStringLiteral("Rescan VoiceBanks"), this,
                     &MainWindow::rescanVoiceBanks);
    tools->addAction(QStringLiteral("Debug Window"), this,
                     &MainWindow::showDiagnostics);

    auto* render = menuBar()->addAction(QStringLiteral("Render"));
    render->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    connect(render, &QAction::triggered, this, &MainWindow::renderProject);
}

void MainWindow::connectUi()
{
    connect(m_toolbar, &MainToolBar::toolChanged,
            this, &MainWindow::setTool);
    connect(m_toolbar, &MainToolBar::snapToggled,
            this, [this](bool enabled) { m_editor->setSnapEnabled(enabled); });
    connect(m_toolbar, &MainToolBar::gridToggled,
            this, [this](bool enabled) { m_editor->setShowGrid(enabled); });

    connect(m_trackPanel, &TrackPanel::trackSelected,
            this, &MainWindow::selectTrack);
    connect(m_trackPanel, &TrackPanel::trackSettingsChanged,
            this, [this](int) {
                m_editor->update();
                updateTitle();
            });

    connect(m_editor, &PianoRollEditor::documentChanged,
            this, [this] { updateTitle(); });
    connect(m_editor, &PianoRollEditor::verticalPitchChanged,
            this, [this](int topMidi) {
                m_keyboard->setScrollPitch(topMidi);
            });
    connect(m_keyboard, &PianoKeyboard::keyPressed,
            m_editor, &PianoRollEditor::setKeyboardPitch);

    connect(m_transport, &TransportBar::playPause,
            this, &MainWindow::togglePlay);
    connect(m_transport, &TransportBar::stopPressed,
            this, &MainWindow::stopPlayback);
}

void MainWindow::refreshVoiceBanks()
{
    const QSettings settings;
    const QString configured = settings.value("voicebanks/path").toString();
    const std::filesystem::path exeDir(
        QCoreApplication::applicationDirPath().toStdWString());
    const std::filesystem::path projectRoot =
        m_project && !m_project->path().empty()
            ? m_project->path().parent_path()
            : exeDir.parent_path();
    const std::filesystem::path workingDir = std::filesystem::current_path();

    std::vector<std::filesystem::path> roots = {
        exeDir / "VoiceBanks",
        projectRoot / "VoiceBanks",
        workingDir / "VoiceBanks"
    };
    if (!configured.isEmpty()) {
        roots.insert(roots.begin(),
                     std::filesystem::path(configured.toStdWString()));
    }
    m_singers.scan(roots);
}

void MainWindow::rescanVoiceBanks()
{
    refreshVoiceBanks();
    m_trackPanel->setSingerManager(&m_singers);
    m_trackPanel->refresh();
    statusBar()->showMessage(
        QStringLiteral("VoiceBanks: %1 valid / %2 found")
            .arg(m_singers.validCount())
            .arg(m_singers.singers().size()), 5000);
}

void MainWindow::applyDefaults(Project& project)
{
    const QSettings settings;
    project.tempoMap().setBpm(
        settings.value("defaults/bpm", 120.0).toDouble());

    const QString defaultPhonemizer =
        settings.value("defaults/phonemizer", "Default CV").toString();
    const QString defaultSinger = settings.value("defaults/singer").toString();

    if (project.tracks().isEmpty()) {
        project.addTrack();
    }

    for (auto& track : project.tracks()) {
        track.setPhonemizer(defaultPhonemizer);
        if (!defaultSinger.isEmpty()) {
            if (auto singer = m_singers.findByName(defaultSinger)) {
                track.setSingerPath(
                    QString::fromStdWString(singer->path().wstring()));
            } else if (auto singer = m_singers.findByPath(defaultSinger)) {
                track.setSingerPath(
                    QString::fromStdWString(singer->path().wstring()));
            }
        }
        if (track.singerPath().isEmpty() && !m_singers.singers().empty()) {
            const auto& singer = m_singers.singers().front();
            track.setSingerPath(
                QString::fromStdWString(singer->path().wstring()));
        }
    }
}

void MainWindow::newProject()
{
    auto project = std::make_unique<Project>();
    applyDefaults(*project);
    setProject(std::move(project));
}

void MainWindow::openProject()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open Project"), {},
        QStringLiteral("MyVocalSynth Project (*.vocalproj);;All Files (*)"));
    if (path.isEmpty()) {
        return;
    }

    QString error;
    auto project = Project::load(
        std::filesystem::path(path.toStdWString()), &error);
    if (!project) {
        QMessageBox::critical(this, QStringLiteral("Open failed"), error);
        return;
    }
    setProject(std::move(project));
}

void MainWindow::saveProject()
{
    if (m_project->path().empty()) {
        saveProjectAs();
        return;
    }
    QString error;
    if (!m_project->save(m_project->path(), &error)) {
        QMessageBox::critical(this, QStringLiteral("Save failed"), error);
    }
}

void MainWindow::saveProjectAs()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save Project"), {},
        QStringLiteral("MyVocalSynth Project (*.vocalproj)"));
    if (path.isEmpty()) {
        return;
    }

    const auto filesystemPath = std::filesystem::path(path.toStdWString());
    QString error;
    if (m_project->save(filesystemPath, &error)) {
        m_project->setPath(filesystemPath);
        updateTitle();
    } else {
        QMessageBox::critical(this, QStringLiteral("Save failed"), error);
    }
}

void MainWindow::importMidi()
{
    QMessageBox::information(
        this, QStringLiteral("MIDI"),
        QStringLiteral("MIDI import is not implemented in this build yet."));
}

void MainWindow::exportWav()
{
    renderSelected();
}

void MainWindow::addTrack()
{
    Track& track = m_project->addTrack();
    const QSettings settings;
    track.setPhonemizer(
        settings.value("defaults/phonemizer", "Default CV").toString());
    if (!m_singers.singers().empty()) {
        track.setSingerPath(
            QString::fromStdWString(m_singers.singers().front()->path().wstring()));
    }
    m_trackPanel->refresh();
    m_trackPanel->setCurrentRow(m_project->tracks().size() - 1);
    m_editor->setActiveTrack(m_project->tracks().size() - 1);
    updateTitle();
}

void MainWindow::removeTrack()
{
    if (m_project->tracks().size() <= 1) {
        return;
    }
    int index = m_trackPanel->currentRow();
    if (index < 0) {
        index = m_project->tracks().size() - 1;
    }
    if (!m_project->removeTrack(index)) {
        return;
    }
    m_trackPanel->refresh();
    const int active = qBound(0, index, m_project->tracks().size() - 1);
    m_trackPanel->setCurrentRow(active);
    m_editor->setActiveTrack(active);
    updateTitle();
}

void MainWindow::selectTrack(int index)
{
    m_editor->setActiveTrack(index);
}

void MainWindow::setTool(EditTool tool)
{
    m_editor->setTool(tool);
}

void MainWindow::togglePlay()
{
    if (m_audio.isPlaying()) {
        m_audio.pause();
    } else {
        m_audio.play();
    }
}

void MainWindow::stopPlayback()
{
    m_audio.stop();
    m_editor->setPlayheadTick(0);
}

void MainWindow::renderProject()
{
    renderSelected();
}

void MainWindow::renderSelected()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Render Project"), {}, QStringLiteral("WAV (*.wav)"));
    if (path.isEmpty()) {
        return;
    }

    QProgressDialog progress(
        QStringLiteral("Rendering..."), QStringLiteral("Cancel"), 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    connect(&m_renderer, &Renderer::progress,
            &progress, &QProgressDialog::setValue);

    QString error;
    if (m_renderer.renderProject(*m_project, path, &error)) {
        m_audio.load(path);
        QMessageBox::information(
            this, QStringLiteral("Render"), QStringLiteral("Render complete."));
    } else {
        QMessageBox::warning(this, QStringLiteral("Render failed"), error);
    }
}

void MainWindow::showPreferences()
{
    PreferencesDialog dialog(&m_resampler, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QSettings settings;
    m_resampler = settings.value("renderer/moresampler", m_resampler).toString();
    m_renderer.setResampler(m_resampler);
    refreshVoiceBanks();
    m_trackPanel->setSingerManager(&m_singers);
    m_trackPanel->refresh();
}

void MainWindow::showDiagnostics()
{
    QString text = QStringLiteral("VoiceBank Diagnostics\n\nSearch roots:\n");
    for (const auto& root : m_singers.searchRoots()) {
        text += QStringLiteral("  %1\n").arg(root);
    }
    text += QStringLiteral("\nSingers:\n");

    for (const auto& singer : m_singers.singers()) {
        text += QStringLiteral("%1\n  Path: %2\n  oto.ini: %3\n")
                    .arg(singer->info().name)
                    .arg(QString::fromStdWString(singer->path().wstring()))
                    .arg(singer->isValid()
                             ? QStringLiteral("OK (%1 entries)")
                                   .arg(singer->oto().getEntries().size())
                             : QStringLiteral("ERROR: %1").arg(singer->oto().error()));
    }

    DiagnosticsDialog dialog(text, this);
    dialog.exec();
}

void MainWindow::setProject(std::unique_ptr<Project> project)
{
    if (!project) {
        return;
    }

    if (m_editor) {
        disconnect(m_toolbar, &MainToolBar::snapToggled,
                   m_editor, &PianoRollEditor::setSnapEnabled);
        disconnect(m_toolbar, &MainToolBar::gridToggled,
                   m_editor, &PianoRollEditor::setShowGrid);
    }

    m_project = std::move(project);
    refreshVoiceBanks();

    if (auto* old = centralWidget()) {
        old->deleteLater();
    }

    m_editor = new PianoRollEditor(m_project.get(), this);
    m_keyboard = new PianoKeyboard(this);
    m_keyboard->setRowHeight(22);
    m_keyboard->setScrollPitch(108);

    auto* editorContainer = new QWidget(this);
    auto* editorLayout = new QHBoxLayout(editorContainer);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);
    editorLayout->addWidget(m_keyboard);
    editorLayout->addWidget(m_editor, 1);
    setCentralWidget(editorContainer);

    if (auto* dock = findChild<QDockWidget*>(QStringLiteral("TrackDock"))) {
        if (m_trackPanel) {
            m_trackPanel->deleteLater();
        }
        m_trackPanel = new TrackPanel(m_project.get(), &m_singers, dock);
        dock->setWidget(m_trackPanel);
    }

    if (auto* dock = findChild<QDockWidget*>(QStringLiteral("ParameterDock"))) {
        if (m_params) {
            m_params->deleteLater();
        }
        m_params = new ParameterPanel(m_project.get(), dock);
        dock->setWidget(m_params);
    }

    m_undo.clear();
    m_trackPanel->refresh();
    m_editor->setActiveTrack(0);

    connect(m_toolbar, &MainToolBar::snapToggled,
            m_editor, &PianoRollEditor::setSnapEnabled);
    connect(m_toolbar, &MainToolBar::gridToggled,
            m_editor, &PianoRollEditor::setShowGrid);
    connect(m_trackPanel, &TrackPanel::trackSelected,
            this, &MainWindow::selectTrack);
    connect(m_trackPanel, &TrackPanel::trackSettingsChanged,
            this, [this](int) {
                m_editor->update();
                updateTitle();
            });
    connect(m_editor, &PianoRollEditor::documentChanged,
            this, [this] { updateTitle(); });
    connect(m_editor, &PianoRollEditor::verticalPitchChanged,
            this, [this](int topMidi) { m_keyboard->setScrollPitch(topMidi); });
    connect(m_keyboard, &PianoKeyboard::keyPressed,
            m_editor, &PianoRollEditor::setKeyboardPitch);

    const QSettings settings;
    m_editor->setGridTicks(settings.value("defaults/grid", 120).toInt());
    m_editor->setSnapEnabled(true);
    m_editor->setShowGrid(true);
    updateTitle();
}

void MainWindow::updateTitle()
{
    if (m_project) {
        setWindowTitle(QStringLiteral("%1 — MyVocalSynth").arg(m_project->title()));
    }
}

QString MainWindow::defaultProjectPath() const
{
    return {};
}

}
