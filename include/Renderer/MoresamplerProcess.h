#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>

namespace myvocal {

struct ResamplerRequest {
    QString inputWav;
    QString outputWav;
    QString noteName;
    int velocity{100};
    QString flags;
    double offsetMs{0.0};
    double requiredLengthMs{100.0};
    double consonantMs{0.0};
    double cutoffMs{0.0};
    double volume{100.0};
    double modulation{0.0};
    double tempo{120.0};
    QString pitchBend;
};

struct ResamplerResult {
    bool success{false};
    int exitCode{-1};
    QString error;
    QString outputPath;
};

class MoresamplerProcess : public QObject {
    Q_OBJECT
public:
    explicit MoresamplerProcess(QObject* parent = nullptr);

    void setExecutable(const QString& path);
    QString executable() const;

    ResamplerResult render(const ResamplerRequest& request,
                           int timeoutMs = 30000);

    // Kept for compatibility with the original renderer API.
    ResamplerResult render(const QString& input, const QString& output,
                           int noteNumber, double velocity,
                           double pitchRatio, int timeoutMs = 30000);

    void cancel();

private:
    QString m_executable;
    QProcess m_process;
};

}
