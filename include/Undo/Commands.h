#pragma once
#include <functional>
#include "Undo/Command.h"
namespace myvocal { class LambdaCommand final: public Command { public: LambdaCommand(std::function<void()>u,std::function<void()>r):m_u(std::move(u)),m_r(std::move(r)){}void undo()override{m_u();}void redo()override{m_r();}private:std::function<void()>m_u,m_r;}; }
