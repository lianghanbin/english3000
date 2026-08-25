#include "ai_client.h"
#include <QDebug>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace {

constexpr int kTimeoutMs = 10 * 60 * 1000;

// 从服务端错误响应里提取人类可读信息。各家格式不一:
// 标准 OpenAI: {"error":{"message":"...","type":"insufficient_quota"}}
// 通义/部分国产: {"code":"...","message":"..."} / {"msg":"..."}
QString extractServerMessage(const QByteArray &body)
{
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
    if (pe.error != QJsonParseError::NoError) {
        const QString s = QString::fromUtf8(body).simplified();
        return s.left(120);
    }
    const QJsonObject obj = doc.object();
    QString msg;
    const QJsonValue err = obj.value(QStringLiteral("error"));
    if (err.isObject()) {
        msg = err.toObject().value(QStringLiteral("message")).toString();
    } else if (err.isString()) {
        msg = err.toString();
    }
    if (msg.isEmpty())
        msg = obj.value(QStringLiteral("message")).toString();
    if (msg.isEmpty())
        msg = obj.value(QStringLiteral("msg")).toString();
    if (msg.isEmpty())
        msg = obj.value(QStringLiteral("errorMessage")).toString();
    return msg.simplified().left(160);
}

// 把网络/HTTP 错误归类成对用户可操作的中文提示,
// 重点覆盖欠费、额度不足、Key 无效、模型名错误等高频情况。
QString classifyError(const QNetworkReply *reply, const QByteArray &body)
{
    const int http =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString server = extractServerMessage(body);
    const QString lc = server.toLower();

    auto hint = [&](const QString &base) {
        return server.isEmpty() ? base
                                : base + QStringLiteral("（") + server
                                      + QStringLiteral("）");
    };

    if (http == 401 || http == 403) {
        if (lc.contains(QStringLiteral("quota"))
            || lc.contains(QStringLiteral("balance"))
            || lc.contains(QStringLiteral("arrears"))
            || lc.contains(QStringLiteral("insufficient"))
            || lc.contains(QStringLiteral("余额"))
            || lc.contains(QStringLiteral("欠费"))
            || lc.contains(QStringLiteral("额度"))) {
            return hint(QStringLiteral(
                "云端账户余额/额度不足，请前往服务商控制台充值或开通额度"));
        }
        return hint(QStringLiteral(
            "API Key 无效或无权限（%1），请到设置页检查 Key").arg(http));
    }
    if (http == 402 || lc.contains(QStringLiteral("insufficient_quota"))
        || lc.contains(QStringLiteral("billing"))
        || lc.contains(QStringLiteral("余额不足"))
        || lc.contains(QStringLiteral("欠费"))) {
        return hint(QStringLiteral(
            "云端账户已欠费或额度用尽，请前往服务商控制台充值"));
    }
    if (http == 429) {
        return hint(QStringLiteral(
            "请求过于频繁或额度已用完（429），请稍后再试或更换服务商"));
    }
    if (http == 404) {
        return hint(QStringLiteral(
            "服务地址或模型名有误（404），请到设置页检查模型名是否正确"));
    }
    if (http == 400) {
        return hint(QStringLiteral(
            "请求被拒绝（400），通常是模型名不支持或参数有误"));
    }
    if (http >= 500) {
        return hint(QStringLiteral(
            "服务商服务器暂时不可用（%1），请稍后重试").arg(http));
    }
    if (http > 0) {
        return hint(QStringLiteral("AI 请求失败（HTTP %1）").arg(http));
    }
    switch (reply->error()) {
    case QNetworkReply::TimeoutError:
    case QNetworkReply::OperationCanceledError:
        return QStringLiteral("连接超时，请检查网络后重试");
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::ConnectionRefusedError:
        return QStringLiteral("无法连接服务器，请检查网络或服务地址");
    case QNetworkReply::SslHandshakeFailedError:
        return QStringLiteral("网络安全连接失败，请检查网络或系统时间");
    default:
        return hint(QStringLiteral("AI 请求失败：%1")
                        .arg(reply->errorString()));
    }
}

} // namespace

AiClient::AiClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_timeout(new QTimer(this))
{
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(kTimeoutMs);
    connect(m_timeout, &QTimer::timeout, this, [this] {
        m_timedOut = true;
        if (m_reply)
            m_reply->abort();
    });
}

AiClient::~AiClient()
{
    cancel();
}

void AiClient::setEndpoint(const QString &baseUrl, const QString &model)
{
    m_baseUrl = baseUrl;
    if (!m_baseUrl.endsWith(QLatin1Char('/')))
        m_baseUrl += QLatin1Char('/');
    m_model = model;
}

