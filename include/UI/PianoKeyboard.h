#pragma once

#include <QWidget>

namespace myvocal {

class PianoKeyboard final : public QWidget {
    Q_OBJECT
public:
    explicit PianoKeyboard(QWidget* parent = nullptr);

    void setScrollPitch(int midi);
    void setRowHeight(int pixels);

signals:
    void keyPressed(int midi);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    int midiAtY(int y) const;
    int m_topMidi{84};
    int m_rowHeight{22};
};

}