#include "bridge.h"

#include "ai_probe.h"
#include "ai_client.h"
#include "core.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QGuiApplication>
#include <QClipboard>

#ifdef ENGLISH3000_HAS_TTS
#include <QDebug>
#include <QTextToSpeech>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QTemporaryFile>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QWebSocket>
#include <QCryptographicHash>
#include <QUuid>
#include <QUrlQuery>
#include <QTimer>
#include <QLocale>
#include <QDateTime>
#if defined(Q_OS_ANDROID)
#include <espeak-ng/speak_lib.h>
#include <QJniObject>
#endif
#endif

namespace {

// 学习卡片优先使用离线词典的完整释义;查不到原词时尝试词形还原
void enrichCardFromDict(WordStore *store, QVariantMap &card, const Word &w)
{
    std::optional<Word> dict = store->lookupDict(w.word);
    if (!dict && !w.word.isEmpty()) {
        const QString lemma = store->lookupLemma(w.word);
        if (!lemma.isEmpty())
            dict = store->lookupDict(lemma);
    }
    if (dict && !dict->meaning.isEmpty()) {
        card.insert(QStringLiteral("pos"), dict->pos);
        card.insert(QStringLiteral("phonetic"), dict->phonetic);
        card.insert(QStringLiteral("meaning"), dict->meaning);
    }
}

#if defined(Q_OS_ANDROID)
bool isWaydroidContainer()
{
    QProcess proc;
    proc.start(QStringLiteral("getprop"),
               {QStringLiteral("ro.product.manufacturer")});
    if (!proc.waitForFinished(1500))
        return false;
    return proc.readAll().contains(QByteArrayLiteral("Waydroid"));
}
#endif

#if defined(Q_OS_ANDROID) && defined(ENGLISH3000_HAS_TTS)
// edge-tts 音频的持久磁盘缓存目录(AppData/tts-cache)。
// 让自然音色跨启动复用,越用越快、越用越自然。
QString ttsCacheDir()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/tts-cache");
    QDir().mkpath(dir);
    return dir;
}

QString ttsCachePath(const QString &text, const QString &voice)
{
    if (text.size() > 200)
        return {};
    const QByteArray hash =
        QCryptographicHash::hash((voice + QLatin1Char('|') + text).toUtf8(),
                                 QCryptographicHash::Sha1).toHex();
    return ttsCacheDir() + QLatin1Char('/') + QString::fromLatin1(hash)
           + QStringLiteral(".mp3");
}

void saveTtsCacheLocal(const QString &text, const QString &voice,
                       const QByteArray &data)
{
    const QString path = ttsCachePath(text, voice);
    if (path.isEmpty() || data.isEmpty())
        return;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(data);
        f.close();
    }
}

// 把内置的 espeak-ng-data 从资源解压到应用数据目录(仅首次)。
QString ensureEspeakData()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString dataDir = dir + QStringLiteral("/espeak-ng-data");
    QDir d(dataDir);
    if (d.exists() && QFileInfo::exists(dataDir + QStringLiteral("/phondata")))
        return dir; // 已解压
    QDir().mkpath(dataDir);
    QDirIterator it(QStringLiteral(":/espeak-ng-data"),
                    QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString rel = it.filePath().mid(
            QStringLiteral(":/espeak-ng-data").size() + 1);
        QFile in(it.filePath());
        QFile out(dataDir + QLatin1Char('/') + rel);
        QDir().mkpath(QFileInfo(out).absolutePath());
        if (in.open(QIODevice::ReadOnly) && out.open(QIODevice::WriteOnly)) {
            out.write(in.readAll());
            out.close();
        }
    }
    return dir;
}

// 用 espeak-ng 合成英文,返回完整 WAV(16bit mono)。
QByteArray synthEspeak(const QString &text)
{
    static const QString dataPath = ensureEspeakData();
    QByteArray pcm;
    static QByteArray *sink = &pcm;

    // espeak 回调收集样本
    auto cb = [](short *wav, int numsamples, espeak_EVENT *) -> int {
        if (numsamples > 0 && wav)
            sink->append(reinterpret_cast<const char *>(wav),
                         numsamples * sizeof(short));
        return 0;
    };

    const int rate = espeak_Initialize(AUDIO_OUTPUT_SYNCHRONOUS, 0,
                                       dataPath.toUtf8().constData(),
                                       espeakINITIALIZE_DONT_EXIT);
    if (rate <= 0)
        return QByteArray();
    espeak_SetSynthCallback(cb);
    espeak_SetVoiceByName("en-us");
    espeak_SetParameter(espeakRATE, 150, 0);
    espeak_SetParameter(espeakPITCH, 50, 0);
    espeak_SetParameter(espeakVOLUME, 100, 0);

    pcm.clear();
    sink = &pcm;
    const QByteArray utf8 = text.toUtf8();
    espeak_Synth(utf8.constData(), utf8.size() + 1, 0, POS_CHARACTER, 0,
                 espeakCHARS_UTF8, nullptr, nullptr);
    espeak_Terminate();
    sink = nullptr;
    if (pcm.isEmpty())
        return QByteArray();

    // 组 WAV 头
    QByteArray wav;
    const qint32 dataLen = pcm.size();
    const qint32 byteRate = rate * 2;
    auto put32 = [&wav](quint32 v) {
        wav.append(char(v & 0xff));
        wav.append(char((v >> 8) & 0xff));
        wav.append(char((v >> 16) & 0xff));
        wav.append(char((v >> 24) & 0xff));
    };
    auto put16 = [&wav](quint16 v) {
        wav.append(char(v & 0xff));
        wav.append(char((v >> 8) & 0xff));
    };
    wav.append("RIFF", 4);
    put32(36 + dataLen);
    wav.append("WAVE", 4);
    wav.append("fmt ", 4);
    put32(16);
    put16(1);          // PCM
    put16(1);          // mono
    put32(rate);
    put32(byteRate);
    put16(2);          // block align
    put16(16);         // bits per sample
    wav.append("data", 4);
    put32(dataLen);
    wav.append(pcm);
    return wav;
}
#endif

} // namespace

