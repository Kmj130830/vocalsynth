#include "App/MainWindow.h"

#include "UI/ArrangementEditor.h"
#include "UI/AudioSpectrumWidget.h"
#include "UI/DiagnosticsDialog.h"
#include "UI/MainToolBar.h"
#include "UI/PhonemeStripEditor.h"
#include "UI/PianoKeyboard.h"
#include "UI/PreferencesDialog.h"
#include "UI/TrackPanel.h"
#include "UI/TransportBar.h"
#include "Undo/Commands.h"

#include <QApplication>
#include <QAudioOutput>
#include <QDockWidget>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressDialog>
#include <QShortcut>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <filesystem>

namespace myvocal {
namespace {
QString findMoresampler(const std::filesystem::path& exeDir, const std::filesystem::path& projectRoot, const std::filesystem::path& cwd)
{
    const QString configured = QSettings().value("renderer/moresampler").toString();
    if (!configured.isEmpty() && QFileInfo(configured).isFile()) return configured;
    const QList<std::filesystem::path> roots = {exeDir / "resampler", projectRoot / "resampler", cwd / "resampler"};
    const QStringList names = {QStringLiteral("moresampler.exe"), QStringLiteral("resampler.exe")};
    for (const auto& root : roots) {
        std::error_code ec;
        if (!std::filesystem::is_directory(root, ec)) continue;
        for (const QString& name : names) {
            const auto candidate = root / name.toStdWString();
            if (std::filesystem::is_regular_file(candidate, ec)) return QString::fromStdWString(candidate.wstring());
        }
    }
    return {};
}
qint64 probeDurationMs(const QString& path)
{
    QMediaPlayer player; QAudioOutput output;
    player.setAudioOutput(&output); player.setSource(QUrl::fromLocalFile(path));
    QEventLoop loop; QTimer timeout; timeout.setSingleShot(true); timeout.setInterval(3000);
    QObject::connect(&player, &QMediaPlayer::durationChanged, &loop, &QEventLoop::quit);
    QObject::connect(&player, &QMediaPlayer::errorOccurred, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(); loop.exec();
    return qMax<qint64>(0, player.duration());
}
bool isTextInputFocused()
{
    QWidget* widget = QApplication::focusWidget();
    while (widget) {
        if (qobject_cast<QLineEdit*>(widget)) return true;
        widget = widget->parentWidget();
    }
    return false;
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), m_project(std::make_unique<Project>()), m_renderer(&m_singers, this)
{
    m_playback = std::make_unique<PlaybackController>(&m_audio, &m_renderer, this);
    buildUi(); buildMenus(); connectUi();
    refreshVoiceBanks(); applyDefaults(*m_project);
    const auto exeDir = std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString());
    m_resampler = findMoresampler(exeDir, exeDir.parent_path(), std::filesystem::current_path());
    m_renderer.setResampler(m_resampler);
    if (m_resampler.isEmpty()) statusBar()->showMessage(QStringLiteral("Moresampler not found. Set it in Tools > Preferences."), 6000);
    m_playback->setProject(m_project.get());
    m_trackPanel->refresh(); m_trackPanel->setCurrentRow(0);
    m_editor->setActiveTrack(0); m_phonemeStrip->setActiveTrack(0);
    refreshUndoSnapshot();
    resize(1440, 930); updateTitle();
}

void MainWindow::buildUi()
{
    m_toolbar = new MainToolBar(this); addToolBar(Qt::TopToolBarArea, m_toolbar);
    m_trackPanel = new TrackPanel(m_project.get(), &m_singers, this);
    auto* trackDock = new QDockWidget(QStringLiteral("Tracks"), this);
    trackDock->setObjectName(QStringLiteral("TrackDock")); trackDock->setMinimumWidth(300); trackDock->setWidget(m_trackPanel);
    addDockWidget(Qt::LeftDockWidgetArea, trackDock);

    m_editor = new PianoRollEditor(m_project.get(), this);
    m_keyboard = new PianoKeyboard(this); m_keyboard->setRowHeight(22); m_keyboard->setScrollPitch(108);
    auto* pianoContainer = new QWidget(this);
    auto* pianoLayout = new QHBoxLayout(pianoContainer);
    pianoLayout->setContentsMargins(0, 0, 0, 0); pianoLayout->setSpacing(0);
    pianoLayout->addWidget(m_keyboard); pianoLayout->addWidget(m_editor, 1);

    m_arrangement = new ArrangementEditor(m_project.get(), this);
    m_arrangement->setPixelsPerSecond(90.0); m_arrangement->setTrackHeight(60);
    auto* spectrum = new AudioSpectrumWidget(m_project.get(), this);
    spectrum->setObjectName(QStringLiteral("AudioSpectrum")); spectrum->setMinimumHeight(72); spectrum->setMaximumHeight(100);
    m_phonemeStrip = new PhonemeStripEditor(m_project.get(), this); m_phonemeStrip->setPixelsPerSecond(90.0);

    auto* splitter = new QSplitter(Qt::Vertical, this); splitter->setChildrenCollapsible(false);
    splitter->addWidget(m_arrangement); splitter->addWidget(pianoContainer); splitter->addWidget(spectrum); splitter->addWidget(m_phonemeStrip);
    splitter->setStretchFactor(0, 2); splitter->setStretchFactor(1, 7); splitter->setStretchFactor(2, 1); splitter->setStretchFactor(3, 1);
    splitter->setSizes({180, 560, 84, 100}); setCentralWidget(splitter);

    m_transport = new TransportBar(&m_audio, this); statusBar()->addPermanentWidget(m_transport, 1);
}

void MainWindow::buildMenus()
{
    auto* file = menuBar()->addMenu(QStringLiteral("File"));
    file->addAction(QStringLiteral("New"), QKeySequence::New, this, &MainWindow::newProject);
    file->addAction(QStringLiteral("Open"), QKeySequence::Open, this, &MainWindow::openProject);
    file->addAction(QStringLiteral("Save"), QKeySequence::Save, this, &MainWindow::saveProject);
    file->addAction(QStringLiteral("Save As"), QKeySequence::SaveAs, this, &MainWindow::saveProjectAs);
    file->addSeparator(); file->addAction(QStringLiteral("Import Audio..."), this, &MainWindow::importAudio);
    file->addAction(QStringLiteral("Export WAV..."), this, &MainWindow::exportWav); file->addSeparator();
    file->addAction(QStringLiteral("Exit"), qApp, &QApplication::quit);

    auto* edit = menuBar()->addMenu(QStringLiteral("Edit"));
    auto* undo = edit->addAction(QStringLiteral("Undo"), this, [this] { m_undo.undo(); }); undo->setShortcut(QKeySequence::Undo);
    auto* redo = edit->addAction(QStringLiteral("Redo"), this, [this] { m_undo.redo(); }); redo->setShortcut(QKeySequence::Redo);
    auto* selectAll = edit->addAction(QStringLiteral("Select All")); selectAll->setShortcut(QKeySequence::SelectAll);
    connect(selectAll, &QAction::triggered, this, [this] {
        if (isTextInputFocused() || !m_project || !m_editor) return;
        const int index = m_editor->activeTrack();
        if (index < 0 || index >= m_project->tracks().size()) return;
        for (auto& note : m_project->tracks()[index].notes()) note.setSelected(true);
        m_editor->viewport()->update();
    });
    edit->addAction(QStringLiteral("Delete"), QKeySequence::Delete, this, [this] {
        if (isTextInputFocused() || !m_editor) return;
        m_editor->setFocus();
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
        QApplication::sendEvent(m_editor, &event);
    });

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
    auto* render = menuBar()->addAction(QStringLiteral("Render")); render->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    connect(render, &QAction::triggered, this, &MainWindow::renderProject);

    auto* space = new QShortcut(QKeySequence(Qt::Key_Space), this);
    space->setContext(Qt::WindowShortcut);
    connect(space, &QShortcut::activated, this, [this] { if (!isTextInputFocused()) togglePlay(); });
}

void MainWindow::connectUi()
{
    connect(m_toolbar, &MainToolBar::toolChanged, this, &MainWindow::setTool);
    connect(m_toolbar, &MainToolBar::snapToggled, this, [this](bool enabled) { if (m_project) { m_project->setSnapEnabled(enabled); m_editor->setSnapEnabled(enabled); } });
    connect(m_toolbar, &MainToolBar::gridToggled, this, [this](bool enabled) { if (m_project) { m_project->setGridVisible(enabled); m_editor->setShowGrid(enabled); } });
    connect(m_toolbar, &MainToolBar::gridResolutionChanged, this, [this](int div) { if (m_project && div > 0) { const qint64 ticks = qMax<qint64>(1, qRound64(m_project->ppq() * 4.0 / div)); m_project->setGridTicks(ticks); m_editor->setGridTicks(ticks); } });
    connect(m_trackPanel, &TrackPanel::trackSelected, this, &MainWindow::selectTrack);
    connect(m_trackPanel, &TrackPanel::addTrackRequested, this, &MainWindow::addTrack);
    connect(m_trackPanel, &TrackPanel::removeTrackRequested, this, [this](int index) { m_trackPanel->setCurrentRow(index); removeTrack(); });
    connect(m_trackPanel, &TrackPanel::trackSettingsChanged, this, [this](int) { m_playback->invalidateCache(); m_editor->viewport()->update(); m_arrangement->viewport()->update(); if (auto* s = findChild<AudioSpectrumWidget*>(QStringLiteral("AudioSpectrum"))) s->refresh(); updateTitle(); });

    connect(m_editor, &PianoRollEditor::documentChanged, this, [this] {
        if (!m_restoringSnapshot && m_project) {
            const QByteArray before = m_undoSnapshot;
            const QByteArray after = m_project->serializeJson();
            if (!before.isEmpty() && before != after) {
                m_undoSnapshot = after;
                auto apply = [this](const QByteArray& state) { restoreProjectSnapshot(state); };
                m_undo.push(std::make_unique<LambdaCommand>([apply, before] { apply(before); }, [apply, after] { apply(after); }));
            } else m_undoSnapshot = after;
        }
        m_playback->invalidateCache(); m_editor->viewport()->update(); m_arrangement->viewport()->update(); m_phonemeStrip->viewport()->update(); updateTitle();
    });
    connect(m_editor, &PianoRollEditor::verticalPitchChanged, this, [this](int topMidi) { m_keyboard->setScrollPitch(topMidi); });
    connect(m_editor, &PianoRollEditor::keyboardPreviewRequested, this, [this](int midi) { m_audio.previewTone(midi); });
    connect(m_editor, &PianoRollEditor::requestPlaybackTick, this, [this](qint64 tick) {
        if (!m_project) return;
        seekFromTimeline(qRound64(m_project->tempoMap().tickToSeconds(static_cast<double>(tick), m_project->ppq()) * 1000.0));
    });
    connect(m_keyboard, &PianoKeyboard::keyPressed, this, [this](int midi) { m_audio.previewTone(midi); });
    connect(m_arrangement, &ArrangementEditor::trackClicked, this, &MainWindow::selectTrack);
    connect(m_arrangement, &ArrangementEditor::positionClicked, this, &MainWindow::seekFromTimeline);
    connect(m_arrangement, &ArrangementEditor::documentChanged, this, [this] { m_playback->invalidateCache(); updateTitle(); });
    connect(m_phonemeStrip, &PhonemeStripEditor::positionClicked, this, &MainWindow::seekFromTimeline);
    connect(m_transport, &TransportBar::playPause, this, &MainWindow::togglePlay);
    connect(m_transport, &TransportBar::stopPressed, this, &MainWindow::stopPlayback);
    connect(m_playback.get(), &PlaybackController::positionChanged, this, [this](qint64 ms) {
        if (!m_project) return;
        const qint64 tick = qRound64(m_project->tempoMap().secondsToTick(ms / 1000.0, m_project->ppq()));
        m_editor->setPlayheadTick(tick); m_arrangement->setPlayheadMs(ms); m_phonemeStrip->setPlayheadMs(ms);
        if (auto* s = findChild<AudioSpectrumWidget*>(QStringLiteral("AudioSpectrum"))) s->setPlayheadMs(ms);
    });
    connect(m_playback.get(), &PlaybackController::preparingChanged, this, [this](bool preparing) { if (preparing) statusBar()->showMessage(QStringLiteral("Preparing voice playback...")); else statusBar()->clearMessage(); });
    connect(m_playback.get(), &PlaybackController::playbackError, this, [this](const QString& message) { QMessageBox::critical(this, QStringLiteral("Playback failed"), message); });
}

void MainWindow::refreshVoiceBanks()
{
    const QSettings settings; const QString configured = settings.value("voicebanks/path").toString();
    const auto exeDir = std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString());
    const auto projectRoot = m_project && !m_project->path().empty() ? m_project->path().parent_path() : exeDir;
    const auto cwd = std::filesystem::current_path(); std::vector<std::filesystem::path> roots;
    if (!configured.isEmpty()) roots.push_back(std::filesystem::path(configured.toStdWString()));
    roots.push_back(exeDir / "VoiceBanks"); roots.push_back(projectRoot / "VoiceBanks"); roots.push_back(cwd / "VoiceBanks");
    m_singers.scan(roots);
}

void MainWindow::applyDefaults(Project& project)
{
    const QSettings settings; const QString phonemizer = settings.value("defaults/phonemizer", "Default CV").toString(); const QString singerName = settings.value("defaults/singer").toString();
    if (project.tracks().isEmpty()) project.addTrack();
    for (auto& track : project.tracks()) {
        if (track.phonemizer().isEmpty()) track.setPhonemizer(phonemizer);
        if (!singerName.isEmpty()) if (auto singer = m_singers.findByName(singerName)) track.setSingerPath(QString::fromStdWString(singer->path().wstring()));
        if (track.singerPath().isEmpty()) for (const auto& singer : m_singers.singers()) if (singer->isValid()) { track.setSingerPath(QString::fromStdWString(singer->path().wstring())); break; }
    }
}

void MainWindow::newProject() { auto project = std::make_unique<Project>(); applyDefaults(*project); setProject(std::move(project)); }
void MainWindow::openProject()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open Project"), {}, QStringLiteral("MyVocalSynth Project (*.vocalproj);;All Files (*)"));
    if (path.isEmpty()) return; QString error; auto project = Project::load(std::filesystem::path(path.toStdWString()), &error);
    if (!project) { QMessageBox::critical(this, QStringLiteral("Open failed"), error); return; } setProject(std::move(project));
}
void MainWindow::saveProject()
{
    if (m_project->path().empty()) { saveProjectAs(); return; }
    QString error; if (!m_project->save(m_project->path(), &error)) QMessageBox::critical(this, QStringLiteral("Save failed"), error);
}
void MainWindow::saveProjectAs()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Project"), {}, QStringLiteral("MyVocalSynth Project (*.vocalproj)"));
    if (path.isEmpty()) return; const auto filePath = std::filesystem::path(path.toStdWString()); QString error;
    if (!m_project->save(filePath, &error)) { QMessageBox::critical(this, QStringLiteral("Save failed"), error); return; }
    m_project->setPath(filePath); updateTitle();
}
void MainWindow::importAudio()
{
    const QStringList paths = QFileDialog::getOpenFileNames(this, QStringLiteral("Import Audio"), {}, QStringLiteral("Audio (*.wav *.mp3 *.flac *.ogg *.m4a)"));
    if (paths.isEmpty()) return; const qint64 startMs = currentMs();
    for (const QString& path : paths) { AudioClip clip; clip.path = path; clip.startMs = startMs; clip.durationMs = qMax<qint64>(1000, probeDurationMs(path)); m_project->addAudioClip(clip); }
    m_audio.setBackingClips(m_project->audioClips()); m_playback->invalidateCache();
    if (auto* s = findChild<AudioSpectrumWidget*>(QStringLiteral("AudioSpectrum"))) s->refresh();
    m_arrangement->viewport()->update(); refreshUndoSnapshot(); updateTitle();
}
void MainWindow::exportWav() { renderProject(); }
void MainWindow::addTrack()
{
    Track& track = m_project->addTrack(); track.setPhonemizer(QSettings().value("defaults/phonemizer", "Default CV").toString());
    for (const auto& singer : m_singers.singers()) if (singer->isValid()) { track.setSingerPath(QString::fromStdWString(singer->path().wstring())); break; }
    const int index = m_project->tracks().size() - 1; m_trackPanel->refresh(); m_trackPanel->setCurrentRow(index); m_editor->setActiveTrack(index); m_phonemeStrip->setActiveTrack(index); m_playback->invalidateCache(); refreshUndoSnapshot(); updateTitle();
}
void MainWindow::removeTrack()
{
    if (m_project->tracks().size() <= 1) return; int index = m_trackPanel->currentRow(); if (index < 0) index = m_project->tracks().size() - 1;
    if (!m_project->removeTrack(index)) return; const int active = qBound(0, index, m_project->tracks().size() - 1);
    m_trackPanel->refresh(); m_trackPanel->setCurrentRow(active); m_editor->setActiveTrack(active); m_phonemeStrip->setActiveTrack(active); m_playback->invalidateCache(); refreshUndoSnapshot(); updateTitle();
}
void MainWindow::selectTrack(int index) { if (!m_project || index < 0 || index >= m_project->tracks().size()) return; m_editor->setActiveTrack(index); m_phonemeStrip->setActiveTrack(index); if (m_trackPanel->currentRow() != index) m_trackPanel->setCurrentRow(index); m_editor->setFocus(); }
void MainWindow::setTool(EditTool tool) { m_editor->setTool(tool); }
void MainWindow::togglePlay() { if (m_audio.isPlaying()) m_playback->pause(); else m_playback->playFromMs(currentMs()); }
void MainWindow::stopPlayback() { m_playback->stop(QSettings().value("playback/stopBehavior", 0).toInt() == 0); }
void MainWindow::renderProject()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Render Project"), {}, QStringLiteral("WAV (*.wav)"));
    if (path.isEmpty()) return; QProgressDialog progress(QStringLiteral("Rendering..."), QStringLiteral("Cancel"), 0, 100, this); progress.setWindowModality(Qt::WindowModal);
    connect(&m_renderer, &Renderer::progress, &progress, &QProgressDialog::setValue, Qt::UniqueConnection); QString error;
    if (m_renderer.renderProject(*m_project, path, &error)) { m_audio.load(path); m_audio.setBackingClips(m_project->audioClips()); statusBar()->showMessage(QStringLiteral("Render complete: %1").arg(path), 6000); }
    else QMessageBox::critical(this, QStringLiteral("Render failed"), error);
}
void MainWindow::showPreferences()
{
    PreferencesDialog dialog(&m_resampler, &m_singers, this); if (dialog.exec() != QDialog::Accepted) return; refreshVoiceBanks();
    m_resampler = QSettings().value("renderer/moresampler", m_resampler).toString();
    if (m_resampler.isEmpty()) { const auto exeDir = std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString()); m_resampler = findMoresampler(exeDir, exeDir, std::filesystem::current_path()); }
    m_renderer.setResampler(m_resampler); m_trackPanel->setSingerManager(&m_singers); m_trackPanel->refresh(); m_playback->invalidateCache();
}
void MainWindow::showDiagnostics()
{
    QString text = QStringLiteral("VoiceBank Diagnostics\n\n");
    for (const auto& root : m_singers.searchRoots()) text += QStringLiteral("Search root: %1\n").arg(root);
    text += QStringLiteral("\nMoresampler: %1\n\n").arg(m_resampler.isEmpty() ? QStringLiteral("NOT FOUND") : m_resampler);
    for (const auto& singer : m_singers.singers()) text += QStringLiteral("Singer: %1\nPath: %2\noto.ini: %3\nAliases: %4\n\n").arg(singer->info().name).arg(QString::fromStdWString(singer->path().wstring())).arg(singer->isValid() ? QStringLiteral("OK") : singer->oto().error()).arg(singer->oto().getEntries().size());
    DiagnosticsDialog dialog(text, this); dialog.exec();
}
void MainWindow::rescanVoiceBanks() { refreshVoiceBanks(); m_trackPanel->setSingerManager(&m_singers); m_trackPanel->refresh(); statusBar()->showMessage(QStringLiteral("VoiceBanks: %1 found").arg(m_singers.singers().size()), 4000); }
void MainWindow::seekFromTimeline(qint64 ms)
{
    if (!m_project) return; m_playback->seekMs(ms);
    const qint64 tick = qRound64(m_project->tempoMap().secondsToTick(ms / 1000.0, m_project->ppq()));
    m_editor->setPlayheadTick(tick); m_arrangement->setPlayheadMs(ms); m_phonemeStrip->setPlayheadMs(ms);
    if (auto* s = findChild<AudioSpectrumWidget*>(QStringLiteral("AudioSpectrum"))) s->setPlayheadMs(ms);
}