void AiClient::setProvider(Provider provider)
{
    m_provider = provider;
}

void AiClient::setApiKey(const QString &apiKey)
{
    m_apiKey = apiKey.trimmed();
}

QString AiClient::levelLabel(int level)
{
    switch (level) {
    case 1:
        return QStringLiteral("最简单（只用最常用的 1000 词）");
    case 2:
        return QStringLiteral("简单（只用最常用的 2000 词）");
    case 3:
        return QStringLiteral("中等（尽量用牛津 3000 词表）");
    default:
        return QStringLiteral("偏难（以牛津 3000 为主，允许少量生词）");
    }
}

QString AiClient::topicPrompt(const QString &topic, int level, int wordCount)
{
    const QString lengthRule = wordCount > 0
        ? QStringLiteral("Length: about %1 words.\n").arg(wordCount)
        : QStringLiteral("Length: a complete, natural-length article for "
                         "the topic and level (typically 250-450 words).\n");
    return QStringLiteral(
               "You are an English teacher writing graded reading material.\n"
               "Write a short English article about: %1\n"
               "Level: %2\n"
               "%3"
               "Rules:\n"
               "- Use short, clear sentences.\n"
               "- Prefer the most common English words.\n"
               "- Divide the text into 3 to 5 paragraphs, separated by "
               "a blank line.\n"
               "- Output ONLY the article text. No title, no quotes, "
               "no explanations.")
        .arg(topic.trimmed(), levelLabel(level), lengthRule);
}

QString AiClient::topicPrompt(const QString &topic, int level, int wordCount,
                              const QStringList &preferredWords)
{
    QString prompt = topicPrompt(topic, level, wordCount);
    if (!preferredWords.isEmpty()) {
        const int take = qMin(80, preferredWords.size());
        QStringList words;
        for (int i = 0; i < take; ++i)
            words << preferredWords[i];
        prompt += QStringLiteral(
                      "\nTry to use these words where natural: %1")
                      .arg(words.join(QStringLiteral(", ")));
    }
    return prompt;
}

QString AiClient::rewritePrompt(const QString &sourceText, int level)
{
    return QStringLiteral(
               "You are an English teacher. Rewrite the following text in "
               "%1 English.\n"
               "Keep the meaning and all key details.\n"
               "Use short sentences and prefer the most common English words.\n"
               "Output ONLY the rewritten text, no explanations.\n\n"
               "---SOURCE---\n%2")
        .arg(levelLabel(level), sourceText.trimmed());
}

QString AiClient::wordListPrompt(const QString &domain, int count)
{
    return QStringLiteral(
               "You are a vocabulary expert. List exactly %1 English words "
               "commonly used in the field of: %2.\n"
               "Rules:\n"
               "- Output ONLY lines in this exact format:\n"
               "  word | part of speech | Simplified Chinese meaning | "
               "one short English example sentence\n"
               "- Example: algorithm | n. | 算法 | The algorithm sorts "
               "the data in seconds.\n"
               "- Use ONLY the | separator. Do NOT use colons, dashes, "
               "or parentheses. Bad: algorithm: 算法\n"
               "- One line per word, lowercase word, no numbering, "
               "no duplicate words.\n"
               "- Include important nouns, verbs, and adjectives.\n"
               "- The example sentence must use the exact word, be short "
               "and simple, and contain no | character.")
        .arg(count)
        .arg(domain.trimmed());
}

QString AiClient::fillMeaningsPrompt(const QStringList &words)
{
    return QStringLiteral(
               "Give the part of speech and Simplified Chinese meaning for "
               "each English word below.\n"
               "Rules:\n"
               "- Output ONLY lines in this exact format:\n"
               "  word | part of speech | meaning1; meaning2\n"
               "- Example: run | v. | 跑; 经营; 运转\n"
               "- Use ONLY the | separator. Do NOT use colons, dashes, "
               "or parentheses. Bad: algorithm: 算法\n"
               "- One line per word, lowercase, no numbering, no extra text.\n"
               "- Keep 2 to 4 common meanings separated by '; '.\n\n"
               "%1")
        .arg(words.join(QLatin1Char('\n')));
}

QString AiClient::translatePrompt(const QString &text, bool toChinese)
{
    return QStringLiteral(
               "You are a translator. Translate the following text "
               "into natural %1.\n"
               "Keep the meaning and tone. Output ONLY the translation, "
               "no explanations, no quotes.\n\n"
               "---TEXT---\n%2")
        .arg(toChinese ? QStringLiteral("Simplified Chinese")
                       : QStringLiteral("English"),
             text.trimmed());
}

