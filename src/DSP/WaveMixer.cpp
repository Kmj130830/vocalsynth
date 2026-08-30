#include "DSP/WaveMixer.h"
#include <algorithm>
#include <cmath>
namespace myvocal { void WaveMixer::mix(std::vector<float>&d,const std::vector<float>&s,size_t st,float g){if(d.size()<st+s.size())d.resize(st+s.size());for(size_t i=0;i<s.size();++i)d[st+i]+=s[i]*g;} void WaveMixer::normalize(std::vector<float>&s,float p){float m=0;for(float v:s)m=std::max(m,std::abs(v));if(m<=0)return;float g=p/m;for(float&v:s)v*=g;} }
