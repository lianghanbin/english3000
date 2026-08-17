#pragma once

#include <QStringList>
#include <QWidget>

class AiClient;
class QLabel;
class QLineEdit;
class QProcess;
class QPushButton;
class QTextBrowser;
class WordStore;

class ConversationWindow : public QWidget {
    Q_OBJECT

public:
    explicit ConversationWindow(WordStore *store, QWidget *parent = nullptr);
    ~ConversationWindow() override;

    void applyAiSettings();
    void openWithArticle(const QString &title, const QString &content);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void sendMessage();
    void speakLast();
    void onChatFinished(const QString &response);
    void onChatFailed(const QString &message);

private:
    QString buildPrompt() const;
    void speak(const QString &text);

    WordStore *m_store = nullptr;
    AiClient *m_ai = nullptr;
    QTextBrowser *m_log = nullptr;
    QLineEdit *m_input = nullptr;
    QPushButton *m_sendButton = nullptr;
    QPushButton *m_speakButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QString m_title;
    QString m_context;
    QStringList m_history;
    QString m_lastReply;
    QProcess *m_tts = nullptr;
    QProcess *m_player = nullptr;
};
