#include <QDate>
#include <QCoreApplication>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QTemporaryDir>

#include <cstdio>

#include "core.h"
#include "ai_client.h"

namespace {

int g_failures = 0;

void check(bool ok, const char *name)
{
    if (ok) {
        std::printf("ok   %s\n", name);
    } else {
        std::printf("FAIL %s\n", name);
        ++g_failures;
    }
}

QString sampleCsv()
{
    return QStringLiteral(
        "序号,单词,词性,中文释义\n"
        "1,the,art.,art. 那\n"
        "2,of,prep.,prep. 的\n"
        "3,and,conj.,conj. 和\n"
        "4,to,prep.,prep. 到\n"
        "5,a,,第一个字母 A\n");
}

qint64 makeList(WordStore &store, const QStringList &words)
{
    const qint64 id = store.createWordList(
        QStringLiteral("测试"), {}, QStringLiteral("manual"));
    for (int i = 0; i < words.size(); ++i) {
        store.addWordToList(id, words[i], QStringLiteral("n."),
                            QStringLiteral("释义%1").arg(i), i);
    }
    store.setCurrentWordList(id);
    return id;
}

void testImport()
{
    QTemporaryDir dir;
    const QString csv = dir.filePath(QStringLiteral("words.csv"));
    QFile f(csv);
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(sampleCsv().toUtf8());
    f.close();

    WordStore store(dir.filePath(QStringLiteral("test.db")));
    const int n = store.importCsv(csv, false);
    check(n == 5, "import 5 rows");
    check(store.countWords() == 5, "count words");
    check(store.search(QStringLiteral("单词")).isEmpty(),
          "header row not imported");
    makeList(store, {QStringLiteral("the"), QStringLiteral("of"),
                     QStringLiteral("and"), QStringLiteral("to"),
                     QStringLiteral("a")});
    const Counts c = store.counts();
    check(c.total == 5 && c.newTotal == 5, "all new initially");
    check(c.known == 0, "known count initially zero");
}

void testReimportKeepsProgress()
{
    QTemporaryDir dir;
    const QString csv = dir.filePath(QStringLiteral("words.csv"));
    QFile f(csv);
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(sampleCsv().toUtf8());
    f.close();

    WordStore store(dir.filePath(QStringLiteral("test.db")));
    store.importCsv(csv, false);
    makeList(store, {QStringLiteral("the"), QStringLiteral("of"),
                     QStringLiteral("and"), QStringLiteral("to"),
                     QStringLiteral("a")});
    const QVector<Word> words = store.learnCards(1);
    store.answerStudy(words.first().itemId, true);
    store.importCsv(csv, false);
    const auto w = store.findInCurrentList(words.first().word);
    check(w.has_value() && w->box == 6, "reimport keeps progress");
}

void testAnswerStudy()
{
    QTemporaryDir dir;
    WordStore store(dir.filePath(QStringLiteral("test.db")));
    makeList(store, {QStringLiteral("kernel"),
                     QStringLiteral("folder")});
    const QDate day(2026, 1, 1);

    QVector<Word> cards = store.learnCards(10);
    check(cards.size() == 2, "two unlearned cards");
    const ReviewResult r1 = store.answerStudy(cards[0].itemId, true, day);
    check(r1.box == 6 && r1.wasNew, "known -> mastered");
    const auto w1 = store.findInCurrentList(cards[0].word);
    check(w1 && w1->box == 6, "mastered persisted");
    const ReviewResult r2 =
        store.answerStudy(cards[1].itemId, false, day);
    check(r2.box == 0 && r2.wasNew, "unknown stays unlearned");
    const auto w2 = store.findInCurrentList(cards[1].word);
    check(w2 && w2->box == 0 && w2->reviewCount == 1,
          "unknown keeps review count");
    check(store.learnCards(10).isEmpty(), "learned card leaves learn queue");
    check(store.reviewCards(10).size() == 1
              && store.reviewCards(10).first().word
                  == QStringLiteral("folder"),
          "unknown card goes to review queue");
}

void testMarkAndReset()
{
    QTemporaryDir dir;
    WordStore store(dir.filePath(QStringLiteral("test.db")));
    const qint64 listId = makeList(store, {QStringLiteral("kernel"),
                                           QStringLiteral("folder")});
    const QVector<Word> cards = store.learnCards(10);
    store.markItemKnown(cards[0].itemId);
    check(store.findInCurrentList(cards[0].word)->box == 6,
          "mark known -> box6");
    store.resetItem(cards[0].itemId);
    check(store.findInCurrentList(cards[0].word)->box == 0,
          "reset item -> box0");
    store.resetList(listId);
    check(store.knownInWordList(listId) == 0, "reset list clears progress");
}

void testAddWordAndSearch()
{
    QTemporaryDir dir;
    WordStore store(dir.filePath(QStringLiteral("test.db")));
    const qint64 id = store.addWord(QStringLiteral("kernel"),
                                    QStringLiteral("n."),
                                    QStringLiteral("内核"));
    check(id > 0, "add word");
    check(store.search(QStringLiteral("kernel")).size() == 1, "search word");
    check(store.search(QStringLiteral("内核")).size() == 1, "search meaning");
    check(store.addWord(QStringLiteral("kernel"), QString(), QString()) < 0,
          "duplicate rejected");
}

void testDailyLogAndStreak()
{
    QTemporaryDir dir;
    WordStore store(dir.filePath(QStringLiteral("test.db")));
    makeList(store, {QStringLiteral("kernel"),
                     QStringLiteral("folder")});
    const QVector<Word> words = store.learnCards(2);
    store.answerStudy(words[0].itemId, true, QDate(2026, 1, 1));
    store.answerStudy(words[1].itemId, false, QDate(2026, 1, 1));
    store.answerStudy(words[0].itemId, true, QDate(2026, 1, 2));
    const DailySummary s = store.dailySummary(QDate(2026, 1, 2));
    check(s.newCount == 0 && s.reviewCount == 1
              && s.correct == 1 && s.wrong == 0,
          "daily summary");
    check(store.streak(QDate(2026, 1, 2)) == 2, "streak 2 days");
}

void testCsvParser()
{
    const auto rows = parseCsv(
        QStringLiteral("a,b,c\n\"x,y\",\"he said \"\"hi\"\"\",z\n"));
    check(rows.size() == 2, "csv rows");
    check(rows[0].size() == 3 && rows[0][1] == QStringLiteral("b"),
          "csv simple row");
    check(rows[1][0] == QStringLiteral("x,y"), "csv quoted comma");
    check(rows[1][1] == QStringLiteral("he said \"hi\""), "csv escaped quote");
    const auto emptyRow = parseCsv(QStringLiteral("a,,b\n"));
    check(!emptyRow[0][1].isNull() && emptyRow[0][1].isEmpty(),
          "csv empty field is empty string, not null");
}

void testArticleSaveAndStats()
{
    QTemporaryDir dir;
    WordStore store(dir.filePath(QStringLiteral("test.db")));
    makeList(store, {QStringLiteral("the")});
    const QVector<Word> cards = store.learnCards(10);
    store.answerStudy(cards.first().itemId, true); // "the" 变为已掌握

    const qint64 articleId = store.saveArticle(
        QStringLiteral("Test Article"),
        QStringLiteral("The of and to a zebra zebra!"),
        QStringLiteral("test"), 2);
    check(articleId > 0, "save article");
    const auto article = store.getArticle(articleId);
    check(article && article->title == QStringLiteral("Test Article")
              && article->difficulty == 2,
          "get article");
    check(store.listArticles().size() == 1, "list articles");

    const ArticleStats stats = store.articleStats(articleId);
    check(stats.total == 7, "stats total");
    check(stats.known == 1, "stats known");
    check(stats.inList == 1, "stats in list");
    check(stats.outOfList == 6, "stats out of list");
    check(qAbs(stats.coverage - 100.0 / 7.0) < 0.01, "stats coverage");

    store.deleteArticle(articleId);
    check(store.listArticles().isEmpty(), "delete article");
}

void testPoolPromoteAndPriority()
{
    QTemporaryDir dir;
    WordStore store(dir.filePath(QStringLiteral("test.db")));
    const qint64 articleId = store.saveArticle(
        QStringLiteral("Cat Story"),
        QStringLiteral("The cat is here. The dog runs."),
        QStringLiteral("test"), 1);

    check(store.addToPool(articleId, QStringLiteral("the"),
                          QStringLiteral("The cat is here.")),
          "add to pool");
    check(store.poolList().size() == 1, "pool has one row");
    check(store.addToPool(articleId, QStringLiteral("the"),
                          QStringLiteral("dup")),
          "duplicate pool add ignored");
    check(store.poolList().size() == 1, "pool still one row");

    const qint64 poolId = store.poolList().first().id;
    check(store.promoteFromPool(poolId), "promote from pool");
    check(store.poolList().isEmpty(), "pool empty after promote");
    const auto word = store.findWordByText(QStringLiteral("the"));
    check(word.has_value(), "promoted word in dictionary");
    check(word && word->exampleSentence == QStringLiteral("The cat is here."),
          "promoted word keeps example sentence");

    // 词表外生词：池中释义自动带入新词
    check(store.addToPool(articleId, QStringLiteral("cat"),
                          QStringLiteral("The cat is here."),
                          QStringLiteral("猫")),
          "add out-of-list word with meaning");
    const qint64 catPoolId = store.poolList().first().id;
    check(store.promoteFromPool(catPoolId), "promote cat");
    const auto cat = store.findWordByText(QStringLiteral("cat"));
    check(cat && cat->meaning == QStringLiteral("猫"),
          "pool meaning carried to new word");
}

void testMigration()
{
    QTemporaryDir dir;
    const QString dbPath = dir.filePath(QStringLiteral("old.db"));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), QStringLiteral("migration"));
        db.setDatabaseName(dbPath);
        db.open();
        QSqlQuery q(db);
        q.exec(QStringLiteral(
            "CREATE TABLE words ("
            "id INTEGER PRIMARY KEY, rank INTEGER, "
            "word TEXT NOT NULL UNIQUE, pos TEXT NOT NULL DEFAULT '', "
            "meaning TEXT NOT NULL DEFAULT '', box INTEGER NOT NULL DEFAULT 0, "
            "due TEXT, review_count INTEGER NOT NULL DEFAULT 0, "
            "correct_count INTEGER NOT NULL DEFAULT 0, "
            "wrong_count INTEGER NOT NULL DEFAULT 0)"));
        q.exec(QStringLiteral(
            "CREATE TABLE settings (key TEXT PRIMARY KEY, value TEXT NOT NULL)"));
        q.exec(QStringLiteral(
            "CREATE TABLE daily_log (date TEXT PRIMARY KEY, "
            "new_count INTEGER NOT NULL DEFAULT 0, "
            "review_count INTEGER NOT NULL DEFAULT 0, "
            "correct INTEGER NOT NULL DEFAULT 0, "
            "wrong INTEGER NOT NULL DEFAULT 0)"));
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("migration"));

    WordStore store(dbPath);
    QSqlQuery pragma =
        store.rawQuery(QStringLiteral("PRAGMA table_info(words)"));
    bool hasExample = false;
    bool hasPriority = false;
    bool hasPhonetic = false;
    while (pragma.next()) {
        if (pragma.value(1).toString() == QStringLiteral("example_sentence"))
            hasExample = true;
        if (pragma.value(1).toString() == QStringLiteral("queue_priority"))
            hasPriority = true;
        if (pragma.value(1).toString() == QStringLiteral("phonetic"))
            hasPhonetic = true;
    }
    check(hasExample && hasPriority && hasPhonetic,
          "migration adds new columns");
    QSqlQuery itemPragma = store.rawQuery(QStringLiteral(
        "PRAGMA table_info(word_list_items)"));
    bool hasBox = false;
    bool hasReviewCount = false;
    while (itemPragma.next()) {
        if (itemPragma.value(1).toString() == QStringLiteral("box"))
            hasBox = true;
        if (itemPragma.value(1).toString()
            == QStringLiteral("review_count")) {
            hasReviewCount = true;
        }
    }
    check(hasBox && hasReviewCount,
          "word list items migration adds progress columns");
    QSqlQuery listPragma = store.rawQuery(QStringLiteral(
        "PRAGMA table_info(word_lists)"));
    bool hasSortOrder = false;
    while (listPragma.next()) {
        if (listPragma.value(1).toString() == QStringLiteral("sort_order"))
            hasSortOrder = true;
    }
    check(hasSortOrder, "word lists migration adds sort_order column");
    const qint64 articleId = store.saveArticle(
        QStringLiteral("Migrated"), QStringLiteral("hello world"),
        QStringLiteral("test"), 1);
    check(articleId > 0, "articles table works after migration");
}

