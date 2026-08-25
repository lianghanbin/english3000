#pragma once

#include <QObject>
#include <QHash>
#include <QQueue>
#include <QSet>
#include <QVariantList>

class AiClient;
class AiProbe;
class QAudioOutput;
class QMediaPlayer;
class QNetworkAccessManager;
class QNetworkReply;
class QTextToSpeech;
class QTemporaryFile;
class QTimer;
class QWebSocket;
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
    Q_PROPERTY(QString aiMode READ aiMode WRITE setAiMode NOTIFY countsChanged)
    Q_PROPERTY(bool dictReady READ dictReady NOTIFY dictReadyChanged)

public:
    MobileBridge(WordStore *store, AiClient *ai, QObject *parent = nullptr);

    int newCount() const;
    int dueCount() const;
    int masteredCount() const;
    int streak() const;
    bool dictReady() const;
    QString currentListName() const;
    QString aiProvider() const;
    QString aiApiKey() const;
    QString aiMode() const;
    void setAiMode(const QString &mode);

    Q_INVOKABLE QVariantList newCards(int limit);
    Q_INVOKABLE QVariantList reviewCards(int limit);
    Q_INVOKABLE QVariantList wordLists();
    Q_INVOKABLE QVariantList wordListRows(qint64 listId, int limit);
    Q_INVOKABLE QVariantList wordListPageRows(qint64 listId, int offset,
                                              int limit);
    Q_INVOKABLE QVariantMap wordInfo(const QString &word);
    Q_INVOKABLE qint64 currentListId() const;
    Q_INVOKABLE QString currentListTitle() const;
    Q_INVOKABLE void setCurrentList(qint64 listId);
    Q_INVOKABLE void deleteWordList(qint64 listId);
    Q_INVOKABLE void answer(qint64 wordId, bool known);
    Q_INVOKABLE void translate(const QString &text, const QString &model);
    Q_INVOKABLE void requestExample(qint64 wordId, const QString &word);
    Q_INVOKABLE void cancelExample();
    // 后台预取 AI 例句(串行,不显示),预热后面的卡片
    Q_INVOKABLE void prefetchExample(qint64 wordId, const QString &word);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantList articles();
    Q_INVOKABLE QString articleHtml(qint64 articleId);
    Q_INVOKABLE QString articleContent(qint64 articleId);
    Q_INVOKABLE QString highlightText(const QString &text);
    Q_INVOKABLE QString sentenceForArticle(qint64 articleId,
                                           const QString &word);
    Q_INVOKABLE void addReadingWord(const QString &word);
    Q_INVOKABLE void addToReadingList(const QString &word);
    Q_INVOKABLE void speak(const QString &text);
    // 后台预取短文本发音(不播放),供卡片显示时提前缓存
    Q_INVOKABLE void prefetchSpeak(const QString &text);
    // 发音音色:"system"=纯系统本地 TTS(零延迟、离线);
    // 其余为 edge-tts 神经网络音色名(自然、需联网首次缓存)
    Q_INVOKABLE QString ttsVoice() const;
    Q_INVOKABLE void setTtsVoice(const QString &voice);
    Q_INVOKABLE QVariantList ttsVoices() const; // 可选音色列表
    Q_INVOKABLE QString systemTtsEngine() const; // 当前系统 TTS 引擎名(可读)
    // 预下载当前词表所有单词的神经网络发音到磁盘缓存(离线可用)。
    // 进度通过 ttsPreloadProgress 信号上报;cancelTtsPreload 可中断。
    Q_INVOKABLE void preloadCurrentListTts();
    Q_INVOKABLE void cancelTtsPreload();
    // 估算当前词表尚未缓存的发音总大小,返回可读字符串如"约 75 MB"
    Q_INVOKABLE QString ttsPreloadEstimate();
    Q_INVOKABLE QVariantList coverageHistory(int days);
    Q_INVOKABLE void aiGenerateWordList(const QString &domain, int count);
    Q_INVOKABLE void aiSupplementWordList(const QString &domain, int count);
    Q_INVOKABLE void aiGenerateArticle(const QString &topic,
                                       int wordCount = 300,
                                       int level = 1);
    Q_INVOKABLE void aiCancel();
    Q_INVOKABLE void chatOpen(const QString &title, const QString &content);
    Q_INVOKABLE void chatSend(const QString &message);
    Q_INVOKABLE void chatClear();
    Q_INVOKABLE void importUrl(const QString &url);
    Q_INVOKABLE void importArticleFile(const QString &path);
    Q_INVOKABLE void deleteArticle(qint64 articleId);
    Q_INVOKABLE void reimportBuiltin();
    Q_INVOKABLE void resetAllProgress();
    Q_INVOKABLE void resetListItem(qint64 itemId);
    Q_INVOKABLE void aiProbe();
    Q_INVOKABLE void testConnection();
    Q_INVOKABLE void notifyDictReady();
    Q_INVOKABLE void openUrl(const QString &url);
    Q_INVOKABLE QString clipboardText() const;

    Q_INVOKABLE QString aiUrl() const;
    Q_INVOKABLE void setAiUrl(const QString &url);
    Q_INVOKABLE QString aiModel() const;
    Q_INVOKABLE void setAiModel(const QString &model);
    Q_INVOKABLE void setAiProvider(const QString &provider);
    Q_INVOKABLE void setAiApiKey(const QString &key);
    Q_INVOKABLE QString aiPreset() const;
    Q_INVOKABLE void setAiPreset(const QString &preset);
    // 首次引导:是否已看过、标记为已看
    Q_INVOKABLE bool guideSeen() const;
    Q_INVOKABLE void setGuideSeen(bool seen);
    // 请求重新播放引导(设置页按钮调用,main.qml 监听)
    Q_INVOKABLE void requestGuide();

