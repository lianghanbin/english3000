#include "core.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QPair>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QTextStream>
#include <QVariant>
#include <QDir>

#include <optional>

#include <algorithm>

namespace {

const QStringList kSchema = {
    QStringLiteral(
        "CREATE TABLE IF NOT EXISTS words ("
        "id INTEGER PRIMARY KEY,"
        "rank INTEGER,"
        "word TEXT NOT NULL UNIQUE,"
        "pos TEXT NOT NULL DEFAULT '',"
        "meaning TEXT NOT NULL DEFAULT '',"
        "box INTEGER NOT NULL DEFAULT 0,"
        "due TEXT,"
        "review_count INTEGER NOT NULL DEFAULT 0,"
        "correct_count INTEGER NOT NULL DEFAULT 0,"
        "wrong_count INTEGER NOT NULL DEFAULT 0,"
        "example_sentence TEXT NOT NULL DEFAULT '',"
        "queue_priority INTEGER NOT NULL DEFAULT 0)"),
    QStringLiteral(
        "CREATE TABLE IF NOT EXISTS settings ("
        "key TEXT PRIMARY KEY,"
        "value TEXT NOT NULL)"),
    QStringLiteral(
        "CREATE TABLE IF NOT EXISTS daily_log ("
        "date TEXT PRIMARY KEY,"
        "new_count INTEGER NOT NULL DEFAULT 0,"
        "review_count INTEGER NOT NULL DEFAULT 0,"
        "correct INTEGER NOT NULL DEFAULT 0,"
        "wrong INTEGER NOT NULL DEFAULT 0)"),
    QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_words_box_due ON words(box, due)"),
    QStringLiteral(
        "CREATE TABLE IF NOT EXISTS articles ("
        "id INTEGER PRIMARY KEY,"
        "title TEXT NOT NULL,"
        "content TEXT NOT NULL,"
        "source TEXT NOT NULL DEFAULT 'import',"
        "difficulty INTEGER NOT NULL DEFAULT 0,"
        "created_at TEXT NOT NULL)"),
    QStringLiteral(
        "CREATE TABLE IF NOT EXISTS article_words ("
        "id INTEGER PRIMARY KEY,"
        "article_id INTEGER NOT NULL,"
        "word TEXT NOT NULL,"
        "sentence TEXT NOT NULL DEFAULT '',"
        "meaning TEXT NOT NULL DEFAULT '',"
        "added_at TEXT NOT NULL,"
        "UNIQUE(article_id, word))"),
    QStringLiteral(
        "CREATE TABLE IF NOT EXISTS word_forms ("
        "form TEXT PRIMARY KEY,"
        "lemma TEXT NOT NULL)"),
    QStringLiteral(
        "CREATE TABLE IF NOT EXISTS coverage_log ("
        "date TEXT NOT NULL,"
        "article_id INTEGER NOT NULL,"
        "total INTEGER NOT NULL DEFAULT 0,"
        "known INTEGER NOT NULL DEFAULT 0,"
        "in_list INTEGER NOT NULL DEFAULT 0,"
        "out_of_list INTEGER NOT NULL DEFAULT 0,"
        "coverage REAL NOT NULL DEFAULT 0,"
        "PRIMARY KEY(date, article_id))"),
    QStringLiteral(
        "CREATE TABLE IF NOT EXISTS word_lists ("
        "id INTEGER PRIMARY KEY,"
        "name TEXT NOT NULL UNIQUE,"
        "description TEXT NOT NULL DEFAULT '',"
        "source TEXT NOT NULL DEFAULT 'manual',"
        "sort_order INTEGER NOT NULL DEFAULT 0,"
        "created_at TEXT NOT NULL)"),
    QStringLiteral(
        "CREATE TABLE IF NOT EXISTS word_list_items ("
        "id INTEGER PRIMARY KEY,"
        "list_id INTEGER NOT NULL,"
        "word TEXT NOT NULL,"
        "pos TEXT NOT NULL DEFAULT '',"
        "meaning TEXT NOT NULL DEFAULT '',"
        "sort_order INTEGER NOT NULL DEFAULT 0,"
        "box INTEGER NOT NULL DEFAULT 0,"
        "review_count INTEGER NOT NULL DEFAULT 0,"
        "UNIQUE(list_id, word))"),
};

const QSet<QString> kStopwords = {
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
};

QString dueSql(const QDate &day)
{
    return day.toString(Qt::ISODate);
}

Word wordFromQuery(const QSqlQuery &query)
{
    Word w;
    w.id = query.value(0).toLongLong();
    w.rank = query.value(1).toInt();
    w.word = query.value(2).toString();
    w.pos = query.value(3).toString();
    w.meaning = query.value(4).toString();
    w.box = query.value(5).toInt();
    const QString due = query.value(6).toString();
    w.hasDue = !due.isEmpty();
    w.due = QDate::fromString(due, Qt::ISODate);
    w.reviewCount = query.value(7).toInt();
    w.correctCount = query.value(8).toInt();
    w.wrongCount = query.value(9).toInt();
    w.exampleSentence = query.value(10).toString();
    w.queuePriority = query.value(11).toInt();
    w.phonetic = query.value(12).toString();
    return w;
}

} // namespace

WordStore::WordStore(const QString &dbPath)
    : m_dbPath(dbPath)
{
    QDir().mkpath(QFileInfo(dbPath).absolutePath());
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                     QStringLiteral("english3000"));
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        qFatal("无法打开数据库 %s: %s", qPrintable(dbPath),
               qPrintable(m_db.lastError().text()));
    }
    execStatements(kSchema);
    m_dict = QSqlDatabase::addDatabase(
        QStringLiteral("QSQLITE"), QStringLiteral("english3000_dict"));
    m_dict.setDatabaseName(
        QFileInfo(dbPath).absolutePath() + QStringLiteral("/dict.db"));
    if (m_dict.open()) {
        QSqlQuery q(m_dict);
        q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS dict ("
            "word TEXT PRIMARY KEY,"
            "pos TEXT NOT NULL DEFAULT '',"
            "translation TEXT NOT NULL DEFAULT '')"));
    }
    ensureColumn(QStringLiteral("words"), QStringLiteral("example_sentence"),
                 QStringLiteral("TEXT NOT NULL DEFAULT ''"));
    ensureColumn(QStringLiteral("words"), QStringLiteral("queue_priority"),
                 QStringLiteral("INTEGER NOT NULL DEFAULT 0"));
    ensureColumn(QStringLiteral("words"), QStringLiteral("phonetic"),
                 QStringLiteral("TEXT NOT NULL DEFAULT ''"));
    ensureColumn(QStringLiteral("word_list_items"), QStringLiteral("box"),
                 QStringLiteral("INTEGER NOT NULL DEFAULT 0"));
    ensureColumn(QStringLiteral("word_list_items"),
                 QStringLiteral("review_count"),
                 QStringLiteral("INTEGER NOT NULL DEFAULT 0"));
    ensureColumn(QStringLiteral("word_lists"), QStringLiteral("sort_order"),
                 QStringLiteral("INTEGER NOT NULL DEFAULT 0"));
    // 一次性迁移：旧主库已掌握的词，带入词表条目
    if (getSetting(QStringLiteral("list_box_migrated"),
                   QStringLiteral("0"))
        != QLatin1String("1")) {
        QSqlQuery migrate(m_db);
        migrate.exec(QStringLiteral(
            "UPDATE word_list_items SET box=6 WHERE word IN "
            "(SELECT word FROM words WHERE box=6)"));
        setSetting(QStringLiteral("list_box_migrated"),
                   QStringLiteral("1"));
    }
    // 词表排序：默认按 id，核心 3000 置顶
    if (getSetting(QStringLiteral("list_order_migrated"),
                   QStringLiteral("0"))
        != QLatin1String("1")) {
        QSqlQuery order(m_db);
        order.exec(QStringLiteral(
            "UPDATE word_lists SET sort_order=id"));
        QSqlQuery top(m_db);
        top.exec(QStringLiteral(
            "UPDATE word_lists SET sort_order=0 "
            "WHERE name='核心 3000'"));
        setSetting(QStringLiteral("list_order_migrated"),
                   QStringLiteral("1"));
    }
    if (m_dict.isOpen()) {
        QSqlQuery pragma(m_dict);
        pragma.exec(QStringLiteral("PRAGMA table_info(dict)"));
        bool hasPhonetic = false;
        while (pragma.next()) {
            if (pragma.value(1).toString() == QStringLiteral("phonetic"))
                hasPhonetic = true;
        }
        if (!hasPhonetic) {
            QSqlQuery alter(m_dict);
            alter.exec(QStringLiteral(
                "ALTER TABLE dict ADD COLUMN phonetic TEXT NOT NULL DEFAULT ''"));
        }
    }
    const QList<QPair<QString, QString>> defaults = {
        {QStringLiteral("daily_new"), QStringLiteral("25")},
        {QStringLiteral("ai_base_url"),
         QStringLiteral("http://127.0.0.1:11434")},
        {QStringLiteral("ai_model"), QStringLiteral("qwen2.5:1.5b")},
        {QStringLiteral("translate_enabled"), QStringLiteral("1")},
        {QStringLiteral("translate_hotkey"),
         QStringLiteral("Ctrl+Alt+T")},
        {QStringLiteral("translate_screenshot_hotkey"),
         QStringLiteral("Ctrl+Alt+O")},
        {QStringLiteral("translate_default_model"),
         QStringLiteral("qwen2.5:1.5b")},
        {QStringLiteral("ai_mode"), QStringLiteral("auto")},
        {QStringLiteral("current_word_list"), QStringLiteral("-1")},
        {QStringLiteral("auto_pronounce"), QStringLiteral("1")},
    };
    for (const auto &pair : defaults) {
        if (getSetting(pair.first).isEmpty())
            setSetting(pair.first, pair.second);
    }
}

