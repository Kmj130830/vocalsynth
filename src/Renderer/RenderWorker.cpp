#include "Renderer/RenderWorker.h"
#include "Renderer/Renderer.h"
namespace myvocal { RenderWorker::RenderWorker(Renderer*r,Project*p,const QString&o,QObject*par):QObject(par),m_renderer(r),m_project(p),m_output(o){}void RenderWorker::run(){QString e;const bool ok=m_renderer->renderProject(*m_project,m_output,&e);emit finished(ok,e);} }
