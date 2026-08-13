#pragma once

#include <QKeyEvent>
#include <QMainWindow>
#include <QVector>

#include "core.h"

class AiClient;
class GlobalHotkey;
class QCheckBox;
class QComboBox;
class CoverageChart;
class ConversationWindow;
class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QListWidget;
class QMenu;
class QNetworkAccessManager;
class QNetworkReply;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QProcess;
class QRadioButton;
class QSpinBox;
class QStackedWidget;
class QSystemTrayIcon;
class QTabWidget;
class QTableWidget;
class QTextBrowser;
class TranslatorWindow;
class WordListPage;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(WordStore *store, QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void startSession(const QString &kind);
    void reveal();
    void answer(bool known);
    void backToToday();
    void refreshAll();
    void reimportBuiltin();
    void resetAllDialog();
    void onArticleSelected();
    void importArticleFile();
    void importUrl();
    void onWebFinished();
    void deleteCurrentArticle();
    void onWordContextMenu(const QPoint &pos);
    void showWordMenu(const QString &rawWord);
    void speakText(const QString &text);
    void stopSpeaking();
    void onTtsFinished(int exitCode);
    void onPlayFinished(int exitCode);
    void queueClickedWord();
    void markClickedWordKnown();
    void onGenerateClicked();
    void onCancelClicked();
    void onAiFinished(const QString &articleText);
    void onTranslationFinished(const QString &translation);
    void onAiFailed(const QString &message);
    void onTranslateSelection();
    void onTranslateFull();
    void startTranslate(const QString &text);
    void onTabChanged(int index);
    void refreshStats();
    void applyHotkeys();
    bool autoPronounceEnabled() const;

private:
    void buildDashboard();
    void buildStudy();
    void buildReading();
    void buildAi(QWidget *container);
    void buildWordLists();
    void buildStats();
    void buildSettings();
    void applyStyle();
    void refreshDashboard();
    void showCard();
    void finishSession();
    void infoBox(const QString &title, const QString &text);
    QString findAssetCsv() const;
    void refreshArticleList();
    void loadArticle(qint64 articleId);
    QString renderArticleHtml(const QString &content) const;
    QString sentenceForWord(const QString &content,
                            const QString &word) const;

    WordStore *m_store = nullptr;

    QTabWidget *m_tabs = nullptr;
    QStackedWidget *m_studyStack = nullptr;
    QStackedWidget *m_homeStack = nullptr;
    QWidget *m_dashboardPage = nullptr;
    QWidget *m_aiPanel = nullptr;

    // 今日
    QLabel *m_newCountLabel = nullptr;
    QLabel *m_dueCountLabel = nullptr;
    QLabel *m_masteredLabel = nullptr;
    QLabel *m_streakLabel = nullptr;
    QLabel *m_dailyLabel = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_startNewButton = nullptr;
    QPushButton *m_startReviewButton = nullptr;

    // 学习
    QLabel *m_rankLabel = nullptr;
    QLabel *m_wordLabel = nullptr;
    QLabel *m_posLabel = nullptr;
    QLabel *m_meaningLabel = nullptr;
    QLabel *m_exampleLabel = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QPushButton *m_revealButton = nullptr;
    QPushButton *m_unknownButton = nullptr;
    QPushButton *m_knownButton = nullptr;
    QPushButton *m_pronounceButton = nullptr;
    QPushButton *m_translateExampleButton = nullptr;

    // 设置
    QSpinBox *m_dailySpin = nullptr;
    QLabel *m_dataDirLabel = nullptr;
    QLineEdit *m_aiUrlEdit = nullptr;
    QLineEdit *m_aiModelEdit = nullptr;

    // 阅读
    QListWidget *m_articleList = nullptr;
    QTextBrowser *m_reader = nullptr;
    QTextBrowser *m_translatePanel = nullptr;
    QPushButton *m_translateSelectionButton = nullptr;
    QPushButton *m_translateFullButton = nullptr;
    QPushButton *m_hideTranslateButton = nullptr;
    qint64 m_currentArticleId = -1;
    QString m_currentArticleContent;
    bool m_translating = false;

    // 全局翻译
    TranslatorWindow *m_translator = nullptr;
    GlobalHotkey *m_hotkeys = nullptr;
    QCheckBox *m_translateEnableCheck = nullptr;
    QCheckBox *m_autoPronounceCheck = nullptr;
    QKeySequenceEdit *m_translateHotkeyEdit = nullptr;
    QKeySequenceEdit *m_screenshotHotkeyEdit = nullptr;
    QComboBox *m_translateModelCombo = nullptr;

    // 领域词表
    WordListPage *m_wordListPage = nullptr;
    QCheckBox *m_useCurrentListCheck = nullptr;
    ConversationWindow *m_conversation = nullptr;

    // AI
    QRadioButton *m_radioTopic = nullptr;
    QRadioButton *m_radioRewrite = nullptr;
    QLineEdit *m_topicEdit = nullptr;
    QSpinBox *m_wordCountSpin = nullptr;
    QComboBox *m_topicLevelCombo = nullptr;
    QComboBox *m_rewriteLevelCombo = nullptr;
    QPlainTextEdit *m_rewriteEdit = nullptr;
    QPushButton *m_generateButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QLabel *m_aiStatusLabel = nullptr;
    AiClient *m_ai = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_wordMenu = nullptr;
    QString m_clickedWord;
    QString m_clickedDictMeaning;
    QPushButton *m_speakButton = nullptr;
    QProcess *m_tts = nullptr;
    QProcess *m_player = nullptr;
    bool m_speaking = false;
    QNetworkAccessManager *m_webManager = nullptr;
    QNetworkReply *m_webReply = nullptr;

    // 数据
    CoverageChart *m_chart = nullptr;
    QLabel *m_statsReadLabel = nullptr;
    QLabel *m_statsCoverageLabel = nullptr;
    QLabel *m_statsStreakLabel = nullptr;
    QLabel *m_statsMasteredLabel = nullptr;

    enum Tab {
        TabStudy = 0,
        TabReading = 1,
        TabWordLists = 2,
        TabStats = 3,
        TabSettings = 4,
    };

    struct SessionCard {
        qint64 id = 0;
        int rank = 0;
        QString word;
        QString pos;
        QString meaning;
        QString exampleSentence;
    };
    QVector<SessionCard> m_session;
    int m_sessionIndex = 0;
    int m_sessionCorrect = 0;
    bool m_revealed = false;
};
