#include "App/MainWindow.h"

#include "UI/ArrangementEditor.h"
#include "UI/DiagnosticsDialog.h"
#include "UI/MainToolBar.h"
#include "UI/ParameterPanel.h"
#include "UI/PianoKeyboard.h"
#include "UI/PreferencesDialog.h"
#include "UI/TrackPanel.h"
#include "UI/TransportBar.h"

#include <QApplication>
#include <QDockWidget>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <filesystem>

namespace myvocal {

namespace {
QString findMoresampler(const std::filesystem::path& exeDir,
                        const std::filesystem::path& projectRoot,
                        const std::filesystem::path& cwd)
{
    const QString configured = QSettings().value("renderer/moresampler").toString();
    if (!configured.isEmpty() && QFileInfo::isFile(configured)) return configured;

    const QList<std::filesystem::path> roots = {
        exeDir / "resampler", projectRoot / "resampler", cwd / "resampler"};
    const QStringList names = {
        QStringLiteral("moresampler.exe"),
        QStringLiteral("resampler.exe")};

    for (const auto& root : roots) {
        std::error_code ec;
        if (!std::filesystem::is_directory(root, ec)) continue;
        for (const QString& name : names) {
            const auto candidate = root / name.toStdWString();
            if (std::filesystem::is_regular_file(candidate, ec)) {
                return QString::fromStdWString(candidate.wstring());
            }
        }
    }
    return {};
}

qint64 probeDurationMs(const QString& path)
{
    QMediaPlayer player;
    QAudioOutput output;
    player.setAudioOutput(&output);
    player.setSource(QUrl::fromLocalFile(path));

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(2000);
    QObject::connect(&player, &QMediaPlayer::durationChanged, &loop, &QEventLoop::quit);
    QObject::connect(&player, &QMediaPlayer::errorOccurred, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start();
    loop.exec();
    return qMax<qint64>(0, player.duration());
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_project(std::make_unique<Project>()),
      m_renderer(&m_singers, this)
{
    m_playback = std::make_unique<PlaybackController>(&m_audio, &m_renderer, this);

    buildUi();
    buildMenus();
    connectUi();

    refreshVoiceBanks();
    applyDefaults(*m_project);

    const auto exeDir = std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString());
    const auto projectRoot = exeDir.parent_path();
    m_resampler = findMoresampler(exeDir, projectRoot, std::filesystem::current_path());
    if (m_resampler.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("Moresampler not found; playback/render will report the searched paths."), 7000);
    }
    m_renderer.setResampler(m_resampler);
    m_playback->setProject(m_project.get());

    m_trackPanel->refresh();
    m_trackPanel->setCurrentRow(0);
    resize(1440, 930);
    updateTitle();
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

    auto* pianoContainer = new QWidget(this);
    auto* pianoLayout = new QHBoxLayout(pianoContainer);
    pianoLayout->setContentsMargins(0, 0, 0, 0);
    pianoLayout->setSpacing(0);
    pianoLayout->addWidget(m_keyboard);
    pianoLayout->addWidget(m_editor, 1);

    m_arrangement = new ArrangementEditor(m_project.get(), this);
    m_arrangement->setPixelsPerSecond(90.0);

    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(pianoContainer);
    splitter->addWidget(m_arrangement);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    m_params = new ParameterPanel(m_project.get(), this);
    auto* parameterDock = new QDockWidget(QStringLiteral("Parameters"), this);
    parameterDock->setObjectName(QStringLiteral("ParameterDock"));
    parameterDock->setMinimumHeight(160);
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
    file->addAction(QStringLiteral("Import Audio..."), this, &MainWindow::importAudio);
    file->addAction(QStringLiteral("Import MIDI..."), this, &MainWindow::importMidi);
    file->addAction(QStringLiteral("Export WAV..."), this, &MainWindow::exportWav);
    file->addSeparator();
    file->addAction(QStringLiteral("Exit"), qApp, &QApplication::quit);

    auto* edit = menuBar()->addMenu(QStringLiteral("Edit"));
    edit->addAction(QStringLiteral("Undo"), this, [this] { m_undo.undo(); m_editor->update(); m_arrangement->update(); });
    edit->addAction(QStringLiteral("Redo"), this, [this] { m_undo.redo(); m_editor->update(); m_arrangement->update(); });
    edit->addAction(QStringLiteral("Delete"), this, [this] {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
        QApplication::sendEvent(m_editor, &event);
    }, QKeySequence::Delete);

    auto* track = menuBar()->addMenu(QStringLiteral("Track"));
    track->addAction(QStringLiteral("Add Track"), this, &MainWindow::addTrack);
    track->addAction(QStringLiteral("Delete Track"), this, &MainWindow::removeTrack);

    auto* singer = menuBar()->addMenu(QStringLiteral("Singer"));
    singer->addAction(QStringLiteral("Rescan VoiceBanks"), this, &MainWindow::rescanVoiceBanks);
    singer->addAction(QStringLiteral("Diagnostics"), this, &MainWindow::showDiagnostics);

    auto* tools = menuBar()->addMenu(QStringLiteral("Tools"));
    tools->addAction(QStringLiteral("Preferences"), this, &MainWindow::showPreferences);
    tools->addAction(QStringLiteral("Rescan VoiceBanks"), this, &MainWindow::rescanVoiceBanks);
    tools->addAction(QStringLiteral("Diagnostics"), this, &MainWindow::showDiagnostics);

    auto* render = menuBar()->addAction(QStringLiteral("Render"));
    render->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    connect(render, &QAction::triggered, this, &MainWindow::renderProject);
}

void MainWindow::connectUi()
{
    connect(m_toolbar, &MainToolBar::toolChanged, this, &MainWindow::setTool);
    connect(m_toolbar, &MainToolBar::snapToggled, m_editor, &PianoRollEditor::setSnapEnabled);
    connect(m_toolbar, &MainToolBar::gridToggled, m_editor, &PianoRollEditor::setShowGrid);
    connect(m_toolbar, &MainToolBar::gridResolutionChanged, this, [this](int div) {
        m_editor->setGridTicks(qMax<qint64>(1, qRound64(m_project->ppq() * 4.0 / div)));
    });
    connect(m_trackPanel, &TrackPanel::trackSelected, this, &MainWindow::selectTrack);
    connect(m_trackPanel, &TrackPanel::trackSettingsChanged, this, [this](int) {
        m_playback->invalidateCache();
        m_editor->update();
        m_arrangement->update();
        updateTitle();
    });
    connect(m_editor, &PianoRollEditor::documentChanged, this, [this] {
        m_playback->invalidateCache();
        m_arrangement->update();
        updateTitle();
    });
    connect(m_editor, &PianoRollEditor::verticalPitchChanged, this,
            [this](int topMidi) { m_keyboard->setScrollPitch(topMidi); });
    connect(m_keyboard, &PianoKeyboard::keyPressed,
            m_editor, &PianoRollEditor::setKeyboardPitch);

    connect(m_arrangement, &ArrangementEditor::positionClicked,
            this, &MainWindow::seekFromTimeline);
    connect(m_playback.get(), &PlaybackController::positionChanged, this,
            [this](qint64 ms) {
                const qint64 tick = qRound64(m_project->tempoMap().secondsToTick(ms / 1000.0, m_project->ppq()));
                m_editor->setPlayheadTick(tick);
                m_arrangement->setPlayheadMs(ms);
            });
    connect(m_playback.get(), &PlaybackController::preparingChanged, this,
            [this](bool preparing) {
                if (preparing) statusBar()->showMessage(QStringLiteral("Preparing voice playback..."));
                else statusBar()->clearMessage();
            });
    connect(m_playback.get(), &PlaybackController::playbackError, this,
            [this](const QString& error) {
                QMessageBox::critical(this, QStringLiteral("Playback failed"), error);
            });

    connect(&m_audio, &AudioEngine::mediaError, this, [this](const QString& error) {
        statusBar()->showMessage(QStringLiteral("Audio: %1").arg(error), 5000);
    });
}

void MainWindow::refreshVoiceBanks()
{
    const QSettings settings;
    const QString configured = settings.value("voicebanks/path").toString();
    const auto exeDir = std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString());
    const auto projectRoot = m_project && !m_project->path().empty() ? m_project->path().parent_path() : exeDir.parent_path();
    const auto cwd = std::filesystem::current_path();