WordStore::~WordStore()
{
    const QString conn = m_db.connectionName();
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(conn);
    const QString dictConn = m_dict.connectionName();
    if (!dictConn.isEmpty()) {
        m_dict.close();
        m_dict = QSqlDatabase();
        QSqlDatabase::removeDatabase(dictConn);
    }
}

void WordStore::execStatements(const QStringList &statements)
{
    for (const QString &sql : statements) {
        QSqlQuery query(m_db);
        if (!query.exec(sql)) {
            qWarning("SQL 执行失败: %s\n%s", qPrintable(sql),
                     qPrintable(query.lastError().text()));
        }
    }
}

void WordStore::ensureColumn(const QString &table, const QString &column,
                             const QString &columnDdl)
{
    QSqlQuery q = rawQuery(
        QStringLiteral("PRAGMA table_info(%1)").arg(table));
    while (q.next()) {
        if (q.value(1).toString() == column)
            return;
    }
    QSqlQuery alter(m_db);
    alter.exec(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3")
                   .arg(table, column, columnDdl));
}

QSqlQuery WordStore::rawQuery(const QString &sql, const QVariantList &args) const
{
    QSqlQuery query(m_db);
    query.prepare(sql);
    for (const QVariant &arg : args)
        query.addBindValue(arg);
    if (!query.exec())
        qWarning("SQL 执行失败: %s\n%s", qPrintable(sql),
                 qPrintable(query.lastError().text()));
    return query;
}

// ---------- 词库 ----------

