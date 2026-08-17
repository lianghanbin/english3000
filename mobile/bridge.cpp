#include "bridge.h"

#include "ai_client.h"
#include "core.h"

#include <QRegularExpression>
#include <QSet>

#ifdef ENGLISH3000_HAS_TTS
#include <QTextToSpeech>
#endif

namespace {

const QSet<QString> kNoiseWords = {
    "the", "a", "an", "and", "or", "but", "if", "of", "in", "on", "at",
    "to", "for", "with", "from", "by", "as", "is", "are", "was", "were",
    "be", "been", "being", "have", "has", "had", "do", "does", "did",
    "will", "would", "shall", "should", "can", "could", "may", "might",
    "must", "not", "no", "yes", "this", "that", "these", "those", "it",
    "its", "he", "she", "they", "we", "you", "i", "me", "him", "her",
    "them", "us", "my", "your", "his", "their", "our", "what", "which",
    "who", "whom", "when", "where", "why", "how", "all", "any", "some",
    "each", "every", "both", "few", "more", "most", "other", "such",
    "there", "here", "then", "than", "so", "very", "just", "also", "too",
    "into", "about", "after", "before", "between", "under", "over",
    "again", "once", "only", "own", "same", "through", "during",
    "list", "words", "word", "example", "please", "include", "output",
    "one", "per", "line", "lowercase", "numbers", "explanations",
    "duplicates", "important", "nouns", "verbs", "adjectives", "field",
    "domain", "common", "commonly", "used", "exactly", "often",
    "related", "following", "below", "above", "see", "etc", "like",
};

QStringList splitWordList(const QString &raw)
{
    QString normalized = raw;
    normalized.replace(QLatin1Char(','), QLatin1Char(' '));
    const QStringList parts =
        normalized.split(QRegularExpression(QStringLiteral("\\s+")),
                         Qt::SkipEmptyParts);
    QStringList result;
    for (const QString &part : parts) {
        QString word = part.toLower();
        bool lettersOnly = true;
        for (const QChar c : word) {
            if (!c.isLetter() && c != QLatin1Char('-')
                && c != QLatin1Char('\'')) {
                lettersOnly = false;
                break;
            }
        }
        if (lettersOnly && word.size() >= 2 && !result.contains(word))
            if (!kNoiseWords.contains(word))
                result << word;
    }
    return result;
}

} // namespace

MobileBridge::MobileBridge(WordStore *store, AiClient *ai, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_ai(ai)
{
    connect(m_ai, &AiClient::translationFinished, this,
            [this](const QString &t) { emit translationReady(t); });
    connect(m_ai, &AiClient::failed, this,
            [this](const QString &m) { emit translationFailed(m); });
    connect(m_ai, &AiClient::failed, this,
            [this](const QString &m) { emit aiFailed(m); });
    connect(m_ai, &AiClient::chatFinished, this,
            [this](const QString &text) {
                if (m_pendingExampleId <= 0)
                    return;
                const qint64 id = m_pendingExampleId;
                m_pendingExampleId = -1;
                emit exampleReady(id, text.trimmed().simplified());
            });
    connect(m_ai, &AiClient::wordListFinished, this,
            &MobileBridge::onWordListFinished);
    connect(m_ai, &AiClient::finished, this,
            &MobileBridge::onArticleFinished);
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
        card.insert(QStringLiteral("rank"), w.rank);
        card.insert(QStringLiteral("pos"), w.pos);
        card.insert(QStringLiteral("phonetic"), w.phonetic);
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
        card.insert(QStringLiteral("rank"), w.rank);
        card.insert(QStringLiteral("pos"), w.pos);
        card.insert(QStringLiteral("phonetic"), w.phonetic);
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

QVariantList MobileBridge::wordListRows(qint64 listId, int limit)
{
    QVariantList rows;
    const QVector<Word> words = m_store->wordsInWordList(listId);
    const int n = qMin(limit > 0 ? limit : 50, words.size());
    for (int i = 0; i < n; ++i) {
        const Word &w = words.at(i);
        QVariantMap r;
        r.insert(QStringLiteral("word"), w.word);
        r.insert(QStringLiteral("pos"), w.pos);
        r.insert(QStringLiteral("meaning"), w.meaning);
        const QString status =
            w.box >= 6 ? QStringLiteral("mastered")
                       : (w.box == 0 ? QStringLiteral("new")
                                     : QStringLiteral("learning"));
        r.insert(QStringLiteral("status"), status);
        rows.append(r);
    }
    return rows;
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

QVariantList MobileBridge::articles()
{
    QVariantList out;
    for (const Article &a : m_store->listArticles()) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), a.id);
        m.insert(QStringLiteral("title"), a.title);
        m.insert(QStringLiteral("difficulty"), a.difficulty);
        out.append(m);
    }
    return out;
}

