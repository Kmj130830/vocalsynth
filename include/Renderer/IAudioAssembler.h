#pragma once

#include <QString>
#include <vector>

namespace myvocal {

struct RenderSegment {
    QString wavPath;
    qint64 startMs{0};
    qint64 lengthMs{0};
    double gain{1.0};
    double overlapMs{0.0};
    int trackIndex{0};
};

class IAudioAssembler {
public:
    virtual ~IAudioAssembler() = default;
    virtual bool assemble(const std::vector<RenderSegment>& segments,
                          const QString& output,
                          QString* error) = 0;
};

}