int WordStore::importCsv(const QString &csvPath, bool reset)
{
    QFile file(csvPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return -1;
    const QVector<QStringList> rows = parseCsv(QString::fromUtf8(file.readAll()));
    file.close();

    m_db.transaction();
    if (reset) {
        QSqlQuery q(m_db);
        q.exec(QStringLiteral("DELETE FROM words"));
    }
    int count = 0;
    QSqlQuery insert(m_db);
    insert.prepare(
        QStringLiteral(
            "INSERT INTO words(rank, word, pos, meaning) VALUES(?, ?, ?, ?) "
            "ON CONFLICT(word) DO UPDATE SET "
            "rank=excluded.rank, pos=excluded.pos, meaning=excluded.meaning"));
    for (const QStringList &row : rows) {
        if (row.size() < 4)
            continue;
        if (row[0].trimmed() == QStringLiteral("序号")
            || row[1].trimmed() == QStringLiteral("单词")) {
            continue;
        }
        bool ok = false;
        const int rank = row[0].trimmed().toInt(&ok);
        const QString word = row[1].trimmed();
        if (word.isEmpty())
            continue;
        insert.bindValue(0, ok ? QVariant(rank) : QVariant());
        insert.bindValue(1, word);
        insert.bindValue(2, row[2].trimmed());
        insert.bindValue(3, row[3].trimmed());
        if (insert.exec()) {
            ++count;
        } else {
            qWarning("导入失败 [%s]: %s", qPrintable(word),
                     qPrintable(insert.lastError().text()));
        }
    }
    m_db.commit();
    return count;
}

int WordStore::countWords() const
{
    QSqlQuery q = rawQuery(QStringLiteral("SELECT COUNT(*) FROM words"));
    return q.next() ? q.value(0).toInt() : 0;
}

Counts WordStore::counts(const QDate &day) const
{
    Counts c;
    const qint64 listId = currentWordListId();
    if (listId <= 0)
        return c;
    QSqlQuery q = rawQuery(
        QStringLiteral(
            "SELECT COUNT(*),"
            "SUM(CASE WHEN box=0 THEN 1 ELSE 0 END),"
            "SUM(CASE WHEN box=0 AND review_count>0 THEN 1 ELSE 0 END),"
            "SUM(CASE WHEN box=6 THEN 1 ELSE 0 END),"
            "SUM(CASE WHEN box>0 THEN 1 ELSE 0 END),"
            "SUM(CASE WHEN box=0 AND review_count=0 THEN 1 ELSE 0 END) "
            "FROM word_list_items WHERE list_id=?"),
        {listId});
    if (q.next()) {
        c.total = q.value(0).toInt();
        c.newTotal = q.value(1).toInt();
        c.learning = q.value(2).toInt(); // 待复习
        c.mastered = q.value(3).toInt();
        c.known = q.value(4).toInt();
        c.newTotal = q.value(5).toInt(); // 未学（没学过的）
    }
    return c;
}

QVector<Word> WordStore::learnCards(int limit)
{
    const qint64 listId = currentWordListId();
    if (listId <= 0)
        return {};
    QSqlQuery q = rawQuery(
        QStringLiteral(
            "SELECT i.id, i.word, i.pos, i.meaning, i.box, "
            "i.review_count, w.id, w.phonetic, w.example_sentence "
            "FROM word_list_items i "
            "LEFT JOIN words w ON w.word = i.word COLLATE NOCASE "
            "WHERE i.list_id=? AND i.box=0 AND i.review_count=0 "
            "ORDER BY i.sort_order, i.id LIMIT ?"),
        {listId, limit});
    QVector<Word> words;
    while (q.next()) {
        Word w;
        w.itemId = q.value(0).toLongLong();
        w.word = q.value(1).toString();
        w.pos = q.value(2).toString();
        w.meaning = q.value(3).toString();
        w.box = q.value(4).toInt();
        w.reviewCount = q.value(5).toInt();
        w.id = q.value(6).toLongLong();
        w.phonetic = q.value(7).toString();
        w.exampleSentence = q.value(8).toString();
        if (w.id <= 0)
            w.id = addWord(w.word, w.pos, w.meaning); // 词表单词补进词典
        words.append(w);
    }
    return words;
}

QVector<Word> WordStore::reviewCards(int limit)
{
    const qint64 listId = currentWordListId();
    if (listId <= 0)
        return {};
    QSqlQuery q = rawQuery(
        QStringLiteral(
            "SELECT i.id, i.word, i.pos, i.meaning, i.box, "
            "i.review_count, w.id, w.phonetic, w.example_sentence "
            "FROM word_list_items i "
            "LEFT JOIN words w ON w.word = i.word COLLATE NOCASE "
            "WHERE i.list_id=? AND i.box=0 AND i.review_count>0 "
            "ORDER BY i.review_count DESC, i.sort_order, i.id LIMIT ?"),
        {listId, limit});
    QVector<Word> words;
    while (q.next()) {
        Word w;
        w.itemId = q.value(0).toLongLong();
        w.word = q.value(1).toString();
        w.pos = q.value(2).toString();
        w.meaning = q.value(3).toString();
        w.box = q.value(4).toInt();
        w.reviewCount = q.value(5).toInt();
        w.id = q.value(6).toLongLong();
        w.phonetic = q.value(7).toString();
        w.exampleSentence = q.value(8).toString();
        if (w.id <= 0)
            w.id = addWord(w.word, w.pos, w.meaning);
        words.append(w);
    }
    return words;
}

std::optional<Word> WordStore::getWord(qint64 id) const
{
    QSqlQuery q = rawQuery(QStringLiteral("SELECT * FROM words WHERE id=?"),
                           {id});
    if (!q.next())
        return std::nullopt;
    return wordFromQuery(q);
}

QVector<Word> WordStore::search(const QString &query, int limit) const
{
    QSqlQuery q;
    if (query.trimmed().isEmpty()) {
        q = rawQuery(
            QStringLiteral("SELECT * FROM words ORDER BY rank LIMIT ?"),
            {limit});
    } else {
        const QString like = QStringLiteral("%%1%").arg(query.trimmed());
        q = rawQuery(
            QStringLiteral(
                "SELECT * FROM words WHERE word LIKE ? OR meaning LIKE ? "
                "ORDER BY rank LIMIT ?"),
            {like, like, limit});
    }
    QVector<Word> words;
    while (q.next())
        words.append(wordFromQuery(q));
    return words;
}

qint64 WordStore::addWord(const QString &word, const QString &pos,
                          const QString &meaning)
{
    const QString w = word.trimmed();
    if (w.isEmpty())
        return -1;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT MAX(rank) FROM words WHERE rank IS NOT NULL"));
    q.exec();
    int rank = 1;
    if (q.next())
        rank = q.value(0).toInt() + 1;

    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral(
        "INSERT INTO words(rank, word, pos, meaning) VALUES(?, ?, ?, ?)"));
    insert.addBindValue(rank);
    insert.addBindValue(w);
    insert.addBindValue(pos.isEmpty() ? QStringLiteral("")
                                      : pos.trimmed());
    insert.addBindValue(meaning.isEmpty() ? QStringLiteral("")
                                          : meaning.trimmed());
    if (!insert.exec())
        return -1;
    const qint64 id = insert.lastInsertId().toLongLong();
    const std::optional<Word> dictWord = lookupDict(w);
    if (dictWord && !dictWord->phonetic.isEmpty()) {
        QSqlQuery upd(m_db);
        upd.prepare(QStringLiteral(
            "UPDATE words SET phonetic=? WHERE id=?"));
        upd.addBindValue(dictWord->phonetic);
        upd.addBindValue(id);
        upd.exec();
    }
    return id;
}

std::optional<Word> WordStore::findWordByText(const QString &word) const
{
    QSqlQuery q = rawQuery(
        QStringLiteral("SELECT * FROM words WHERE word=? COLLATE NOCASE"),
        {word.trimmed()});
    if (!q.next())
        return std::nullopt;
    return wordFromQuery(q);
}

// ---------- 文章与阅读 ----------

qint64 WordStore::saveArticle(const QString &title, const QString &content,
                              const QString &source, int difficulty)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO articles(title, content, source, difficulty, created_at) "
        "VALUES(?, ?, ?, ?, ?)"));
    q.addBindValue(title.trimmed().isEmpty()
                       ? QStringLiteral("未命名文章")
                       : title.trimmed());
    q.addBindValue(content);
    q.addBindValue(source);
    q.addBindValue(difficulty);
    q.addBindValue(QDate::currentDate().toString(Qt::ISODate));
    if (!q.exec())
        return -1;
    return q.lastInsertId().toLongLong();
}

QVector<Article> WordStore::listArticles() const
{
    QSqlQuery q = rawQuery(
        QStringLiteral(
            "SELECT id, title, content, source, difficulty, created_at "
            "FROM articles ORDER BY id DESC"));
    QVector<Article> articles;
    while (q.next()) {
        Article a;
        a.id = q.value(0).toLongLong();
        a.title = q.value(1).toString();
        a.content = q.value(2).toString();
        a.source = q.value(3).toString();
        a.difficulty = q.value(4).toInt();
        a.createdAt = q.value(5).toString();
        articles.append(a);
    }
    return articles;
}

std::optional<Article> WordStore::getArticle(qint64 articleId) const
{
    QSqlQuery q = rawQuery(
        QStringLiteral(
            "SELECT id, title, content, source, difficulty, created_at "
            "FROM articles WHERE id=?"),
        {articleId});
    if (!q.next())
        return std::nullopt;
    Article a;
    a.id = q.value(0).toLongLong();
    a.title = q.value(1).toString();
    a.content = q.value(2).toString();
    a.source = q.value(3).toString();
    a.difficulty = q.value(4).toInt();
    a.createdAt = q.value(5).toString();
    return a;
}

void WordStore::deleteArticle(qint64 articleId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM articles WHERE id=?"));
    q.addBindValue(articleId);
    q.exec();
    QSqlQuery q2(m_db);
    q2.prepare(QStringLiteral("DELETE FROM article_words WHERE article_id=?"));
    q2.addBindValue(articleId);
    q2.exec();
}

QStringList WordStore::tokenizeWords(const QString &text)
{
    QStringList tokens;
    QString current;
    for (const QChar c : text) {
        if (c.isLetterOrNumber() || c == QLatin1Char('\'')
            || c == QLatin1Char('-')) {
            current += c;
        } else if (!current.isEmpty()) {
            tokens << current.toLower();
            current.clear();
        }
    }
    if (!current.isEmpty())
        tokens << current.toLower();
    return tokens;
}

ArticleStats WordStore::articleStats(qint64 articleId) const
{
    const std::optional<Article> article = getArticle(articleId);
    ArticleStats stats;
    if (!article)
        return stats;

    QSet<QString> allWords;
    QSet<QString> knownWords;
    const qint64 listId = currentWordListId();
    QSqlQuery q;
    if (listId > 0) {
        q = rawQuery(QStringLiteral(
            "SELECT word, box FROM word_list_items WHERE list_id=?"),
            {listId});
    } else {
        q = rawQuery(QStringLiteral(
            "SELECT word, box FROM word_list_items WHERE 0"));
    }
    while (q.next()) {
        const QString word = q.value(0).toString().toLower();
        allWords.insert(word);
        if (q.value(1).toInt() > 0)
            knownWords.insert(word);
    }

    const QStringList tokens = tokenizeWords(article->content);
    for (const QString &token : tokens) {
        ++stats.total;
        const QString lemma = lookupLemma(token);
        if (knownWords.contains(lemma)) {
            ++stats.known;
        }
        if (allWords.contains(lemma)) {
            ++stats.inList;
        } else {
            ++stats.outOfList;
        }
    }
    if (stats.total > 0)
        stats.coverage = 100.0 * stats.known / stats.total;
    return stats;
}

