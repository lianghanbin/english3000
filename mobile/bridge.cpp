#include "bridge.h"

#include "ai_probe.h"
#include "ai_client.h"
#include "core.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

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
                    emit countsChanged();
                }
                emit translationReady(t);
            });
    connect(m_ai, &AiClient::failed, this,
            [this](const QString &m) {
                // 例句请求失败也要释放挂起标记,否则以后不会再请求例句
                m_pendingExampleId = -1;
                if (m_pendingChat) {
                    m_pendingChat = false;
                    emit chatFailed(m);
                }
                emit translationFailed(m);
                emit aiFailed(m);
            });
    connect(m_ai, &AiClient::chatFinished, this,
            [this](const QString &text) {
                if (m_pendingExampleId > 0) {
                    const qint64 id = m_pendingExampleId;
                    m_pendingExampleId = -1;
                    emit exampleReady(id, text.trimmed().simplified());
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

bool MobileBridge::dictReady() const
{
    return m_store->dictReady();
}

void MobileBridge::notifyDictReady()
{
    emit dictReadyChanged();
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
        std::optional<Word> dict = m_store->lookupDict(w.word);
        if (!dict && !w.word.isEmpty()) {
            const QString lemma = m_store->lookupLemma(w.word);
            if (!lemma.isEmpty())
                dict = m_store->lookupDict(lemma);
        }
        if (dict && !dict->meaning.isEmpty()) {
            r.insert(QStringLiteral("pos"), dict->pos);
            r.insert(QStringLiteral("meaning"), dict->meaning);
        }
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
    emit listChanged();
    emit countsChanged();
}

void MobileBridge::deleteWordList(qint64 listId)
{
    m_store->deleteWordList(listId);
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
    // 用当前配置的模型(不要写死小模型名,DeepSeek 等云端不认识它)
    m_ai->chat(prompt, 120, m_ai->model());
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
    emit countsChanged();
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
    emit countsChanged();
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
        // Waydroid 的安卓 TTS/音频栈不可靠:
        // 让本机中继用 Piper 合成并在电脑上播放(/play 接口)。
        const QUrl url(
            QStringLiteral("http://127.0.0.1:8099/play?text=")
            + QString::fromLatin1(QUrl::toPercentEncoding(t)));
        m_net->get(QNetworkRequest(url));
        return;
    }
    // 真机: 优先 Edge TTS 神经网络语音(自然、免费、无需Key),
    // 失败/断网时自动回退内置 espeak-ng,保证离线也有声音。
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

void MobileBridge::startEdgeTts(const QString &text)
{
    qDebug("edge-tts: start len=%d", int(text.size()));
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
            [this, ws, text, reqId] {
                if (ws != m_edgeWs)
                    return;
                qDebug("edge-tts: connected");
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
                        "<voice name='en-US-JennyNeural'>"
                        "<prosody pitch='+0Hz' rate='+0%' volume='+0%'>"
                        "%3</prosody></voice></speak>")
                        .arg(reqId, ts, escaped);
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
                qDebug("edge-tts: audio chunk += %d",
                       int(message.size() - 2 - headerLen));
            });
    connect(ws, &QWebSocket::textMessageReceived, this,
            [this, ws](const QString &message) {
                if (ws != m_edgeWs)
                    return;
                qDebug("edge-tts: text path=%s",
                       qPrintable(message.left(120).trimmed()));
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
    qDebug("edge-tts: play audio bytes=%d", int(m_edgeAudio.size()));
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
    playAudioBytes(m_edgeAudio);
    m_edgeAudio.clear();
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
    playAudioBytes(synthEspeak(m_edgeText));
#endif
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
    m_store->seedWordPhonetics();
    emit countsChanged();
}

void MobileBridge::resetAllProgress()
{
    m_store->resetAllLists();
    emit countsChanged();
}

void MobileBridge::resetListItem(qint64 itemId)
{
    m_store->resetItem(itemId);
    emit countsChanged();
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
                    emit countsChanged();
                }
                emit aiProbeFinished(label);
            });
    m_probe->start();
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

QString MobileBridge::aiMode() const
{
    return m_store->getSetting(QStringLiteral("ai_mode"),
                               QStringLiteral("auto"));
}

void MobileBridge::setAiMode(const QString &mode)
{
    m_store->setSetting(QStringLiteral("ai_mode"), mode.trimmed());
    emit countsChanged();
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
        emit countsChanged();
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
    emit countsChanged();
}