void testAiPrompts()
{
    const QString topic = AiClient::topicPrompt(
        QStringLiteral("linux"), 2, 150);
    check(topic.contains(QStringLiteral("linux")), "topic prompt has topic");
    check(topic.contains(QStringLiteral("150 words")), "topic prompt has length");
    check(topic.contains(QStringLiteral("2000")), "topic prompt has level");

    const QString rewrite = AiClient::rewritePrompt(
        QStringLiteral("Hello world."), 1);
    check(rewrite.contains(QStringLiteral("Hello world.")),
          "rewrite prompt has source");
    const QString translate = AiClient::translatePrompt(
        QStringLiteral("The cat is here."));
    check(translate.contains(QStringLiteral("The cat is here."))
              && translate.contains(QStringLiteral("Chinese")),
          "translate prompt has source and target");
    const QString reverse = AiClient::translatePrompt(
        QStringLiteral("你好"), false);
    check(reverse.contains(QStringLiteral("你好"))
              && reverse.contains(QStringLiteral("English")),
          "translate prompt reverse direction");
    const QString listPrompt = AiClient::wordListPrompt(
        QStringLiteral("linux"), 50);
    check(listPrompt.contains(QStringLiteral("linux"))
              && listPrompt.contains(QStringLiteral("50")),
          "word list prompt");
    const QString constrained = AiClient::topicPrompt(
        QStringLiteral("linux"), 2, 100,
        {QStringLiteral("kernel"), QStringLiteral("directory")});
    check(constrained.contains(QStringLiteral("kernel"))
              && constrained.contains(QStringLiteral("directory")),
          "topic prompt with preferred words");
    check(AiClient::levelLabel(1).contains(QStringLiteral("1000")),
          "level label");
}

