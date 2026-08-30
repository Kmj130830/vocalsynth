#include "Renderer/MoresamplerProcess.h"

#include <QFileInfo>
#include <QProcessEnvironment>

namespace myvocal {

MoresamplerProcess::MoresamplerProcess(QObject* parent)
    : QObject(parent)
{
}

void MoresamplerProcess::setExecutable(const QString& path)
{
    m_executable = path.trimmed();
}

QString MoresamplerProcess::executable() const
{
    return m_executable;
}

ResamplerResult MoresamplerProcess::render(const ResamplerRequest& request,
                                           int timeoutMs)
{
    ResamplerResult result;
    result.outputPath = request.outputWav;

    if (m_executable.isEmpty()) {
        result.error = QStringLiteral(
            "Moresampler executable is not configured. Set it in Tools > Preferences.");
        return result;
    }
    if (!QFileInfo::exists(m_executable) || !QFileInfo(m_executable).isFile()) {
        result.error = QStringLiteral("Moresampler executable not found: %1")
                           .arg(m_executable);
        return result;
    }
    if (!QFileInfo::exists(request.inputWav)) {
        result.error = QStringLiteral("Moresampler input WAV not found: %1")
                           .arg(request.inputWav);
        return result;
    }

    // This follows the command line used by OpenUtau's ExeResampler:
    // input output tone velocity flags offset duration consonant cutoff volume
    // modulation !tempo pitch-bend.
    QStringList args;
    args << request.inputWav
         << request.outputWav
         << request.noteName
         << QString::number(request.velocity)
         << request.flags
         << QString::number(request.offsetMs, 'f', 4)
         << QString::number(request.requiredLengthMs, 'f', 4)
         << QString::number(request.consonantMs, 'f', 4)
         << QString::number(request.cutoffMs, 'f', 4)
         << QString::number(request.volume, 'f', 4)
         << QString::number(request.modulation, 'f', 4)
         << QStringLiteral("!") + QString::number(request.tempo, 'f', 4)
         << request.pitchBend;

    m_process.setProgram(m_executable);
    m_process.setArguments(args);
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_process.start();

    if (!m_process.waitForStarted(3000)) {
        result.error = QStringLiteral("Failed to start Moresampler: %1")
                           .arg(m_process.errorString());
        return result;
    }

    if (!m_process.waitForFinished(timeoutMs)) {
        m_process.kill();
        m_process.waitForFinished(1000);
        result.error = QStringLiteral("Moresampler timed out after %1 ms.")
                           .arg(timeoutMs);
        return result;
    }

    result.exitCode = m_process.exitCode();
    const QByteArray stderrData = m_process.readAllStandardError();
    const QByteArray stdoutData = m_process.readAllStandardOutput();

    result.success = m_process.exitStatus() == QProcess::NormalExit &&
                     result.exitCode == 0 &&
                     QFileInfo::exists(request.outputWav);
    if (!result.success) {
        result.error = QString::fromLocal8Bit(stderrData).trimmed();
        if (result.error.isEmpty()) {
            result.error = QString::fromLocal8Bit(stdoutData).trimmed();
        }
        if (result.error.isEmpty()) {
            result.error = QStringLiteral(
                "Moresampler failed with exit code %1 and produced no output WAV.")
                .arg(result.exitCode);
        }
    }

    return result;
}

ResamplerResult MoresamplerProcess::render(const QString& input,
                                           const QString& output,
                                           int noteNumber,
                                           double velocity,
                                           double pitchRatio,
                                           int timeoutMs)
{
    ResamplerRequest request;
    request.inputWav = input;
    request.outputWav = output;
    request.noteName = QString::number(noteNumber);
    request.velocity = qBound(0, qRound(velocity), 200);
    request.flags = QStringLiteral("g0B0H0P100");
    request.requiredLengthMs = 100.0;
    request.tempo = 120.0;
    request.pitchBend = QString::number(pitchRatio, 'f', 4);
    return render(request, timeoutMs);
}

void MoresamplerProcess::cancel()
{
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
    }
}

}
