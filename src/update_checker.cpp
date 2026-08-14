#include "update_checker.h"

#include "build_info.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

namespace {

const char *kUpdateUrl =
    "https://github.com/lianghanbin/english3000/releases/download/"
    "app-update/english3000-update.json";

} // namespace

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
}

UpdateChecker::~UpdateChecker()
{
    if (m_reply) {
        m_reply->abort();
        m_reply = nullptr;
    }
}

void UpdateChecker::check(bool silent)
{
    const QString localId = QStringLiteral(ENGLISH3000_BUILD_ID);
    if (localId == QLatin1String("dev")) {
        if (!silent)
            emit failed(QStringLiteral("当前是本地开发版，不检查更新。"));
        return;
    }
    if (m_reply)
        return;
    m_silent = silent;
    const QUrl url = QUrl(QString::fromLatin1(kUpdateUrl));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("English3000-Updater"));
    m_reply = m_manager->get(request);
    connect(m_reply, &QNetworkReply::finished, this,
            &UpdateChecker::onReplyFinished);
    QTimer::singleShot(15000, this, [this] {
        if (m_reply) {
            m_reply->abort();
            m_reply = nullptr;
            if (!m_silent)
                emit failed(QStringLiteral("检查更新超时，请稍后重试。"));
        }
    });
}

void UpdateChecker::onReplyFinished()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    if (!reply)
        return;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        if (!m_silent)
            emit failed(QStringLiteral("检查更新失败：%1")
                            .arg(reply->errorString()));
        return;
    }
    const QJsonDocument doc =
        QJsonDocument::fromJson(reply->readAll());
    const QJsonObject obj = doc.object();
    m_lastInfo.buildId =
        obj.value(QStringLiteral("build_id")).toString();
    m_lastInfo.version =
        obj.value(QStringLiteral("version")).toString();
    m_lastInfo.notes =
        obj.value(QStringLiteral("notes")).toString();
    m_lastInfo.desktopUrl =
        obj.value(QStringLiteral("desktop_url")).toString();
    m_lastInfo.oneclickUrl =
        obj.value(QStringLiteral("oneclick_url")).toString();
    m_lastInfo.desktopSize =
        obj.value(QStringLiteral("desktop_size")).toDouble();
    m_lastInfo.oneclickSize =
        obj.value(QStringLiteral("oneclick_size")).toDouble();
    m_lastInfo.desktopSha256 =
        obj.value(QStringLiteral("desktop_sha256")).toString();
    m_lastInfo.oneclickSha256 =
        obj.value(QStringLiteral("oneclick_sha256")).toString();
    const QString remoteId = m_lastInfo.buildId;
    if (remoteId.isEmpty() || pickUrl().isEmpty()) {
        if (!m_silent)
            emit failed(QStringLiteral("更新信息不完整，请稍后重试。"));
        return;
    }
    const QString localId = QStringLiteral(ENGLISH3000_BUILD_ID);
    if (remoteId == localId) {
        if (!m_silent)
            emit upToDate();
        return;
    }
    emit updateAvailable(m_lastInfo.version, m_lastInfo.notes, pickUrl());
}

bool UpdateChecker::isOneClick() const
{
    return QFileInfo(
        QCoreApplication::applicationDirPath()
        + QStringLiteral("/llama"))
        .exists();
}

QString UpdateChecker::pickUrl() const
{
    return isOneClick() ? m_lastInfo.oneclickUrl
                        : m_lastInfo.desktopUrl;
}

qint64 UpdateChecker::pickSize() const
{
    return isOneClick() ? m_lastInfo.oneclickSize
                        : m_lastInfo.desktopSize;
}

QString UpdateChecker::pickSha256() const
{
    return isOneClick() ? m_lastInfo.oneclickSha256
                        : m_lastInfo.desktopSha256;
}

void UpdateChecker::startDownload()
{
    if (m_downloadReply || pickUrl().isEmpty())
        return;
    const QString suffix = isOneClick()
                               ? QStringLiteral(".exe")
                               : QStringLiteral(".zip");
    m_downloadPath =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/english3000-update-%1%2")
              .arg(m_lastInfo.buildId, suffix);
    QFile::remove(m_downloadPath);
    m_downloadFile.setFileName(m_downloadPath);
    if (!m_downloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit downloadFailed(QStringLiteral("无法创建下载文件。"));
        return;
    }
    const QUrl downloadUrl = QUrl(pickUrl());
    QNetworkRequest request(downloadUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("English3000-Updater"));
    m_downloadReply = m_manager->get(request);
    connect(m_downloadReply, &QNetworkReply::downloadProgress, this,
            &UpdateChecker::onDownloadProgress);
    connect(m_downloadReply, &QNetworkReply::finished, this,
            &UpdateChecker::onDownloadFinished);
    emit downloadProgress(0, pickSize());
}

void UpdateChecker::cancelDownload()
{
    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply = nullptr;
    }
    m_downloadFile.close();
}

void UpdateChecker::onDownloadProgress(qint64 received, qint64 total)
{
    emit downloadProgress(received, total > 0 ? total : pickSize());
}

void UpdateChecker::onDownloadFinished()
{
    QNetworkReply *reply = m_downloadReply;
    m_downloadReply = nullptr;
    if (!reply)
        return;
    reply->deleteLater();
    m_downloadFile.close();
    if (reply->error() != QNetworkReply::NoError) {
        QFile::remove(m_downloadPath);
        emit downloadFailed(QStringLiteral("下载失败：%1")
                                .arg(reply->errorString()));
        return;
    }
    const qint64 size = QFileInfo(m_downloadPath).size();
    const qint64 expected = pickSize();
    if (expected > 0 && size != expected) {
        QFile::remove(m_downloadPath);
        emit downloadFailed(
            QStringLiteral("下载文件不完整（%1/%2 字节），请重试。")
                .arg(size)
                .arg(expected));
        return;
    }
    const QString expectedSha = pickSha256();
    if (!expectedSha.isEmpty()) {
        QFile f(m_downloadPath);
        QString actual;
        if (f.open(QIODevice::ReadOnly)) {
            actual = QString::fromLatin1(
                QCryptographicHash::hash(f.readAll(),
                                         QCryptographicHash::Sha256)
                    .toHex());
        }
        if (actual.compare(expectedSha, Qt::CaseInsensitive) != 0) {
            QFile::remove(m_downloadPath);
            emit downloadFailed(
                QStringLiteral("下载文件校验失败，请重试。"));
            return;
        }
    }
    emit downloadFinished(m_downloadPath);
}