MobileBridge::MobileBridge(WordStore *store, AiClient *ai, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_ai(ai)
{
    connect(m_ai, &AiClient::translationFinished, this,
            [this](const QString &t) {
                if (!m_lastTranslateSource.trimmed().isEmpty()) {
                    const QVector<Word> unknown =
                        m_store->extractUnknownWords(
                            m_lastTranslateSource, 20);
                    m_store->queueWordsFromTranslation(
                        unknown, m_lastTranslateSource);
                    reloadCounts();
                }
                emit translationReady(t);
            });
    connect(m_ai, &AiClient::failed, this,
            [this](const QString &m) {
                const qint64 exampleId = m_pendingExampleId;
                m_pendingExampleId = -1;
                m_exampleBusy = false;
                if (m_pendingChat) {
                    m_pendingChat = false;
                    emit chatFailed(m);
                }
                // 只在确实是翻译请求时报"翻译失败";
                // 例句生成失败只走 aiFailed,避免误报成翻译失败。
                if (m_pendingTranslate) {
                    m_pendingTranslate = false;
                    emit translationFailed(m);
                }
                m_pendingTranslate = false;
                if (exampleId > 0)
                    emit exampleFailed(exampleId, m);
                emit aiFailed(m);
                kickExamplePrefetch();
            });
    connect(m_ai, &AiClient::chatFinished, this,
            [this](const QString &text) {
                if (m_pendingExampleId > 0) {
                    const qint64 id = m_pendingExampleId;
                    m_pendingExampleId = -1;
                    m_exampleBusy = false;
                    const QString sentence = text.trimmed().simplified();
                    m_exampleCache.insert(id, sentence);
                    emit exampleReady(id, sentence);
                    // 例句就绪后,顺手把例句发音也预取了
                    if (!sentence.isEmpty())
                        prefetchSpeak(sentence);
                    kickExamplePrefetch();
                    return;
                }
                if (m_pendingChat) {
                    m_pendingChat = false;
                    const QString reply = text.trimmed();
                    m_chatHistory << QStringLiteral("AI: %1").arg(reply);
                    emit chatReady(reply);
                }
            });
    connect(m_ai, &AiClient::wordListFinished, this,
            &MobileBridge::onWordListFinished);
    connect(m_ai, &AiClient::finished, this,
            &MobileBridge::onArticleFinished);
    m_net = new QNetworkAccessManager(this);
    // 首次启动导入核心 3000 的预置例句(随包资源,只导一次)
    m_store->importBuiltinExamples();
#if defined(Q_OS_ANDROID)
    // 预热 Android 系统 TTS:启动时就异步初始化引擎,
    // 第一次点单词时引擎基本已就绪,避免首声延迟。
    QTimer::singleShot(500, this, [this]{ nativeSpeak(QString()); });
#endif
    reloadCounts();
}

void MobileBridge::reloadCounts()
{
    const Counts c = m_store->counts();
    m_newCount = c.newTotal;
    m_dueCount = c.learning;
    m_masteredCount = c.mastered;
    m_streak = m_store->streak();
    m_currentListName = m_store->currentWordListName();
    emit countsChanged();
}

int MobileBridge::newCount() const
{
    return m_newCount;
}

int MobileBridge::dueCount() const
{
    return m_dueCount;
}

int MobileBridge::masteredCount() const
{
    return m_masteredCount;
}

int MobileBridge::streak() const
{
    return m_streak;
}

bool MobileBridge::dictReady() const
{
    return m_store->dictReady();
}

void MobileBridge::notifyDictReady()
{
    reloadCounts();
    emit dictReadyChanged();
}

void MobileBridge::openUrl(const QString &url)
{
    if (!url.isEmpty())
        QDesktopServices::openUrl(QUrl(url));
}

QString MobileBridge::clipboardText() const
{
    if (auto *cb = QGuiApplication::clipboard())
        return cb->text().trimmed();
    return {};
}

QString MobileBridge::currentListName() const
{
    return m_currentListName;
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
        enrichCardFromDict(m_store, card, w);
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
        enrichCardFromDict(m_store, card, w);
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
    const QVector<Word> words =
        m_store->wordsInWordList(listId, limit > 0 ? limit : 0);
    const int n = limit > 0 ? qMin(limit, words.size()) : words.size();
    for (int i = 0; i < n; ++i) {
        const Word &w = words.at(i);
        QVariantMap r;
        r.insert(QStringLiteral("id"), w.itemId);
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

QVariantList MobileBridge::wordListPageRows(qint64 listId, int offset,
                                             int limit)
{
    QVariantList rows;
    if (limit <= 0)
        return rows;
    const QVector<Word> words =
        m_store->wordsInWordList(listId, limit, qMax(0, offset));
    for (const Word &w : words) {
        QVariantMap r;
        r.insert(QStringLiteral("id"), w.itemId);
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

QVariantMap MobileBridge::wordInfo(const QString &word)
{
    QVariantMap out;
    const QString w = word.trimmed();
    if (w.isEmpty())
        return out;
    std::optional<Word> found = m_store->findWordByText(w);
    std::optional<Word> dict = m_store->lookupDict(w);
    if (!dict && !w.isEmpty()) {
        const QString lemma = m_store->lookupLemma(w);
        if (!lemma.isEmpty())
            dict = m_store->lookupDict(lemma);
    }
    // 优先完整离线词典释义,词表释义兜底
    if (dict && !dict->meaning.isEmpty())
        found = dict;
    if (found) {
        out.insert(QStringLiteral("word"), found->word);
        out.insert(QStringLiteral("phonetic"), found->phonetic);
        out.insert(QStringLiteral("pos"), found->pos);
        out.insert(QStringLiteral("meaning"), found->meaning);
    } else {
        out.insert(QStringLiteral("word"), w);
        out.insert(QStringLiteral("meaning"),
                   QStringLiteral("（词库中暂无释义，可点翻译查询）"));
    }
    return out;
}

void MobileBridge::setCurrentList(qint64 listId)
{
    m_store->setCurrentWordList(listId);
    // 只是切换当前词表,不增删词表:发更轻的 currentListChanged,
    // 让词表页只刷新右侧数据、不重建左侧词表列表,避免切换卡顿。
    reloadCounts();
    emit currentListChanged();
}

qint64 MobileBridge::currentListId() const
{
    return m_store->currentWordListId();
}

QString MobileBridge::currentListTitle() const
{
    return m_store->currentWordListName();
}

void MobileBridge::deleteWordList(qint64 listId)
{
#if defined(Q_OS_ANDROID) && defined(ENGLISH3000_HAS_TTS)
    // 删除前记下该词表的单词与例句文本,删完后清理已不再被任何
    // 词表使用的发音缓存,释放磁盘空间。
    const QVector<Word> oldWords = m_store->wordsInWordList(listId, 0, 0);
#endif
    m_store->deleteWordList(listId);
    reloadCounts();
    emit listChanged();
#if defined(Q_OS_ANDROID) && defined(ENGLISH3000_HAS_TTS)
    if (!oldWords.isEmpty()) {
        // 收集删除后库里仍存在的所有文本(单词+例句),用于判断保留哪些缓存
        QSet<QString> stillUsed;
        QSqlQuery q = m_store->rawQuery(QStringLiteral(
            "SELECT DISTINCT w.word FROM words w "
            "JOIN word_list_items i ON i.word=w.word COLLATE NOCASE"));
        while (q.next())
            stillUsed.insert(q.value(0).toString().trimmed().toLower());
        q = m_store->rawQuery(QStringLiteral(
            "SELECT DISTINCT example_sentence FROM words "
            "WHERE example_sentence<>''"));
        while (q.next())
            stillUsed.insert(q.value(0).toString().trimmed());

        const QString voice = ttsVoice();
        int removed = 0;
        for (const Word &w : oldWords) {
            const QStringList texts = { w.word.trimmed(),
                                        w.exampleSentence.trimmed() };
            for (const QString &t : texts) {
                if (t.isEmpty() || t.size() > 200)
                    continue;
                if (stillUsed.contains(t))
                    continue; // 别的词表还在用,保留缓存
                const QString path = ttsCachePath(t, voice);
                if (!path.isEmpty() && QFile::exists(path)) {
                    QFile::remove(path);
                    ++removed;
                }
                m_speakCache.remove(t);
            }
        }
        qWarning("tts cache: pruned %d unused file(s) after list delete",
                 removed);
    }
#endif
}

void MobileBridge::answer(qint64 wordId, bool known)
{
    m_store->answerStudy(wordId, known);
    reloadCounts();
}

void MobileBridge::translate(const QString &text, const QString &model)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;
#if defined(Q_OS_ANDROID)
    // 手机上的 127.0.0.1/localhost 是手机自己，不是电脑；
    // 直接给出可操作的提示，避免白白等超时。
    const QString base = aiUrl().toLower();
    if (base.contains(QStringLiteral("127.0.0.1"))
        || base.contains(QStringLiteral("localhost"))) {
        emit translationFailed(QStringLiteral(
            "手机连不上本机 AI。请到设置页选择云端模型"
            "（DeepSeek/通义/GLM/Kimi/OpenAI）并填写 API Key，"
            "或把地址改成电脑的局域网 IP。"));
        return;
    }
#endif
    // 翻译完成后再收生词（避免点击瞬间卡顿；原句当例句）
    m_lastTranslateSource = trimmed;
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
    m_pendingTranslate = true;
    reloadCounts();
}

void MobileBridge::requestExample(qint64 wordId, const QString &word)
{
    if (wordId <= 0)
        return;
    // 已缓存:直接回投
    auto cached = m_exampleCache.constFind(wordId);
    if (cached != m_exampleCache.constEnd()) {
        emit exampleReady(wordId, cached.value());
        return;
    }
    // 从预取队列里摘掉(即将被显式请求)
    for (int i = 0; i < m_examplePrefetch.size(); ++i) {
        if (m_examplePrefetch.at(i).first == wordId) {
            m_examplePrefetch.removeAt(i);
            break;
        }
    }
    m_requestedExampleIds.insert(wordId);
    // 正在处理别的例句:把本次请求插到队首(用户显式请求优先)
    if (m_exampleBusy || m_pendingExampleId > 0) {
        if (m_pendingExampleId != wordId)
            m_examplePrefetch.prepend({wordId, word});
        return;
    }
    m_exampleBusy = true;
    m_pendingExampleId = wordId;
    const QString prompt =
        QStringLiteral(
            "Write one short, simple English sentence using the word "
            "\"%1\". Use the exact word. Output only the sentence.")
            .arg(word);
    m_ai->chat(prompt, 120, m_ai->model());
}

void MobileBridge::prefetchExample(qint64 wordId, const QString &word)
{
    if (wordId <= 0)
        return;
    if (m_exampleCache.contains(wordId)
        || m_requestedExampleIds.contains(wordId))
        return;
    for (const auto &p : m_examplePrefetch) {
        if (p.first == wordId)
            return;
    }
    // 最多排队 4 个,超出丢最旧的
    while (m_examplePrefetch.size() >= 4)
        m_examplePrefetch.dequeue();
    m_examplePrefetch.enqueue({wordId, word});
    kickExamplePrefetch();
}

void MobileBridge::kickExamplePrefetch()
{
    if (m_exampleBusy || m_pendingExampleId > 0)
        return;
    while (!m_examplePrefetch.isEmpty()) {
        const auto pair = m_examplePrefetch.dequeue();
        const qint64 id = pair.first;
        if (m_exampleCache.contains(id)
            || m_requestedExampleIds.contains(id))
            continue;
        m_requestedExampleIds.insert(id);
        m_exampleBusy = true;
        m_pendingExampleId = id;
        const QString prompt =
            QStringLiteral(
                "Write one short, simple English sentence using the word "
                "\"%1\". Use the exact word. Output only the sentence.")
                .arg(pair.second);
        m_ai->chat(prompt, 120, m_ai->model());
        return;
    }
}

void MobileBridge::cancelExample()
{
    m_examplePrefetch.clear();
    m_pendingExampleId = -1;
    m_exampleBusy = false;
    m_ai->cancel();
}

void MobileBridge::refresh()
{
    reloadCounts();
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

QString MobileBridge::highlightText(const QString &text)
{
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

    QString html;
    QString word;
    for (const QChar c : text) {
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
    return html;
}

QString MobileBridge::articleHtml(qint64 articleId)
{
    const std::optional<Article> article = m_store->getArticle(articleId);
    if (!article)
        return {};
    m_currentArticleId = articleId;
    m_currentArticleContent = article->content;
    return QStringLiteral(
               "<style>a { text-decoration: none; }</style>"
               "<div style='font-size:18px; line-height:1.7;'>")
        + highlightText(article->content) + QStringLiteral("</div>");
}

QString MobileBridge::articleContent(qint64 articleId)
{
    const std::optional<Article> article = m_store->getArticle(articleId);
    return article ? article->content : QString();
}

QString MobileBridge::sentenceForArticle(qint64 articleId,
                                         const QString &word)
{
    const std::optional<Article> article = m_store->getArticle(articleId);
    if (!article)
        return {};
    return WordStore::sentenceContaining(article->content, word.trimmed());
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
    reloadCounts();
}

void MobileBridge::addToReadingList(const QString &word)
{
    const QString w = word.trimmed();
    if (w.isEmpty())
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
    m_store->queueWordToReadingList(w, meaning, {});
    reloadCounts();
}

QVariantList MobileBridge::coverageHistory(int days)
{
    QVariantList out;
    const QVector<CoveragePoint> points =
        m_store->coverageHistory(qBound(7, days, 30));
    for (const CoveragePoint &p : points) {
        QVariantMap m;
        m.insert(QStringLiteral("date"), p.date);
        m.insert(QStringLiteral("coverage"), p.coverage);
        m.insert(QStringLiteral("articles"), p.articles);
        out.append(m);
    }
    return out;
}

void MobileBridge::speak(const QString &text)
{
#ifdef ENGLISH3000_HAS_TTS
#if defined(Q_OS_ANDROID)
    const QString t = text.trimmed();
    if (t.isEmpty())
        return;
    if (isWaydroidContainer()) {
        const QUrl url(
            QStringLiteral("http://127.0.0.1:8099/play?text=")
            + QString::fromLatin1(QUrl::toPercentEncoding(t)));
        m_net->get(QNetworkRequest(url));
        return;
    }
    // 短文本(单词/例句):
    //  - 选"system"时纯走系统本地 TTS(离线、零延迟、机械音)。
    //  - 选 edge 音色时优先用其缓存(内存/磁盘,毫秒级自然音);
    //    没缓存则立刻用系统 TTS 兜底保证零延迟,同时后台拉取该
    //    edge 音色存盘,下次即为自然音。
    // 长文直接走 edge-tts 神经网络音色。
    if (t.size() <= 200) {
        if (ttsVoice() == QLatin1String("system")) {
            nativeSpeak(t);
        } else {
            playCachedOrFetch(t, false);
        }
        return;
    }
    speakEdgeOrFallback(t);
#else
    // 桌面端也走本机中继的 Piper 合成,声音比系统 speechd/espeak 自然。
    const QString t = text.trimmed();
    if (t.isEmpty())
        return;
    const QUrl url(QStringLiteral("http://127.0.0.1:8099/play?text=")
                   + QString::fromLatin1(QUrl::toPercentEncoding(t)));
    m_net->get(QNetworkRequest(url));
#endif
#else
    Q_UNUSED(text);
#endif
}

#ifdef ENGLISH3000_HAS_TTS
void MobileBridge::playAudioBytes(const QByteArray &audio)
{
    if (audio.isEmpty())
        return;
    if (m_player && m_ttsFile) {
        m_player->stop();
        m_ttsFile->close();
        m_ttsFile->remove();
        delete m_ttsFile;
        m_ttsFile = nullptr;
    }
    if (!m_player) {
        m_player = new QMediaPlayer(this);
        m_audioOut = new QAudioOutput(this);
        m_player->setAudioOutput(m_audioOut);
    }
    auto *f = new QTemporaryFile(this);
    if (!f->open()) {
        delete f;
        return;
    }
    f->write(audio);
    f->flush();
    m_ttsFile = f;
    m_player->stop();
    m_player->setSource(QUrl::fromLocalFile(f->fileName()));
    m_player->play();
}

void MobileBridge::speakEdgeOrFallback(const QString &text)
{
    startEdgeTts(text);
}

void MobileBridge::playCachedOrFetch(const QString &text, bool prefetchOnly)
{
    const QString voice = ttsVoice();
    // 1) 内存缓存
    auto it = m_speakCache.constFind(text);
    if (it != m_speakCache.constEnd() && !it->isEmpty()) {
        if (!prefetchOnly) {
            m_nativeSpeaking.clear();
            m_edgeIsPlayback = true;
            playAudioBytes(it.value());
        }
        return;
    }
    // 2) 磁盘缓存(跨启动持久化,按音色区分)
    const QString path = ttsCachePath(text, voice);
    if (!path.isEmpty()) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            const QByteArray data = f.readAll();
            f.close();
            if (!data.isEmpty()) {
                if (m_speakCache.size() < 200)
                    m_speakCache.insert(text, data);
                if (!prefetchOnly) {
                    m_nativeSpeaking.clear();
                    m_edgeIsPlayback = true;
                    playAudioBytes(data);
                }
                return;
            }
        }
    }
    // 3) 未命中:拉取所选 edge 神经网络音色。
    //    选了神经网络音就坚决用它(不用系统机械音兜底),
    //    保证用户听到的就是所选音色;卡片显示时已预取,绝大多数情况
    //    点开即命中缓存。prefetchOnly=true 只拉取不播放。
    startEdgeTts(text, prefetchOnly);
}

QString MobileBridge::ttsVoice() const
{
    return m_store->getSetting(QStringLiteral("tts_voice"),
                               QStringLiteral("en-US-JennyNeural"));
}

void MobileBridge::setTtsVoice(const QString &voice)
{
    if (ttsVoice() == voice)
        return;
    m_store->setSetting(QStringLiteral("tts_voice"), voice);
    // 内存缓存是按当前音色的音频,换音色后作废,下次从该音色的磁盘缓存读
    m_speakCache.clear();
    emit ttsVoiceChanged();
}

QVariantList MobileBridge::ttsVoices() const
{
    // id: "system" 表示纯本地 TTS;其余为 edge-tts 神经网络音色。
    QVariantList list;
    auto add = [&](const QString &id, const QString &label,
                   const QString &desc) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), id);
        m.insert(QStringLiteral("label"), label);
        m.insert(QStringLiteral("desc"), desc);
        list.append(m);
    };
    add(QStringLiteral("system"),
        QStringLiteral("系统离线(极速)"),
        QStringLiteral("零延迟、离线,但声音偏机械"));
    add(QStringLiteral("en-US-JennyNeural"),
        QStringLiteral("Jenny · 美音女声"),
        QStringLiteral("自然亲切,推荐(默认)"));
    add(QStringLiteral("en-US-GuyNeural"),
        QStringLiteral("Guy · 美音男声"),
        QStringLiteral("沉稳自然"));
    add(QStringLiteral("en-US-AriaNeural"),
        QStringLiteral("Aria · 美音女声"),
        QStringLiteral("清亮、播报感"));
    add(QStringLiteral("en-GB-SoniaNeural"),
        QStringLiteral("Sonia · 英音女声"),
        QStringLiteral("英式发音"));
    add(QStringLiteral("en-US-AnaNeural"),
        QStringLiteral("Ana · 美音童声"),
        QStringLiteral("儿童音色"));
    return list;
}

QString MobileBridge::systemTtsEngine() const
{
#if defined(Q_OS_ANDROID)
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity",
        "()Landroid/app/Activity;");
    QJniObject context = activity.isValid() ? activity
        : QJniObject::callStaticObjectMethod(
            "android/app/ActivityThread", "currentApplication",
            "()Landroid/app/Application;");
    if (!context.isValid())
        return QString();
    QJniObject helper = QJniObject::callStaticObjectMethod(
        "org/liang/english3000/TtsHelper", "get",
        "(Landroid/content/Context;)Lorg/liang/english3000/TtsHelper;",
        context.object<jobject>());
    if (!helper.isValid())
        return QString();
    const QString pkg = helper.callObjectMethod(
        "defaultEngine", "()Ljava/lang/String;").toString();
    // 包名翻译成用户可读的厂商名
    static const QHash<QString, QString> names = {
        {QStringLiteral("com.xiaomi.mibrain.speech"), QStringLiteral("小米语音引擎")},
        {QStringLiteral("com.google.android.tts"), QStringLiteral("Google 语音服务")},
        {QStringLiteral("com.samsung.SMT"), QStringLiteral("三星 TTS")},
        {QStringLiteral("com.huawei.hiai"), QStringLiteral("华为 TTS")},
        {QStringLiteral("com.iflytek.speechcloud"), QStringLiteral("讯飞语音引擎")},
        {QStringLiteral("com.iflytek.tts"), QStringLiteral("讯飞 TTS")},
        {QStringLiteral("com.baidu.duersdk.opensdk"), QStringLiteral("百度语音引擎")},
        {QStringLiteral("com.aispeech.dui"), QStringLiteral("思必驰 TTS")},
    };
    auto it = names.constFind(pkg);
    if (it != names.constEnd())
        return it.value();
    return pkg; // 未知引擎直接显示包名
#else
    return QString();
#endif
}

