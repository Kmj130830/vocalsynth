#pragma once
#include <QDialog>
#include <QLineEdit>
namespace myvocal { class LyricEditor : public QDialog { Q_OBJECT public: explicit LyricEditor(QWidget*parent=nullptr); QString lyric()const; void setLyric(const QString&); private: QLineEdit*m_edit; }; }
