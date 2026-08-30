#pragma once
#include <memory>
#include <vector>
#include "Undo/Command.h"
namespace myvocal { class UndoManager { public: void push(std::unique_ptr<Command>); void undo(); void redo(); bool canUndo()const noexcept; bool canRedo()const noexcept; void clear(); private: std::vector<std::unique_ptr<Command>>m_undo,m_redo; }; }