void MobileBridge::prefetchSpeak(const QString &text)
{
#ifdef ENGLISH3000_HAS_TTS
#if defined(Q_OS_ANDROID)
    const QString t = text.trimmed();
    if (t.isEmpty() || t.size() > 200)
        return;
    const QString voice = ttsVoice();
    if (voice == QLatin1String("system"))
        return; // 纯本地 TTS 无需联网预取
    // 已在内存/磁盘缓存则无需联网
    if (m_speakCache.contains(t) || QFile::exists(ttsCachePath(t, voice)))
        return;
    // 后台拉取所选 edge 音色存盘(只缓存不播放)。
    startEdgeTts(t, true);
#else
    const QString t = text.trimmed();
    if (t.isEmpty() || t.size() > 200)
        return;
    if (m_speakCache.contains(t))
        return;
    playCachedOrFetch(t, true);
#endif
#endif
}

void MobileBridge::preloadCurrentListTts()
{
#ifdef ENGLISH3000_HAS_TTS
#if defined(Q_OS_ANDROID)
    if (m_preloading)
        return;
    if (ttsVoice() == QLatin1String("system"))
        return; // 系统音本就离线,无需下载
    const qint64 listId = m_store->currentWordListId();
    const QVector<Word> words = m_store->wordsInWordList(listId, 0, 0);
    if (words.isEmpty()) {
        emit ttsPreloadProgress(0, 0);
        return;
    }
    const QString voice = ttsVoice();
    // 先统计未缓存的文本:每个单词 + 它的例句(若有)。
    // 已缓存的跳过;单词和例句交替入队,保证整词表学起来都秒播。
    QStringList todo;
    auto needCache = [&](const QString &t) {
        if (t.isEmpty() || t.size() > 200)
            return;
        if (m_speakCache.contains(t)
            || QFile::exists(ttsCachePath(t, voice)))
            return;
        if (!todo.contains(t))
            todo.append(t);
    };
    for (const Word &w : words) {
        needCache(w.word.trimmed());
        if (!w.exampleSentence.isEmpty())
            needCache(w.exampleSentence.trimmed());
    }
    m_preloadQueue = todo;
    m_preloadTotal = todo.size();
    m_preloadDone = 0;
    m_preloadCancel = false;
    m_preloading = true;
    emit ttsPreloadProgress(0, m_preloadTotal);
    preloadNextWord();
#endif
#endif
}

