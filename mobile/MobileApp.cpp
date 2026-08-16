#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QUrl>

#include "ai_client.h"
#include "bridge.h"
#include "core.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
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

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"),
                                             &bridge);
    engine.load(QUrl(QStringLiteral("qrc:/mobile/qml/main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