void AiClient::generateArticle(const QString &topic, int level, int wordCount)
{
    m_requestType = RequestType::Generate;
    // 文章长度按词数放大输出上限;不指定词数时给一个自然长度上限
    m_requestPredict = wordCount > 0 ? qBound(3000, wordCount * 12 + 1000, 9000)
                                     : 6000;
    m_requestTimeoutMs = 20 * 60 * 1000;
    start(topicPrompt(topic, level, wordCount));
}

void AiClient::generateArticle(const QString &topic, int level, int wordCount,
                               const QStringList &preferredWords)
{
    m_requestType = RequestType::Generate;
    m_requestPredict = wordCount > 0 ? qBound(3000, wordCount * 12 + 1000, 9000)
                                     : 6000;
    m_requestTimeoutMs = 20 * 60 * 1000;
    start(topicPrompt(topic, level, wordCount, preferredWords));
}

void AiClient::rewriteText(const QString &sourceText, int level)
{
    m_requestType = RequestType::Rewrite;
    start(rewritePrompt(sourceText, level));
}

void AiClient::translateText(const QString &text, const QString &model,
                             bool toChinese)
{
    m_requestType = RequestType::Translate;
    m_requestModel = model.trimmed();
    m_requestPredict = 4096; // 长文本翻译预留足够输出空间
    start(translatePrompt(text, toChinese));
}

void AiClient::generateWordList(const QString &domain, int count)
{
    m_requestType = RequestType::WordList;
    m_requestPredict = qBound(2000, count * 8 + 1000, 6000);
    m_requestTimeoutMs = 20 * 60 * 1000;
    start(wordListPrompt(domain, count), /*streaming=*/true);
}

void AiClient::fillMeanings(const QStringList &words)
{
    m_requestType = RequestType::WordList;
    m_requestPredict = qBound(2000, words.size() * 20 + 1000, 6000);
    m_requestTimeoutMs = 10 * 60 * 1000;
    start(fillMeaningsPrompt(words));
}

void AiClient::chat(const QString &prompt, int maxTokens,
                    const QString &model)
{
    m_requestType = RequestType::Chat;
    m_requestPredict = qBound(100, maxTokens, 2000);
    m_requestModel = model.trimmed();
    start(prompt);
}

void AiClient::cancel()
{
    m_requestType = RequestType::None;
    if (m_reply) {
        m_reply->abort();
        m_reply = nullptr;
    }
    m_timeout->stop();
}

void AiClient::start(const QString &prompt, bool streaming)
{
    if (m_reply) {
        m_reply->abort();
        m_reply = nullptr;
    }
    m_timedOut = false;
    m_streaming = streaming;
    m_streamBuffer.clear();
    m_streamText.clear();

    QJsonObject body;
    body.insert(QStringLiteral("model"),
                m_requestModel.isEmpty() ? m_model : m_requestModel);
    body.insert(QStringLiteral("stream"), streaming);
    const int predict = m_requestPredict > 0 ? m_requestPredict : 1500;

    QNetworkRequest request;
    if (m_provider == Provider::OpenAI) {
        QJsonArray messages;
        QJsonObject message;
        message.insert(QStringLiteral("role"), QStringLiteral("user"));
        message.insert(QStringLiteral("content"), prompt);
        messages.append(message);
        body.insert(QStringLiteral("messages"), messages);
        body.insert(QStringLiteral("temperature"), 0.7);
        body.insert(QStringLiteral("max_tokens"), predict);
        QString chatUrl = m_baseUrl;
        if (chatUrl.endsWith(QLatin1String("/v1"))
            || chatUrl.endsWith(QLatin1String("/v1/"))) {
            chatUrl += QStringLiteral("chat/completions");
        } else {
            chatUrl += QStringLiteral("v1/chat/completions");
        }
        request = QNetworkRequest(QUrl(chatUrl));
        if (!m_apiKey.isEmpty()) {
            request.setRawHeader(
                "Authorization",
                "Bearer " + m_apiKey.toUtf8());
        }
    } else {
        body.insert(QStringLiteral("prompt"), prompt);
        body.insert(QStringLiteral("think"),
                    false); // qwen3 关闭思考，直接输出正文
        QJsonObject options;
        options.insert(QStringLiteral("num_predict"), predict);
        options.insert(QStringLiteral("temperature"), 0.7);
        body.insert(QStringLiteral("options"), options);
        request = QNetworkRequest(
            QUrl(m_baseUrl + QStringLiteral("api/generate")));
    }
    // Waydroid/部分容器网络下 keep-alive 连接容易半死:
    // 连接看似 ESTABLISHED 但数据不通,复用会让请求一直挂起。
    // 每次请求强制新建连接,代价可忽略。
    request.setRawHeader("Connection", "close");
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    m_reply = m_manager->post(request, QJsonDocument(body).toJson());
    if (streaming) {
        connect(m_reply, &QIODevice::readyRead, this,
                &AiClient::onStreamReady);
    }
    connect(m_reply, &QNetworkReply::finished, this,
            &AiClient::onReplyFinished);
    m_timeout->start(m_requestTimeoutMs);
}

