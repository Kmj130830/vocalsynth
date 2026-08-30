#include "Core/Note.h"
#include <cassert>
void noteTest(){myvocal::Note n(1);assert(n.getLyric()=="a");n.setMidiNote(200);assert(n.getMidiNote()==127);n.setDurationTick(0);assert(n.getDurationTick()==1);} 
