#pragma once

#include <QObject>
#include <QTimer>

#include <functional>
#include <vector>

class UpdateCoalescer final : public QObject
{
    Q_OBJECT
public:
    explicit UpdateCoalescer(QObject *parent = nullptr);

    void post(std::function<void()> fn);

private:
    void flush();

    QTimer m_timer;
    std::vector<std::function<void()>> m_pending;
};