QString MobileBridge::articleHtml(qint64 articleId)
{
    const std::optional<Article> article = m_store->getArticle(articleId);
    if (!article)
        return {};
    m_currentArticleId = articleId;
    m_currentArticleContent = article->content;
    const QSet<QString> inAny = m_store->allListWords();
    const QSet<QString> current = m_store->currentListWords();
    const QSet<QString> mastered = m_store->masteredListWords();

    auto colorFor = [&](const QString &w) {
        QString lookup = w.toLower();
        const bool known =
            inAny.contains(lookup)
            || inAny.contains(m_store->lookupLemma(lookup));
        const bool isMastered =
            mastered.contains(lookup)
            || mastered.contains(m_store->lookupLemma(lookup));
        const bool inCurrent =
            current.contains(lookup)
            || current.contains(m_store->lookupLemma(lookup));
        if (!known)
            return QStringLiteral("#c62828");
        if (isMastered)
            return QStringLiteral("#000000");
        if (inCurrent)
            return QStringLiteral("#2e7d32");
        return QStringLiteral("#1565c0");
    };

    QString html = QStringLiteral(
        "<div style='font-size:18px; line-height:1.7;'>");
    QString word;
    const QString content = article->content;
    for (const QChar c : content) {
        if (c.isLetterOrNumber() || c == QLatin1Char('\'')
            || c == QLatin1Char('-')) {
            word += c;
        } else {
            if (!word.isEmpty()) {
                html += QStringLiteral(
                            "<a href=\"word://%1\"><font color=\"%2\">%3"
                            "</font></a>")
                            .arg(word.toLower().toHtmlEscaped(),
                                 colorFor(word), word.toHtmlEscaped());
                word.clear();
            }
            html += QString(c).toHtmlEscaped();
        }
    }
    if (!word.isEmpty()) {
        html += QStringLiteral(
                    "<a href=\"word://%1\"><font color=\"%2\">%3</font></a>")
                    .arg(word.toLower().toHtmlEscaped(),
                         colorFor(word), word.toHtmlEscaped());
    }
    html += QStringLiteral("</div>");
    return html;
}

QString MobileBridge::articleContent(qint64 articleId)
{
    const std::optional<Article> article = m_store->getArticle(articleId);
    return article ? article->content : QString();
}

void MobileBridge::addReadingWord(const QString &word)
{
    const QString w = word.trimmed();
    if (w.isEmpty() || m_currentArticleContent.isEmpty())
        return;
    QString meaning;
    const std::optional<Word> found = m_store->findWordByText(w);
    if (found) {
        meaning = found->meaning;
    } else {
        const std::optional<Word> dict = m_store->lookupDict(w);
        if (dict)
            meaning = dict->meaning;
    }
    m_store->queueWordToReadingList(
        w, meaning,
        WordStore::sentenceContaining(m_currentArticleContent, w));
    emit countsChanged();
}

void MobileBridge::speak(const QString &text)
{
#ifdef ENGLISH3000_HAS_TTS
#if defined(Q_OS_ANDROID)
    // Waydroid/无 TTS 引擎的安卓:Qt 安卓插件误报有引擎,创建后第二次
    // say() 会触发错误线程回调并把整个 app 带退出。真机 TTS 后续接入
    // 绑定检测后再启用。
    Q_UNUSED(text);
    return;
#else
    if (!m_tts) {
        // 没有可用 TTS 引擎时不要创建对象:安卓插件在初始化失败后
        // 会从错误线程回调并导致应用退出(Waydroid/精简系统无引擎)。
        if (QTextToSpeech::availableEngines().isEmpty())
            return;
        m_tts = new QTextToSpeech(this);
    }
    m_tts->say(text);
#endif
#else
    Q_UNUSED(text);
#endif
}