void MobileBridge::cancelTtsPreload()
{
    m_preloadCancel = true;
}

QString MobileBridge::ttsPreloadEstimate()
{
#if defined(Q_OS_ANDROID) && defined(ENGLISH3000_HAS_TTS)
    if (ttsVoice() == QLatin1String("system"))
        return QString(); // 系统音本就离线,无需下载
    const qint64 listId = m_store->currentWordListId();
    const QVector<Word> words = m_store->wordsInWordList(listId, 0, 0);
    const QString voice = ttsVoice();
    qint64 bytes = 0;
    int items = 0;
    // edge-tts 输出 48kbps mono MP3:单词平均约 10KB;
    // 例句按字符数估算(约 550 字节/字符,整句至少 10KB)。
    auto addText = [&](const QString &t) {
        if (t.isEmpty() || t.size() > 200)
            return;
        if (m_speakCache.contains(t)
            || QFile::exists(ttsCachePath(t, voice)))
            return; // 已缓存,不计入
        // 词平均 10KB,长句按字符估;短于 20 字符的按词算
        qint64 est;
        if (t.size() <= 20)
            est = 10 * 1024;
        else
            est = qMax<qint64>(10 * 1024, qint64(t.size()) * 550);
        bytes += est;
        ++items;
    };
    for (const Word &w : words) {
        addText(w.word.trimmed());
        if (!w.exampleSentence.isEmpty())
            addText(w.exampleSentence.trimmed());
    }
    if (items == 0)
        return QStringLiteral("已全部缓存");
    if (bytes < 1024 * 1024)
        return QStringLiteral("约 %1 KB").arg((bytes + 512) / 1024);
    return QStringLiteral("约 %1 MB").arg(
        QString::number(double(bytes) / (1024.0 * 1024.0), 'f',
                        bytes < 10 * 1024 * 1024 ? 1 : 0));
#else
    return QString();
#endif
}