void MainWindow::setProject(std::unique_ptr<Project> project)
{
    if (!project) return; m_project = std::move(project); refreshVoiceBanks(); applyDefaults(*m_project); m_playback->setProject(m_project.get()); m_audio.setBackingClips(m_project->audioClips());
    if (auto* oldCentral = centralWidget()) delete oldCentral;
    m_editor = new PianoRollEditor(m_project.get(), this); m_keyboard = new PianoKeyboard(this); m_keyboard->setRowHeight(22); m_keyboard->setScrollPitch(108);
    auto* pianoContainer = new QWidget(this); auto* pianoLayout = new QHBoxLayout(pianoContainer); pianoLayout->setContentsMargins(0, 0, 0, 0); pianoLayout->setSpacing(0); pianoLayout->addWidget(m_keyboard); pianoLayout->addWidget(m_editor, 1);
    m_arrangement = new ArrangementEditor(m_project.get(), this); m_arrangement->setPixelsPerSecond(90.0); m_arrangement->setTrackHeight(60);
    auto* spectrum = new AudioSpectrumWidget(m_project.get(), this); spectrum->setObjectName(QStringLiteral("AudioSpectrum"));
    m_phonemeStrip = new PhonemeStripEditor(m_project.get(), this); m_phonemeStrip->setPixelsPerSecond(90.0);
    auto* splitter = new QSplitter(Qt::Vertical, this); splitter->setChildrenCollapsible(false); splitter->addWidget(m_arrangement); splitter->addWidget(pianoContainer); splitter->addWidget(spectrum); splitter->addWidget(m_phonemeStrip); splitter->setStretchFactor(0, 2); splitter->setStretchFactor(1, 7); splitter->setStretchFactor(2, 1); splitter->setStretchFactor(3, 1); splitter->setSizes({180, 560, 84, 100}); setCentralWidget(splitter);
    if (auto* dock = findChild<QDockWidget*>(QStringLiteral("TrackDock"))) { if (m_trackPanel) m_trackPanel->deleteLater(); m_trackPanel = new TrackPanel(m_project.get(), &m_singers, dock); dock->setWidget(m_trackPanel); }
    m_undo.clear(); m_trackPanel->refresh(); m_trackPanel->setCurrentRow(0); m_editor->setActiveTrack(0); m_phonemeStrip->setActiveTrack(0); m_editor->setSnapEnabled(m_project->snapEnabled()); m_editor->setShowGrid(m_project->gridVisible()); m_editor->setGridTicks(m_project->gridTicks()); refreshUndoSnapshot();
    connect(m_trackPanel, &TrackPanel::trackSelected, this, &MainWindow::selectTrack, Qt::UniqueConnection);
    connect(m_trackPanel, &TrackPanel::addTrackRequested, this, &MainWindow::addTrack, Qt::UniqueConnection);
    connect(m_trackPanel, &TrackPanel::removeTrackRequested, this, [this](int index) { m_trackPanel->setCurrentRow(index); removeTrack(); });
    connect(m_trackPanel, &TrackPanel::trackSettingsChanged, this, [this](int) { m_playback->invalidateCache(); m_editor->viewport()->update(); m_arrangement->viewport()->update(); if (auto* s = findChild<AudioSpectrumWidget*>(QStringLiteral("AudioSpectrum"))) s->refresh(); updateTitle(); });
    connect(m_editor, &PianoRollEditor::documentChanged, this, [this] { if (!m_restoringSnapshot && m_project) { const QByteArray before = m_undoSnapshot; const QByteArray after = m_project->serializeJson(); if (!before.isEmpty() && before != after) { m_undoSnapshot = after; auto apply = [this](const QByteArray& state) { restoreProjectSnapshot(state); }; m_undo.push(std::make_unique<LambdaCommand>([apply, before] { apply(before); }, [apply, after] { apply(after); })); } else m_undoSnapshot = after; } m_playback->invalidateCache(); m_editor->viewport()->update(); m_arrangement->viewport()->update(); m_phonemeStrip->viewport()->update(); updateTitle(); }, Qt::UniqueConnection);
    connect(m_editor, &PianoRollEditor::verticalPitchChanged, this, [this](int midi) { m_keyboard->setScrollPitch(midi); });
    connect(m_editor, &PianoRollEditor::keyboardPreviewRequested, this, [this](int midi) { m_audio.previewTone(midi); });
    connect(m_editor, &PianoRollEditor::requestPlaybackTick, this, [this](qint64 tick) { if (m_project) seekFromTimeline(qRound64(m_project->tempoMap().tickToSeconds(static_cast<double>(tick), m_project->ppq()) * 1000.0)); });
    connect(m_keyboard, &PianoKeyboard::keyPressed, this, [this](int midi) { m_audio.previewTone(midi); });
    connect(m_arrangement, &ArrangementEditor::trackClicked, this, &MainWindow::selectTrack, Qt::UniqueConnection); connect(m_arrangement, &ArrangementEditor::positionClicked, this, &MainWindow::seekFromTimeline, Qt::UniqueConnection); connect(m_arrangement, &ArrangementEditor::documentChanged, this, [this] { m_playback->invalidateCache(); updateTitle(); }, Qt::UniqueConnection);
    connect(m_phonemeStrip, &PhonemeStripEditor::positionClicked, this, &MainWindow::seekFromTimeline, Qt::UniqueConnection);
    connect(m_playback.get(), &PlaybackController::positionChanged, this, [this](qint64 ms) { if (!m_project) return; const qint64 tick = qRound64(m_project->tempoMap().secondsToTick(ms / 1000.0, m_project->ppq())); m_editor->setPlayheadTick(tick); m_arrangement->setPlayheadMs(ms); m_phonemeStrip->setPlayheadMs(ms); if (auto* s = findChild<AudioSpectrumWidget*>(QStringLiteral("AudioSpectrum"))) s->setPlayheadMs(ms); });
    updateTitle();
}