    std::vector<std::filesystem::path> roots = {
        exeDir / "VoiceBanks", projectRoot / "VoiceBanks", cwd / "VoiceBanks"};
    if (!configured.isEmpty()) roots.insert(roots.begin(), std::filesystem::path(configured.toStdWString()));
    m_singers.scan(roots);
}

void MainWindow::applyDefaults(Project& project)
{
    const QSettings settings;
    project.tempoMap().setBpm(settings.value("defaults/bpm", 120.0).toDouble());
    const QString defaultPhonemizer = settings.value("defaults/phonemizer", "Default CV").toString();
    const QString defaultSinger = settings.value("defaults/singer").toString();
    if (project.tracks().isEmpty()) project.addTrack();

    for (auto& track : project.tracks()) {
        if (track.phonemizer().isEmpty()) track.setPhonemizer(defaultPhonemizer);
        if (!defaultSinger.isEmpty()) {
            if (auto singer = m_singers.findByName(defaultSinger)) track.setSingerPath(QString::fromStdWString(singer->path().wstring()));
        }
        if (track.singerPath().isEmpty() && !m_singers.singers().empty()) track.setSingerPath(QString::fromStdWString(m_singers.singers().front()->path().wstring()));
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
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open Project"), {}, QStringLiteral("MyVocalSynth Project (*.vocalproj);;All Files (*)"));
    if (path.isEmpty()) return;
    QString error;
    auto project = Project::load(std::filesystem::path(path.toStdWString()), &error);
    if (!project) { QMessageBox::critical(this, QStringLiteral("Open failed"), error); return; }
    setProject(std::move(project));
}

void MainWindow::saveProject()
{
    if (m_project->path().empty()) { saveProjectAs(); return; }
    QString error;
    if (!m_project->save(m_project->path(), &error)) QMessageBox::critical(this, QStringLiteral("Save failed"), error);
}

void MainWindow::saveProjectAs()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Project"), {}, QStringLiteral("MyVocalSynth Project (*.vocalproj)"));
    if (path.isEmpty()) return;
    const auto filePath = std::filesystem::path(path.toStdWString());
    QString error;
    if (!m_project->save(filePath, &error)) { QMessageBox::critical(this, QStringLiteral("Save failed"), error); return; }
    m_project->setPath(filePath);
    updateTitle();
}

void MainWindow::importAudio()
{
    const QStringList paths = QFileDialog::getOpenFileNames(this, QStringLiteral("Import Audio"), {}, QStringLiteral("Audio (*.wav *.mp3)"));
    if (paths.isEmpty()) return;

    const qint64 startMs = currentMs();
    for (const QString& path : paths) {
        AudioClip clip;
        clip.path = path;
        clip.startMs = startMs;
        clip.durationMs = probeDurationMs(path);
        m_project->addAudioClip(clip);
    }

    m_audio.setBackingClips(m_project->audioClips());
    m_arrangement->update();
    updateTitle();
}

void MainWindow::importMidi()
{
    QMessageBox::information(this, QStringLiteral("MIDI"), QStringLiteral("MIDI import is not implemented as a fake UI action in this branch."));
}

void MainWindow::exportWav()
{
    renderProject();
}

void MainWindow::addTrack()
{
    Track& track = m_project->addTrack();
    track.setPhonemizer(QSettings().value("defaults/phonemizer", "Default CV").toString());
    if (!m_singers.singers().empty()) track.setSingerPath(QString::fromStdWString(m_singers.singers().front()->path().wstring()));
    m_trackPanel->refresh();
    const int index = m_project->tracks().size() - 1;
    m_trackPanel->setCurrentRow(index);
    m_editor->setActiveTrack(index);
    m_playback->invalidateCache();
    updateTitle();
}

void MainWindow::removeTrack()
{
    if (m_project->tracks().size() <= 1) return;
    int index = m_trackPanel->currentRow();
    if (index < 0) index = m_project->tracks().size() - 1;
    if (!m_project->removeTrack(index)) return;
    const int active = qBound(0, index, m_project->tracks().size() - 1);
    m_trackPanel->refresh();
    m_trackPanel->setCurrentRow(active);
    m_editor->setActiveTrack(active);
    m_playback->invalidateCache();
    updateTitle();
}

void MainWindow::selectTrack(int index)
{
    m_editor->setActiveTrack(index);
    m_editor->setFocus();
}

void MainWindow::setTool(EditTool tool)
{
    m_editor->setTool(tool);
}

void MainWindow::togglePlay()
{
    if (m_audio.isPlaying()) m_playback->pause();
    else m_playback->playFromMs(currentMs());
}

void MainWindow::stopPlayback()
{
    const bool returnToStart = QSettings().value("playback/stopBehavior", 0).toInt() == 0;
    m_playback->stop(returnToStart);
}

void MainWindow::renderProject()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Render Project"), {}, QStringLiteral("WAV (*.wav)"));
    if (path.isEmpty()) return;

