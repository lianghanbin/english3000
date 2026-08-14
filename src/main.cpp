#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPair>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

#include <functional>

#include "core.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("english3000"));
    QCoreApplication::setOrganizationName(QStringLiteral("liang"));

    const QStringList args = app.arguments();
    QString screenshotPath;
    int screenshotTab = 0;
    bool demoMode = args.contains(QStringLiteral("--demo"));
    const int shotIdx = args.indexOf(QStringLiteral("--screenshot"));
    if (shotIdx >= 0 && shotIdx + 1 < args.size()) {
        screenshotPath = args.at(shotIdx + 1);
        const int tabIdx = args.indexOf(QStringLiteral("--tab"));
        if (tabIdx >= 0 && tabIdx + 1 < args.size())
            screenshotTab = args.at(tabIdx + 1).toInt();
    }

    QTemporaryDir shotDir;
    const QString dataDir =
        screenshotPath.isEmpty()
            ? QStandardPaths::writableLocation(
                  QStandardPaths::AppDataLocation)
            : shotDir.path();
    QDir().mkpath(dataDir);

    // 单实例保护：已有实例时通知它显示并退出
    QLocalServer singleServer;
    if (screenshotPath.isEmpty() && !demoMode) {
        const QString singleName = QStringLiteral("english3000-single");
        QLocalSocket probe;
        probe.connectToServer(singleName);
        if (probe.waitForConnected(300)) {
            probe.write("show");
            probe.flush();
            return 0;
        }
        QLocalServer::removeServer(singleName);
        if (!singleServer.listen(singleName)) {
            qWarning("单实例锁创建失败，继续启动");
        }
    }
    const QString dbPath = dataDir + QStringLiteral("/english3000.db");

    WordStore store(dbPath);
    const auto findAsset = [dataDir](const QString &name) -> QString {
        const QStringList candidates = {
            dataDir + QLatin1Char('/') + name,
            QCoreApplication::applicationDirPath()
                + QStringLiteral("/assets/") + name,
            QStringLiteral(ENGLISH3000_ASSET_DIR) + QLatin1Char('/') + name,
        };
        for (const QString &path : candidates) {
            if (QFileInfo::exists(path))
                return path;
        }
        return {};
    };

    if (store.countWords() == 0) {
        const QString csv = findAsset(QStringLiteral("oxford3000.csv"));
        if (!csv.isEmpty())
            store.importCsv(csv, false);
    }

    const QString lemmaFile = findAsset(QStringLiteral("lemma.en.txt"));
    if (!lemmaFile.isEmpty())
        store.importWordForms(lemmaFile);
    if (store.countWords() > 0)
        store.seedBuiltinWordList();
    store.seedWordPhonetics();
    // 首次使用默认选中「核心 3000」词表
    if (store.currentWordListId() <= 0) {
        const QVector<WordListInfo> lists = store.listWordLists();
        for (const WordListInfo &info : lists) {
            if (info.source == QLatin1String("builtin")) {
                store.setCurrentWordList(info.id);
                break;
            }
        }
    }

    if (store.listArticles().isEmpty()) {
        struct SampleArticle {
            const char *path;
            const char *title;
            int level;
        };
        const SampleArticle samples[] = {
            {"samples/linux-files.txt", "The Linux File System", 2},
            {"samples/morning-routine.txt", "My Morning Routine", 1},
            {"samples/coffee.txt", "A Cup of Coffee", 2},
        };
        for (const SampleArticle &sample : samples) {
            const QString path = findAsset(QString::fromLatin1(sample.path));
            if (path.isEmpty())
                continue;
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                continue;
            store.saveArticle(QString::fromLatin1(sample.title),
                              QString::fromUtf8(file.readAll()),
                              QStringLiteral("sample"), sample.level);
        }
    }
    store.seedExamplesFromArticles();

    MainWindow window(&store);
    QObject::connect(&singleServer, &QLocalServer::newConnection,
                     &window, [&window, &singleServer] {
                         while (singleServer.hasPendingConnections()) {
                             QLocalSocket *sock =
                                 singleServer.nextPendingConnection();
                             sock->deleteLater();
                         }
                         window.show();
                         window.raise();
                         window.activateWindow();
                     });
    window.show();
    if (demoMode) {
        const auto lists = store.listWordLists();
        const qint64 firstList = lists.isEmpty() ? -1 : lists.first().id;
        const qint64 secondList =
            lists.size() > 1 ? lists.at(1).id : firstList;
        const qint64 transList = store.getOrCreateWordList(
            QStringLiteral("翻译生词"),
            QStringLiteral("翻译时自动收集的生词"),
            QStringLiteral("translation"));
        const auto articles = store.listArticles();
        const qint64 articleId =
            articles.isEmpty() ? -1 : articles.first().id;
        struct Step {
            int ms;
            std::function<void()> fn;
        };
        const QVector<Step> steps = {
            {4000, [&] { window.showTab(0); }},
            {8000, [&] { window.showTab(1); }},
            {11000, [&] { window.demoJumpToList(secondList); }},
            {15000, [&] { window.demoJumpToList(firstList); }},
            {19000, [&] { window.showTab(0); }},
            {21000, [&] {
                 QMetaObject::invokeMethod(
                     &window, "startSession", Qt::QueuedConnection,
                     Q_ARG(QString, QStringLiteral("learn")));
             }},
            {25000, [&] {
                 QMetaObject::invokeMethod(&window, "reveal",
                                           Qt::QueuedConnection);
             }},
            {29000, [&] {
                 QMetaObject::invokeMethod(&window, "answer",
                                           Qt::QueuedConnection,
                                           Q_ARG(bool, true));
             }},
            {32000, [&] {
                 QMetaObject::invokeMethod(&window, "reveal",
                                           Qt::QueuedConnection);
             }},
            {36000, [&] {
                 QMetaObject::invokeMethod(&window, "answer",
                                           Qt::QueuedConnection,
                                           Q_ARG(bool, true));
             }},
            {39000, [&] {
                 QMetaObject::invokeMethod(&window, "reveal",
                                           Qt::QueuedConnection);
             }},
            {43000, [&] {
                 QMetaObject::invokeMethod(&window, "answer",
                                           Qt::QueuedConnection,
                                           Q_ARG(bool, false));
             }},
            {47000, [&] {
                 QMetaObject::invokeMethod(&window, "backToToday",
                                           Qt::QueuedConnection);
             }},
            {50000, [&] { window.showTab(2); }},
            {53000, [&] {
                 if (articleId > 0) {
                     QMetaObject::invokeMethod(
                         &window, "loadArticle", Qt::QueuedConnection,
                         Q_ARG(qint64, articleId));
                 }
             }},
            {57000, [&] { window.demoScrollReader(300); }},
            {61000, [&] { window.demoScrollReader(300); }},
            {65000, [&] { window.showTab(1); }},
            {68000, [&] { window.demoJumpToList(transList); }},
            {73000, [&] { window.demoJumpToList(firstList); }},
            {77000, [&] { window.showTab(0); }},
            {80000, [&] {
                 window.demoTranslate(
                     QStringLiteral(
                         "The quick brown fox jumps over the lazy dog."));
             }},
            {101000, [&] { window.demoHideTranslator(); }},
            {104000, [&] {
                 window.showTab(1);
                 window.demoJumpToList(transList);
             }},
            {109000, [&] { window.demoJumpToList(firstList); }},
            {113000, [&] { window.showTab(0); }},
        };
        for (const Step &step : steps)
            QTimer::singleShot(step.ms, &window, step.fn);
    }
    if (!screenshotPath.isEmpty()) {
        QTimer::singleShot(1200, &window, [&window, screenshotTab] {
            window.showTab(screenshotTab);
        });
        QTimer::singleShot(2200, &window,
                           [&window, screenshotPath] {
                               window.grab().save(screenshotPath);
                               QCoreApplication::quit();
                           });
    }
    return app.exec();
}
