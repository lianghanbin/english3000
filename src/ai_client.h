#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class AiClient : public QObject {
    Q_OBJECT

public:
    enum class RequestType {
        None, Generate, Rewrite, Translate, WordList, Chat
    };

    explicit AiClient(QObject *parent = nullptr);
    ~AiClient() override;

    AiClient(const AiClient &) = delete;
    AiClient &operator=(const AiClient &) = delete;

    void setEndpoint(const QString &baseUrl, const QString &model);
    QString baseUrl() const { return m_baseUrl; }
    QString model() const { return m_model; }

    // 异步生成/改写；完成或失败通过信号通知，cancel() 可随时中止
    void generateArticle(const QString &topic, int level, int wordCount);
    void generateArticle(const QString &topic, int level, int wordCount,
                         const QStringList &preferredWords);
    void rewriteText(const QString &sourceText, int level);
    void translateText(const QString &text, const QString &model = {},
                       bool toChinese = true);
    void generateWordList(const QString &domain, int count);
    void chat(const QString &prompt, int maxTokens = 400);
    void cancel();

    // prompt 模板（纯函数，便于单元测试）
    static QString topicPrompt(const QString &topic, int level, int wordCount);
    static QString topicPrompt(const QString &topic, int level, int wordCount,
                               const QStringList &preferredWords);
    static QString rewritePrompt(const QString &sourceText, int level);
    static QString translatePrompt(const QString &text, bool toChinese = true);
    static QString wordListPrompt(const QString &domain, int count);
    static QString levelLabel(int level);
    RequestType requestType() const { return m_requestType; }

signals:
    void finished(const QString &articleText);
    void wordListFinished(const QString &rawText);
    void chatFinished(const QString &response);
    void translationFinished(const QString &translation);
    void failed(const QString &message);

private:
    void start(const QString &prompt);
    void onReplyFinished();

    QNetworkAccessManager *m_manager = nullptr;
    QNetworkReply *m_reply = nullptr;
    QTimer *m_timeout = nullptr;
    bool m_timedOut = false;
    RequestType m_requestType = RequestType::None;
    QString m_requestModel;
    int m_requestPredict = 0;
    int m_requestTimeoutMs = 10 * 60 * 1000;
    QString m_baseUrl = QStringLiteral("http://127.0.0.1:11434");
    QString m_model = QStringLiteral("qwen3:14b");
};