signals:
    void countsChanged();
    void listChanged();
    // 只切换当前词表(未增删词表),界面只需换右侧数据,不用重建左侧列表
    void currentListChanged();
    void translationReady(const QString &translation);
    void translationFailed(const QString &message);
    void exampleReady(qint64 wordId, const QString &sentence);
    void exampleFailed(qint64 wordId, const QString &message);
    void wordListReady(const QString &name, int count);
    void dictReadyChanged();
    void articleReady(qint64 articleId, const QString &title);
    void articleImported(qint64 articleId, const QString &title);
    void chatReady(const QString &text);
    void chatFailed(const QString &message);
    void aiProbeFinished(const QString &label);
    void connectionTested(bool ok, const QString &message);
    void aiFailed(const QString &message);
    // 发音音色改变,QML 据此刷新选中态
    void ttsVoiceChanged();
    // 预下载进度:done/总数(0/0 表示空闲)
    void ttsPreloadProgress(int done, int total);
    // 请求重新播放引导
    void guideRequested();

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
    bool m_pendingTranslate = false;
    QSet<qint64> m_requestedExampleIds;
    QHash<qint64, QString> m_exampleCache;
    QQueue<QPair<qint64, QString>> m_examplePrefetch;
    bool m_exampleBusy = false;
    void kickExamplePrefetch();
    QString m_lastTranslateSource;
    qint64 m_currentArticleId = -1;
    QString m_currentArticleContent;
    QString m_pendingListName;
    qint64 m_pendingListId = -1;
    QString m_pendingArticleTitle;
    QString m_chatTitle;
    QString m_chatContext;
    QStringList m_chatHistory;
    bool m_pendingChat = false;

    // 启动时取一次学习计数,避免 QML 绑定反复触发 COUNT(*) 查询;
    // 所有改动词表进度的操作后调用 reloadCounts() 刷新并通知界面。
    int m_newCount = 0;
    int m_dueCount = 0;
    int m_masteredCount = 0;
    int m_streak = 0;
    QString m_currentListName;
    void reloadCounts();
#ifdef ENGLISH3000_HAS_TTS
    // edge-tts 自然音色的音频缓存(内存 + 磁盘)。
    // 命中则毫秒级播放;未命中则先用系统本地 TTS 即时朗读保证零延迟,
    // 同时后台拉取 edge-tts 存盘,下次该词即为自然音色。
    QHash<QString, QByteArray> m_speakCache;
    QString m_nativeSpeaking; // 系统 TTS 正在朗读的文本(避免被 edge 回放抢占)
    bool m_edgeIsPlayback = false; // 当前 edge 合成是缓存回放(非预取)
    void playCachedOrFetch(const QString &text, bool prefetchOnly);
    QTextToSpeech *m_tts = nullptr;
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOut = nullptr;
    QTemporaryFile *m_ttsFile = nullptr;
    QWebSocket *m_edgeWs = nullptr;
    QTimer *m_edgeTimer = nullptr;
    QByteArray m_edgeAudio;
    QString m_edgeText;
    bool m_edgePrefetchOnly = false;
    QStringList m_prefetchQueue; // 等待串行预取的文本(最多 4 个)
    void kickPrefetchQueue();
    // 批量预下载当前词表发音
    bool m_preloading = false;
    bool m_preloadCancel = false;
    int m_preloadDone = 0;
    int m_preloadTotal = 0;
    QStringList m_preloadQueue;
    QString m_preloadInFlight;
    void preloadNextWord();
#if defined(Q_OS_ANDROID)
    void nativeSpeak(const QString &text);
#endif
    void speakEdgeOrFallback(const QString &text);
    void startEdgeTts(const QString &text, bool prefetchOnly = false);
    void edgePlayAudio();
    void edgeFallback();
    void startEdgeTimer();
    void playAudioBytes(const QByteArray &audio);
#endif
};
