#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class UpdateChecker : public QObject {
    Q_OBJECT

public:
    explicit UpdateChecker(QObject *parent = nullptr);
    ~UpdateChecker() override;

    void check(bool silent);

signals:
    void updateAvailable(const QString &version, const QString &notes,
                         const QString &downloadUrl);
    void upToDate();
    void failed(const QString &message);

private:
    void onReplyFinished();

    QNetworkAccessManager *m_manager = nullptr;
    QNetworkReply *m_reply = nullptr;
    bool m_silent = false;
};
