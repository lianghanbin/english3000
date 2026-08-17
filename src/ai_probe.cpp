#include "ai_probe.h"

#include "core.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace {

constexpr int kTimeoutMs = 4000;

} // namespace

AiProbe::AiProbe(WordStore *store, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_manager(new QNetworkAccessManager(this))
{
}

void AiProbe::start()
{
    if (m_reply)
        return;
    m_step = 0;
    probeNext();
}

void AiProbe::probeNext()
{
    switch (m_step) {
    case 0:
        checkCloud();
        break;
    case 1:
        checkLlama();
        break;
    case 2:
        checkOllama();
        break;
    default:
        emit finished(QString(), QString(), QString(),
                      QStringLiteral("未检测到可用 AI"));
        break;
    }
}

void AiProbe::checkLlama()
{
    ++m_step;
    QNetworkRequest request(
        QUrl(QStringLiteral("http://127.0.0.1:8080/health")));
    request.setTransferTimeout(kTimeoutMs);
    m_reply = m_manager->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        const bool ok = m_reply->error() == QNetworkReply::NoError;
        if (ok) {
            m_provider = QStringLiteral("openai");
            m_baseUrl = QStringLiteral("http://127.0.0.1:8080");
            m_model = QStringLiteral("qwen2.5:1.5b");
            m_label = QStringLiteral("本地小模型（Qwen2.5 1.5B）");
        }
        onReply(m_reply, ok);
    });
}

void AiProbe::checkOllama()
{
    ++m_step;
    QNetworkRequest request(
        QUrl(QStringLiteral("http://127.0.0.1:11434/api/tags")));
    request.setTransferTimeout(kTimeoutMs);
    m_reply = m_manager->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        bool ok = m_reply->error() == QNetworkReply::NoError;
        QString model;
        if (ok) {
            const QJsonDocument doc =
                QJsonDocument::fromJson(m_reply->readAll());
            const QJsonArray models =
                doc.object().value(QStringLiteral("models")).toArray();
            const QString preferred =
                m_store->getSetting(QStringLiteral("ai_model"),
                                    QStringLiteral("qwen2.5:1.5b"));
            QString first;
            for (const QJsonValue &v : models) {
                const QString name =
                    v.toObject().value(QStringLiteral("name")).toString();
                if (name.isEmpty())
                    continue;
                if (name.contains(QLatin1String("vl"))
                    || name.contains(QLatin1String("embedding"))
                    || name.contains(QLatin1String("tts")))
                    continue; // 跳过视觉/嵌入/语音模型
                if (name == preferred) {
                    model = name;
                    break;
                }
                if (first.isEmpty())
                    first = name;
            }
            if (model.isEmpty())
                model = first;
            if (model.isEmpty()) {
                ok = false;
            }
        }
        if (ok) {
            m_provider = QStringLiteral("ollama");
            m_baseUrl = QStringLiteral("http://127.0.0.1:11434");
            m_model = model;
            m_label = QStringLiteral("本地 Ollama（%1）").arg(model);
        }
        onReply(m_reply, ok);
    });
}

void AiProbe::checkCloud()
{
    ++m_step;
    const QString key =
        m_store->getSetting(QStringLiteral("ai_api_key"));
    const QString base =
        m_store->getSetting(QStringLiteral("ai_base_url"));
    const QString model =
        m_store->getSetting(QStringLiteral("ai_model"));
    if (key.isEmpty() || base.isEmpty() || model.isEmpty()
        || base.startsWith(QStringLiteral("http://127.0.0.1"))) {
        qWarning("probe: cloud skipped (key=%d base=%s model=%s)",
                 key.isEmpty() ? 0 : 1, qPrintable(base),
                 qPrintable(model));
        probeNext();
        return;
    }
    QString chatUrl = base;
    if (!chatUrl.endsWith(QLatin1Char('/')))
        chatUrl += QLatin1Char('/');
    if (chatUrl.endsWith(QLatin1String("/v1/"))) {
        chatUrl += QStringLiteral("chat/completions");
    } else {
        chatUrl += QStringLiteral("v1/chat/completions");
    }
    qWarning("probe: cloud -> %s model=%s", qPrintable(chatUrl),
             qPrintable(model));
    QJsonObject body;
    body.insert(QStringLiteral("model"), model);
    QJsonArray messages;
    QJsonObject msg;
    msg.insert(QStringLiteral("role"), QStringLiteral("user"));
    msg.insert(QStringLiteral("content"), QStringLiteral("hi"));
    messages.append(msg);
    body.insert(QStringLiteral("messages"), messages);
    body.insert(QStringLiteral("max_tokens"), 5);
    const QUrl chatEndpoint = QUrl(chatUrl);
    QNetworkRequest request(chatEndpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         "Bearer " + key.toUtf8());
    request.setTransferTimeout(kTimeoutMs);
    m_reply = m_manager->post(request, QJsonDocument(body).toJson());
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        const bool ok = m_reply->error() == QNetworkReply::NoError;
        qWarning("probe: cloud finished ok=%d err=%s",
                 ok ? 1 : 0,
                 qPrintable(m_reply->errorString()));
        if (ok) {
            const QUrl url = m_reply->url();
            m_provider = QStringLiteral("openai");
            m_baseUrl = m_store->getSetting(
                QStringLiteral("ai_base_url"));
            m_model = m_store->getSetting(QStringLiteral("ai_model"));
            m_label = QStringLiteral("云端（%1）").arg(url.host());
        }
        onReply(m_reply, ok);
    });
}

void AiProbe::onReply(QNetworkReply *reply, bool ok)
{
    reply->deleteLater();
    m_reply = nullptr;
    if (ok) {
        emit finished(m_provider, m_baseUrl, m_model, m_label);
        return;
    }
    probeNext();
}
