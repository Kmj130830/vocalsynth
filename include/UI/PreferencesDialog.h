#pragma once
#include <QDialog>
#include <QString>
namespace myvocal { class PreferencesDialog : public QDialog { Q_OBJECT public: explicit PreferencesDialog(QString*resampler,QWidget*parent=nullptr); private: QString*m_resampler; }; }