void MobileBridge::preloadNextWord()
{
#if defined(Q_OS_ANDROID) && defined(ENGLISH3000_HAS_TTS)
    if (m_preloadCancel) {
        m_preloading = false;
        m_preloadQueue.clear();
        m_preloadInFlight.clear();
        emit ttsPreloadProgress(m_preloadDone, m_preloadTotal);
        return;
    }
    if (m_preloadQueue.isEmpty()) {
        m_preloading = false;
        m_preloadInFlight.clear();
        emit ttsPreloadProgress(m_preloadDone, m_preloadTotal);
        return;
    }
    // edge 正忙(正常背单词的预取/播放)就稍后再试,不与之抢连接
    if (m_edgeWs) {
        QTimer::singleShot(150, this, &MobileBridge::preloadNextWord);
        return;
    }
    const QString word = m_preloadQueue.takeFirst();
    m_preloadInFlight = word;
    startEdgeTts(word, true);
#endif
}


#if defined(Q_OS_ANDROID)
void MobileBridge::nativeSpeak(const QString &text)
{
    // 直接调用 Android 原生 TextToSpeech,走系统本地引擎,
    // 离线、毫秒级。这是背单词"点了就响"的关键路径。
    //
    // Android TextToSpeech 必须在主线程操作;但本方法可能从 AI 线程
    // 的例句回调里被调用,因此统一切到主线程(对象所在线程)执行,
    // 避免跨线程 JNI 调用静默失败(例句不发音)。
    QMetaObject::invokeMethod(this, [this, text] {
        QJniObject activity = QJniObject::callStaticObjectMethod(
            "org/qtproject/qt/android/QtNative", "activity",
            "()Landroid/app/Activity;");
        QJniObject context = activity.isValid() ? activity
            : QJniObject::callStaticObjectMethod(
                "android/app/ActivityThread", "currentApplication",
                "()Landroid/app/Application;");
        if (!context.isValid())
            return;
        // 构造单例(首次调用会异步初始化系统 TTS 引擎)
        QJniObject helper = QJniObject::callStaticObjectMethod(
            "org/liang/english3000/TtsHelper", "get",
            "(Landroid/content/Context;)Lorg/liang/english3000/TtsHelper;",
            context.object<jobject>());
        if (!helper.isValid())
            return;
        if (text.isEmpty())
            return; // 仅预热(已通过 get() 触发引擎初始化)
        QJniObject jtext = QJniObject::fromString(text);
        helper.callMethod<void>("speak", "(Ljava/lang/String;)V",
                                jtext.object<jstring>());
    }, Qt::QueuedConnection);
}
#endif

void MobileBridge::kickPrefetchQueue()
{
    // 一个预取/播放刚结束,若队列里还有待预取的词,接着取下一个。
    // 已缓存或正在合成的跳过。
    while (!m_prefetchQueue.isEmpty()) {
        const QString t = m_prefetchQueue.takeFirst();
        if (m_speakCache.contains(t)
            || QFile::exists(ttsCachePath(t, ttsVoice())))
            continue;
        if (m_edgeWs && m_edgeText == t)
            return;
        startEdgeTts(t, true);
        return;
    }
}

