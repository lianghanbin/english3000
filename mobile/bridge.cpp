#include "bridge.h"

#include "ai_client.h"
#include "core.h"

MobileBridge::MobileBridge(WordStore *store, AiClient *ai, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_ai(ai)
{
    connect(m_ai, &AiClient::translationFinished, this,
            [this](const QString &t) { emit translationReady(t); });
    connect(m_ai, &AiClient::failed, this,
            [this](const QString &m) { emit translationFailed(m); });
    connect(m_ai, &AiClient::chatFinished, this,
            [this](const QString &text) {
                if (m_pendingExampleId <= 0)
                    return;
                const qint64 id = m_pendingExampleId;
                m_pendingExampleId = -1;
                emit exampleReady(id, text.trimmed().simplified());
            });
}

int MobileBridge::newCount() const
{
    return m_store->counts().newTotal;
}

int MobileBridge::dueCount() const
{
    return m_store->counts().learning;
}

int MobileBridge::masteredCount() const
{
    return m_store->counts().mastered;
}

int MobileBridge::streak() const
{
    return m_store->streak();
}

QString MobileBridge::currentListName() const
{
    return m_store->currentWordListName();
}

QString MobileBridge::aiProvider() const
{
    return m_store->getSetting(QStringLiteral("ai_provider"),
                               QStringLiteral("ollama"));
}

QString MobileBridge::aiApiKey() const
{
    return m_store->getSetting(QStringLiteral("ai_api_key"));
}

QVariantList MobileBridge::newCards(int limit)
{
    QVariantList cards;
    const QVector<Word> words = m_store->learnCards(limit);
    for (const Word &w : words) {
        QVariantMap card;
        card.insert(QStringLiteral("id"), w.itemId);
        card.insert(QStringLiteral("word"), w.word);
        card.insert(QStringLiteral("pos"), w.pos);
        card.insert(QStringLiteral("meaning"), w.meaning);
        card.insert(QStringLiteral("example"), w.exampleSentence);
        cards.append(card);
    }
    return cards;
}

QVariantList MobileBridge::reviewCards(int limit)
{
    QVariantList cards;
    const QVector<Word> words = m_store->reviewCards(limit);
    for (const Word &w : words) {
        QVariantMap card;
        card.insert(QStringLiteral("id"), w.itemId);
        card.insert(QStringLiteral("word"), w.word);
        card.insert(QStringLiteral("pos"), w.pos);
        card.insert(QStringLiteral("meaning"), w.meaning);
        card.insert(QStringLiteral("example"), w.exampleSentence);
        cards.append(card);
    }
    return cards;
}

QVariantList MobileBridge::wordLists()
{
    QVariantList lists;
    const qint64 current = m_store->currentWordListId();
    for (const WordListInfo &info : m_store->listWordLists()) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), info.id);
        m.insert(QStringLiteral("name"), info.name);
        m.insert(QStringLiteral("wordCount"), info.wordCount);
        m.insert(QStringLiteral("current"), info.id == current);
        lists.append(m);
    }
    return lists;
}

void MobileBridge::setCurrentList(qint64 listId)
{
    m_store->setCurrentWordList(listId);
    emit countsChanged();
}

void MobileBridge::answer(qint64 wordId, bool known)
{
    m_store->answerStudy(wordId, known);
    emit countsChanged();
}

void MobileBridge::translate(const QString &text, const QString &model)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;
    // 翻译前先收生词（自动加入「翻译生词」词表，原句当例句）
    const QVector<Word> unknown = m_store->extractUnknownWords(trimmed, 20);
    for (const Word &w : unknown) {
        const QString sentence =
            WordStore::sentenceContaining(trimmed, w.word);
        m_store->queueWordFromTranslation(w.word, w.meaning, sentence);
    }
    int cjk = 0;
    int total = 0;
    for (const QChar c : trimmed) {
        if (c.unicode() == 0x20)
            continue;
        ++total;
        const quint32 u = c.unicode();
        if ((u >= 0x4E00 && u <= 0x9FFF)
            || (u >= 0x3400 && u <= 0x4DBF)
            || (u >= 0xF900 && u <= 0xFAFF)) {
            ++cjk;
        }
    }
    const bool toChinese =
        !(total > 0 && double(cjk) / total >= 0.3);
    m_ai->translateText(trimmed, model, toChinese);
    emit countsChanged();
}

void MobileBridge::requestExample(qint64 wordId, const QString &word)
{
    if (m_pendingExampleId > 0)
        return;
    m_pendingExampleId = wordId;
    const QString prompt =
        QStringLiteral(
            "Write one short, simple English sentence using the word "
            "\"%1\". Use the exact word. Output only the sentence.")
            .arg(word);
    m_ai->chat(prompt, 120, QStringLiteral("qwen2.5:1.5b"));
}

void MobileBridge::refresh()
{
    emit countsChanged();
}

QString MobileBridge::aiUrl() const
{
    return m_store->getSetting(QStringLiteral("ai_base_url"),
                               QStringLiteral("http://127.0.0.1:11434"));
}

void MobileBridge::setAiUrl(const QString &url)
{
    m_store->setSetting(QStringLiteral("ai_base_url"), url.trimmed());
    m_ai->setEndpoint(url.trimmed(), m_ai->model());
}

QString MobileBridge::aiModel() const
{
    return m_store->getSetting(QStringLiteral("ai_model"),
                               QStringLiteral("qwen2.5:1.5b"));
}

void MobileBridge::setAiModel(const QString &model)
{
    m_store->setSetting(QStringLiteral("ai_model"), model.trimmed());
    m_ai->setEndpoint(m_ai->baseUrl(), model.trimmed());
}

void MobileBridge::setAiProvider(const QString &provider)
{
    m_store->setSetting(QStringLiteral("ai_provider"), provider.trimmed());
    m_ai->setProvider(
        provider.trimmed() == QLatin1String("openai")
            ? AiClient::Provider::OpenAI
            : AiClient::Provider::Ollama);
    emit countsChanged();
}

void MobileBridge::setAiApiKey(const QString &key)
{
    m_store->setSetting(QStringLiteral("ai_api_key"), key.trimmed());
    m_ai->setApiKey(key.trimmed());
    emit countsChanged();
}
