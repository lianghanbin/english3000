#include "ai_client.h"

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
    return QStringLiteral(
               "You are an English teacher writing graded reading material.\n"
               "Write a short English article about: %1\n"
               "Level: %2\n"
               "Length: about %3 words.\n"
               "Rules:\n"
               "- Use short, clear sentences.\n"
               "- Prefer the most common English words.\n"
               "- Output ONLY the article text. No title, no quotes, "
               "no explanations.")
        .arg(topic.trimmed(), levelLabel(level))
        .arg(wordCount);
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
               "- Output ONLY the words, one per line, lowercase.\n"
               "- Do not include numbers, explanations, or duplicate words.\n"
               "- Include important nouns, verbs, and adjectives.")
        .arg(count)
        .arg(domain.trimmed());
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
    start(topicPrompt(topic, level, wordCount));
}

void AiClient::generateArticle(const QString &topic, int level, int wordCount,
                               const QStringList &preferredWords)
{
    m_requestType = RequestType::Generate;
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
    start(wordListPrompt(domain, count));
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

void AiClient::start(const QString &prompt)
{
    if (m_reply) {
        m_reply->abort();
        m_reply = nullptr;
    }
    m_timedOut = false;

    QJsonObject body;
    body.insert(QStringLiteral("model"),
                m_requestModel.isEmpty() ? m_model : m_requestModel);
    body.insert(QStringLiteral("stream"), false);
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
    connect(m_reply, &QNetworkReply::finished, this,
            &AiClient::onReplyFinished);
    m_timeout->start(m_requestTimeoutMs);
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

    if (reply->error() != QNetworkReply::NoError) {
        const QString message =
            m_timedOut
                ? QStringLiteral("生成超时（超过 10 分钟），已停止。")
                : (reply->error() == QNetworkReply::OperationCanceledError
                       ? QStringLiteral("已取消。")
                       : QStringLiteral("AI 请求失败：%1")
                             .arg(reply->errorString()));
        reply->deleteLater();
        emit failed(message);
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
