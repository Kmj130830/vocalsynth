#pragma once

#include <QJsonObject>
#include <QString>

namespace myvocal {

struct AudioClip {
    QString path;
    qint64 startMs{0};
    qint64 offsetMs{0};
    qint64 durationMs{0};
    double volume{1.0};
    bool muted{false};

    QJsonObject serialize() const {
        QJsonObject o;
        o["path"] = path;
        o["startMs"] = QString::number(startMs);
        o["offsetMs"] = QString::number(offsetMs);
        o["durationMs"] = QString::number(durationMs);
        o["volume"] = volume;
        o["muted"] = muted;
        return o;
    }

    static AudioClip deserialize(const QJsonObject& o) {
        AudioClip clip;
        clip.path = o["path"].toString();
        clip.startMs = o["startMs"].toString().toLongLong();
        clip.offsetMs = o["offsetMs"].toString().toLongLong();
        clip.durationMs = o["durationMs"].toString().toLongLong();
        clip.volume = o["volume"].toDouble(1.0);
        clip.muted = o["muted"].toBool(false);
        return clip;
    }
};

}