int WordStore::importWordForms(const QString &path)
{
    QSqlQuery count(m_db);
    count.exec(QStringLiteral("SELECT COUNT(*) FROM word_forms"));
    if (count.next() && count.value(0).toInt() > 0)
        return 0;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return -1;
    const QStringList lines =
        QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    file.close();

    m_db.transaction();
    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO word_forms(form, lemma) VALUES(?, ?)"));
    int count2 = 0;
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char(';')))
            continue;
        const int arrow = line.indexOf(QStringLiteral("->"));
        if (arrow < 0)
            continue;
        QString lemma = line.left(arrow).trimmed();
        const int slash = lemma.indexOf(QLatin1Char('/'));
        if (slash >= 0)
            lemma = lemma.left(slash).trimmed();
        if (lemma.isEmpty())
            continue;
        insert.bindValue(0, lemma);
        insert.bindValue(1, lemma);
        if (insert.exec())
            ++count2;
        const QStringList forms =
            line.mid(arrow + 2).split(QLatin1Char(','));
        for (const QString &form : forms) {
            const QString f = form.trimmed();
            if (f.isEmpty() || f == lemma)
                continue;
            insert.bindValue(0, f);
            insert.bindValue(1, lemma);
            if (insert.exec())
                ++count2;
        }
    }
    m_db.commit();
    return count2;
}

QString WordStore::lookupLemma(const QString &form) const
{
    const QString f = form.trimmed().toLower();
    if (f.isEmpty())
        return f;
    QSqlQuery q = rawQuery(
        QStringLiteral("SELECT lemma FROM word_forms WHERE form=?"), {f});
    return q.next() ? q.value(0).toString() : f;
}

// ---------- 翻译联动 ----------

QVector<Word> WordStore::extractUnknownWords(const QString &text,
                                             int limit)
{
    const qint64 listId = currentWordListId();
    const qint64 transList = getOrCreateWordList(
        QStringLiteral("翻译生词"),
        QStringLiteral("翻译时自动收集的生词"),
        QStringLiteral("translation"));
    QSet<QString> inList;
    if (listId > 0) {
        QSqlQuery q = rawQuery(QStringLiteral(
            "SELECT word FROM word_list_items WHERE list_id=?"),
            {listId});
        while (q.next())
            inList.insert(q.value(0).toString().toLower());
    }
    if (transList > 0) {
        QSqlQuery q = rawQuery(QStringLiteral(
            "SELECT word FROM word_list_items WHERE list_id=?"),
            {transList});
        while (q.next())
            inList.insert(q.value(0).toString().toLower());
    }
    const QStringList tokens = tokenizeWords(text);
    QVector<Word> result;
    QSet<QString> seen;
    for (const QString &token : tokens) {
        if (result.size() >= limit)
            break;
        QString lemma = token.toLower();
        if (kStopwords.contains(lemma) || inList.contains(lemma))
            continue;
        lemma = lookupLemma(lemma);
        if (inList.contains(lemma) || seen.contains(lemma))
            continue;
        seen.insert(lemma);
        Word w;
        w.word = lemma;
        const std::optional<Word> dict = lookupDict(lemma);
        if (dict) {
            w.pos = dict->pos;
            w.meaning = dict->meaning;
        }
        result.append(w);
    }
    return result;
}

qint64 WordStore::queueWordFromTranslation(const QString &word,
                                           const QString &meaning,
                                           const QString &sentence)
{
    const QString w = word.trimmed().toLower();
    if (w.isEmpty())
        return -1;
    std::optional<Word> existing = findWordByText(word);
    qint64 wordId = 0;
    if (existing) {
        wordId = existing->id;
    } else {
        wordId = addWord(word, QString(), meaning);
        if (wordId < 0)
            return -1;
    }
    if (!sentence.trimmed().isEmpty()) {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "UPDATE words SET "
            "example_sentence=COALESCE(NULLIF(?, ''), example_sentence) "
            "WHERE id=?"));
        q.addBindValue(sentence.trimmed());
        q.addBindValue(wordId);
        q.exec();
    }
    // 翻译生词自动加入「翻译生词」词表
    const qint64 transList = getOrCreateWordList(
        QStringLiteral("翻译生词"),
        QStringLiteral("翻译时自动收集的生词"),
        QStringLiteral("translation"));
    if (transList <= 0)
        return wordId;
    QSqlQuery item = rawQuery(QStringLiteral(
        "SELECT id FROM word_list_items "
        "WHERE list_id=? AND word=? COLLATE NOCASE"),
        {transList, w});
    if (item.next())
        return wordId;
    QSqlQuery mx = rawQuery(QStringLiteral(
        "SELECT COALESCE(MAX(sort_order), 0) + 1 "
        "FROM word_list_items WHERE list_id=?"),
        {transList});
    const int order = mx.next() ? mx.value(0).toInt() : 0;
    QString pos;
    QString meaning2 = meaning;
    if (existing) {
        pos = existing->pos;
        if (meaning2.isEmpty())
            meaning2 = existing->meaning;
    }
    addWordToList(transList, w, pos, meaning2, order);
    return wordId;
}

qint64 WordStore::queueWordToReadingList(const QString &word,
                                         const QString &meaning,
                                         const QString &sentence)
{
    const QString w = word.trimmed().toLower();
    if (w.isEmpty())
        return -1;
    std::optional<Word> existing = findWordByText(w);
    qint64 wordId = 0;
    if (existing) {
        wordId = existing->id;
    } else {
        wordId = addWord(w, QString(), meaning);
        if (wordId < 0)
            return -1;
    }
    if (!sentence.trimmed().isEmpty()) {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "UPDATE words SET "
            "example_sentence=COALESCE(NULLIF(?, ''), example_sentence) "
            "WHERE id=?"));
        q.addBindValue(sentence.trimmed());
        q.addBindValue(wordId);
        q.exec();
    }
    const qint64 readList = getOrCreateWordList(
        QStringLiteral("阅读生词"),
        QStringLiteral("阅读时收集的生词"),
        QStringLiteral("reading"));
    if (readList <= 0)
        return wordId;
    QSqlQuery item = rawQuery(QStringLiteral(
        "SELECT id FROM word_list_items "
        "WHERE list_id=? AND word=? COLLATE NOCASE"),
        {readList, w});
    if (item.next())
        return wordId;
    QSqlQuery mx = rawQuery(QStringLiteral(
        "SELECT COALESCE(MAX(sort_order), 0) + 1 "
        "FROM word_list_items WHERE list_id=?"),
        {readList});
    const int order = mx.next() ? mx.value(0).toInt() : 0;
    QString pos;
    QString meaning2 = meaning;
    if (existing) {
        pos = existing->pos;
        if (meaning2.isEmpty())
            meaning2 = existing->meaning;
    }
    addWordToList(readList, w, pos, meaning2, order);
    return wordId;
}

