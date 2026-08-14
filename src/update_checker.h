#pragma once

#include <QObject>
#include <QFile>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class UpdateChecker : public QObject {
    Q_OBJECT

public:
    explicit UpdateChecker(QObject *parent = nullptr);
    ~UpdateChecker() override;

    void check(bool silent);
    void startDownload();
    void cancelDownload();
    bool downloadRunning() const { return m_downloadReply != nullptr; }

    struct UpdateInfo {
        QString buildId;
        QString version;
        QString notes;
        QString desktopUrl;
        QString oneclickUrl;
        qint64 desktopSize = 0;
        qint64 oneclickSize = 0;
        QString desktopSha256;
        QString oneclickSha256;
    };
    UpdateInfo lastInfo() const { return m_lastInfo; }

signals:
    void updateAvailable(const QString &version, const QString &notes,
                         const QString &downloadUrl);
    void upToDate();
    void failed(const QString &message);
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(const QString &path);
    void downloadFailed(const QString &message);

private:
    void onReplyFinished();
    void onDownloadFinished();
    void onDownloadProgress(qint64 received, qint64 total);
    bool isOneClick() const;
    QString pickUrl() const;
    qint64 pickSize() const;
    QString pickSha256() const;

    QNetworkAccessManager *m_manager = nullptr;
    QNetworkReply *m_reply = nullptr;
    QNetworkReply *m_downloadReply = nullptr;
    QFile m_downloadFile;
    QString m_downloadPath;
    bool m_silent = false;
    UpdateInfo m_lastInfo;
};
