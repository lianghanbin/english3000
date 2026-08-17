#pragma once

#include <QObject>
#include <QVariantList>

class AiClient;
class AiProbe;
class QNetworkAccessManager;
class QNetworkReply;
#if defined(Q_OS_ANDROID)
class QMediaPlayer;
#endif
class QTextToSpeech;
class WordStore;

class MobileBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(int newCount READ newCount NOTIFY countsChanged)
    Q_PROPERTY(int dueCount READ dueCount NOTIFY countsChanged)
    Q_PROPERTY(int masteredCount READ masteredCount NOTIFY countsChanged)
    Q_PROPERTY(int streak READ streak NOTIFY countsChanged)
    Q_PROPERTY(QString currentListName READ currentListName NOTIFY countsChanged)
    Q_PROPERTY(QString aiProvider READ aiProvider NOTIFY countsChanged)
    Q_PROPERTY(QString aiApiKey READ aiApiKey NOTIFY countsChanged)

public:
    MobileBridge(WordStore *store, AiClient *ai, QObject *parent = nullptr);

    int newCount() const;
    int dueCount() const;
    int masteredCount() const;
    int streak() const;
    QString currentListName() const;
    QString aiProvider() const;
    QString aiApiKey() const;

    Q_INVOKABLE QVariantList newCards(int limit);
    Q_INVOKABLE QVariantList reviewCards(int limit);
    Q_INVOKABLE QVariantList wordLists();
    Q_INVOKABLE QVariantList wordListRows(qint64 listId, int limit);
    Q_INVOKABLE void setCurrentList(qint64 listId);
    Q_INVOKABLE void answer(qint64 wordId, bool known);
    Q_INVOKABLE void translate(const QString &text, const QString &model);
    Q_INVOKABLE void requestExample(qint64 wordId, const QString &word);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantList articles();
    Q_INVOKABLE QString articleHtml(qint64 articleId);
    Q_INVOKABLE QString articleContent(qint64 articleId);
    Q_INVOKABLE void addReadingWord(const QString &word);
    Q_INVOKABLE void speak(const QString &text);
    Q_INVOKABLE void aiGenerateWordList(const QString &domain, int count);
    Q_INVOKABLE void aiSupplementWordList(const QString &domain, int count);
    Q_INVOKABLE void aiGenerateArticle(const QString &topic);
    Q_INVOKABLE void aiCancel();
    Q_INVOKABLE void chatOpen(const QString &title, const QString &content);
    Q_INVOKABLE void chatSend(const QString &message);
    Q_INVOKABLE void chatClear();
    Q_INVOKABLE void importUrl(const QString &url);
    Q_INVOKABLE void deleteArticle(qint64 articleId);
    Q_INVOKABLE void reimportBuiltin();
    Q_INVOKABLE void resetAllProgress();
    Q_INVOKABLE void resetListItem(qint64 itemId);
    Q_INVOKABLE void aiProbe();

    Q_INVOKABLE QString aiUrl() const;
    Q_INVOKABLE void setAiUrl(const QString &url);
    Q_INVOKABLE QString aiModel() const;
    Q_INVOKABLE void setAiModel(const QString &model);
    Q_INVOKABLE void setAiProvider(const QString &provider);
    Q_INVOKABLE void setAiApiKey(const QString &key);

signals:
    void countsChanged();
    void translationReady(const QString &translation);
    void translationFailed(const QString &message);
    void exampleReady(qint64 wordId, const QString &sentence);
    void wordListReady(const QString &name, int count);
    void articleReady(qint64 articleId, const QString &title);
    void articleImported(qint64 articleId, const QString &title);
    void chatReady(const QString &text);
    void chatFailed(const QString &message);
    void aiProbeFinished(const QString &label);
    void aiFailed(const QString &message);

private:
    void onWordListFinished(const QString &rawText);
    void onArticleFinished(const QString &articleText);
    QString chatBuildPrompt() const;
    void onImportFinished();

    WordStore *m_store = nullptr;
    AiClient *m_ai = nullptr;
    AiProbe *m_probe = nullptr;
    QNetworkAccessManager *m_net = nullptr;
    QNetworkReply *m_importReply = nullptr;
    qint64 m_pendingExampleId = -1;
    qint64 m_currentArticleId = -1;
    QString m_currentArticleContent;
    QString m_pendingListName;
    qint64 m_pendingListId = -1;
    QString m_pendingArticleTitle;
    QString m_chatTitle;
    QString m_chatContext;
    QStringList m_chatHistory;
    bool m_pendingChat = false;
#ifdef ENGLISH3000_HAS_TTS
#if defined(Q_OS_ANDROID)
    QMediaPlayer *m_player = nullptr;
#else
    QTextToSpeech *m_tts = nullptr;
#endif
#endif
};
