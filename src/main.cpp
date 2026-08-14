#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPair>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

#include "core.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("english3000"));
    QCoreApplication::setOrganizationName(QStringLiteral("liang"));

    QString screenshotPath;
    int screenshotTab = 0;
    const QStringList args = app.arguments();
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
    if (screenshotPath.isEmpty()) {
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
