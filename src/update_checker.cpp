#include "update_checker.h"

#include "build_info.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
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
    const QString remoteId =
        obj.value(QStringLiteral("build_id")).toString();
    const QString version =
        obj.value(QStringLiteral("version")).toString();
    const QString notes =
        obj.value(QStringLiteral("notes")).toString();
    const bool oneClick = QFileInfo(
        QCoreApplication::applicationDirPath()
        + QStringLiteral("/llama"))
                              .exists();
    QString url = obj.value(
        oneClick ? QStringLiteral("oneclick_url")
                 : QStringLiteral("desktop_url"))
                     .toString();
    if (remoteId.isEmpty() || url.isEmpty()) {
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
    emit updateAvailable(version, notes, url);
}
