#pragma once
#include <vector>
namespace myvocal { class WaveMixer { public: static void mix(std::vector<float>&dst,const std::vector<float>&src,size_t start,float gain=1.0f); static void normalize(std::vector<float>&samples,float peak=0.98f); }; }