void MobileBridge::startEdgeTts(const QString &text, bool prefetchOnly)
{
    // 同一个词正在合成中:
    //  - 正在预取,现在要播放 -> 直接升级为播放,复用进行中的连接
    //  - 正在播放,又来预取 -> 不打断,等它播完(结果也会进缓存)
    //  - 正在播放别的词,又来播放 -> 才中断旧的
    if (m_edgeWs && m_edgeText == text) {
        if (!prefetchOnly && m_edgePrefetchOnly)
            m_edgePrefetchOnly = false; // 升级为播放
        return;
    }
    if (prefetchOnly) {
        // 预取绝不打断正在进行的播放/预取:排队(最多保留 4 个)
        if (m_edgeWs) {
            while (m_prefetchQueue.size() >= 4)
                m_prefetchQueue.removeFirst();
            if (!m_prefetchQueue.contains(text))
                m_prefetchQueue.append(text);
            return;
        }
    } else {
        // 用户主动播放:当前词插队,但保留后面卡的预取,
        // 这样快速连切时后面的词仍会被预热。
        m_prefetchQueue.removeAll(text);
        m_prefetchQueue.prepend(text);
        if (m_edgeWs && m_edgeText == text) {
            m_edgePrefetchOnly = false; // 升级为播放
            return;
        }
    }

    m_edgePrefetchOnly = prefetchOnly;
    m_edgeText = text;
    const QString edgeVoice =
        ttsVoice() == QLatin1String("system")
            ? QStringLiteral("en-US-JennyNeural") : ttsVoice();
    if (m_edgeWs) {
        m_edgeWs->abort();
        m_edgeWs->deleteLater();
        m_edgeWs = nullptr;
    }
    m_edgeAudio.clear();

    const qint64 unixSecs = QDateTime::currentSecsSinceEpoch();
    const qint64 rounded = unixSecs - (unixSecs % 300); // 向下取整到 5 分钟
    const qint64 ticks = (rounded + 11644473600LL) * 10000000LL;
    const QByteArray digest = QCryptographicHash::hash(
        QByteArray::number(ticks)
            + QByteArrayLiteral("6A5AA1D4EAFF4E9FB37E23D68491D6F4"),
        QCryptographicHash::Sha256).toHex().toUpper();
    const QString gec = QString::fromLatin1(digest);
    const QString connId = QUuid::createUuid()
                               .toString(QUuid::WithoutBraces)
                               .remove(QLatin1Char('-'));
    const QString reqId = connId;

    QUrl url(QStringLiteral(
        "wss://speech.platform.bing.com/consumer/speech/"
        "synthesize/readaloud/edge/v1"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("TrustedClientToken"),
                   QStringLiteral("6A5AA1D4EAFF4E9FB37E23D68491D6F4"));
    q.addQueryItem(QStringLiteral("Sec-MS-GEC"), gec);
    q.addQueryItem(QStringLiteral("Sec-MS-GEC-Version"),
                   QStringLiteral("1-143.0.3650.75"));
    q.addQueryItem(QStringLiteral("ConnectionId"), connId);
    url.setQuery(q);

    QNetworkRequest request(url);
    request.setRawHeader(
        "User-Agent",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36 Edg/143.0.0.0");
    request.setRawHeader("Accept-Encoding", "gzip, deflate, br, zstd");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.9");
    request.setRawHeader("Pragma", "no-cache");
    request.setRawHeader("Cache-Control", "no-cache");
    request.setRawHeader(
        "Origin",
        "chrome-extension://jdiccldimpdaibmpdkjnbmckianbfold");
    const QString muid = QUuid::createUuid()
                             .toString(QUuid::WithoutBraces)
                             .remove(QLatin1Char('-'))
                             .toUpper();
    request.setRawHeader("Cookie", "muid=" + muid.toLatin1() + ";");

    auto *ws = new QWebSocket(QString(),
                              QWebSocketProtocol::VersionLatest, this);
    m_edgeWs = ws;
    connect(ws, &QWebSocket::connected, this,
            [this, ws, text, reqId, edgeVoice] {
                if (ws != m_edgeWs)
                    return;
                const QString ts =
                    QLocale(QLocale::English).toString(
                        QDateTime::currentDateTimeUtc(),
                        QStringLiteral("ddd MMM dd yyyy HH:mm:ss"))
                    + QStringLiteral(
                        " GMT+0000 (Coordinated Universal Time)");
                const QString config =
                    QStringLiteral(
                        "X-Timestamp:%1\r\n"
                        "Content-Type:application/json; charset=utf-8\r\n"
                        "Path:speech.config\r\n\r\n"
                        "{\"context\":{\"synthesis\":{\"audio\":{"
                        "\"metadataoptions\":{\"sentenceBoundaryEnabled\":"
                        "\"false\",\"wordBoundaryEnabled\":\"false\"},"
                        "\"outputFormat\":"
                        "\"audio-24khz-48kbitrate-mono-mp3\"}}}}\r\n")
                        .arg(ts);
                ws->sendTextMessage(config);

                QString escaped = text;
                escaped.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
                escaped.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
                escaped.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
                escaped.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
                escaped.replace(QLatin1Char('\''), QStringLiteral("&apos;"));
                const QString ssml =
                    QStringLiteral(
                        "X-RequestId:%1\r\n"
                        "Content-Type:application/ssml+xml\r\n"
                        "X-Timestamp:%2\r\n"
                        "Path:ssml\r\n\r\n"
                        "<speak version='1.0' "
                        "xmlns='http://www.w3.org/2001/10/synthesis' "
                        "xml:lang='en-US'>"
                        "<voice name='%4'>"
                        "<prosody pitch='+0Hz' rate='+0%' volume='+0%'>"
                        "%3</prosody></voice></speak>")
                        .arg(reqId, ts, escaped, edgeVoice);
                ws->sendTextMessage(ssml);
            });
    connect(ws, &QWebSocket::binaryMessageReceived, this,
            [this, ws](const QByteArray &message) {
                if (ws != m_edgeWs)
                    return;
                if (message.size() < 2)
                    return;
                const quint16 headerLen = static_cast<quint16>(
                    (static_cast<quint8>(message.at(0)) << 8)
                    | static_cast<quint8>(message.at(1)));
                if (2 + headerLen > message.size())
                    return;
                const QByteArray head = message.mid(2, headerLen);
                if (!head.contains("Path:audio"))
                    return;
                m_edgeAudio.append(message.mid(2 + headerLen));
            });
    connect(ws, &QWebSocket::textMessageReceived, this,
            [this, ws](const QString &message) {
                if (ws != m_edgeWs)
                    return;
                if (message.contains(QStringLiteral("Path:turn.end")))
                    edgePlayAudio();
            });
    connect(ws, &QWebSocket::errorOccurred, this,
            [this, ws](QAbstractSocket::SocketError) {
                if (ws != m_edgeWs)
                    return;
                qWarning("edge-tts: ws error: %s",
                         qPrintable(ws->errorString()));
                edgeFallback();
            });
    ws->open(request);
    startEdgeTimer();
}

void MobileBridge::edgePlayAudio()
{
    if (m_edgeTimer)
        m_edgeTimer->stop();
    if (m_edgeAudio.isEmpty()) {
        edgeFallback();
        return;
    }
    if (m_edgeWs) {
        m_edgeWs->close();
        m_edgeWs->deleteLater();
        m_edgeWs = nullptr;
    }
    // 持久缓存自然音色:内存 + 磁盘(按音色区分),跨启动复用
    if (m_edgeText.size() <= 200) {
        if (m_speakCache.size() < 200)
            m_speakCache.insert(m_edgeText, m_edgeAudio);
        saveTtsCacheLocal(m_edgeText, ttsVoice(), m_edgeAudio);
    }
    // 预取:只缓存不播放;主动请求:拿到音频立即播放(就是所选音色)
    if (m_edgePrefetchOnly) {
        m_edgeAudio.clear();
        // 若是批量预下载中的一个词,推进进度并取下一个
        if (m_preloading && m_preloadInFlight == m_edgeText) {
            m_preloadInFlight.clear();
            ++m_preloadDone;
            emit ttsPreloadProgress(m_preloadDone, m_preloadTotal);
            QTimer::singleShot(30, this, &MobileBridge::preloadNextWord);
            return;
        }
        kickPrefetchQueue();
        return;
    }
    playAudioBytes(m_edgeAudio);
    m_edgeAudio.clear();
    kickPrefetchQueue();
}

void MobileBridge::edgeFallback()
{
    qWarning("edge-tts: fallback to espeak");
    if (m_edgeTimer)
        m_edgeTimer->stop();
    if (m_edgeWs) {
        m_edgeWs->abort();
        m_edgeWs->deleteLater();
        m_edgeWs = nullptr;
    }
#if defined(Q_OS_ANDROID)
    // edge 失败时用内置 espeak 临时兜底朗读,但绝不写缓存
    // (避免把机械音固化,下次重试仍走 edge)
    if (!m_edgePrefetchOnly) {
        const QByteArray wav = synthEspeak(m_edgeText);
        playAudioBytes(wav);
    }
#endif
    m_edgeAudio.clear();
    if (m_preloading && m_preloadInFlight == m_edgeText) {
        m_preloadInFlight.clear();
        ++m_preloadDone;
        emit ttsPreloadProgress(m_preloadDone, m_preloadTotal);
        QTimer::singleShot(30, this, &MobileBridge::preloadNextWord);
        return;
    }
    kickPrefetchQueue();
}

void MobileBridge::startEdgeTimer()
{
    if (!m_edgeTimer) {
        m_edgeTimer = new QTimer(this);
        m_edgeTimer->setSingleShot(true);
        connect(m_edgeTimer, &QTimer::timeout, this,
                &MobileBridge::edgeFallback);
    }
    m_edgeTimer->start(30000);
}
#endif


void MobileBridge::onWordListFinished(const QString &rawText)
{
    const QVector<WordEntry> entries = parseWordEntries(rawText);
    const QString name = m_pendingListName;
    const qint64 listId = m_pendingListId;
    m_pendingListId = -1;
    if (entries.isEmpty()) {
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
        for (const WordEntry &e : entries) {
            if (existing.contains(e.word))
                continue;
            QString pos;
            QString meaning;
            if (e.pos.isEmpty() || e.meaning.isEmpty()) {
                const std::optional<Word> found =
                    m_store->findWordByText(e.word);
                if (found) {
                    pos = found->pos;
                    meaning = found->meaning;
                } else {
                    const std::optional<Word> dict =
                        m_store->lookupDict(e.word);
                    if (dict) {
                        pos = dict->pos;
                        meaning = dict->meaning;
                    }
                }
            }
            if (!e.pos.isEmpty())
                pos = e.pos;
            if (!e.meaning.isEmpty())
                meaning = e.meaning;
            if (m_store->addWordToList(listId, e.word, pos, meaning,
                                       order)) {
                if (!e.example.isEmpty())
                    m_store->setExampleSentence(e.word, e.example);
                existing.insert(e.word);
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
    for (int i = 0; i < entries.size(); ++i) {
        const WordEntry &e = entries.at(i);
        QString pos;
        QString meaning;
        if (e.pos.isEmpty() || e.meaning.isEmpty()) {
            const std::optional<Word> found =
                m_store->findWordByText(e.word);
            if (found) {
                pos = found->pos;
                meaning = found->meaning;
            } else {
                const std::optional<Word> dict =
                    m_store->lookupDict(e.word);
                if (dict) {
                    pos = dict->pos;
                    meaning = dict->meaning;
                }
            }
        }
        if (!e.pos.isEmpty())
            pos = e.pos;
        if (!e.meaning.isEmpty())
            meaning = e.meaning;
        m_store->addWordToList(newId, e.word, pos, meaning, i);
        if (!e.example.isEmpty())
            m_store->setExampleSentence(e.word, e.example);
    }
    emit wordListReady(name, entries.size());
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
    m_ai->generateWordList(d, qBound(50, count, 500));
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
    m_ai->generateWordList(d, qBound(50, count, 500));
}

void MobileBridge::aiGenerateArticle(const QString &topic, int wordCount,
                                     int level)
{
    const qint64 listId = m_store->currentWordListId();
    QString t = topic.trimmed();
    if (t.isEmpty())
        t = m_store->currentWordListName();
    if (t.isEmpty())
        t = QStringLiteral("英语学习");
    m_pendingArticleTitle = t;
    const int count = qBound(50, wordCount, 500);
    const int lvl = qBound(1, level, 3);
    if (listId > 0) {
        QStringList preferred;
        const QVector<Word> words = m_store->wordsInWordList(listId, 200);
        for (const Word &w : words)
            preferred << w.word;
        m_ai->generateArticle(t, lvl, count, preferred);
    } else {
        m_ai->generateArticle(t, lvl, count);
    }
}

void MobileBridge::aiCancel()
{
    m_ai->cancel();
}

void MobileBridge::chatOpen(const QString &title, const QString &content)
{
    m_chatTitle = title;
    m_chatContext = content.simplified();
    if (m_chatContext.size() > 2000)
        m_chatContext = m_chatContext.left(2000) + QStringLiteral("…");
    m_chatHistory.clear();
    m_pendingChat = true;
    m_ai->chat(chatBuildPrompt());
}

void MobileBridge::chatSend(const QString &message)
{
    const QString text = message.trimmed();
    if (text.isEmpty() || m_pendingChat)
        return;
    m_chatHistory << QStringLiteral("Student: %1").arg(text);
    m_pendingChat = true;
    m_ai->chat(chatBuildPrompt());
}

void MobileBridge::chatClear()
{
    m_chatHistory.clear();
    m_chatTitle.clear();
    m_chatContext.clear();
    m_pendingChat = false;
    m_ai->cancel();
}

QString MobileBridge::chatBuildPrompt() const
{
    QString prompt;
    if (m_chatContext.isEmpty()) {
        prompt = QStringLiteral(
            "You are an English conversation partner for a Chinese learner.\n"
            "Rules:\n"
            "- Ask ONE short English question at a time.\n"
            "- After the student answers, praise first, then gently correct "
            "any mistakes with the correct sentence, then ask a follow-up.\n"
            "- Keep every reply under 80 words.\n\n");
    } else {
        prompt = QStringLiteral(
            "You are an English conversation partner for a Chinese learner.\n"
            "We are discussing this article (title: %1):\n%2\n\n"
            "Rules:\n"
            "- Ask ONE short English question about the article at a time.\n"
            "- After the student answers, praise first, then gently correct "
            "any mistakes with the correct sentence, then ask a follow-up.\n"
            "- Keep every reply under 80 words.\n\n")
                     .arg(m_chatTitle, m_chatContext);
    }
    for (const QString &line : m_chatHistory)
        prompt += line + QLatin1Char('\n');
    prompt += QStringLiteral("AI:");
    return prompt;
}

void MobileBridge::importUrl(const QString &url)
{
    if (m_importReply)
        return;
    const QUrl u = QUrl::fromUserInput(url.trimmed());
    if (!u.isValid() || u.scheme().isEmpty()) {
        emit aiFailed(QStringLiteral("网址格式不正确"));
        return;
    }
    QNetworkRequest request(u);
    request.setTransferTimeout(20000);
    m_importReply = m_net->get(request);
    connect(m_importReply, &QNetworkReply::finished, this,
            &MobileBridge::onImportFinished);
}

void MobileBridge::onImportFinished()
{
    QNetworkReply *reply = m_importReply;
    m_importReply = nullptr;
    if (!reply)
        return;
    const QUrl url = reply->url();
    if (reply->error() != QNetworkReply::NoError) {
        emit aiFailed(QStringLiteral("导入失败：%1").arg(reply->errorString()));
        reply->deleteLater();
        return;
    }
    QString html = QString::fromUtf8(reply->readAll());
    reply->deleteLater();
    QString title = url.host();
    const QRegularExpression titleRe(
        QStringLiteral("<title[^>]*>(.*?)</title>"),
        QRegularExpression::CaseInsensitiveOption
            | QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch m = titleRe.match(html);
    if (m.hasMatch()) {
        QString t = m.captured(1).trimmed();
        t.remove(QRegularExpression(QStringLiteral("\\s+")));
        if (!t.isEmpty() && t.size() < 120)
            title = t;
    }
    // 粗略去标签，保留段落
    html.remove(QRegularExpression(
        QStringLiteral("<script[^>]*>.*?</script>"),
        QRegularExpression::CaseInsensitiveOption
            | QRegularExpression::DotMatchesEverythingOption));
    html.remove(QRegularExpression(
        QStringLiteral("<style[^>]*>.*?</style>"),
        QRegularExpression::CaseInsensitiveOption
            | QRegularExpression::DotMatchesEverythingOption));
    html.replace(QRegularExpression(QStringLiteral("<br\\s*/?>")),
                 QStringLiteral("\n"));
    html.replace(QRegularExpression(QStringLiteral("</p>")),
                 QStringLiteral("\n"));
    html.remove(QRegularExpression(QStringLiteral("<[^>]+>")));
    html.replace(QRegularExpression(QStringLiteral("&nbsp;")),
                 QStringLiteral(" "));
    html.replace(QRegularExpression(QStringLiteral("&amp;")),
                 QStringLiteral("&"));
    html.replace(QRegularExpression(QStringLiteral("&lt;")),
                 QStringLiteral("<"));
    html.replace(QRegularExpression(QStringLiteral("&gt;")),
                 QStringLiteral(">"));
    html = html.simplified();
    if (html.size() < 40) {
        emit aiFailed(QStringLiteral("导入失败：网页没有可用正文"));
        return;
    }
    const qint64 id = m_store->saveArticle(
        title, html, url.toString(), 0);
    emit articleImported(id, title);
}

void MobileBridge::importArticleFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit aiFailed(QStringLiteral("导入失败：无法读取文件"));
        return;
    }
    const QString content = QString::fromUtf8(file.readAll());
    file.close();
    if (content.simplified().size() < 40) {
        emit aiFailed(QStringLiteral("导入失败：文件内容太少"));
        return;
    }
    QFileInfo info(path);
    QString title = info.completeBaseName();
    if (title.isEmpty())
        title = QStringLiteral("导入文章");
    const qint64 id = m_store->saveArticle(
        title, content, QStringLiteral("file"), 0);
    emit articleImported(id, title);
}

void MobileBridge::deleteArticle(qint64 articleId)
{
    m_store->deleteArticle(articleId);
}

void MobileBridge::reimportBuiltin()
{
    m_store->importCsv(QStringLiteral(":/assets/oxford3000.csv"), false);
    m_store->importWordForms(QStringLiteral(":/assets/lemma.en.txt"));
    m_store->seedBuiltinWordList();
    m_store->seedExamplesFromArticles();
    m_store->importBuiltinExamples();
    m_store->seedWordPhonetics();
    reloadCounts();
}

void MobileBridge::resetAllProgress()
{
    m_store->resetAllLists();
    reloadCounts();
}

void MobileBridge::resetListItem(qint64 itemId)
{
    m_store->resetItem(itemId);
    reloadCounts();
}

void MobileBridge::aiProbe()
{
    if (m_probe)
        return;
    m_probe = new AiProbe(m_store, this);
    connect(m_probe, &AiProbe::finished, this,
            [this](const QString &provider, const QString &baseUrl,
                   const QString &model, const QString &label) {
                m_probe->deleteLater();
                m_probe = nullptr;
                const bool autoMode =
                    m_store->getSetting(QStringLiteral("ai_mode"),
                                        QStringLiteral("auto"))
                    == QLatin1String("auto");
                if (autoMode && !provider.isEmpty()) {
                    m_store->setSetting(QStringLiteral("ai_provider"),
                                        provider);
                    m_store->setSetting(QStringLiteral("ai_base_url"),
                                        baseUrl);
                    m_store->setSetting(QStringLiteral("ai_model"), model);
                    m_store->setSetting(QStringLiteral("ai_engine_label"),
                                        label);
                    m_ai->setEndpoint(baseUrl, model);
                    m_ai->setProvider(
                        provider == QLatin1String("openai")
                            ? AiClient::Provider::OpenAI
                            : AiClient::Provider::Ollama);
    reloadCounts();
                }
                emit aiProbeFinished(label);
            });
    m_probe->start();
}

void MobileBridge::testConnection()
{
    const QString key =
        m_store->getSetting(QStringLiteral("ai_api_key")).trimmed();
    QString base =
        m_store->getSetting(QStringLiteral("ai_base_url")).trimmed();
    const QString model =
        m_store->getSetting(QStringLiteral("ai_model")).trimmed();

    auto fail = [this](const QString &msg) {
        emit connectionTested(false, msg);
    };

    if (base.isEmpty() || model.isEmpty()) {
        fail(QStringLiteral("服务地址或模型名为空"));
        return;
    }
    if (!key.isEmpty()
        && !base.startsWith(QStringLiteral("http://127.0.0.1"))
        && !base.startsWith(QStringLiteral("http://localhost"))) {
        // 云端需要 Key
    }
    if (!base.endsWith(QLatin1Char('/')))
        base += QLatin1Char('/');
    QString chatUrl = base;
    if (chatUrl.endsWith(QLatin1String("/v1/")))
        chatUrl += QStringLiteral("chat/completions");
    else
        chatUrl += QStringLiteral("v1/chat/completions");

    QJsonObject body;
    body.insert(QStringLiteral("model"), model);
    QJsonArray messages;
    QJsonObject msg;
    msg.insert(QStringLiteral("role"), QStringLiteral("user"));
    msg.insert(QStringLiteral("content"), QStringLiteral("hi"));
    messages.append(msg);
    body.insert(QStringLiteral("messages"), messages);
    body.insert(QStringLiteral("max_tokens"), 5);

    QNetworkRequest request{QUrl(chatUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    if (!key.isEmpty())
        request.setRawHeader("Authorization",
                             "Bearer " + key.toUtf8());
    request.setTransferTimeout(8000);

    QNetworkReply *reply =
        m_net->post(request, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, model] {
        reply->deleteLater();
        const QNetworkReply::NetworkError err = reply->error();
        if (err == QNetworkReply::NoError) {
            emit connectionTested(
                true, QStringLiteral("连接成功，可用模型：%1").arg(model));
            return;
        }
        const int httpStatus =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                .toInt();
        QString detail = reply->errorString();
        // 尝试从响应体里提取服务端错误信息
        const QByteArray data = reply->readAll();
        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
        if (pe.error == QJsonParseError::NoError) {
            const QJsonObject obj = doc.object();
            const QJsonObject e =
                obj.value(QStringLiteral("error")).toObject();
            const QString srvMsg =
                e.value(QStringLiteral("message")).toString();
            if (!srvMsg.isEmpty())
                detail = srvMsg;
        }
        QString msg;
        if (httpStatus == 401 || httpStatus == 403)
            msg = QStringLiteral(
                "API Key 无效或无权限（%1），请检查后重新粘贴").arg(httpStatus);
        else if (httpStatus == 404)
            msg = QStringLiteral(
                "服务地址或模型名有误（404），请检查地址和模型名");
        else if (httpStatus == 429)
            msg = QStringLiteral("请求过于频繁或额度不足（429）");
        else if (err == QNetworkReply::TimeoutError)
            msg = QStringLiteral("连接超时，请检查网络或服务地址");
        else if (err == QNetworkReply::HostNotFoundError
                 || err == QNetworkReply::ConnectionRefusedError)
            msg = QStringLiteral("无法连接服务器，请检查网络或地址");
        else
            msg = QStringLiteral("连接失败（%1）：%2")
                      .arg(httpStatus > 0 ? QString::number(httpStatus)
                                          : QStringLiteral("网络"))
                      .arg(detail);
        emit connectionTested(false, msg);
    });
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
    reloadCounts();
}

void MobileBridge::setAiApiKey(const QString &key)
{
    m_store->setSetting(QStringLiteral("ai_api_key"), key.trimmed());
    m_ai->setApiKey(key.trimmed());
    reloadCounts();
}

QString MobileBridge::aiMode() const
{
    return m_store->getSetting(QStringLiteral("ai_mode"),
                               QStringLiteral("auto"));
}

void MobileBridge::setAiMode(const QString &mode)
{
    m_store->setSetting(QStringLiteral("ai_mode"), mode.trimmed());
    reloadCounts();
}

QString MobileBridge::aiPreset() const
{
    const QString base =
        m_store->getSetting(QStringLiteral("ai_base_url"));
    const QString provider =
        m_store->getSetting(QStringLiteral("ai_provider"));
    if (aiMode() == QLatin1String("auto"))
        return QStringLiteral("auto");
    if (base.contains(QStringLiteral("127.0.0.1:8080")))
        return QStringLiteral("local");
    if (provider == QLatin1String("ollama"))
        return QStringLiteral("ollama");
    if (base.contains(QStringLiteral("deepseek.com")))
        return QStringLiteral("deepseek");
    if (base.contains(QStringLiteral("dashscope")))
        return QStringLiteral("dashscope");
    if (base.contains(QStringLiteral("bigmodel.cn")))
        return QStringLiteral("glm");
    if (base.contains(QStringLiteral("moonshot.cn")))
        return QStringLiteral("moonshot");
    if (base.contains(QStringLiteral("openai.com")))
        return QStringLiteral("openai");
    return QStringLiteral("custom");
}

void MobileBridge::setAiPreset(const QString &preset)
{
    const QString key = preset.trimmed();
    if (key == QLatin1String("auto")) {
        m_store->setSetting(QStringLiteral("ai_mode"),
                            QStringLiteral("auto"));
        aiProbe();
    reloadCounts();
        return;
    }
    m_store->setSetting(QStringLiteral("ai_mode"),
                        QStringLiteral("manual"));
    QString base = aiUrl();
    QString model = aiModel();
    QString provider = aiProvider();
    if (key == QLatin1String("local")) {
        base = QStringLiteral("http://127.0.0.1:8080");
        model = QStringLiteral("qwen2.5:1.5b");
        provider = QStringLiteral("openai");
    } else if (key == QLatin1String("ollama")) {
        base = QStringLiteral("http://127.0.0.1:11434");
        provider = QStringLiteral("ollama");
    } else if (key == QLatin1String("deepseek")) {
        base = QStringLiteral("https://api.deepseek.com");
        model = QStringLiteral("deepseek-chat");
        provider = QStringLiteral("openai");
    } else if (key == QLatin1String("dashscope")) {
        base = QStringLiteral(
            "https://dashscope.aliyuncs.com/compatible-mode/v1");
        model = QStringLiteral("qwen-plus");
        provider = QStringLiteral("openai");
    } else if (key == QLatin1String("glm")) {
        base = QStringLiteral("https://open.bigmodel.cn/api/paas/v4");
        model = QStringLiteral("glm-4-flash");
        provider = QStringLiteral("openai");
    } else if (key == QLatin1String("moonshot")) {
        base = QStringLiteral("https://api.moonshot.cn/v1");
        model = QStringLiteral("moonshot-v1-8k");
        provider = QStringLiteral("openai");
    } else if (key == QLatin1String("openai")) {
        base = QStringLiteral("https://api.openai.com/v1");
        model = QStringLiteral("gpt-4o-mini");
        provider = QStringLiteral("openai");
    }
    m_store->setSetting(QStringLiteral("ai_base_url"), base);
    m_store->setSetting(QStringLiteral("ai_model"), model);
    m_store->setSetting(QStringLiteral("ai_provider"), provider);
    m_ai->setEndpoint(base, model);
    m_ai->setProvider(provider == QLatin1String("openai")
                          ? AiClient::Provider::OpenAI
                          : AiClient::Provider::Ollama);
    reloadCounts();
}

bool MobileBridge::guideSeen() const
{
    return m_store->getSetting(QStringLiteral("guide_seen"),
                               QStringLiteral("0"))
           == QLatin1String("1");
}

void MobileBridge::setGuideSeen(bool seen)
{
    m_store->setSetting(QStringLiteral("guide_seen"),
                        seen ? QStringLiteral("1")
                             : QStringLiteral("0"));
}

void MobileBridge::requestGuide()
{
    emit guideRequested();
}