void testLemmaNormalization()
{
    QTemporaryDir dir;
    const QString lemmaPath = dir.filePath(QStringLiteral("lemma.txt"));
    QFile lf(lemmaPath);
    if (!lf.open(QIODevice::WriteOnly))
        return;
    lf.write("; comment line\nfile/100 -> files,filed\nstore/50 -> stores,stored\n");
    lf.close();

    WordStore store(dir.filePath(QStringLiteral("test.db")));
    check(store.importWordForms(lemmaPath) > 0, "import word forms");
    check(store.lookupLemma(QStringLiteral("files")) == QStringLiteral("file"),
          "lookup lemma files");
    check(store.lookupLemma(QStringLiteral("stored")) == QStringLiteral("store"),
          "lookup lemma stored");
    check(store.lookupLemma(QStringLiteral("unknownxyz"))
              == QStringLiteral("unknownxyz"),
          "lemma identity fallback");
    const QString inflections = store.inflectionSummary(QStringLiteral("file"));
    const QStringList formList = inflections.split(QStringLiteral(", "));
    check(formList.contains(QStringLiteral("files"))
              && !formList.contains(QStringLiteral("file")),
          "inflection summary lists forms, excludes base");
    check(store.addInflections(QStringLiteral("file"),
                               {QStringLiteral("files2"),
                                QStringLiteral("filed")}),
          "add generated inflections");
    check(store.inflectionSummary(QStringLiteral("file"))
              .contains(QStringLiteral("filed")),
          "generated inflections cached and shown");

    store.addWord(QStringLiteral("file"), QStringLiteral("n."),
                  QStringLiteral("文件"));
    const qint64 listId = store.createWordList(
        QStringLiteral("读"), {}, QStringLiteral("manual"));
    store.addWordToList(listId, QStringLiteral("file"),
                        QStringLiteral("n."), QStringLiteral("文件"), 0);
    store.setCurrentWordList(listId);
    const qint64 id = store.saveArticle(
        QStringLiteral("T"), QStringLiteral("files folder"), QStringLiteral("test"), 1);
    const ArticleStats s = store.articleStats(id);
    check(s.inList == 1 && s.outOfList == 1, "stats use lemma mapping");
}

