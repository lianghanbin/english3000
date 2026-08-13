#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPair>
#include <QStandardPaths>

#include "core.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("english3000"));
    QCoreApplication::setOrganizationName(QStringLiteral("liang"));

    const QString dataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
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
    window.show();
    return app.exec();
}
