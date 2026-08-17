#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class WordStore;

class AiProbe : public QObject {
    Q_OBJECT

public:
    explicit AiProbe(WordStore *store, QObject *parent = nullptr);
    void start();

signals:
    void finished(const QString &provider, const QString &baseUrl,
                  const QString &model, const QString &label);

private:
    void probeNext();
    void checkLlama();
    void checkOllama();
    void checkCloud();
    void onReply(QNetworkReply *reply, bool ok);

    WordStore *m_store = nullptr;
    QNetworkAccessManager *m_manager = nullptr;
    QNetworkReply *m_reply = nullptr;
    int m_step = 0;
    QString m_provider;
    QString m_baseUrl;
    QString m_model;
    QString m_label;
};
