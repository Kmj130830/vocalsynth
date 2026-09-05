#pragma once

#include <QVector>
#include <QWidget>

class QAudioDecoder;
class QAudioFormat;

namespace myvocal {

class Project;

class AudioSpectrumWidget final : public QWidget {
    Q_OBJECT
public:
    explicit AudioSpectrumWidget(Project* project, QWidget* parent = nullptr);
    void setProject(Project* project);
    void setPlayheadMs(qint64 ms);
    void refresh();

protected:
    void paintEvent(QPaintEvent*) override;

private slots:
    void readBuffer();
    void decoderFinished();
    void decoderError();

private:
    void resetData();
    void decodeFile(const QString& path);

    Project* m_project{nullptr};
    QAudioDecoder* m_decoder{nullptr};
    QVector<QVector<float>> m_columns;
    QString m_sourcePath;
    qint64 m_playheadMs{0};
    qint64 m_decodedFrames{0};
    int m_sampleRate{44100};
};

}