QString WordStore::sentenceContaining(const QString &text,
                                      const QString &word,
                                      int maxLen)
{
    QStringList sentences;
    QString current;
    for (const QChar c : text) {
        current += c;
        if (c == QLatin1Char('.') || c == QLatin1Char('!')
            || c == QLatin1Char('?') || c == QLatin1Char('\n')) {
            const QString s = current.trimmed();
            if (!s.isEmpty())
                sentences << s;
            current.clear();
        }
    }
    if (!current.trimmed().isEmpty())
        sentences << current.trimmed();
    const QString lower = word.toLower();
    for (const QString &s : sentences) {
        if (s.toLower().contains(lower)) {
            QString result = s.simplified();
            if (result.size() > maxLen)
                result = result.left(maxLen - 1) + QStringLiteral("…");
            return result;
        }
    }
    return {};
}

// ---------- 领域词表 ----------

qint64 WordStore::createWordList(const QString &name,
                                 const QString &description,
                                 const QString &source)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return -1;
    QSqlQuery mx = rawQuery(QStringLiteral(
        "SELECT COALESCE(MAX(sort_order), 0) + 1 FROM word_lists"));
    const int order = mx.next() ? mx.value(0).toInt() : 0;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO word_lists"
        "(name, description, source, sort_order, created_at) "
        "VALUES(?, ?, ?, ?, ?)"));
    q.addBindValue(trimmed);
    const QString desc = description.trimmed();
    q.addBindValue(desc.isEmpty() ? QStringLiteral("") : desc);
    q.addBindValue(source.isEmpty() ? QStringLiteral("manual") : source);
    q.addBindValue(order);
    q.addBindValue(QDate::currentDate().toString(Qt::ISODate));
    if (!q.exec())
        return -1;
    return q.lastInsertId().toLongLong();
}

qint64 WordStore::getOrCreateWordList(const QString &name,
                                      const QString &description,
                                      const QString &source)
{
    QSqlQuery q = rawQuery(QStringLiteral(
        "SELECT id FROM word_lists WHERE name=?"),
        {name.trimmed()});
    if (q.next())
        return q.value(0).toLongLong();
    return createWordList(name, description, source);
}

bool WordStore::deleteWordList(qint64 listId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM word_lists WHERE id=?"));
    q.addBindValue(listId);
    q.exec();
    QSqlQuery q2(m_db);
    q2.prepare(QStringLiteral("DELETE FROM word_list_items WHERE list_id=?"));
    q2.addBindValue(listId);
    q2.exec();
    if (currentWordListId() == listId)
        setCurrentWordList(-1);
    return true;
}

bool WordStore::setWordListOrder(qint64 listId, int order)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE word_lists SET sort_order=? WHERE id=?"));
    q.addBindValue(order);
    q.addBindValue(listId);
    return q.exec();
}

QVector<WordListInfo> WordStore::listWordLists(const QString &search) const
{
    QSqlQuery q;
    if (search.trimmed().isEmpty()) {
        q = rawQuery(QStringLiteral(
            "SELECT wl.id, wl.name, wl.description, wl.source, "
            "(SELECT COUNT(*) FROM word_list_items i WHERE i.list_id=wl.id) "
            "FROM word_lists wl ORDER BY wl.sort_order, wl.id"));
    } else {
        const QString like =
            QStringLiteral("%%1%").arg(search.trimmed());
        q = rawQuery(QStringLiteral(
                         "SELECT wl.id, wl.name, wl.description, wl.source, "
                         "(SELECT COUNT(*) FROM word_list_items i "
                         "WHERE i.list_id=wl.id) "
                         "FROM word_lists wl "
                         "WHERE wl.name LIKE ? OR wl.description LIKE ? "
                         "ORDER BY wl.sort_order, wl.id"),
                     {like, like});
    }
    QVector<WordListInfo> lists;
    while (q.next()) {
        WordListInfo info;
        info.id = q.value(0).toLongLong();
        info.name = q.value(1).toString();
        info.description = q.value(2).toString();
        info.source = q.value(3).toString();
        info.wordCount = q.value(4).toInt();
        lists.append(info);
    }
    return lists;
}

bool WordStore::addWordToList(qint64 listId, const QString &word,
                              const QString &pos, const QString &meaning,
                              int order)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO word_list_items"
        "(list_id, word, pos, meaning, sort_order) VALUES(?, ?, ?, ?, ?)"));
    q.addBindValue(listId);
    q.addBindValue(word.trimmed().toLower());
    q.addBindValue(pos.isEmpty() ? QStringLiteral("") : pos.trimmed());
    q.addBindValue(meaning.isEmpty() ? QStringLiteral("") : meaning.trimmed());
    q.addBindValue(order);
    return q.exec();
}

QVector<Word> WordStore::wordsInWordList(qint64 listId) const
{
    QSqlQuery q = rawQuery(QStringLiteral(
        "SELECT i.id, i.word, i.pos, i.meaning, i.box, "
        "i.review_count, w.id, w.phonetic, w.example_sentence "
        "FROM word_list_items i "
        "LEFT JOIN words w ON w.word = i.word COLLATE NOCASE "
        "WHERE i.list_id=? ORDER BY i.sort_order, i.id"),
        {listId});
    QVector<Word> words;
    while (q.next()) {
        Word w;
        w.itemId = q.value(0).toLongLong();
        w.word = q.value(1).toString();
        w.pos = q.value(2).toString();
        w.meaning = q.value(3).toString();
        w.box = q.value(4).toInt();
        w.reviewCount = q.value(5).toInt();
        w.id = q.value(6).toLongLong();
        w.phonetic = q.value(7).toString();
        w.exampleSentence = q.value(8).toString();
        words.append(w);
    }
    return words;
}

QVector<WordListInfo> WordStore::listsContainingWord(
    const QString &word) const
{
    QSqlQuery q = rawQuery(QStringLiteral(
        "SELECT wl.id, wl.name, wl.description, wl.source, "
        "COUNT(i2.id) "
        "FROM word_list_items i "
        "JOIN word_lists wl ON wl.id=i.list_id "
        "LEFT JOIN word_list_items i2 ON i2.list_id=wl.id "
        "WHERE i.word=? COLLATE NOCASE "
        "GROUP BY wl.id "
        "ORDER BY wl.sort_order, wl.id"),
        {word.trimmed().toLower()});
    QVector<WordListInfo> lists;
    while (q.next()) {
        WordListInfo info;
        info.id = q.value(0).toLongLong();
        info.name = q.value(1).toString();
        info.description = q.value(2).toString();
        info.source = q.value(3).toString();
        info.wordCount = q.value(4).toInt();
        lists.append(info);
    }
    return lists;
}

QSet<QString> WordStore::allListWords() const
{
    QSet<QString> words;
    QSqlQuery q = rawQuery(QStringLiteral(
        "SELECT DISTINCT word FROM word_list_items"));
    while (q.next())
        words.insert(q.value(0).toString().toLower());
    return words;
}

QSet<QString> WordStore::currentListWords() const
{
    QSet<QString> words;
    const qint64 listId = currentWordListId();
    if (listId <= 0)
        return words;
    QSqlQuery q = rawQuery(QStringLiteral(
        "SELECT word FROM word_list_items WHERE list_id=?"),
        {listId});
    while (q.next())
        words.insert(q.value(0).toString().toLower());
    return words;
}

QSet<QString> WordStore::masteredListWords() const
{
    QSet<QString> words;
    QSqlQuery q = rawQuery(QStringLiteral(
        "SELECT DISTINCT word FROM word_list_items WHERE box=6"));
    while (q.next())
        words.insert(q.value(0).toString().toLower());
    return words;
}

void WordStore::resetWordInAllLists(const QString &word)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE word_list_items SET box=0, review_count=0 "
        "WHERE word=? COLLATE NOCASE"));
    q.addBindValue(word.trimmed().toLower());
    q.exec();
}

