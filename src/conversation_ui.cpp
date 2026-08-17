#include "conversation_ui.h"

#include "ai_client.h"
#include "core.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QVBoxLayout>

ConversationWindow::ConversationWindow(WordStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
{
    setWindowTitle(QStringLiteral("对话练习"));
    resize(620, 560);

    m_ai = new AiClient(this);
    applyAiSettings();
    connect(m_ai, &AiClient::chatFinished, this,
            &ConversationWindow::onChatFinished);
    connect(m_ai, &AiClient::failed, this,
            &ConversationWindow::onChatFailed);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto *titleRow = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("对话练习"), this);
    title->setObjectName(QStringLiteral("wordLabel"));
    titleRow->addWidget(title);
    titleRow->addStretch();
    m_speakButton = new QPushButton(QStringLiteral("🔊 朗读回答"), this);
    m_speakButton->setEnabled(false);
    connect(m_speakButton, &QPushButton::clicked, this,
            &ConversationWindow::speakLast);
    titleRow->addWidget(m_speakButton);
    auto *closeButton = new QPushButton(QStringLiteral("✕"), this);
    connect(closeButton, &QPushButton::clicked, this, &QWidget::close);
    titleRow->addWidget(closeButton);
    layout->addLayout(titleRow);

    m_log = new QTextBrowser(this);
    m_log->setOpenExternalLinks(false);
    layout->addWidget(m_log, 1);

    auto *inputRow = new QHBoxLayout;
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(QStringLiteral("用英文回答…"));
    connect(m_input, &QLineEdit::returnPressed, this,
            &ConversationWindow::sendMessage);
    inputRow->addWidget(m_input, 1);
    m_sendButton = new QPushButton(QStringLiteral("发送"), this);
    m_sendButton->setObjectName(QStringLiteral("primaryButton"));
    connect(m_sendButton, &QPushButton::clicked, this,
            &ConversationWindow::sendMessage);
    inputRow->addWidget(m_sendButton);
    layout->addLayout(inputRow);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("hintLabel"));
    layout->addWidget(m_statusLabel);
}

void ConversationWindow::applyAiSettings()
{
    m_ai->setEndpoint(
        m_store->getSetting(QStringLiteral("ai_base_url"),
                            QStringLiteral("http://127.0.0.1:11434")),
        m_store->getSetting(QStringLiteral("ai_model"),
                            QStringLiteral("qwen2.5:1.5b")));
    const bool openAi =
        m_store->getSetting(QStringLiteral("ai_provider"),
                            QStringLiteral("ollama"))
        == QLatin1String("openai");
    m_ai->setProvider(openAi ? AiClient::Provider::OpenAI
                             : AiClient::Provider::Ollama);
    m_ai->setApiKey(
        m_store->getSetting(QStringLiteral("ai_api_key")));
}

ConversationWindow::~ConversationWindow()
{
    if (m_tts) {
        m_tts->kill();
        m_tts->deleteLater();
    }
    if (m_player) {
        m_player->kill();
        m_player->deleteLater();
    }
}

void ConversationWindow::openWithArticle(const QString &title,
                                         const QString &content)
{
    m_title = title;
    m_context = content.simplified();
    if (m_context.size() > 2000)
        m_context = m_context.left(2000) + QStringLiteral("…");
    m_history.clear();
    m_lastReply.clear();
    m_log->clear();
    m_log->append(
        QStringLiteral("【对话练习】文章：%1\nAI 会先用英文提问，你用英文回答，"
                       "它会纠错并继续追问。")
            .arg(title));
    m_speakButton->setEnabled(false);
    m_sendButton->setEnabled(false);
    m_statusLabel->setText(QStringLiteral("AI 准备第一个问题…"));
    m_ai->chat(buildPrompt());

    show();
    raise();
    activateWindow();
    m_input->setFocus();
}

QString ConversationWindow::buildPrompt() const
{
    QString prompt = QStringLiteral(
        "You are an English conversation partner for a Chinese learner.\n"
        "We are discussing this article (title: %1):\n%2\n\n"
        "Rules:\n"
        "- Ask ONE short English question about the article at a time.\n"
        "- After the student answers, give brief feedback: praise first, "
        "then gently correct any mistakes with the correct sentence, "
        "then ask a follow-up question.\n"
        "- Keep every reply under 80 words.\n\n")
        .arg(m_title, m_context);
    for (const QString &line : m_history)
        prompt += line + QLatin1Char('\n');
    prompt += QStringLiteral("AI:");
    return prompt;
}

