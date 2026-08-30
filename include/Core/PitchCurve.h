#pragma once
#include <QJsonArray>
#include <vector>
namespace myvocal {
struct PitchPoint { double time{0.0}; double semitoneOffset{0.0}; };
class PitchCurve {
public:
    const std::vector<PitchPoint>& points() const noexcept;
    std::vector<PitchPoint>& points() noexcept;
    void clear();
    void addPoint(PitchPoint p);
    double evaluateLinear(double time) const;
    double evaluateSmooth(double time) const;
    QJsonArray serialize() const;
    static PitchCurve deserialize(const QJsonArray& array);
private:
    std::vector<PitchPoint> m_points;
};
}
