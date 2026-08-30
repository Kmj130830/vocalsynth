#include "Singer/OtoParser.h"
#include <cassert>
void otoTest(){myvocal::OtoParser p;myvocal::OtoEntry e;assert(p.parseLine("a.wav=a,0,120,0,120,60",e));assert(e.alias=="a");}