    QProgressDialog progress(QStringLiteral("Rendering..."), QStringLiteral("Cancel"), 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    connect(&m_renderer, &Renderer::progress, &progress, &QProgressDialog::setValue, Qt::UniqueConnection);

    QString error;
    if (m_renderer.renderProject(*m_project, path, &error)) {
        m_audio.load(path);
        m_audio.setBackingClips(m_project->audioClips());
        statusBar()->showMessage(QStringLiteral("Render complete: %1").arg(path), 6000);
    } else {
        QMessageBox::critical(this, QStringLiteral("Render failed"), error);
    }
}

void MainWindow::showPreferences()
{
    PreferencesDialog dialog(&m_resampler, &m_singers, this);
    if (dialog.exec() != QDialog::Accepted) return;

    refreshVoiceBanks();
    m_resampler = QSettings().value("renderer/moresampler", m_resampler).toString();
    if (m_resampler.isEmpty()) {
        const auto exeDir = std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString());
        m_resampler = findMoresampler(exeDir, exeDir.parent_path(), std::filesystem::current_path());
    }
    m_renderer.setResampler(m_resampler);
    m_trackPanel->setSingerManager(&m_singers);
    m_trackPanel->refresh();
    m_playback->invalidateCache();
}

void MainWindow::showDiagnostics()
{
    QString text = QStringLiteral("VoiceBank Diagnostics\n\n");
    for (const auto& root : m_singers.searchRoots()) text += QStringLiteral("Search root: %1\n").arg(root);
    text += QStringLiteral("\nMoresampler: %1\n\n").arg(m_resampler.isEmpty() ? QStringLiteral("NOT FOUND") : m_resampler);
    for (const auto& singer : m_singers.singers()) {
        text += QStringLiteral("Singer: %1\nPath: %2\noto.ini: %3\nAliases: %4\n\n")
            .arg(singer->info().name)
            .arg(QString::fromStdWString(singer->path().wstring()))
            .arg(singer->isValid() ? QStringLiteral("OK") : singer->oto().error())
            .arg(singer->oto().getEntries().size());
    }
    DiagnosticsDialog dialog(text, this);
    dialog.exec();
}

