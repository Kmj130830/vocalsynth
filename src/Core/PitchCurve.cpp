#include "Core/PitchCurve.h"
#include <QJsonObject>
#include <algorithm>
#include <cmath>
namespace myvocal {
const std::vector<PitchPoint>& PitchCurve::points() const noexcept { return m_points; }
std::vector<PitchPoint>& PitchCurve::points() noexcept { return m_points; }
void PitchCurve::clear() { m_points.clear(); }
void PitchCurve::addPoint(PitchPoint p) { m_points.push_back(p); std::sort(m_points.begin(), m_points.end(), [](const auto&a,const auto&b){return a.time<b.time;}); }
double PitchCurve::evaluateLinear(double time) const {
    if (m_points.empty()) return 0.0;
    if (time <= m_points.front().time) return m_points.front().semitoneOffset;
    if (time >= m_points.back().time) return m_points.back().semitoneOffset;
    for (std::size_t i=1;i<m_points.size();++i) {
        if (time <= m_points[i].time) {
            const auto&a=m_points[i-1]; const auto&b=m_points[i];
            const double t=(time-a.time)/(b.time-a.time); return a.semitoneOffset+(b.semitoneOffset-a.semitoneOffset)*t;
        }
    }
    return 0.0;
}
double PitchCurve::evaluateSmooth(double time) const {
    if (m_points.size()<3) return evaluateLinear(time);
    if (time <= m_points.front().time || time >= m_points.back().time) return evaluateLinear(time);
    std::size_t i=1; while (i<m_points.size() && time>m_points[i].time) ++i;
    const auto&p0=m_points[i-2]; const auto&p1=m_points[i-1]; const auto&p2=m_points[i]; const auto&p3=(i+1<m_points.size()?m_points[i+1]:p2);
    const double u=(time-p1.time)/(p2.time-p1.time); const double u2=u*u, u3=u2*u;
    return 0.5*((2*p1.semitoneOffset)+(-p0.semitoneOffset+p2.semitoneOffset)*u+(2*p0.semitoneOffset-5*p1.semitoneOffset+4*p2.semitoneOffset-p3.semitoneOffset)*u2+(-p0.semitoneOffset+3*p1.semitoneOffset-3*p2.semitoneOffset+p3.semitoneOffset)*u3);
}
QJsonArray PitchCurve::serialize() const { QJsonArray a; for(const auto&p:m_points){QJsonObject o; o["time"]=p.time; o["semitoneOffset"]=p.semitoneOffset; a.append(o);} return a; }
PitchCurve PitchCurve::deserialize(const QJsonArray&a){ PitchCurve c; for(const auto&v:a){auto o=v.toObject(); c.addPoint({o["time"].toDouble(),o["semitoneOffset"].toDouble()});} return c; }
}
