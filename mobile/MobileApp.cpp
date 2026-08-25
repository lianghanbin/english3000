#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include "ai_client.h"
#include "bridge.h"
#include "core.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
#if defined(Q_OS_ANDROID)
    // Waydroid 下 Qt 的 androidmedia 后端与系统集成有问题
    // (QtAudioDeviceManager setActivity 缺失),强制用 ffmpeg 后端播放。
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");
    // 使用应用内置的 OpenSSL 3 库做 TLS,避免加载系统 BoringSSL。
    qputenv("ANDROID_OPENSSL_SUFFIX", "_3");
#endif
    QCoreApplication::setApplicationName(QStringLiteral("english3000"));
    QCoreApplication::setOrganizationName(QStringLiteral("liang"));

    const QString dataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);

    WordStore store(dataDir + QStringLiteral("/english3000.db"));
    if (store.countWords() == 0) {
        store.importCsv(QStringLiteral(":/assets/oxford3000.csv"), false);
        store.importWordForms(QStringLiteral(":/assets/lemma.en.txt"));
        store.seedBuiltinWordList();
    }
    if (store.currentWordListId() < 0) {
        const auto lists = store.listWordLists();
        if (!lists.isEmpty())
            store.setCurrentWordList(lists.first().id);
    }
    if (store.listArticles().isEmpty()) {
        struct SampleArticle {
            const char *path;
            const char *title;
            int level;
        };
        const SampleArticle samples[] = {
            {"assets/samples/linux-files.txt", "The Linux File System", 2},
            {"assets/samples/morning-routine.txt", "My Morning Routine", 1},
            {"assets/samples/coffee.txt", "A Cup of Coffee", 2},
        };
        for (const SampleArticle &sample : samples) {
            QFile file(QStringLiteral(":/") + QString::fromLatin1(sample.path));
            if (!file.open(QIODevice::ReadOnly))
                continue;
            store.saveArticle(QString::fromLatin1(sample.title),
                              QString::fromUtf8(file.readAll()),
                              QStringLiteral("sample"), sample.level);
        }
    }
    // seedExamplesFromArticles 会对每个缺例句的词遍历所有文章做子串匹配,
    // 是启动期最重的同步操作。放到首帧显示后再用 0 延时执行,让界面先出来。
    QTimer::singleShot(0, [&store]() { store.seedExamplesFromArticles(); });
    AiClient ai;
    ai.setEndpoint(
        store.getSetting(QStringLiteral("ai_base_url"),
                         QStringLiteral("http://127.0.0.1:11434")),
        store.getSetting(QStringLiteral("ai_model"),
                         QStringLiteral("qwen2.5:1.5b")));
    const bool openAi =
        store.getSetting(QStringLiteral("ai_provider"),
                         QStringLiteral("ollama"))
        == QLatin1String("openai");
    ai.setProvider(openAi ? AiClient::Provider::OpenAI
                          : AiClient::Provider::Ollama);
    ai.setApiKey(store.getSetting(QStringLiteral("ai_api_key")));

    MobileBridge bridge(&store, &ai);

    // 手机端内置完整离线词典(ECDICT),首次启动后台导入,完成后通知界面
    if (!store.dictReady()) {
        const QString dictPath =
            dataDir + QStringLiteral("/dict.db");
        auto *watcher = new QFutureWatcher<int>(&app);
        QObject::connect(watcher, &QFutureWatcher<int>::finished, &app,
                         [watcher, &bridge]() {
                             bridge.notifyDictReady();
                             watcher->deleteLater();
                         });
        watcher->setFuture(QtConcurrent::run([dictPath]() {
            return importDictCsvInto(
                dictPath, QStringLiteral(":/assets/dict/ecdict.csv"));
        }));
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"),
                                             &bridge);
    engine.load(QUrl(QStringLiteral("qrc:/mobile/qml/main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