void MobileBridge::onWordListFinished(const QString &rawText)
{
    const QStringList words = splitWordList(rawText);
    const QString name = m_pendingListName;
    const qint64 listId = m_pendingListId;
    m_pendingListId = -1;
    if (words.isEmpty()) {
        emit aiFailed(QStringLiteral("AI 没有返回有效单词"));
        return;
    }
    if (listId > 0) {
        QSet<QString> existing;
        const QVector<Word> current = m_store->wordsInWordList(listId);
        for (const Word &w : current)
            existing.insert(w.word);
        int added = 0;
        int order = current.size();
        for (const QString &word : words) {
            if (existing.contains(word))
                continue;
            QString pos;
            QString meaning;
            const std::optional<Word> found = m_store->findWordByText(word);
            if (found) {
                pos = found->pos;
                meaning = found->meaning;
            } else {
                const std::optional<Word> dict = m_store->lookupDict(word);
                if (dict) {
                    pos = dict->pos;
                    meaning = dict->meaning;
                }
            }
            if (m_store->addWordToList(listId, word, pos, meaning, order)) {
                existing.insert(word);
                ++added;
                ++order;
            }
        }
        emit wordListReady(name, added);
        return;
    }
    const qint64 newId = m_store->createWordList(
        name, QStringLiteral("AI 生成领域词表"), QStringLiteral("ai"));
    if (newId < 0) {
        emit aiFailed(QStringLiteral("创建失败:同名词表可能已存在"));
        return;
    }
    for (int i = 0; i < words.size(); ++i) {
        const QString &word = words[i];
        QString pos;
        QString meaning;
        const std::optional<Word> found = m_store->findWordByText(word);
        if (found) {
            pos = found->pos;
            meaning = found->meaning;
        } else {
            const std::optional<Word> dict = m_store->lookupDict(word);
            if (dict) {
                pos = dict->pos;
                meaning = dict->meaning;
            }
        }
        m_store->addWordToList(newId, word, pos, meaning, i);
    }
    emit wordListReady(name, words.size());
}

void MobileBridge::onArticleFinished(const QString &articleText)
{
    QString title = m_pendingArticleTitle.trimmed();
    m_pendingArticleTitle.clear();
    if (title.isEmpty())
        title = QStringLiteral("AI 生成文章");
    const qint64 id = m_store->saveArticle(
        title, articleText, QStringLiteral("ai"), 1);
    emit articleReady(id, title);
}

void MobileBridge::aiGenerateWordList(const QString &domain, int count)
{
    const QString d = domain.trimmed();
    if (d.isEmpty()) {
        emit aiFailed(QStringLiteral("请先输入领域"));
        return;
    }
    m_pendingListId = -1;
    m_pendingListName = d;
    m_ai->generateWordList(d, qBound(20, count, 300));
}

void MobileBridge::aiSupplementWordList(const QString &domain, int count)
{
    const qint64 listId = m_store->currentWordListId();
    if (listId <= 0) {
        emit aiFailed(QStringLiteral("请先选择一个词表"));
        return;
    }
    const QString name = m_store->currentWordListName();
    const QString d = domain.trimmed().isEmpty() ? name : domain.trimmed();
    m_pendingListId = listId;
    m_pendingListName = name;
    m_ai->generateWordList(d, qBound(20, count, 200));
}

void MobileBridge::aiGenerateArticle(const QString &topic)
{
    const qint64 listId = m_store->currentWordListId();
    QString t = topic.trimmed();
    if (t.isEmpty())
        t = m_store->currentWordListName();
    if (t.isEmpty())
        t = QStringLiteral("英语学习");
    m_pendingArticleTitle = t;
    if (listId > 0) {
        QStringList preferred;
        const QVector<Word> words = m_store->wordsInWordList(listId);
        for (const Word &w : words)
            preferred << w.word;
        if (preferred.size() > 200)
            preferred = preferred.mid(0, 200);
        m_ai->generateArticle(t, 1, 300, preferred);
    } else {
        m_ai->generateArticle(t, 1, 300);
    }
}

void MobileBridge::aiCancel()
{
    m_ai->cancel();
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