void ConversationWindow::sendMessage()
{
    const QString text = m_input->text().trimmed();
    if (text.isEmpty() || !m_sendButton->isEnabled())
        return;
    m_input->clear();
    m_log->append(QStringLiteral("<b>你：</b>%1").arg(text.toHtmlEscaped()));
    m_history << QStringLiteral("Student: %1").arg(text);
    m_sendButton->setEnabled(false);
    m_statusLabel->setText(QStringLiteral("AI 思考中…"));
    m_ai->chat(buildPrompt());
}

void ConversationWindow::onChatFinished(const QString &response)
{
    const QString reply = response.trimmed();
    m_lastReply = reply;
    m_log->append(QStringLiteral("<b>AI：</b>%1").arg(reply.toHtmlEscaped()));
    m_history << QStringLiteral("AI: %1").arg(reply);
    m_sendButton->setEnabled(true);
    m_speakButton->setEnabled(!reply.isEmpty());
    m_statusLabel->setText(QStringLiteral("回答已生成，正在朗读…"));
    speak(reply);
    m_input->setFocus();
}

void ConversationWindow::onChatFailed(const QString &message)
{
    m_sendButton->setEnabled(true);
    m_statusLabel->setText(QStringLiteral("对话失败：%1").arg(message));
}

void ConversationWindow::speakLast()
{
    if (!m_lastReply.isEmpty())
        speak(m_lastReply);
}

void ConversationWindow::speak(const QString &text)
{
    if (text.isEmpty())
        return;
#if defined(Q_OS_WIN)
    if (m_tts) {
        m_tts->kill();
        m_tts->deleteLater();
        m_tts = nullptr;
    }
    if (m_player) {
        m_player->kill();
        m_player->deleteLater();
        m_player = nullptr;
    }
    m_tts = new QProcess(this);
    connect(m_tts,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
                m_tts->deleteLater();
                m_tts = nullptr;
            });
    QString escaped = text;
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
    const QString command =
        QStringLiteral(
            "Add-Type -AssemblyName System.Speech;"
            "$s=New-Object System.Speech.Synthesis.SpeechSynthesizer;"
            "$v=$s.GetInstalledVoices()|Where-Object"
            "{$_.VoiceInfo.Culture -like 'en*'}|Select-Object -First 1;"
            "if($v){$s.SelectVoice($v.VoiceInfo.Name)};"
            "$s.Speak('%1')")
            .arg(escaped);
    m_tts->start(QStringLiteral("powershell"),
                 {QStringLiteral("-NoProfile"),
                  QStringLiteral("-Command"), command});
    return;
#else
    if (m_tts) {
        m_tts->kill();
        m_tts->deleteLater();
        m_tts = nullptr;
    }
    if (m_player) {
        m_player->kill();
        m_player->deleteLater();
        m_player = nullptr;
    }
    QString piperPath;
    const QString localPiper =
        QDir::homePath() + QStringLiteral("/.local/bin/piper");
    if (QFileInfo::exists(localPiper)) {
        piperPath = localPiper;
    } else {
        piperPath = QStandardPaths::findExecutable(QStringLiteral("piper"));
    }
    const QString voicePath =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/voices/en_US-lessac-medium.onnx");
    const QString wavPath =
        QDir::tempPath() + QStringLiteral("/english3000_chat_tts.wav");

    m_tts = new QProcess(this);
    if (!piperPath.isEmpty() && QFileInfo::exists(voicePath)) {
        m_tts->setProcessChannelMode(QProcess::SeparateChannels);
        m_tts->setStandardOutputFile(QProcess::nullDevice());
        m_tts->setStandardErrorFile(QProcess::nullDevice());
        m_tts->start(piperPath,
                     {QStringLiteral("-m"), voicePath,
                      QStringLiteral("-f"), wavPath});
        if (m_tts->waitForStarted(3000)) {
            m_tts->write(text.toUtf8());
            m_tts->closeWriteChannel();
            QProcess::connect(m_tts,
                              QOverload<int, QProcess::ExitStatus>::of(
                                  &QProcess::finished),
                              this, [this, wavPath](int, QProcess::ExitStatus) {
                                  m_player = new QProcess(this);
                                  m_player->start(QStringLiteral("paplay"),
                                                  {wavPath});
                              });
            return;
        }
    }
    m_tts->start(QStringLiteral("espeak-ng"),
                 {QStringLiteral("-v"), QStringLiteral("en-us"),
                  QStringLiteral("-s"), QStringLiteral("155"),
                  QStringLiteral("-w"), wavPath, text});
    QProcess::connect(m_tts,
                      QOverload<int, QProcess::ExitStatus>::of(
                          &QProcess::finished),
                      this, [this, wavPath](int, QProcess::ExitStatus) {
                          m_player = new QProcess(this);
                          m_player->start(QStringLiteral("paplay"), {wavPath});
                      });
#endif
}

void ConversationWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}
