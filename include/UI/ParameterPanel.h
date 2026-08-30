#pragma once
#include <QWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include "Core/Project.h"
namespace myvocal { class ParameterPanel : public QWidget { Q_OBJECT public: explicit ParameterPanel(Project*,QWidget*parent=nullptr); void setSelectedNote(qint64 id); private slots: void commit(); private: Project*m_project; qint64 m_id{-1}; QLineEdit*m_lyric; QSpinBox*m_pitch; QSpinBox*m_velocity; }; }