void testDictAndCoverage()
{
    QTemporaryDir dir;
    const QString dictPath = dir.filePath(QStringLiteral("dict.csv"));
    QFile df(dictPath);
    if (!df.open(QIODevice::WriteOnly))
        return;
    df.write(
        "word,phonetic,definition,translation,pos,collins,oxford,"
        "tag,bnc,frq,exchange,detail,audio\n"
        "kernel,,,内核,n.\n"
        "folder,,,文件夹,n.\n");
    df.close();

    WordStore store(dir.filePath(QStringLiteral("test.db")));
    check(!store.dictReady(), "dict not ready initially");
    check(store.importDictCsv(dictPath) == 2, "import dict csv");
    check(store.dictReady(), "dict ready after import");
    const auto kernel = store.lookupDict(QStringLiteral("kernel"));
    check(kernel && kernel->meaning == QStringLiteral("内核")
              && kernel->pos == QStringLiteral("n."),
          "dict lookup kernel");
    check(!store.lookupDict(QStringLiteral("nonexistentxyz")),
          "dict miss");

    store.addWord(QStringLiteral("file"), QStringLiteral("n."),
                  QStringLiteral("文件"));
    store.rawQuery(
        "INSERT INTO word_forms(form, lemma) VALUES('files', 'file')");
    const qint64 articleId = store.saveArticle(
        QStringLiteral("T"), QStringLiteral("files folder"),
        QStringLiteral("test"), 1);
    store.logCoverage(articleId);
    check(store.coverageHistory(30).size() == 1, "coverage history logged");
    check(store.coverageArticleCount() == 1, "coverage article count");
}

