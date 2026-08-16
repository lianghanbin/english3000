#pragma once

#include <QObject>
#include <QVariantList>

class AiClient;
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

private:
    WordStore *m_store = nullptr;
    AiClient *m_ai = nullptr;
    qint64 m_pendingExampleId = -1;
    qint64 m_currentArticleId = -1;
    QString m_currentArticleContent;
#ifdef ENGLISH3000_HAS_TTS
    QTextToSpeech *m_tts = nullptr;
#endif
};