int WordStore::knownInWordList(qint64 listId) const
{
    QSqlQuery q = rawQuery(QStringLiteral(
        "SELECT COUNT(*) FROM word_list_items "
        "WHERE list_id=? AND box>0"),
        {listId});
    return q.next() ? q.value(0).toInt() : 0;
}

std::optional<Word> WordStore::findInCurrentList(const QString &word) const
{
    const qint64 listId = currentWordListId();
    if (listId <= 0)
        return std::nullopt;
    return findInNamedList(QString(), word);
}

std::optional<Word> WordStore::findInNamedList(const QString &listName,
                                               const QString &word) const
{
    qint64 listId = currentWordListId();
    if (!listName.trimmed().isEmpty()) {
        QSqlQuery ql = rawQuery(QStringLiteral(
            "SELECT id FROM word_lists WHERE name=?"),
            {listName.trimmed()});
        if (!ql.next())
            return std::nullopt;
        listId = ql.value(0).toLongLong();
    }
    if (listId <= 0)
        return std::nullopt;
    QSqlQuery q = rawQuery(QStringLiteral(
        "SELECT i.id, i.word, i.pos, i.meaning, i.box, "
        "i.review_count, w.id, w.phonetic, w.example_sentence "
        "FROM word_list_items i "
        "LEFT JOIN words w ON w.word = i.word COLLATE NOCASE "
        "WHERE i.list_id=? AND i.word=? COLLATE NOCASE"),
        {listId, word.trimmed()});
    if (!q.next())
        return std::nullopt;
    Word w;
    w.itemId = q.value(0).toLongLong();
    w.word = q.value(1).toString();
    w.pos = q.value(2).toString();
    w.meaning = q.value(3).toString();
    w.box = q.value(4).toInt();
    w.reviewCount = q.value(5).toInt();
    w.id = q.value(6).toLongLong();
    w.phonetic = q.value(7).toString();
    w.exampleSentence = q.value(8).toString();
    return w;
}

void WordStore::setCurrentWordList(qint64 listId)
{
    setSetting(QStringLiteral("current_word_list"),
               QString::number(listId));
}

qint64 WordStore::currentWordListId() const
{
    return getSetting(QStringLiteral("current_word_list"),
                      QStringLiteral("-1"))
        .toLongLong();
}

QString WordStore::currentWordListName() const
{
    const qint64 id = currentWordListId();
    if (id < 0)
        return {};
    QSqlQuery q = rawQuery(
        QStringLiteral("SELECT name FROM word_lists WHERE id=?"), {id});
    return q.next() ? q.value(0).toString() : QString();
}

QVector<Word> WordStore::extractDomainWords(const QString &text,
                                            int limit) const
{
    QHash<QString, int> freq;
    for (const QString &token : tokenizeWords(text)) {
        if (token.size() < 2)
            continue;
        QString lemma = lookupLemma(token);
        if (kStopwords.contains(lemma))
            continue;
        ++freq[lemma];
    }
    QList<QPair<int, QString>> ranked;
    for (auto it = freq.constBegin(); it != freq.constEnd(); ++it)
        ranked.append({it.value(), it.key()});
    std::sort(ranked.begin(), ranked.end(),
              [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
                  if (a.first != b.first)
                      return a.first > b.first;
                  return a.second < b.second;
              });
    QVector<Word> result;
    for (int i = 0; i < ranked.size() && result.size() < limit; ++i) {
        const QString lemma = ranked[i].second;
        Word w;
        w.word = lemma;
        const std::optional<Word> found = findWordByText(lemma);
        if (found) {
            w.pos = found->pos;
            w.meaning = found->meaning;
        } else {
            const std::optional<Word> dict = lookupDict(lemma);
            if (dict) {
                w.pos = dict->pos;
                w.meaning = dict->meaning;
            }
        }
        result.append(w);
    }
    return result;
}

QVector<Word> WordStore::extractDomainWordsFromArticles(
    const QVector<qint64> &articleIds, int limit) const
{
    QString combined;
    for (qint64 id : articleIds) {
        const std::optional<Article> article = getArticle(id);
        if (article)
            combined += article->content + QLatin1Char(' ');
    }
    return extractDomainWords(combined, limit);
}

int WordStore::seedBuiltinWordList()
{
    QSqlQuery exists = rawQuery(QStringLiteral(
        "SELECT id FROM word_lists WHERE name='核心 3000'"));
    if (exists.next())
        return 0;
    const qint64 listId = createWordList(
        QStringLiteral("核心 3000"),
        QStringLiteral("内置牛津 3000 高频词，按使用频率排序"),
        QStringLiteral("builtin"));
    if (listId < 0)
        return -1;
    QSqlQuery sel = rawQuery(QStringLiteral(
        "SELECT rank, word, pos, meaning FROM words ORDER BY rank, id"));
    m_db.transaction();
    QSqlQuery ins(m_db);
    ins.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO word_list_items"
        "(list_id, word, pos, meaning, sort_order) VALUES(?, ?, ?, ?, ?)"));
    int count = 0;
    int order = 0;
    while (sel.next()) {
        const int rank = sel.value(0).toInt();
        ins.bindValue(0, listId);
        ins.bindValue(1, sel.value(1).toString());
        ins.bindValue(2, sel.value(2).toString());
        ins.bindValue(3, sel.value(3).toString());
        ins.bindValue(4, rank > 0 ? rank : order);
        if (ins.exec())
            ++count;
        ++order;
    }
    m_db.commit();
    return count;
}

int WordStore::seedExamplesFromArticles()
{
    const QVector<Article> articles = listArticles();
    if (articles.isEmpty())
        return 0;
    QSqlQuery words = rawQuery(QStringLiteral(
        "SELECT id, word FROM words WHERE example_sentence=''"));
    int count = 0;
    QSqlQuery update(m_db);
    update.prepare(QStringLiteral(
        "UPDATE words SET example_sentence=? WHERE id=?"));
    while (words.next()) {
        const qint64 wordId = words.value(0).toLongLong();
        const QString word = words.value(1).toString();
        for (const Article &article : articles) {
            const QString sentence =
                sentenceContaining(article.content, word);
            if (!sentence.isEmpty()) {
                update.bindValue(0, sentence);
                update.bindValue(1, wordId);
                update.exec();
                ++count;
                break;
            }
        }
    }
    return count;
}

void WordStore::setExampleSentence(qint64 wordId, const QString &sentence)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE words SET example_sentence=? WHERE id=?"));
    q.addBindValue(sentence.isEmpty() ? QStringLiteral("") : sentence.trimmed());
    q.addBindValue(wordId);
    q.exec();
}

void WordStore::seedWordPhonetics()
{
    QSqlQuery words = rawQuery(QStringLiteral(
        "SELECT id, word FROM words WHERE phonetic=''"));
    QSqlQuery update(m_db);
    update.prepare(QStringLiteral(
        "UPDATE words SET phonetic=? WHERE id=?"));
    while (words.next()) {
        const std::optional<Word> dict =
            lookupDict(words.value(1).toString());
        if (dict && !dict->phonetic.isEmpty()) {
            update.bindValue(0, dict->phonetic);
            update.bindValue(1, words.value(0).toLongLong());
            update.exec();
        }
    }
}

QString WordStore::inflectionSummary(const QString &word) const
{
    const QString lower = word.trimmed().toLower();
    QSqlQuery q = rawQuery(
        QStringLiteral(
            "SELECT form FROM word_forms WHERE lemma=? AND form<>? "
            "ORDER BY form LIMIT 8"),
        {lower, lower});
    QStringList forms;
    while (q.next())
        forms << q.value(0).toString();
    return forms.join(QStringLiteral(", "));
}