void testTranslationLinkage()
{
    QTemporaryDir dir;
    WordStore store(dir.filePath(QStringLiteral("test.db")));
    store.addWord(QStringLiteral("file"), QStringLiteral("n."),
                  QStringLiteral("文件"));
    makeList(store, {QStringLiteral("file")}); // 已在词表 → 不算生词

    const QString text =
        QStringLiteral("The kernel file is stored here. Folders keep files safe.");
    const QVector<Word> unknown = store.extractUnknownWords(text);
    check(!unknown.isEmpty(), "extract unknown words");
    bool hasKernel = false;
    bool hasFile = false;
    for (const Word &u : unknown) {
        if (u.word == QStringLiteral("kernel"))
            hasKernel = true;
        if (u.word == QStringLiteral("file"))
            hasFile = true;
    }
    check(hasKernel, "unknown word listed");
    check(!hasFile, "learned word skipped");
    check(unknown.size() <= 20, "unknown words capped");

    const qint64 id = store.queueWordFromTranslation(
        QStringLiteral("kernel"), QStringLiteral("内核"),
        QStringLiteral("The kernel file is stored here."));
    check(id > 0, "queue kernel");
    const auto kernel = store.findWordByText(QStringLiteral("kernel"));
    check(kernel && kernel->exampleSentence.contains(QStringLiteral("kernel")),
          "example sentence carried");
    check(store.queueWordFromTranslation(
              QStringLiteral("kernel"), QStringLiteral("内核"), {}) == id,
          "queue dedup by word");
    check(WordStore::sentenceContaining(
              text, QStringLiteral("folders"))
              == QStringLiteral("Folders keep files safe."),
          "sentence containing word");
    const auto kernelItem =
        store.findInNamedList(QStringLiteral("翻译生词"),
                              QStringLiteral("kernel"));
    check(kernelItem.has_value(), "translated word added to translation list");
    store.answerStudy(kernelItem->itemId, true);
    const auto transList = store.listWordLists(
        QStringLiteral("翻译生词"));
    check(transList.size() == 1
              && store.knownInWordList(transList.first().id) == 1,
          "known in translation list counts mastered word");
}