void AiClient::onStreamReady()
{
    if (!m_reply)
        return;
    m_streamBuffer += m_reply->readAll();
    // SSE: 事件以 "\n\n" 分隔;逐块解析
    int cut;
    while ((cut = m_streamBuffer.indexOf("\n\n")) >= 0) {
        const QByteArray rawEvent = m_streamBuffer.left(cut);
        m_streamBuffer.remove(0, cut + 2);
        const QList<QByteArray> lines = rawEvent.split('\n');
        for (const QByteArray &ln : lines) {
            QByteArray line = ln;
            if (!line.startsWith("data:"))
                continue;
            line = line.mid(5).trimmed();
            if (line.isEmpty() || line == "[DONE]")
                continue;
            QJsonParseError err{};
            const QJsonDocument ev =
                QJsonDocument::fromJson(line, &err);
            if (err.error != QJsonParseError::NoError)
                continue;
            QString delta;
            if (m_provider == Provider::OpenAI) {
                const QJsonArray choices =
                    ev.object().value("choices").toArray();
                if (!choices.isEmpty())
                    delta = choices.at(0)
                                .toObject()
                                .value("delta")
                                .toObject()
                                .value("content")
                                .toString();
            } else {
                // Ollama /api/generate 流式: response 字段
                delta = ev.object().value("response").toString();
            }
            if (delta.isEmpty())
                continue;
            m_streamText += delta;
            // 按行切分,每凑齐一行完整词条就发出
            int nl;
            while ((nl = m_streamText.indexOf('\n')) >= 0) {
                const QString entry =
                    m_streamText.left(nl).trimmed();
                m_streamText.remove(0, nl + 1);
                if (!entry.isEmpty())
                    emit wordListLine(entry);
            }
        }
    }
}

void AiClient::onReplyFinished()
{
    m_timeout->stop();
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    if (!reply)
        return;
    const RequestType type = m_requestType;
    m_requestType = RequestType::None;
    m_requestModel.clear();
    m_requestPredict = 0;
    m_requestTimeoutMs = 10 * 60 * 1000;
    const bool wasStreaming = m_streaming;

    if (reply->error() != QNetworkReply::NoError) {
        reply->readAll();
        QString message;
        if (m_timedOut) {
            message = QStringLiteral("生成超时（超过 10 分钟），已停止。");
        } else if (reply->error()
                   == QNetworkReply::OperationCanceledError) {
            message = QStringLiteral("已取消。");
        } else {
            message = classifyError(reply, {});
        }
        reply->deleteLater();
        m_streaming = false;
        emit failed(message);
        return;
    }

    if (wasStreaming) {
        // 流式:内容已在 onStreamReady 中累积,这里只收尾剩余行
        onStreamReady();
        const QString tail = m_streamText.trimmed();
        m_streaming = false;
        if (!tail.isEmpty())
            emit wordListLine(tail);
        if (type == RequestType::WordList) {
            emit wordListFinished(QString());
        } else {
            emit finished(QString());
        }
        reply->readAll();
        reply->deleteLater();
        return;
    }

    const QJsonDocument doc =
        QJsonDocument::fromJson(reply->readAll());
    reply->deleteLater();
    QString text;
    if (m_provider == Provider::OpenAI) {
        const QJsonArray choices =
            doc.object().value(QStringLiteral("choices")).toArray();
        if (!choices.isEmpty()) {
            text = choices.at(0)
                       .toObject()
                       .value(QStringLiteral("message"))
                       .toObject()
                       .value(QStringLiteral("content"))
                       .toString()
                       .trimmed();
        }
    } else {
        text = doc.object()
                   .value(QStringLiteral("response"))
                   .toString()
                   .trimmed();
    }
    if (text.isEmpty()) {
        emit failed(QStringLiteral("AI 返回为空，请检查模型是否可用。"));
        return;
    }
    if (type == RequestType::Translate) {
        emit translationFinished(text);
    } else if (type == RequestType::WordList) {
        emit wordListFinished(text);
    } else if (type == RequestType::Chat) {
        emit chatFinished(text);
    } else {
        emit finished(text);
    }
}
