#pragma once
namespace myvocal { class Command { public: virtual ~Command()=default; virtual void undo()=0; virtual void redo()=0; }; }
