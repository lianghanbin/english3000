#pragma once

#include <QDate>
#include <QList>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

#include <optional>

class QSqlDatabase;

struct Word {
    qint64 id = 0;
    int rank = 0;
    QString word;
    QString pos;
    QString meaning;
    QString phonetic;
    int box = 0;
    QDate due;
    bool hasDue = false;
    int reviewCount = 0;
    int correctCount = 0;
    int wrongCount = 0;
    QString exampleSentence;
    int queuePriority = 0;
};

struct Counts {
    int total = 0;
    int newTotal = 0;
    int known = 0;
    int learning = 0;
    int mastered = 0;
    int due = 0;
};

struct DailySummary {
    int newCount = 0;
    int reviewCount = 0;
    int correct = 0;
    int wrong = 0;
};

struct ReviewResult {
    int box = 0;
    QDate due;
    bool known = false;
    bool wasNew = false;
};

struct Article {
    qint64 id = 0;
    QString title;
    QString content;
    QString source;
    int difficulty = 0;
    QString createdAt;
};

struct ArticleStats {
    int total = 0;        // 总词数（按词元计）
    int known = 0;        // 已掌握/学习中
    int inList = 0;       // 在 3000 词表内
    int outOfList = 0;    // 词表外
    double coverage = 0.0; // 已知覆盖率 0~100
};

struct PoolRow {
    qint64 id = 0;
    QString word;
    QString meaning;
    QString articleTitle;
    QString sentence;
    QString addedAt;
};

struct CoveragePoint {
    QString date;
    double coverage = 0.0;
    int articles = 0;
};

struct WordListInfo {
    qint64 id = 0;
    QString name;
    QString description;
    QString source;
    int wordCount = 0;
};

class WordStore {
public:
    explicit WordStore(const QString &dbPath);
    ~WordStore();

    WordStore(const WordStore &) = delete;
    WordStore &operator=(const WordStore &) = delete;

    // 词库
    int importCsv(const QString &csvPath, bool reset);
    int countWords() const;
    Counts counts(const QDate &day = QDate::currentDate()) const;
    QVector<Word> getNew(int limit) const;
    QVector<Word> getDue(int limit, const QDate &day = QDate::currentDate()) const;
    std::optional<Word> getWord(qint64 id) const;
    QVector<Word> search(const QString &query, int limit = 300) const;
    qint64 addWord(const QString &word, const QString &pos, const QString &meaning);
    std::optional<Word> findWordByText(const QString &word) const;

    // 文章与阅读
    qint64 saveArticle(const QString &title, const QString &content,
                       const QString &source, int difficulty = 0);
    QVector<Article> listArticles() const;
    std::optional<Article> getArticle(qint64 articleId) const;
    void deleteArticle(qint64 articleId);
    ArticleStats articleStats(qint64 articleId) const;
    static QStringList tokenizeWords(const QString &text);

    // 翻译联动
    QVector<Word> extractUnknownWords(const QString &text, int limit = 20) const;
    qint64 queueWordFromTranslation(const QString &word,
                                    const QString &meaning,
                                    const QString &sentence);
    static QString sentenceContaining(const QString &text,
                                      const QString &word,
                                      int maxLen = 180);

    // 领域词表
    qint64 createWordList(const QString &name, const QString &description,
                          const QString &source);
    bool deleteWordList(qint64 listId);
    QVector<WordListInfo> listWordLists(const QString &search = {}) const;
    bool addWordToList(qint64 listId, const QString &word,
                       const QString &pos, const QString &meaning,
                       int order);
    QVector<Word> wordsInWordList(qint64 listId) const;
    void setCurrentWordList(qint64 listId);
    qint64 currentWordListId() const;
    QString currentWordListName() const;
    QVector<Word> extractDomainWords(const QString &text,
                                     int limit = 100) const;
    QVector<Word> extractDomainWordsFromArticles(
        const QVector<qint64> &articleIds, int limit = 100) const;
    int queueWordListToToday(qint64 listId);
    int seedBuiltinWordList();
    int seedExamplesFromArticles();
    void setExampleSentence(qint64 wordId, const QString &sentence);
    void seedWordPhonetics();
    int knownInWordList(qint64 listId) const;

    // 词形归一化（file→file, files→file）
    int importWordForms(const QString &path);
    QString lookupLemma(const QString &form) const;

    // 离线词典（ECDICT）
    bool dictReady() const;
    int importDictCsv(const QString &path);
    std::optional<Word> lookupDict(const QString &word) const;

    // 生词池
    bool addToPool(qint64 articleId, const QString &word,
                   const QString &sentence,
                   const QString &meaning = QString());
    QVector<PoolRow> poolList() const;
    bool removeFromPool(qint64 poolId);
    bool setPoolMeaning(qint64 poolId, const QString &meaning);
    bool promoteFromPool(qint64 poolId,
                         const QDate &day = QDate::currentDate());

    // 阅读统计
    void logCoverage(qint64 articleId,
                     const QDate &day = QDate::currentDate());
    QVector<CoveragePoint> coverageHistory(int days = 30) const;
    int coverageArticleCount() const;

    // 复习调度
    ReviewResult review(qint64 wordId, bool known,
                        const QDate &day = QDate::currentDate());
    void markKnown(qint64 wordId, const QDate &day = QDate::currentDate());
    void resetWord(qint64 wordId);
    void resetAll();

    // 设置与统计
    void setSetting(const QString &key, const QString &value);
    QString getSetting(const QString &key, const QString &defaultValue = {}) const;
    DailySummary dailySummary(const QDate &day = QDate::currentDate()) const;
    int streak(const QDate &day = QDate::currentDate()) const;

    QString dbPath() const { return m_dbPath; }

    // 供测试使用：直接执行 SQL
    QSqlQuery rawQuery(const QString &sql, const QVariantList &args = {}) const;

private:
    void execStatements(const QStringList &statements);
    void ensureColumn(const QString &table, const QString &column,
                      const QString &columnDdl);
    void logDaily(const QDate &day, bool isNew, bool known);
    Word rowToWord(const QSqlQuery &query) const;

    QString m_dbPath;
    QSqlDatabase m_db;
    QSqlDatabase m_dict;
};

// CSV 解析（支持引号包裹的逗号与双引号转义）
QVector<QStringList> parseCsv(const QString &text);
