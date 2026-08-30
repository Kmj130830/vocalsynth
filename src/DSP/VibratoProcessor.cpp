#include "DSP/VibratoProcessor.h"
#include <algorithm>
#include <cmath>
namespace myvocal { void VibratoProcessor::apply(std::vector<float>&s,double sr,const Vibrato&v){if(!v.enabled||s.empty()||sr<=0)return;for(size_t i=0;i<s.size();++i){double t=i/sr;if(t<v.delay||t>v.delay+v.length)continue;double u=(t-v.delay)/std::max(1e-9,v.length);double env=std::sin(std::clamp(u,0.0,1.0)*3.141592653589793);s[i]*=static_cast<float>(1.0+0.002*v.depth*env*std::sin(2*3.141592653589793*v.rate*(t-v.delay)));}} }
