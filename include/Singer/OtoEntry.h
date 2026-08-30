#pragma once
#include <QString>
namespace myvocal { struct OtoEntry { QString filename; QString alias; double offset{0}; double consonant{0}; double cutoff{0}; double preutterance{0}; double overlap{0}; bool valid{false}; }; }