void MainWindow::refreshUndoSnapshot() { if (m_project && !m_restoringSnapshot) m_undoSnapshot = m_project->serializeJson(); }
void MainWindow::restoreProjectSnapshot(const QByteArray& snapshot)
{
    if (!m_project || snapshot.isEmpty()) return;
    QString error; m_restoringSnapshot = true;
    if (!m_project->restoreJson(snapshot, &error)) { m_restoringSnapshot = false; statusBar()->showMessage(QStringLiteral("Undo restore failed: %1").arg(error), 5000); return; }
    m_audio.setBackingClips(m_project->audioClips()); m_playback->setProject(m_project.get()); m_trackPanel->refresh();
    const int active = qBound(0, m_editor->activeTrack(), m_project->tracks().size() - 1); m_trackPanel->setCurrentRow(active); m_editor->setActiveTrack(active); m_arrangement->setProject(m_project.get()); m_phonemeStrip->setProject(m_project.get()); m_editor->setSnapEnabled(m_project->snapEnabled()); m_editor->setShowGrid(m_project->gridVisible()); m_editor->setGridTicks(m_project->gridTicks());
    if (auto* s = findChild<AudioSpectrumWidget*>(QStringLiteral("AudioSpectrum"))) s->setProject(m_project.get());
    m_undoSnapshot = snapshot; m_restoringSnapshot = false; m_editor->viewport()->update(); m_arrangement->viewport()->update(); m_phonemeStrip->viewport()->update(); updateTitle();
}

qint64 MainWindow::currentTick() const { return m_editor ? m_editor->playheadTick() : 0; }
qint64 MainWindow::currentMs() const { return m_project ? qRound64(m_project->tempoMap().tickToSeconds(static_cast<double>(currentTick()), m_project->ppq()) * 1000.0) : 0; }
void MainWindow::updateTitle() { setWindowTitle(QStringLiteral("%1 — MyVocalSynth").arg(m_project ? m_project->title() : QStringLiteral("Untitled"))); }

}