void MainWindow::rescanVoiceBanks()
{
    refreshVoiceBanks();
    m_trackPanel->setSingerManager(&m_singers);
    m_trackPanel->refresh();
    statusBar()->showMessage(QStringLiteral("VoiceBanks: %1 found").arg(m_singers.singers().size()), 4000);
}

void MainWindow::seekFromTimeline(qint64 ms)
{
    m_playback->seekMs(ms);
}

void MainWindow::setProject(std::unique_ptr<Project> project)
{
    if (!project) return;
    m_project = std::move(project);
    refreshVoiceBanks();

    if (m_playback) m_playback->setProject(m_project.get());
    m_audio.setBackingClips(m_project->audioClips());

    if (auto* oldCentral = centralWidget()) oldCentral->deleteLater();
    m_editor = new PianoRollEditor(m_project.get(), this);
    m_keyboard = new PianoKeyboard(this);
    m_keyboard->setRowHeight(22);
    m_keyboard->setScrollPitch(108);
    auto* pianoContainer = new QWidget(this);
    auto* pianoLayout = new QHBoxLayout(pianoContainer);
    pianoLayout->setContentsMargins(0, 0, 0, 0);
    pianoLayout->setSpacing(0);
    pianoLayout->addWidget(m_keyboard);
    pianoLayout->addWidget(m_editor, 1);
    m_arrangement = new ArrangementEditor(m_project.get(), this);
    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(pianoContainer);
    splitter->addWidget(m_arrangement);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    if (auto* dock = findChild<QDockWidget*>(QStringLiteral("TrackDock"))) {
        if (m_trackPanel) m_trackPanel->deleteLater();
        m_trackPanel = new TrackPanel(m_project.get(), &m_singers, dock);
        dock->setWidget(m_trackPanel);
    }
    if (auto* dock = findChild<QDockWidget*>(QStringLiteral("ParameterDock"))) {
        if (m_params) m_params->deleteLater();
        m_params = new ParameterPanel(m_project.get(), dock);
        dock->setWidget(m_params);
    }

    m_undo.clear();
    m_trackPanel->refresh();
    m_trackPanel->setCurrentRow(0);
    m_editor->setActiveTrack(0);

    connectUi();
    updateTitle();
}

qint64 MainWindow::currentTick() const
{
    return m_editor ? m_editor->playheadTick() : 0;
}

qint64 MainWindow::currentMs() const
{
    if (!m_project) return 0;
    return qRound64(m_project->tempoMap().tickToSeconds(static_cast<double>(currentTick()), m_project->ppq()) * 1000.0);
}

void MainWindow::updateTitle()
{
    setWindowTitle(QStringLiteral("%1 — MyVocalSynth").arg(m_project ? m_project->title() : QStringLiteral("Untitled")));
}

}