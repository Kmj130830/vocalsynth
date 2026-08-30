#pragma once
#include <QWidget>
namespace myvocal { class PianoKeyboard : public QWidget { Q_OBJECT public: explicit PianoKeyboard(QWidget* parent=nullptr); void setScrollPitch(int midi); protected: void paintEvent(QPaintEvent*) override; void mousePressEvent(QMouseEvent*) override; signals: void keyPressed(int midi); private: int m_topMidi{84}; }; }
