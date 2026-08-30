#pragma once
#include <QString>
#include <cstddef>
namespace myvocal {
struct Phoneme {
    QString alias;
    double startTick{0.0};
    double lengthTick{0.0};
    double preutterance{0.0};
    double overlap{0.0};
    double offset{0.0};
    double consonant{0.0};
    double cutoff{0.0};
    std::size_t noteIndex{0};
};
}