void testWordLists()
{
    QTemporaryDir dir;
    WordStore store(dir.filePath(QStringLiteral("test.db")));
    const qint64 listId = store.createWordList(
        QStringLiteral("Linux 运维"), QStringLiteral("系统管理相关"),
        QStringLiteral("manual"));
    check(listId > 0, "create word list");
    check(store.createWordList(
              QStringLiteral("Linux 运维"), {}, QStringLiteral("manual")) < 0,
          "duplicate list name rejected");
    check(store.addWordToList(listId, QStringLiteral("kernel"),
                              QStringLiteral("n."),
                              QStringLiteral("内核"), 0),
          "add word to list");
    check(store.addWordToList(listId, QStringLiteral("directory"),
                              QStringLiteral("n."),
                              QStringLiteral("目录"), 1),
          "add second word");
    check(store.wordsInWordList(listId).size() == 2,
          "words in list");
    check(store.listWordLists().size() == 1, "list word lists");
    check(store.listWordLists(QStringLiteral("运维")).size() == 1,
          "search word lists");
    store.setCurrentWordList(listId);
    check(store.currentWordListId() == listId
              && store.currentWordListName() == QStringLiteral("Linux 运维"),
          "current word list");
    check(store.currentListWords().contains(QStringLiteral("kernel"))
              && store.currentListWords().contains(QStringLiteral("directory")),
          "current list words set");

    const qint64 secondId = store.createWordList(
        QStringLiteral("第二个"), {}, QStringLiteral("manual"));
    store.setWordListOrder(secondId, 0);
    store.setWordListOrder(listId, 1);
    check(store.listWordLists().first().id == secondId,
          "word lists order by drag order");
    check(store.queueWordToReadingList(
              QStringLiteral("kernel"), QStringLiteral("内核"),
              QStringLiteral("The kernel runs."))
              > 0,
          "queue word to reading list");
    check(store.findInNamedList(QStringLiteral("阅读生词"),
                                QStringLiteral("kernel"))
              .has_value(),
          "reading list lookup works");
    const auto kernelLists =
        store.listsContainingWord(QStringLiteral("kernel"));
    bool hasReading = false;
    bool hasLinux = false;
    for (const WordListInfo &info : kernelLists) {
        if (info.name == QStringLiteral("阅读生词"))
            hasReading = true;
        if (info.name == QStringLiteral("Linux 运维"))
            hasLinux = true;
    }
    check(kernelLists.size() == 2 && hasReading && hasLinux,
          "lists containing word");
    const auto readItem =
        store.findInNamedList(QStringLiteral("阅读生词"),
                              QStringLiteral("kernel"));
    store.markItemKnown(readItem->itemId);
    check(store.masteredListWords().contains(QStringLiteral("kernel")),
          "mastered list words set");
    store.resetWordInAllLists(QStringLiteral("kernel"));
    check(!store.masteredListWords().contains(QStringLiteral("kernel"))
              && store.allListWords().contains(QStringLiteral("kernel")),
          "reset word in all lists");

    const QString articleText =
        QStringLiteral("The kernel manages memory. The kernel schedules "
                       "processes. Directories hold files. the and of");
    const QVector<Word> domain =
        store.extractDomainWords(articleText, 10);
    check(!domain.isEmpty() && domain.first().word
              == QStringLiteral("kernel"),
          "extract domain words by frequency");

    const QVector<Word> cards = store.learnCards(10);
    check(cards.size() == 2, "study cards from current list");
    store.answerStudy(cards.first().itemId, true);
    check(store.knownInWordList(listId) == 1,
          "known count after answer");
    store.resetList(listId);
    check(store.knownInWordList(listId) == 0,
          "reset list progress");

    check(store.deleteWordList(listId), "delete word list");
    bool deletedGone = true;
    for (const WordListInfo &info : store.listWordLists()) {
        if (info.id == listId)
            deletedGone = false;
    }
    check(deletedGone, "deleted list removed");
    check(store.currentWordListId() == -1, "current list reset on delete");
}