bool WordStore::addInflections(const QString &lemma,
                               const QStringList &forms)
{
    bool ok = true;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO word_forms(form, lemma) VALUES(?, ?)"));
    const QString lowerLemma = lemma.trimmed().toLower();
    for (const QString &raw : forms) {
        const QString form = raw.trimmed().toLower();
        if (form.isEmpty() || form == lowerLemma)
            continue;
        bool alpha = true;
        for (const QChar c : form) {
            if (!c.isLetter() && c != QLatin1Char('-')
                && c != QLatin1Char('\'')) {
                alpha = false;
                break;
            }
        }
        if (!alpha)
            continue;
        q.bindValue(0, form);
        q.bindValue(1, lowerLemma);
        if (!q.exec())
            ok = false;
    }
    return ok;
}

// ---------- 离线词典（ECDICT） ----------

bool WordStore::dictReady() const
{
    if (!m_dict.isOpen())
        return false;
    QSqlQuery q(m_dict);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM dict")))
        return false;
    return q.next() && q.value(0).toInt() > 0;
}

int WordStore::importDictCsv(const QString &path)
{
    if (dictReady())
        return 0;
    if (!m_dict.isOpen())
        return -1;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return -1;

    if (!m_dict.transaction()) {
        qWarning("dict 事务启动失败: %s",
                 qPrintable(m_dict.lastError().text()));
    }
    QSqlQuery insert(m_dict);
    insert.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO dict(word, pos, translation, phonetic) "
        "VALUES(?, ?, ?, ?)"));
    int count = 0;
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    bool first = true;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (first) {
            first = false;
            continue;
        }
        QStringList fields;
        QString field;
        bool inQuotes = false;
        const int n = line.size();
        for (int i = 0; i < n; ++i) {
            const QChar c = line[i];
            if (inQuotes) {
                if (c == QLatin1Char('"')) {
                    if (i + 1 < n && line[i + 1] == QLatin1Char('"')) {
                        field += QLatin1Char('"');
                        ++i;
                    } else {
                        inQuotes = false;
                    }
                } else {
                    field += c;
                }
            } else if (c == QLatin1Char('"')) {
                inQuotes = true;
            } else if (c == QLatin1Char(',')) {
                fields << (field.isNull() ? QStringLiteral("") : field);
                field.clear();
            } else {
                field += c;
            }
        }
        fields << (field.isNull() ? QStringLiteral("") : field);
        if (fields.size() < 5)
            continue;
        const QString word = fields[0].trimmed().toLower();
        if (word.isEmpty())
            continue;
        insert.bindValue(0, word);
        insert.bindValue(1, fields[4].isEmpty() ? QStringLiteral("")
                                                : fields[4].trimmed());
        insert.bindValue(2, fields[3].isEmpty() ? QStringLiteral("")
                                                : fields[3].trimmed());
        insert.bindValue(3, fields[1].isEmpty() ? QStringLiteral("")
                                                : fields[1].trimmed());
        if (insert.exec())
            ++count;
    }
    if (!m_dict.commit()) {
        qWarning("dict 事务提交失败: %s",
                 qPrintable(m_dict.lastError().text()));
    }
    return count;
}

std::optional<Word> WordStore::lookupDict(const QString &word) const
{
    if (!m_dict.isOpen())
        return std::nullopt;
    QSqlQuery q(m_dict);
    q.prepare(QStringLiteral(
        "SELECT word, pos, translation, phonetic FROM dict "
        "WHERE word=? COLLATE NOCASE"));
    q.addBindValue(word.trimmed().toLower());
    if (!q.exec() || !q.next())
        return std::nullopt;
    Word w;
    w.word = q.value(0).toString();
    w.pos = q.value(1).toString();
    w.meaning = q.value(2).toString();
    w.phonetic = q.value(3).toString();
    return w;
}

// ---------- 阅读统计 ----------

void WordStore::logCoverage(qint64 articleId, const QDate &day)
{
    const ArticleStats s = articleStats(articleId);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO coverage_log(date, article_id, total, known, in_list, "
        "out_of_list, coverage) VALUES(?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(date, article_id) DO UPDATE SET "
        "total=excluded.total, known=excluded.known, "
        "in_list=excluded.in_list, out_of_list=excluded.out_of_list, "
        "coverage=excluded.coverage"));
    q.addBindValue(day.toString(Qt::ISODate));
    q.addBindValue(articleId);
    q.addBindValue(s.total);
    q.addBindValue(s.known);
    q.addBindValue(s.inList);
    q.addBindValue(s.outOfList);
    q.addBindValue(s.coverage);
    q.exec();
}

QVector<CoveragePoint> WordStore::coverageHistory(int days) const
{
    const QString from =
        QDate::currentDate().addDays(-(days - 1)).toString(Qt::ISODate);
    QSqlQuery q = rawQuery(
        QStringLiteral(
            "SELECT date, AVG(coverage), COUNT(DISTINCT article_id) "
            "FROM coverage_log WHERE date >= ? "
            "GROUP BY date ORDER BY date"),
        {from});
    QVector<CoveragePoint> points;
    while (q.next()) {
        CoveragePoint p;
        p.date = q.value(0).toString();
        p.coverage = q.value(1).toDouble();
        p.articles = q.value(2).toInt();
        points.append(p);
    }
    return points;
}

int WordStore::coverageArticleCount() const
{
    QSqlQuery q = rawQuery(QStringLiteral(
        "SELECT COUNT(DISTINCT article_id) FROM coverage_log"));
    return q.next() ? q.value(0).toInt() : 0;
}

// ---------- 生词池 ----------

bool WordStore::addToPool(qint64 articleId, const QString &word,
                          const QString &sentence,
                          const QString &meaning)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO article_words"
        "(article_id, word, sentence, meaning, added_at) "
        "VALUES(?, ?, ?, ?, ?)"));
    q.addBindValue(articleId);
    q.addBindValue(word.trimmed().toLower());
    q.addBindValue(sentence.isEmpty() ? QStringLiteral("")
                                      : sentence.trimmed());
    q.addBindValue(meaning.isEmpty() ? QStringLiteral("")
                                     : meaning.trimmed());
    q.addBindValue(QDate::currentDate().toString(Qt::ISODate));
    return q.exec();
}

QVector<PoolRow> WordStore::poolList() const
{
    QSqlQuery q = rawQuery(QStringLiteral(
        "SELECT aw.id, aw.word, aw.meaning, aw.sentence, aw.added_at, "
        "a.title, w.meaning AS wm "
        "FROM article_words aw "
        "LEFT JOIN articles a ON a.id = aw.article_id "
        "LEFT JOIN words w ON w.word = aw.word COLLATE NOCASE "
        "ORDER BY aw.added_at DESC, aw.id DESC"));
    QVector<PoolRow> rows;
    while (q.next()) {
        PoolRow row;
        row.id = q.value(0).toLongLong();
        row.word = q.value(1).toString();
        row.meaning = q.value(2).toString();
        if (row.meaning.isEmpty())
            row.meaning = q.value(6).toString();
        row.sentence = q.value(3).toString();
        row.addedAt = q.value(4).toString();
        row.articleTitle = q.value(5).toString();
        rows.append(row);
    }
    return rows;
}

bool WordStore::removeFromPool(qint64 poolId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM article_words WHERE id=?"));
    q.addBindValue(poolId);
    return q.exec();
}