void testSeedBuiltinWordList()
{
    QTemporaryDir dir;
    const QString csv = dir.filePath(QStringLiteral("words.csv"));
    QFile f(csv);
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(sampleCsv().toUtf8());
    f.close();
    WordStore store(dir.filePath(QStringLiteral("test.db")));
    store.importCsv(csv, false);
    const int count = store.seedBuiltinWordList();
    check(count == 5, "seed builtin word list");
    check(store.seedBuiltinWordList() == 0, "seed idempotent");
    const QVector<WordListInfo> lists = store.listWordLists();
    check(lists.size() == 1
              && lists.first().name == QStringLiteral("核心 3000")
              && lists.first().source == QStringLiteral("builtin")
              && lists.first().wordCount == 5,
          "builtin list visible with words");
    store.setCurrentWordList(lists.first().id);
    const Counts c = store.counts();
    check(c.total == 5 && c.newTotal == 5 && c.mastered == 0,
          "builtin list progress starts fresh");
}

void testExamples()
{
    QTemporaryDir dir;
    WordStore store(dir.filePath(QStringLiteral("test.db")));
    store.addWord(QStringLiteral("kernel"), QStringLiteral("n."),
                  QStringLiteral("内核"));
    store.addWord(QStringLiteral("the"), QStringLiteral("art."),
                  QStringLiteral("art. 那"));
    store.saveArticle(
        QStringLiteral("T"),
        QStringLiteral("The kernel is the heart of the system."),
        QStringLiteral("test"), 1);
    check(store.seedExamplesFromArticles() == 2,
          "seed examples from articles");
    const auto kernel = store.findWordByText(QStringLiteral("kernel"));
    check(kernel && !kernel->exampleSentence.isEmpty()
              && kernel->exampleSentence.contains(QStringLiteral("kernel")),
          "kernel got example");
    store.setExampleSentence(kernel->id,
                             QStringLiteral("Custom sentence here."));
    check(store.findWordByText(QStringLiteral("kernel"))->exampleSentence
              == QStringLiteral("Custom sentence here."),
          "set example sentence");
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    testImport();
    testReimportKeepsProgress();
    testAnswerStudy();
    testMarkAndReset();
    testAddWordAndSearch();
    testDailyLogAndStreak();
    testCsvParser();
    testArticleSaveAndStats();
    testPoolPromoteAndPriority();
    testMigration();
    testAiPrompts();
    testLemmaNormalization();
    testDictAndCoverage();
    testTranslationLinkage();
    testWordLists();
    testSeedBuiltinWordList();
    testExamples();

    if (g_failures == 0) {
        std::printf("all tests passed\n");
        return 0;
    }
    std::printf("%d test(s) failed\n", g_failures);
    return 1;
}