bool WordStore::setPoolMeaning(qint64 poolId, const QString &meaning)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE article_words SET meaning=? WHERE id=?"));
    q.addBindValue(meaning.trimmed());
    q.addBindValue(poolId);
    return q.exec();
}

bool WordStore::promoteFromPool(qint64 poolId, const QDate &day)
{
    QSqlQuery fetch(m_db);
    fetch.prepare(QStringLiteral(
        "SELECT article_id, word, sentence, meaning FROM article_words "
        "WHERE id=?"));
    fetch.addBindValue(poolId);
    if (!fetch.exec() || !fetch.next())
        return false;
    const QString word = fetch.value(1).toString();
    const QString sentence = fetch.value(2).toString();
    const QString meaning = fetch.value(3).toString();

    std::optional<Word> existing = findWordByText(word);
    qint64 wordId = 0;
    if (existing) {
        wordId = existing->id;
    } else {
        wordId = addWord(word, QString(), meaning);
        if (wordId < 0)
            return false;
    }

    if (!sentence.trimmed().isEmpty()) {
        QSqlQuery update(m_db);
        update.prepare(QStringLiteral(
            "UPDATE words SET "
            "example_sentence=COALESCE(NULLIF(?, ''), example_sentence) "
            "WHERE id=?"));
        update.addBindValue(sentence);
        update.addBindValue(wordId);
        update.exec();
    }
    // 生词池转出后加入「阅读生词」词表
    const qint64 listId = getOrCreateWordList(
        QStringLiteral("阅读生词"),
        QStringLiteral("阅读时收集的生词"),
        QStringLiteral("reading"));
    if (listId > 0) {
        QSqlQuery item = rawQuery(QStringLiteral(
            "SELECT id FROM word_list_items "
            "WHERE list_id=? AND word=? COLLATE NOCASE"),
            {listId, word.trimmed().toLower()});
        if (!item.next()) {
            QSqlQuery mx = rawQuery(QStringLiteral(
                "SELECT COALESCE(MAX(sort_order), 0) + 1 "
                "FROM word_list_items WHERE list_id=?"),
                {listId});
            const int order = mx.next() ? mx.value(0).toInt() : 0;
            addWordToList(listId, word, QString(), meaning, order);
        }
    }

    QSqlQuery del(m_db);
    del.prepare(QStringLiteral("DELETE FROM article_words WHERE id=?"));
    del.addBindValue(poolId);
    del.exec();
    Q_UNUSED(day);
    return true;
}

// ---------- 词表学习 ----------

ReviewResult WordStore::answerStudy(qint64 itemId, bool known,
                                    const QDate &day)
{
    QSqlQuery fetch(m_db);
    fetch.prepare(QStringLiteral(
        "SELECT box, review_count FROM word_list_items WHERE id=?"));
    fetch.addBindValue(itemId);
    if (!fetch.exec() || !fetch.next())
        return {};

    ReviewResult result;
    result.known = known;
    result.wasNew = fetch.value(1).toInt() == 0;
    const int newBox = known ? 6 : 0;
    const int reviewCount = fetch.value(1).toInt() + 1;

    QSqlQuery update(m_db);
    update.prepare(QStringLiteral(
        "UPDATE word_list_items SET box=?, review_count=? WHERE id=?"));
    update.addBindValue(newBox);
    update.addBindValue(reviewCount);
    update.addBindValue(itemId);
    update.exec();
    logDaily(day, result.wasNew, known);

    result.box = newBox;
    return result;
}

void WordStore::markItemKnown(qint64 itemId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE word_list_items SET box=6 WHERE id=?"));
    q.addBindValue(itemId);
    q.exec();
}

void WordStore::resetItem(qint64 itemId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE word_list_items SET box=0, review_count=0 WHERE id=?"));
    q.addBindValue(itemId);
    q.exec();
}

void WordStore::resetList(qint64 listId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE word_list_items SET box=0, review_count=0 WHERE list_id=?"));
    q.addBindValue(listId);
    q.exec();
}

void WordStore::resetAllLists()
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "UPDATE word_list_items SET box=0, review_count=0"));
}

void WordStore::logDaily(const QDate &day, bool isNew, bool known)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO daily_log(date, new_count, review_count, correct, wrong) "
        "VALUES(?, ?, ?, ?, ?) "
        "ON CONFLICT(date) DO UPDATE SET "
        "new_count=new_count+excluded.new_count, "
        "review_count=review_count+excluded.review_count, "
        "correct=correct+excluded.correct, "
        "wrong=wrong+excluded.wrong"));
    q.addBindValue(dueSql(day));
    q.addBindValue(isNew ? 1 : 0);
    q.addBindValue(isNew ? 0 : 1);
    q.addBindValue(known ? 1 : 0);
    q.addBindValue(known ? 0 : 1);
    q.exec();
}

// ---------- 设置与统计 ----------

void WordStore::setSetting(const QString &key, const QString &value)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO settings(key, value) VALUES(?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value"));
    q.addBindValue(key);
    q.addBindValue(value);
    q.exec();
}

QString WordStore::getSetting(const QString &key,
                              const QString &defaultValue) const
{
    QSqlQuery q = rawQuery(
        QStringLiteral("SELECT value FROM settings WHERE key=?"), {key});
    return q.next() ? q.value(0).toString() : defaultValue;
}

DailySummary WordStore::dailySummary(const QDate &day) const
{
    QSqlQuery q = rawQuery(
        QStringLiteral(
            "SELECT new_count, review_count, correct, wrong "
            "FROM daily_log WHERE date=?"),
        {dueSql(day)});
    DailySummary s;
    if (q.next()) {
        s.newCount = q.value(0).toInt();
        s.reviewCount = q.value(1).toInt();
        s.correct = q.value(2).toInt();
        s.wrong = q.value(3).toInt();
    }
    return s;
}

int WordStore::streak(const QDate &day) const
{
    QSqlQuery q = rawQuery(
        QStringLiteral("SELECT date FROM daily_log ORDER BY date DESC"));
    QStringList dates;
    while (q.next())
        dates << q.value(0).toString();
    if (dates.isEmpty() || dates.first() != dueSql(day))
        return 0;
    int n = 1;
    QString prev = dueSql(day);
    for (int i = 1; i < dates.size(); ++i) {
        if (QDate::fromString(dates[i], Qt::ISODate).addDays(1)
                .toString(Qt::ISODate) == prev) {
            ++n;
            prev = dates[i];
        } else {
            break;
        }
    }
    return n;
}

// ---------- CSV 解析 ----------

QVector<QStringList> parseCsv(const QString &text)
{
    QVector<QStringList> rows;
    QStringList row;
    QString field;
    bool inQuotes = false;
    int i = 0;
    const int n = text.size();
    while (i < n) {
        const QChar c = text[i];
        if (inQuotes) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < n && text[i + 1] == QLatin1Char('"')) {
                    field += QLatin1Char('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field += c;
            }
        } else if (c == QLatin1Char('"')) {
            inQuotes = true;
        } else if (c == QLatin1Char(',')) {
            row << (field.isNull() ? QStringLiteral("") : field);
            field.clear();
        } else if (c == QLatin1Char('\n')) {
            row << (field.isNull() ? QStringLiteral("") : field);
            rows << row;
            row.clear();
            field.clear();
        } else if (c == QLatin1Char('\r')) {
            // 忽略，交给换行处理
        } else {
            field += c;
        }
        ++i;
    }
    if (!field.isEmpty() || !row.isEmpty()) {
        row << (field.isNull() ? QStringLiteral("") : field);
        rows << row;
    }
    return rows;
}
