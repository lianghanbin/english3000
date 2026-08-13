#include "mainwindow.h"

#include "ai_client.h"
#include "conversation_ui.h"
#include "translator_ui.h"
#include "wordlist_ui.h"

#include <QApplication>
#include <QCheckBox>
#include <QCursor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QShortcut>
#include <QSet>
#include <QKeySequence>
#include <QSplitter>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

class CoverageChart : public QWidget {
public:
    explicit CoverageChart(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(240);
    }

    void setData(const QVector<QPair<QDate, double>> &points)
    {
        m_points = points;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor(QStringLiteral("#fafafa")));

        const int left = 50, right = 16, top = 16, bottom = 30;
        const QRect plot(left, top, width() - left - right,
                         height() - top - bottom);

        p.setPen(QColor(QStringLiteral("#dddddd")));
        for (int i = 0; i <= 4; ++i) {
            const int y = plot.bottom() - plot.height() * i / 4;
            p.drawLine(plot.left(), y, plot.right(), y);
        }
        p.setPen(QColor(QStringLiteral("#888888")));
        for (int i = 0; i <= 4; ++i) {
            const int y = plot.bottom() - plot.height() * i / 4;
            p.drawText(0, y - 7, left - 8, 16, Qt::AlignRight,
                       QString::number(25 * i) + QStringLiteral("%"));
        }

        if (m_points.isEmpty()) {
            p.setPen(QColor(QStringLiteral("#999999")));
            p.drawText(rect(), Qt::AlignCenter,
                       QStringLiteral("还没有阅读记录，去读一篇文章吧"));
            return;
        }

        QVector<QPointF> pts;
        const double step = m_points.size() > 1
                                ? double(plot.width()) / (m_points.size() - 1)
                                : double(plot.width());
        for (int i = 0; i < m_points.size(); ++i) {
            const double x = plot.left() + step * i;
            const double y =
                plot.bottom()
                - plot.height() * qBound(0.0, m_points[i].second, 100.0) / 100.0;
            pts.append(QPointF(x, y));
        }
        QPen line(QColor(QStringLiteral("#2e7d32")), 2);
        p.setPen(line);
        for (int i = 1; i < pts.size(); ++i)
            p.drawLine(pts[i - 1], pts[i]);
        p.setBrush(QColor(QStringLiteral("#2e7d32")));
        for (const QPointF &pt : pts)
            p.drawEllipse(pt, 3.0, 3.0);

        // 每个点标出数值
        p.setPen(QColor(QStringLiteral("#555555")));
        for (int i = 0; i < pts.size(); ++i) {
            const QString label =
                QStringLiteral("%1%").arg(qRound(m_points[i].second));
            const QRectF textRect(pts[i].x() - 24, pts[i].y() - 20, 48, 16);
            p.drawText(textRect, Qt::AlignHCenter | Qt::AlignBottom, label);
        }
        p.setPen(QColor(QStringLiteral("#888888")));
        p.drawText(plot.left(), top, 90, 14, Qt::AlignLeft,
                   QStringLiteral("覆盖率 %"));

        p.setPen(QColor(QStringLiteral("#888888")));
        p.drawText(plot.left(), plot.bottom() + 10, 70, 16, Qt::AlignLeft,
                   m_points.first().first.toString(QStringLiteral("MM-dd")));
        p.drawText(plot.right() - 70, plot.bottom() + 10, 70, 16,
                   Qt::AlignRight,
                   m_points.last().first.toString(QStringLiteral("MM-dd")));
    }

private:
    QVector<QPair<QDate, double>> m_points;
};

namespace {

QWidget *makeStatValue(QLabel *&out, const QString &caption, QWidget *parent)
{
    auto *box = new QVBoxLayout;
    out = new QLabel(QStringLiteral("0"), parent);
    out->setObjectName(QStringLiteral("statValue"));
    out->setAlignment(Qt::AlignCenter);
    auto *captionLabel = new QLabel(caption, parent);
    captionLabel->setObjectName(QStringLiteral("statCaption"));
    captionLabel->setAlignment(Qt::AlignCenter);
    box->addWidget(out);
    box->addWidget(captionLabel);
    auto *container = new QWidget(parent);
    container->setLayout(box);
    return container;
}

} // namespace

MainWindow::MainWindow(WordStore *store, QWidget *parent)
    : QMainWindow(parent)
    , m_store(store)
{
    setWindowTitle(QStringLiteral("English 3000"));
    resize(980, 680);

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    setCentralWidget(m_tabs);

    buildDashboard();
    buildStudy();
    buildReading();
    buildAi(m_aiPanel);
    buildWordLists();
    buildStats();
    buildSettings();
    applyStyle();

    m_ai = new AiClient(this);
    m_ai->setEndpoint(
        m_store->getSetting(QStringLiteral("ai_base_url"),
                            QStringLiteral("http://127.0.0.1:11434")),
        m_store->getSetting(QStringLiteral("ai_model"),
                            QStringLiteral("qwen3:14b")));
    connect(m_ai, &AiClient::finished, this, &MainWindow::onAiFinished);
    connect(m_ai, &AiClient::translationFinished, this,
            &MainWindow::onTranslationFinished);
    connect(m_ai, &AiClient::chatFinished, this,
            &MainWindow::onAiChatFinished);
    connect(m_ai, &AiClient::failed, this, &MainWindow::onAiFailed);

    m_webManager = new QNetworkAccessManager(this);

    m_tray = new QSystemTrayIcon(
        QIcon(QStringLiteral(ENGLISH3000_ASSET_DIR)
              + QStringLiteral("/icon.svg")),
        this);
    m_tray->setToolTip(QStringLiteral("English 3000"));
    if (QSystemTrayIcon::isSystemTrayAvailable())
        m_tray->show();

    connect(m_tabs, &QTabWidget::currentChanged, this,
            &MainWindow::onTabChanged);

    m_translator = new TranslatorWindow(m_store, this);
    m_hotkeys = new GlobalHotkey(this);
    connect(m_hotkeys, &GlobalHotkey::activated, this,
            [this](const QString &token) {
                if (token == QLatin1String("translate")) {
                    m_translator->openWithText();
                } else if (token == QLatin1String("screenshot")) {
                    m_translator->startScreenshot();
                }
            });
    applyHotkeys();

    auto *newShortcut = new QShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_N), this);
    connect(newShortcut, &QShortcut::activated, this,
            [this] { startSession(QStringLiteral("learn")); });
    auto *reviewShortcut = new QShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_R), this);
    connect(reviewShortcut, &QShortcut::activated, this,
            [this] { startSession(QStringLiteral("review")); });

    refreshAll();
}

void MainWindow::buildDashboard()
{
    m_dashboardPage = new QWidget;
    auto *page = m_dashboardPage;
    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(18);
    layout->addStretch();

    auto *statsRow = new QHBoxLayout;
    statsRow->setSpacing(48);
    statsRow->addStretch();
    statsRow->addWidget(makeStatValue(m_newCountLabel, QStringLiteral("未学"), page));
    statsRow->addWidget(makeStatValue(m_dueCountLabel, QStringLiteral("待复习"), page));
    statsRow->addWidget(makeStatValue(m_masteredLabel, QStringLiteral("已掌握"), page));
    statsRow->addWidget(makeStatValue(m_streakLabel, QStringLiteral("连续学习(天)"), page));
    statsRow->addStretch();
    layout->addLayout(statsRow);

    m_currentListLabel = new QLabel(QStringLiteral("当前词表：无"), page);
    m_currentListLabel->setObjectName(QStringLiteral("meaningLabel"));
    m_currentListLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_currentListLabel);

    m_progress = new QProgressBar(page);
    m_progress->setFixedWidth(520);
    m_progress->setFormat(QStringLiteral("%p%"));
    m_progress->setTextVisible(true);
    auto *progressRow = new QHBoxLayout;
    progressRow->addStretch();
    progressRow->addWidget(m_progress);
    progressRow->addStretch();
    layout->addLayout(progressRow);

    m_dailyLabel = new QLabel(QStringLiteral("今天还没有学习记录"), page);
    m_dailyLabel->setAlignment(Qt::AlignCenter);
    m_dailyLabel->setObjectName(QStringLiteral("dailyLabel"));
    layout->addWidget(m_dailyLabel);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(16);
    buttonRow->addStretch();
    m_startNewButton = new QPushButton(QStringLiteral("开始学习"), page);
    m_startNewButton->setObjectName(QStringLiteral("primaryButton"));
    m_startReviewButton = new QPushButton(QStringLiteral("开始复习"), page);
    m_startNewButton->setFocusPolicy(Qt::NoFocus);
    m_startReviewButton->setFocusPolicy(Qt::NoFocus);
    connect(m_startNewButton, &QPushButton::clicked, this,
            [this] { startSession(QStringLiteral("learn")); });
    connect(m_startReviewButton, &QPushButton::clicked, this,
            [this] { startSession(QStringLiteral("review")); });
    buttonRow->addWidget(m_startNewButton);
    buttonRow->addWidget(m_startReviewButton);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);

    auto *shortcutHint = new QLabel(
        QStringLiteral("快捷键：Ctrl+N 学习未学的 · Ctrl+R 复习不认识的"), page);
    shortcutHint->setObjectName(QStringLiteral("hintLabel"));
    shortcutHint->setAlignment(Qt::AlignCenter);
    layout->addWidget(shortcutHint);

    layout->addStretch();
}

void MainWindow::buildStudy()
{
    m_studyStack = new QStackedWidget;

    // 卡片页
    auto *card = new QWidget;
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setSpacing(14);
    cardLayout->addStretch();

    auto *backButton = new QPushButton(QStringLiteral("← 返回"), card);
    backButton->setFocusPolicy(Qt::NoFocus);
    connect(backButton, &QPushButton::clicked, this, [this] {
        stopSpeaking();
        backToToday();
    });
    cardLayout->addWidget(backButton, 0, Qt::AlignLeft);

    m_rankLabel = new QLabel(card);
    m_rankLabel->setObjectName(QStringLiteral("rankLabel"));
    m_rankLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(m_rankLabel);

    m_wordLabel = new QLabel(card);
    m_wordLabel->setObjectName(QStringLiteral("wordLabel"));
    m_wordLabel->setAlignment(Qt::AlignCenter);
    m_wordLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_wordLabel->setFocusPolicy(Qt::StrongFocus);
    cardLayout->addWidget(m_wordLabel);

    m_posLabel = new QLabel(card);
    m_posLabel->setObjectName(QStringLiteral("posLabel"));
    m_posLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(m_posLabel);

    m_pronounceButton = new QPushButton(QStringLiteral("🔊 发音"), card);
    m_pronounceButton->setFocusPolicy(Qt::NoFocus);
    connect(m_pronounceButton, &QPushButton::clicked, this, [this] {
        if (m_sessionIndex < m_session.size())
            speakText(m_session[m_sessionIndex].word);
    });
    cardLayout->addWidget(m_pronounceButton, 0, Qt::AlignHCenter);

    m_translateExampleButton =
        new QPushButton(QStringLiteral("翻译例句"), card);
    m_translateExampleButton->setFocusPolicy(Qt::NoFocus);
    connect(m_translateExampleButton, &QPushButton::clicked, this, [this] {
        if (m_sessionIndex >= m_session.size())
            return;
        const SessionCard &card = m_session[m_sessionIndex];
        const QString text =
            card.exampleSentence.isEmpty() ? card.word : card.exampleSentence;
        if (m_translator)
            m_translator->openWithText(text);
    });
    cardLayout->addWidget(m_translateExampleButton, 0, Qt::AlignHCenter);

    m_meaningLabel = new QLabel(card);
    m_meaningLabel->setObjectName(QStringLiteral("meaningLabel"));
    m_meaningLabel->setAlignment(Qt::AlignCenter);
    m_meaningLabel->setWordWrap(true);
    m_meaningLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_meaningLabel->hide();
    cardLayout->addWidget(m_meaningLabel);

    m_exampleLabel = new QLabel(card);
    m_exampleLabel->setObjectName(QStringLiteral("exampleLabel"));
    m_exampleLabel->setAlignment(Qt::AlignCenter);
    m_exampleLabel->setWordWrap(true);
    m_exampleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_exampleLabel->hide();
    cardLayout->addWidget(m_exampleLabel);

    m_revealButton = new QPushButton(QStringLiteral("显示释义"), card);
    m_revealButton->setObjectName(QStringLiteral("revealButton"));
    m_revealButton->setFocusPolicy(Qt::NoFocus);
    connect(m_revealButton, &QPushButton::clicked, this, &MainWindow::reveal);
    cardLayout->addWidget(m_revealButton, 0, Qt::AlignHCenter);

    auto *answerRow = new QHBoxLayout;
    answerRow->setSpacing(16);
    answerRow->addStretch();
    m_unknownButton = new QPushButton(QStringLiteral("不认识"), card);
    m_unknownButton->setObjectName(QStringLiteral("unknownButton"));
    m_knownButton = new QPushButton(QStringLiteral("认识"), card);
    m_knownButton->setObjectName(QStringLiteral("knownButton"));
    m_unknownButton->setFocusPolicy(Qt::NoFocus);
    m_knownButton->setFocusPolicy(Qt::NoFocus);
    connect(m_unknownButton, &QPushButton::clicked, this,
            [this] { answer(false); });
    connect(m_knownButton, &QPushButton::clicked, this,
            [this] { answer(true); });
    answerRow->addWidget(m_unknownButton);
    answerRow->addWidget(m_knownButton);
    answerRow->addStretch();
    cardLayout->addLayout(answerRow);

    auto *hint = new QLabel(QStringLiteral("快捷键：空格 显示释义 · 1 不认识 · 2 认识"), card);
    hint->setObjectName(QStringLiteral("hintLabel"));
    hint->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(hint);
    cardLayout->addStretch();
    m_studyStack->addWidget(card);

    // 小结页
    auto *summary = new QWidget;
    auto *summaryLayout = new QVBoxLayout(summary);
    summaryLayout->setSpacing(16);
    summaryLayout->addStretch();
    auto *title = new QLabel(QStringLiteral("本次完成"), summary);
    title->setObjectName(QStringLiteral("wordLabel"));
    title->setAlignment(Qt::AlignCenter);
    summaryLayout->addWidget(title);
    m_summaryLabel = new QLabel(summary);
    m_summaryLabel->setObjectName(QStringLiteral("meaningLabel"));
    m_summaryLabel->setAlignment(Qt::AlignCenter);
    summaryLayout->addWidget(m_summaryLabel);
    auto *back = new QPushButton(QStringLiteral("返回今日"), summary);
    connect(back, &QPushButton::clicked, this, &MainWindow::backToToday);
    summaryLayout->addWidget(back, 0, Qt::AlignHCenter);
    summaryLayout->addStretch();
    m_studyStack->addWidget(summary);

    m_homeStack = new QStackedWidget;
    auto *overview = new QWidget;
    auto *overviewLayout = new QVBoxLayout(overview);
    overviewLayout->setContentsMargins(0, 0, 0, 0);
    overviewLayout->addWidget(m_dashboardPage);
    m_homeStack->addWidget(overview);
    m_homeStack->addWidget(m_studyStack);
    m_tabs->addTab(m_homeStack, QStringLiteral("学习"));
}

void MainWindow::buildReading()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(8);

    auto *splitter = new QSplitter(Qt::Horizontal, page);
    auto *left = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    auto *title = new QLabel(QStringLiteral("文章库"), left);
    title->setObjectName(QStringLiteral("rankLabel"));
    leftLayout->addWidget(title);

    m_articleList = new QListWidget(left);
    m_articleList->setAlternatingRowColors(true);
    connect(m_articleList, &QListWidget::currentRowChanged, this,
            [this](int) { onArticleSelected(); });
    leftLayout->addWidget(m_articleList, 1);

    auto *buttonRow = new QHBoxLayout;
    auto *importButton = new QPushButton(QStringLiteral("导入文章"), left);
    auto *deleteButton = new QPushButton(QStringLiteral("删除"), left);
    auto *urlButton = new QPushButton(QStringLiteral("导入网址"), left);
    m_speakButton = new QPushButton(QStringLiteral("朗读文章"), left);
    connect(importButton, &QPushButton::clicked, this,
            &MainWindow::importArticleFile);
    connect(deleteButton, &QPushButton::clicked, this,
            &MainWindow::deleteCurrentArticle);
    connect(urlButton, &QPushButton::clicked, this, &MainWindow::importUrl);
    connect(m_speakButton, &QPushButton::clicked, this, [this] {
        if (m_speaking) {
            stopSpeaking();
        } else if (!m_currentArticleContent.isEmpty()) {
            speakText(m_currentArticleContent);
        } else {
            statusBar()->showMessage(QStringLiteral("请先选择一篇文章"), 3000);
        }
    });
    buttonRow->addWidget(importButton);
    buttonRow->addWidget(deleteButton);
    buttonRow->addWidget(urlButton);
    buttonRow->addWidget(m_speakButton);
    leftLayout->addLayout(buttonRow);
    left->setMinimumWidth(320);

    m_reader = new QTextBrowser(splitter);
    m_reader->setOpenExternalLinks(false);
    m_reader->setOpenLinks(false);
    m_reader->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_reader, &QWidget::customContextMenuRequested, this,
            &MainWindow::onWordContextMenu);
    connect(m_reader, &QTextBrowser::anchorClicked, this,
            [this](const QUrl &url) {
                showWordMenu(
                    url.toString().remove(QStringLiteral("word://")));
            });

    m_translatePanel = new QTextBrowser(splitter);
    m_translatePanel->setPlaceholderText(
        QStringLiteral("翻译会显示在这里"));
    m_translatePanel->setVisible(false);
    m_translatePanel->setMinimumWidth(280);

    splitter->addWidget(left);
    splitter->addWidget(m_reader);
    splitter->addWidget(m_translatePanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 1);
    layout->addWidget(splitter, 1);

    auto *translateRow = new QHBoxLayout;
    translateRow->setSpacing(8);
    m_translateSelectionButton =
        new QPushButton(QStringLiteral("翻译选中"), page);
    m_translateFullButton =
        new QPushButton(QStringLiteral("翻译全文"), page);
    m_hideTranslateButton =
        new QPushButton(QStringLiteral("收起翻译"), page);
    auto *aiToggleButton = new QPushButton(QStringLiteral("AI 生成文章"), page);
    auto *conversationButton = new QPushButton(QStringLiteral("对话练习"), page);
    connect(m_translateSelectionButton, &QPushButton::clicked, this,
            &MainWindow::onTranslateSelection);
    connect(m_translateFullButton, &QPushButton::clicked, this,
            &MainWindow::onTranslateFull);
    connect(m_hideTranslateButton, &QPushButton::clicked, this, [this] {
        m_translatePanel->setVisible(false);
    });
    connect(aiToggleButton, &QPushButton::clicked, this, [this] {
        if (m_aiPanel)
            m_aiPanel->setVisible(!m_aiPanel->isVisible());
    });
    connect(conversationButton, &QPushButton::clicked, this, [this] {
        if (m_currentArticleId < 0) {
            statusBar()->showMessage(
                QStringLiteral("请先选择一篇文章再开始对话练习"), 3000);
            return;
        }
        if (!m_conversation)
            m_conversation = new ConversationWindow(m_store, this);
        const std::optional<Article> article =
            m_store->getArticle(m_currentArticleId);
        m_conversation->openWithArticle(
            article ? article->title : QStringLiteral("文章"),
            m_currentArticleContent);
    });
    translateRow->addWidget(m_translateSelectionButton);
    translateRow->addWidget(m_translateFullButton);
    translateRow->addWidget(m_hideTranslateButton);
    translateRow->addWidget(aiToggleButton);
    translateRow->addWidget(conversationButton);
    translateRow->addStretch();
    layout->addLayout(translateRow);

    auto *hint = new QLabel(
        QStringLiteral("高亮：红色=词表外 · 蓝色=未掌握 · 黑色=已掌握 · 右键单词查释义/发音/加入阅读词表 · 左键拖选后可翻译"),
        page);
    hint->setObjectName(QStringLiteral("hintLabel"));
    layout->addWidget(hint);

    m_aiPanel = new QWidget(page);
    m_aiPanel->hide();
    layout->addWidget(m_aiPanel);

    m_tabs->addTab(page, QStringLiteral("阅读"));
    refreshArticleList();
}

void MainWindow::buildAi(QWidget *container)
{
    auto *page = container;
    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(12);

    auto *modeRow = new QHBoxLayout;
    m_radioTopic = new QRadioButton(QStringLiteral("按主题生成"), page);
    m_radioRewrite = new QRadioButton(QStringLiteral("粘贴文章改写"), page);
    m_radioTopic->setChecked(true);
    modeRow->addWidget(m_radioTopic);
    modeRow->addWidget(m_radioRewrite);
    modeRow->addStretch();
    layout->addLayout(modeRow);

    auto *formStack = new QStackedWidget(page);

    auto *topicForm = new QWidget(formStack);
    auto *topicLayout = new QVBoxLayout(topicForm);
    topicLayout->setSpacing(10);
    m_topicEdit = new QLineEdit(topicForm);
    m_topicEdit->setPlaceholderText(
        QStringLiteral("主题，例如：Linux 文件系统、我的周末、咖啡"));
    topicLayout->addWidget(m_topicEdit);
    auto *topicOptions = new QHBoxLayout;
    topicOptions->addWidget(new QLabel(QStringLiteral("词数"), topicForm));
    m_wordCountSpin = new QSpinBox(topicForm);
    m_wordCountSpin->setRange(50, 500);
    m_wordCountSpin->setSingleStep(25);
    m_wordCountSpin->setValue(200);
    topicOptions->addWidget(m_wordCountSpin);
    topicOptions->addWidget(new QLabel(QStringLiteral("难度"), topicForm));
    m_topicLevelCombo = new QComboBox(topicForm);
    m_topicLevelCombo->addItems(
        {QStringLiteral("最简单（1000 词内）"),
         QStringLiteral("简单（2000 词内）"),
         QStringLiteral("中等（牛津 3000）"),
         QStringLiteral("偏难（3000 + 少量生词）")});
    topicOptions->addWidget(m_topicLevelCombo);
    topicOptions->addStretch();
    topicLayout->addLayout(topicOptions);
    formStack->addWidget(topicForm);

    auto *rewriteForm = new QWidget(formStack);
    auto *rewriteLayout = new QVBoxLayout(rewriteForm);
    rewriteLayout->setSpacing(10);
    m_rewriteEdit = new QPlainTextEdit(rewriteForm);
    m_rewriteEdit->setPlaceholderText(
        QStringLiteral("把英文文章粘贴到这里，AI 会改写成你选的难度…"));
    rewriteLayout->addWidget(m_rewriteEdit, 1);
    auto *rewriteOptions = new QHBoxLayout;
    rewriteOptions->addWidget(new QLabel(QStringLiteral("目标难度"), rewriteForm));
    m_rewriteLevelCombo = new QComboBox(rewriteForm);
    m_rewriteLevelCombo->addItems(
        {QStringLiteral("最简单（1000 词内）"),
         QStringLiteral("简单（2000 词内）"),
         QStringLiteral("中等（牛津 3000）"),
         QStringLiteral("偏难（3000 + 少量生词）")});
    rewriteOptions->addWidget(m_rewriteLevelCombo);
    rewriteOptions->addStretch();
    rewriteLayout->addLayout(rewriteOptions);
    formStack->addWidget(rewriteForm);

    connect(m_radioTopic, &QRadioButton::toggled, this,
            [formStack, this](bool checked) {
                formStack->setCurrentIndex(checked ? 0 : 1);
            });
    layout->addWidget(formStack, 1);

    auto *buttonRow = new QHBoxLayout;
    m_generateButton = new QPushButton(QStringLiteral("开始生成"), page);
    m_generateButton->setObjectName(QStringLiteral("primaryButton"));
    m_cancelButton = new QPushButton(QStringLiteral("取消"), page);
    m_cancelButton->setEnabled(false);
    connect(m_generateButton, &QPushButton::clicked, this,
            &MainWindow::onGenerateClicked);
    connect(m_cancelButton, &QPushButton::clicked, this,
            &MainWindow::onCancelClicked);
    buttonRow->addWidget(m_generateButton);
    buttonRow->addWidget(m_cancelButton);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);

    m_aiStatusLabel = new QLabel(page);
    m_aiStatusLabel->setObjectName(QStringLiteral("aiStatusLabel"));
    layout->addWidget(m_aiStatusLabel);

    m_useCurrentListCheck = new QCheckBox(
        QStringLiteral("按当前词表生成（当前：无）"), page);
    connect(m_useCurrentListCheck, &QCheckBox::toggled, this,
            [this](bool) {
                const QString name = m_store->currentWordListName();
                m_useCurrentListCheck->setText(
                    name.isEmpty()
                        ? QStringLiteral("按当前词表生成（当前：无）")
                        : QStringLiteral("按当前词表生成（当前：%1）")
                              .arg(name));
            });
    layout->addWidget(m_useCurrentListCheck);

}

void MainWindow::buildWordLists()
{
    m_wordListPage = new WordListPage(m_store, this);
    connect(m_wordListPage, &WordListPage::wordSpeakRequested, this,
            &MainWindow::speakText);
    m_tabs->insertTab(TabWordLists, m_wordListPage,
                      QStringLiteral("词表"));
}

void MainWindow::buildStats()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(16);

    auto *statsRow = new QHBoxLayout;
    statsRow->setSpacing(36);
    statsRow->addStretch();
    auto *readBox = makeStatValue(m_statsReadLabel,
                                  QStringLiteral("累计阅读文章"), page);
    auto *coverageBox = makeStatValue(
        m_statsCoverageLabel, QStringLiteral("平均覆盖率"), page);
    auto *knownBox = makeStatValue(
        m_statsKnownLabel, QStringLiteral("未学单词"), page);
    auto *streakBox = makeStatValue(m_statsStreakLabel,
                                    QStringLiteral("连续学习(天)"), page);
    auto *masteredBox = makeStatValue(
        m_statsMasteredLabel, QStringLiteral("已掌握词"), page);
    statsRow->addWidget(readBox);
    statsRow->addWidget(coverageBox);
    statsRow->addWidget(knownBox);
    statsRow->addWidget(streakBox);
    statsRow->addWidget(masteredBox);
    statsRow->addStretch();
    layout->addLayout(statsRow);

    m_chart = new CoverageChart(page);
    layout->addWidget(m_chart, 1);

    auto *hint = new QLabel(
        QStringLiteral("近 30 天阅读覆盖率：曲线越高=读得越懂（每天取平均）"), page);
    hint->setObjectName(QStringLiteral("hintLabel"));
    hint->setAlignment(Qt::AlignCenter);
    layout->addWidget(hint);

    m_tabs->addTab(page, QStringLiteral("数据"));
}

void MainWindow::buildSettings()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(18);
    layout->addStretch();

    auto *pronounceRow = new QHBoxLayout;
    pronounceRow->addStretch();
    m_autoPronounceCheck = new QCheckBox(
        QStringLiteral("学习时自动发音（单词 + 例句）"), page);
    m_autoPronounceCheck->setChecked(
        m_store->getSetting(QStringLiteral("auto_pronounce"),
                            QStringLiteral("1"))
            == QLatin1String("1"));
    connect(m_autoPronounceCheck, &QCheckBox::toggled, this,
            [this](bool checked) {
                m_store->setSetting(
                    QStringLiteral("auto_pronounce"),
                    checked ? QStringLiteral("1") : QStringLiteral("0"));
            });
    pronounceRow->addWidget(m_autoPronounceCheck);
    pronounceRow->addStretch();
    layout->addLayout(pronounceRow);

    auto *transGroup = new QGroupBox(QStringLiteral("全局翻译"), page);
    auto *transLayout = new QVBoxLayout(transGroup);
    transLayout->setSpacing(10);

    m_translateEnableCheck = new QCheckBox(
        QStringLiteral("启用全局热键翻译（X11）"), transGroup);
    m_translateEnableCheck->setChecked(
        m_store->getSetting(QStringLiteral("translate_enabled"),
                            QStringLiteral("1"))
            == QLatin1String("1"));
    connect(m_translateEnableCheck, &QCheckBox::toggled, this,
            [this](bool checked) {
                m_store->setSetting(
                    QStringLiteral("translate_enabled"),
                    checked ? QStringLiteral("1") : QStringLiteral("0"));
                applyHotkeys();
            });
    transLayout->addWidget(m_translateEnableCheck);

    auto *hotkeyRow = new QHBoxLayout;
    hotkeyRow->addWidget(new QLabel(QStringLiteral("翻译热键"), transGroup));
    m_translateHotkeyEdit = new QKeySequenceEdit(transGroup);
    m_translateHotkeyEdit->setKeySequence(QKeySequence(
        m_store->getSetting(QStringLiteral("translate_hotkey"),
                            QStringLiteral("Ctrl+Alt+T"))));
    connect(m_translateHotkeyEdit, &QKeySequenceEdit::editingFinished, this,
            [this] {
                m_store->setSetting(
                    QStringLiteral("translate_hotkey"),
                    m_translateHotkeyEdit->keySequence().toString());
                applyHotkeys();
            });
    hotkeyRow->addWidget(m_translateHotkeyEdit);
    hotkeyRow->addWidget(
        new QLabel(QStringLiteral("截图热键"), transGroup));
    m_screenshotHotkeyEdit = new QKeySequenceEdit(transGroup);
    m_screenshotHotkeyEdit->setKeySequence(QKeySequence(
        m_store->getSetting(QStringLiteral("translate_screenshot_hotkey"),
                            QStringLiteral("Ctrl+Alt+O"))));
    connect(m_screenshotHotkeyEdit, &QKeySequenceEdit::editingFinished, this,
            [this] {
                m_store->setSetting(
                    QStringLiteral("translate_screenshot_hotkey"),
                    m_screenshotHotkeyEdit->keySequence().toString());
                applyHotkeys();
            });
    hotkeyRow->addWidget(m_screenshotHotkeyEdit);
    hotkeyRow->addStretch();
    transLayout->addLayout(hotkeyRow);

    auto *modelRow = new QHBoxLayout;
    modelRow->addWidget(new QLabel(QStringLiteral("默认引擎"), transGroup));
    m_translateModelCombo = new QComboBox(transGroup);
    m_translateModelCombo->addItem(QStringLiteral("快译 qwen2.5:3b"),
                                   QStringLiteral("qwen2.5:3b"));
    m_translateModelCombo->addItem(QStringLiteral("精译 qwen3:14b"),
                                   QStringLiteral("qwen3:14b"));
    const QString defModel = m_store->getSetting(
        QStringLiteral("translate_default_model"),
        QStringLiteral("qwen2.5:3b"));
    const int defIdx = m_translateModelCombo->findData(defModel);
    m_translateModelCombo->setCurrentIndex(defIdx >= 0 ? defIdx : 0);
    connect(m_translateModelCombo, &QComboBox::currentIndexChanged, this,
            [this](int index) {
                m_store->setSetting(
                    QStringLiteral("translate_default_model"),
                    m_translateModelCombo->itemData(index).toString());
            });
    modelRow->addWidget(m_translateModelCombo);
    modelRow->addStretch();
    transLayout->addLayout(modelRow);

    auto *transHint = new QLabel(
        QStringLiteral("应用常驻托盘时生效；关闭应用后热键自动解绑。"),
        transGroup);
    transHint->setObjectName(QStringLiteral("hintLabel"));
    transLayout->addWidget(transHint);
    layout->addWidget(transGroup);

    auto *aiUrlRow = new QHBoxLayout;
    aiUrlRow->addStretch();
    aiUrlRow->addWidget(new QLabel(QStringLiteral("AI 服务地址"), page));
    m_aiUrlEdit = new QLineEdit(page);
    m_aiUrlEdit->setText(
        m_store->getSetting(QStringLiteral("ai_base_url"),
                            QStringLiteral("http://127.0.0.1:11434")));
    m_aiUrlEdit->setMinimumWidth(260);
    connect(m_aiUrlEdit, &QLineEdit::editingFinished, this, [this] {
        m_store->setSetting(QStringLiteral("ai_base_url"),
                            m_aiUrlEdit->text().trimmed());
        if (m_ai) {
            m_ai->setEndpoint(
                m_aiUrlEdit->text().trimmed(),
                m_aiModelEdit->text().trimmed());
        }
    });
    aiUrlRow->addWidget(m_aiUrlEdit);
    aiUrlRow->addStretch();
    layout->addLayout(aiUrlRow);

    auto *aiModelRow = new QHBoxLayout;
    aiModelRow->addStretch();
    aiModelRow->addWidget(new QLabel(QStringLiteral("AI 模型"), page));
    m_aiModelEdit = new QLineEdit(page);
    m_aiModelEdit->setText(
        m_store->getSetting(QStringLiteral("ai_model"),
                            QStringLiteral("qwen3:14b")));
    m_aiModelEdit->setMinimumWidth(260);
    connect(m_aiModelEdit, &QLineEdit::editingFinished, this, [this] {
        m_store->setSetting(QStringLiteral("ai_model"),
                            m_aiModelEdit->text().trimmed());
        if (m_ai) {
            m_ai->setEndpoint(
                m_aiUrlEdit->text().trimmed(),
                m_aiModelEdit->text().trimmed());
        }
    });
    aiModelRow->addWidget(m_aiModelEdit);
    aiModelRow->addStretch();
    layout->addLayout(aiModelRow);

    auto *dataRow = new QHBoxLayout;
    dataRow->addStretch();
    dataRow->addWidget(new QLabel(QStringLiteral("数据目录"), page));
    m_dataDirLabel = new QLabel(QFileInfo(m_store->dbPath()).absolutePath(), page);
    m_dataDirLabel->setObjectName(QStringLiteral("statCaption"));
    dataRow->addWidget(m_dataDirLabel);
    dataRow->addStretch();
    layout->addLayout(dataRow);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();
    auto *reimportButton = new QPushButton(QStringLiteral("重新导入内置词表"), page);
    auto *resetButton = new QPushButton(QStringLiteral("重置全部进度"), page);
    resetButton->setObjectName(QStringLiteral("dangerButton"));
    connect(reimportButton, &QPushButton::clicked, this,
            &MainWindow::reimportBuiltin);
    connect(resetButton, &QPushButton::clicked, this,
            &MainWindow::resetAllDialog);
    buttonRow->addWidget(reimportButton);
    buttonRow->addWidget(resetButton);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);

    auto *note = new QLabel(
        QStringLiteral("数据保存在 SQLite 数据库，重置只会清空学习进度，不影响词表文件。"),
        page);
    note->setObjectName(QStringLiteral("statCaption"));
    note->setAlignment(Qt::AlignCenter);
    layout->addWidget(note);
    layout->addStretch();

    m_tabs->addTab(page, QStringLiteral("设置"));
}

void MainWindow::applyStyle()
{
    const QString qss = QStringLiteral(R"(
        QWidget { font-size: 14px; }
        #wordLabel { font-size: 40px; font-weight: 600; }
        #meaningLabel { font-size: 18px; }
        #posLabel { color: #888888; }
        #rankLabel { color: #888888; }
        #hintLabel { color: #999999; font-size: 12px; }
        #dailyLabel { color: #666666; }
        #statValue { font-size: 28px; font-weight: 600; }
        #statCaption { color: #888888; font-size: 13px; }
        #primaryButton {
            background: #2e7d32; color: white; border: none;
            border-radius: 8px; padding: 10px 24px; font-weight: 600;
        }
        #primaryButton:hover { background: #388e3c; }
        #revealButton {
            background: #1565c0; color: white; border: none;
            border-radius: 8px; padding: 10px 24px; font-weight: 600;
        }
        #revealButton:hover { background: #1e88e5; }
        #knownButton {
            background: #2e7d32; color: white; border: none;
            border-radius: 8px; padding: 10px 24px; font-weight: 600;
        }
        #knownButton:hover { background: #388e3c; }
        #unknownButton {
            background: #c62828; color: white; border: none;
            border-radius: 8px; padding: 10px 24px; font-weight: 600;
        }
        #unknownButton:hover { background: #d32f2f; }
        #dangerButton { color: #c62828; }
        QProgressBar {
            border: 1px solid #cccccc; border-radius: 8px;
            text-align: center; height: 20px;
        }
        QProgressBar::chunk { background: #2e7d32; border-radius: 7px; }
        QTabWidget::pane { border: none; }
    )");
    qApp->setStyleSheet(qss);
}

// ---------- 数据刷新 ----------

void MainWindow::refreshAll()
{
    refreshDashboard();
}

void MainWindow::refreshDashboard()
{
    const Counts c = m_store->counts();
    const DailySummary s = m_store->dailySummary();
    const int streak = m_store->streak();
    m_newCountLabel->setText(QString::number(c.newTotal));
    m_dueCountLabel->setText(QString::number(c.learning));
    m_masteredLabel->setText(QString::number(c.mastered));
    m_streakLabel->setText(QString::number(streak));
    const QString bookName = m_store->currentWordListName();
    m_currentListLabel->setText(
        bookName.isEmpty()
            ? QStringLiteral("当前词表：无（请到词表页选择一个词表）")
            : QStringLiteral("当前词表：%1").arg(bookName));
    m_progress->setMaximum(qMax(1, c.total));
    m_progress->setValue(c.mastered);
    m_progress->setFormat(
        QStringLiteral("已掌握 %1 / %2").arg(c.mastered).arg(c.total));
    m_startNewButton->setEnabled(c.total > 0 && c.newTotal > 0);
    m_startReviewButton->setEnabled(c.learning > 0);
    if (s.newCount > 0 || s.reviewCount > 0) {
        m_dailyLabel->setText(
            QStringLiteral("今天：新学 %1 · 复习 %2 · 认识 %3 · 不认识 %4")
                .arg(s.newCount).arg(s.reviewCount).arg(s.correct).arg(s.wrong));
    } else {
        m_dailyLabel->setText(QStringLiteral("今天还没有学习记录"));
    }
}

// ---------- 学习会话 ----------

void MainWindow::startSession(const QString &kind)
{
    const QVector<Word> words =
        kind == QLatin1String("review")
            ? m_store->reviewCards(100000)
            : m_store->learnCards(100000);
    if (words.isEmpty()) {
        infoBox(QStringLiteral("没有未学的单词"),
                m_store->currentWordListId() > 0
                    ? (kind == QLatin1String("review")
                           ? QStringLiteral("当前词表没有需要复习的词，先学新的吧。")
                           : QStringLiteral("当前词表的新词都学完了，去词表页换一个词表吧。"))
                    : QStringLiteral("请先在词表页选择一个词表。"));
        return;
    }

    m_sessionKind = kind;
    m_session.clear();
    m_session.reserve(words.size());
    for (const Word &w : words) {
        m_session.push_back(
            {w.id, w.itemId, w.rank, w.word, w.pos, w.meaning,
             w.exampleSentence});
    }
    m_sessionIndex = 0;
    m_sessionCorrect = 0;
    m_tabs->setCurrentIndex(TabStudy);
    m_homeStack->setCurrentIndex(1);
    m_studyStack->setCurrentIndex(0);
    showCard();
}

void MainWindow::showCard()
{
    if (m_sessionIndex >= m_session.size()) {
        finishSession();
        return;
    }
    const SessionCard &card = m_session[m_sessionIndex];
    m_rankLabel->setText(
        QStringLiteral("第 %1 词 · %2/%3")
            .arg(card.rank > 0 ? QString::number(card.rank) : QStringLiteral("—"))
            .arg(m_sessionIndex + 1)
            .arg(m_session.size()));
    m_wordLabel->setText(card.word);
    m_posLabel->setText(card.pos);
    m_meaningLabel->setText(card.meaning);
    const bool exampleRequested = m_exampleRequested.contains(card.id);
    m_exampleLabel->setText(
        card.exampleSentence.isEmpty() && exampleRequested
            ? QStringLiteral("例句生成中…")
            : card.exampleSentence);
    m_meaningLabel->hide();
    m_exampleLabel->hide();
    m_revealButton->setEnabled(true);
    m_unknownButton->setEnabled(false);
    m_knownButton->setEnabled(false);
    m_revealed = false;
    m_wordLabel->setFocus(Qt::OtherFocusReason);
    if (card.exampleSentence.isEmpty() && !exampleRequested)
        requestExample(card.id, card.word);
    if (m_sessionIndex + 1 < m_session.size()
        && m_session[m_sessionIndex + 1].exampleSentence.isEmpty()
        && !m_exampleRequested.contains(m_session[m_sessionIndex + 1].id)) {
        requestExample(m_session[m_sessionIndex + 1].id,
                       m_session[m_sessionIndex + 1].word);
    }
    if (autoPronounceEnabled())
        speakText(card.word);
}

void MainWindow::reveal()
{
    if (m_tabs->currentIndex() != TabStudy
        || m_homeStack->currentIndex() != 1
        || m_studyStack->currentIndex() != 0
        || m_revealed || m_session.isEmpty()) {
        return;
    }
    m_meaningLabel->show();
    if (!m_exampleLabel->text().isEmpty())
        m_exampleLabel->show();
    if (autoPronounceEnabled() && m_sessionIndex < m_session.size()
        && !m_session[m_sessionIndex].exampleSentence.isEmpty()) {
        speakText(m_session[m_sessionIndex].exampleSentence);
    }
    m_revealButton->setEnabled(false);
    m_unknownButton->setEnabled(true);
    m_knownButton->setEnabled(true);
    m_revealed = true;
}

void MainWindow::answer(bool known)
{
    if (m_tabs->currentIndex() != TabStudy
        || m_homeStack->currentIndex() != 1
        || m_studyStack->currentIndex() != 0
        || !m_revealed || m_sessionIndex >= m_session.size()) {
        return;
    }
    const SessionCard &card = m_session[m_sessionIndex];
    m_store->answerStudy(card.itemId, known);
    if (known)
        ++m_sessionCorrect;
    ++m_sessionIndex;
    showCard();
}

void MainWindow::finishSession()
{
    const int total = m_session.size();
    const Counts c = m_store->counts();
    if (m_sessionKind == QLatin1String("review")) {
        m_summaryLabel->setText(
            QStringLiteral("本次复习 %1 个单词，认识 %2 个。\n"
                           "当前词表还有 %3 个没记住，下次复习继续。")
                .arg(total)
                .arg(m_sessionCorrect)
                .arg(c.learning));
    } else {
        m_summaryLabel->setText(
            QStringLiteral("本次学习 %1 个单词，认识 %2 个。\n"
                           "没记住的已进入复习（%3 个），当前词表还剩 %4 个未学。")
                .arg(total)
                .arg(m_sessionCorrect)
                .arg(c.learning)
                .arg(c.newTotal));
    }
    m_studyStack->setCurrentIndex(1);
    refreshDashboard();
}

void MainWindow::backToToday()
{
    m_homeStack->setCurrentIndex(0);
    m_tabs->setCurrentIndex(TabStudy);
    refreshDashboard();
}

QString MainWindow::findAssetCsv() const
{
    const QStringList candidates = {
        QFileInfo(m_store->dbPath()).absolutePath()
            + QStringLiteral("/oxford3000.csv"),
        QCoreApplication::applicationDirPath()
            + QStringLiteral("/assets/oxford3000.csv"),
        QStringLiteral(ENGLISH3000_ASSET_DIR)
            + QStringLiteral("/oxford3000.csv"),
    };
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path))
            return path;
    }
    return {};
}

void MainWindow::reimportBuiltin()
{
    const QString path = findAssetCsv();
    if (path.isEmpty()) {
        infoBox(QStringLiteral("找不到词表"),
                QStringLiteral("内置词表未找到，请手动导入 CSV。"));
        return;
    }
    const int count = m_store->importCsv(path, false);
    infoBox(QStringLiteral("导入完成"),
            QStringLiteral("已导入/更新 %1 个单词。").arg(count));
    refreshAll();
}

void MainWindow::resetAllDialog()
{
    const auto answer = QMessageBox::warning(
        this, QStringLiteral("重置全部词表进度？"),
        QStringLiteral("所有词表的“已掌握”状态都会清空，词表本身不受影响。"),
        QMessageBox::Ok | QMessageBox::Cancel);
    if (answer != QMessageBox::Ok)
        return;
    m_store->resetAllLists();
    refreshAll();
}

// ---------- 阅读 ----------

void MainWindow::refreshArticleList()
{
    const qint64 previous = m_currentArticleId;
    m_articleList->clear();
    const QVector<Article> articles = m_store->listArticles();
    QListWidgetItem *selectItem = nullptr;
    for (const Article &a : articles) {
        const ArticleStats stats = m_store->articleStats(a.id);
        const QString level = (a.difficulty >= 1 && a.difficulty <= 4)
                                  ? QStringLiteral("L%1 · ").arg(a.difficulty)
                                  : QString();
        const QString label = QStringLiteral("%1（%2覆盖率 %3%）")
                                  .arg(a.title, level)
                                  .arg(QString::number(stats.coverage, 'f', 0));
        auto *item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, a.id);
        m_articleList->addItem(item);
        if (a.id == previous)
            selectItem = item;
    }
    if (selectItem)
        m_articleList->setCurrentItem(selectItem);
}

void MainWindow::onArticleSelected()
{
    QListWidgetItem *item = m_articleList->currentItem();
    if (item)
        loadArticle(item->data(Qt::UserRole).toLongLong());
}

void MainWindow::loadArticle(qint64 articleId)
{
    const std::optional<Article> article = m_store->getArticle(articleId);
    if (!article)
        return;
    m_currentArticleId = articleId;
    m_currentArticleContent = article->content;
    m_reader->setHtml(renderArticleHtml(article->content));
    m_store->logCoverage(articleId);
}

QString MainWindow::renderArticleHtml(const QString &content) const
{
    auto wordHtml = [this](const QString &w) {
        QString lookup = w.toLower();
        std::optional<Word> inList = m_store->findInCurrentList(lookup);
        if (!inList) {
            lookup = m_store->lookupLemma(lookup);
            inList = m_store->findInCurrentList(lookup);
        }
        QString color = QStringLiteral("#000000");
        if (!inList) {
            color = QStringLiteral("#c62828");
        } else if (inList->box == 0) {
            color = QStringLiteral("#1565c0");
        } else {
            color = QStringLiteral("#000000");
        }
        return QStringLiteral(
                   "<a href=\"word://%1\" style=\"color:%2; "
                   "text-decoration:none;\">%3</a>")
            .arg(w.toLower().toHtmlEscaped(), color, w.toHtmlEscaped());
    };

    QString html = QStringLiteral(
        "<div style=\"font-size:16px; line-height:1.8;\">");
    QString word;
    for (const QChar c : content) {
        if (c.isLetterOrNumber() || c == QLatin1Char('\'')
            || c == QLatin1Char('-')) {
            word += c;
        } else {
            if (!word.isEmpty()) {
                html += wordHtml(word);
                word.clear();
            }
            html += QString(c).toHtmlEscaped();
        }
    }
    if (!word.isEmpty())
        html += wordHtml(word);
    html += QStringLiteral("</div>");
    return html;
}

QString MainWindow::sentenceForWord(const QString &content,
                                    const QString &word) const
{
    QStringList sentences;
    QString current;
    for (const QChar c : content) {
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
            if (result.size() > 180)
                result = result.left(177) + QStringLiteral("…");
            return result;
        }
    }
    return {};
}

void MainWindow::importArticleFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择文章"),
        QString(), QStringLiteral("文本文件 (*.txt *.md);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        infoBox(QStringLiteral("导入失败"),
                QStringLiteral("无法读取文件：%1").arg(path));
        return;
    }
    const QString content = QString::fromUtf8(file.readAll()).trimmed();
    if (content.isEmpty()) {
        infoBox(QStringLiteral("导入失败"), QStringLiteral("文件内容为空。"));
        return;
    }
    const QString title = QFileInfo(path).completeBaseName();
    const qint64 id = m_store->saveArticle(title, content,
                                            QStringLiteral("import"), 0);
    refreshArticleList();
    for (int i = 0; i < m_articleList->count(); ++i) {
        if (m_articleList->item(i)->data(Qt::UserRole).toLongLong() == id) {
            m_articleList->setCurrentRow(i);
            break;
        }
    }
    statusBar()->showMessage(
        QStringLiteral("已导入：%1").arg(title), 3000);
}

void MainWindow::importUrl()
{
    bool ok = false;
    const QString urlText = QInputDialog::getText(
        this, QStringLiteral("导入网址"),
        QStringLiteral("输入文章网址："),
        QLineEdit::Normal, QStringLiteral("https://"), &ok);
    if (!ok || urlText.trimmed().isEmpty())
        return;
    if (m_webReply) {
        m_webReply->abort();
        m_webReply = nullptr;
    }
    statusBar()->showMessage(QStringLiteral("正在抓取网页…"));
    QNetworkRequest request(QUrl(urlText.trimmed()));
    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) English3000/1.0"));
    m_webReply = m_webManager->get(request);
    connect(m_webReply, &QNetworkReply::finished, this,
            &MainWindow::onWebFinished);
}

void MainWindow::onWebFinished()
{
    QNetworkReply *reply = m_webReply;
    m_webReply = nullptr;
    if (!reply)
        return;
    statusBar()->clearMessage();
    if (reply->error() != QNetworkReply::NoError) {
        statusBar()->showMessage(
            QStringLiteral("网页抓取失败：%1").arg(reply->errorString()), 6000);
        reply->deleteLater();
        return;
    }
    const QByteArray data = reply->readAll();
    const QUrl url = reply->request().url();
    reply->deleteLater();

    QTextDocument doc;
    doc.setHtml(QString::fromUtf8(data));
    QStringList lines;
    const QStringList rawLines = doc.toPlainText().split(QLatin1Char('\n'));
    for (const QString &line : rawLines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
            lines << trimmed;
    }
    const QString content = lines.join(QLatin1Char('\n'));
    if (content.size() < 80) {
        statusBar()->showMessage(
            QStringLiteral("没有抓到有效正文（页面可能依赖 JavaScript）"),
            6000);
        return;
    }
    QString title = url.host();
    if (title.isEmpty())
        title = QStringLiteral("网页文章");
    const qint64 id = m_store->saveArticle(
        title, content, QStringLiteral("web"), 0);
    refreshArticleList();
    for (int i = 0; i < m_articleList->count(); ++i) {
        if (m_articleList->item(i)->data(Qt::UserRole).toLongLong() == id) {
            m_articleList->setCurrentRow(i);
            break;
        }
    }
    statusBar()->showMessage(
        QStringLiteral("已导入：%1").arg(title), 4000);
}

void MainWindow::deleteCurrentArticle()
{
    QListWidgetItem *item = m_articleList->currentItem();
    if (!item)
        return;
    const qint64 id = item->data(Qt::UserRole).toLongLong();
    const auto answer = QMessageBox::question(
        this, QStringLiteral("删除文章"),
        QStringLiteral("确定删除这篇文章？"),
        QMessageBox::Ok | QMessageBox::Cancel);
    if (answer != QMessageBox::Ok)
        return;
    m_store->deleteArticle(id);
    if (m_currentArticleId == id) {
        m_currentArticleId = -1;
        m_currentArticleContent.clear();
        m_reader->clear();
    }
    refreshArticleList();
}

void MainWindow::onWordContextMenu(const QPoint &pos)
{
    QTextCursor cursor = m_reader->cursorForPosition(pos);
    cursor.select(QTextCursor::WordUnderCursor);
    showWordMenu(cursor.selectedText().trimmed());
}

void MainWindow::showWordMenu(const QString &rawWord)
{
    QString word = rawWord.trimmed();
    if (word.isEmpty())
        return;
    std::optional<Word> found = m_store->findWordByText(word);
    if (!found) {
        word = m_store->lookupLemma(word);
        found = m_store->findWordByText(word);
    }
    m_clickedWord = word;
    m_clickedDictMeaning.clear();

    auto *menu = new QMenu(this);
    if (found) {
        QString meaning = found->meaning;
        if (!found->pos.isEmpty() && !meaning.startsWith(found->pos))
            meaning = found->pos + QStringLiteral(" ") + meaning;
        if (meaning.isEmpty())
            meaning = QStringLiteral("（暂无释义）");
        if (meaning.size() > 120)
            meaning = meaning.left(117) + QStringLiteral("…");
        auto *meaningAction = menu->addAction(meaning);
        meaningAction->setEnabled(false);
    } else {
        const std::optional<Word> dictWord = m_store->lookupDict(word);
        if (dictWord) {
            QString meaning = dictWord->meaning;
            if (!dictWord->pos.isEmpty())
                meaning = dictWord->pos + QStringLiteral(" ") + meaning;
            m_clickedDictMeaning = dictWord->meaning;
            if (meaning.size() > 120)
                meaning = meaning.left(117) + QStringLiteral("…");
            auto *meaningAction =
                menu->addAction(QStringLiteral("词典：%1").arg(meaning));
            meaningAction->setEnabled(false);
        } else {
            auto *outAction = menu->addAction(
                QStringLiteral("词表外（可加入阅读词表）"));
            outAction->setEnabled(false);
        }
    }
    menu->addSeparator();
    menu->addAction(QStringLiteral("发音"), this,
                    [this] { speakText(m_clickedWord); });
    menu->addAction(QStringLiteral("加入阅读词表"), this,
                    &MainWindow::queueClickedWord);
    const std::optional<Word> inList = m_store->findInCurrentList(word);
    if (inList && inList->box == 0) {
        menu->addAction(QStringLiteral("标记已会"), this,
                        &MainWindow::markClickedWordKnown);
    }
    menu->popup(QCursor::pos());
}

void MainWindow::queueClickedWord()
{
    if (m_currentArticleId < 0 || m_clickedWord.isEmpty())
        return;
    const QString sentence =
        sentenceForWord(m_currentArticleContent, m_clickedWord);
    const qint64 id = m_store->queueWordToReadingList(
        m_clickedWord, m_clickedDictMeaning, sentence);
    if (id > 0) {
        statusBar()->showMessage(
            QStringLiteral("已加入阅读词表：%1").arg(m_clickedWord), 3000);
        refreshDashboard();
    }
}

void MainWindow::markClickedWordKnown()
{
    const std::optional<Word> found = m_store->findInCurrentList(m_clickedWord);
    if (!found)
        return;
    m_store->markItemKnown(found->itemId);
    refreshDashboard();
    if (m_currentArticleId >= 0)
        m_reader->setHtml(renderArticleHtml(m_currentArticleContent));
}

// ---------- 朗读（本地 TTS） ----------

void MainWindow::speakText(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;
    stopSpeaking();
    m_speaking = true;
    if (m_speakButton)
        m_speakButton->setText(QStringLiteral("停止朗读"));

#if defined(Q_OS_WIN)
    // Windows：使用系统 SAPI 离线朗读（优先英文语音）
    m_tts = new QProcess(this);
    connect(m_tts,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
                m_speaking = false;
                if (m_speakButton)
                    m_speakButton->setText(QStringLiteral("朗读文章"));
                m_tts->deleteLater();
                m_tts = nullptr;
            });
    QString escaped = trimmed;
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
    const QString wavPath =
        QDir::tempPath() + QStringLiteral("/english3000_tts.wav");
    m_tts = new QProcess(this);
    connect(m_tts, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
                onTtsFinished(exitCode);
            });

    // 优先用 Piper（神经网络音色），缺失时回退 espeak-ng
    QString piperPath;
    const QString localPiper =
        QDir::homePath()
        + QStringLiteral("/.local/bin/piper");
    if (QFileInfo::exists(localPiper)) {
        piperPath = localPiper;
    } else {
        piperPath = QStandardPaths::findExecutable(QStringLiteral("piper"));
    }
    const QString voicePath =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/voices/en_US-lessac-medium.onnx");

    if (!piperPath.isEmpty() && QFileInfo::exists(voicePath)) {
        m_tts->setProcessChannelMode(QProcess::SeparateChannels);
        m_tts->setStandardOutputFile(QProcess::nullDevice());
        m_tts->setStandardErrorFile(QProcess::nullDevice());
        m_tts->start(piperPath,
                     {QStringLiteral("-m"), voicePath,
                      QStringLiteral("-f"), wavPath});
        if (m_tts->waitForStarted(3000)) {
            m_tts->write(trimmed.toUtf8());
            m_tts->closeWriteChannel();
            return;
        }
        stopSpeaking();
    }

    if (QStandardPaths::findExecutable(QStringLiteral("espeak-ng")).isEmpty()) {
        statusBar()->showMessage(
            QStringLiteral("未找到可用的朗读引擎（piper / espeak-ng）"),
            5000);
        return;
    }
    m_speaking = true;
    if (m_speakButton)
        m_speakButton->setText(QStringLiteral("停止朗读"));
    m_tts = new QProcess(this);
    connect(m_tts, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
                onTtsFinished(exitCode);
            });
    m_tts->start(QStringLiteral("espeak-ng"),
                 {QStringLiteral("-v"), QStringLiteral("en-us"),
                  QStringLiteral("-s"), QStringLiteral("155"),
                  QStringLiteral("-w"), wavPath, trimmed});
#endif
}

void MainWindow::stopSpeaking()
{
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
    m_speaking = false;
    if (m_speakButton)
        m_speakButton->setText(QStringLiteral("朗读文章"));
}

void MainWindow::onTtsFinished(int exitCode)
{
    if (!m_tts)
        return;
    m_tts->deleteLater();
    m_tts = nullptr;
    if (exitCode != 0) {
        m_speaking = false;
        if (m_speakButton)
            m_speakButton->setText(QStringLiteral("朗读文章"));
        statusBar()->showMessage(QStringLiteral("朗读失败"), 3000);
        return;
    }
    const QString wavPath =
        QDir::tempPath() + QStringLiteral("/english3000_tts.wav");
    m_player = new QProcess(this);
    connect(m_player,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
                onPlayFinished(exitCode);
            });
    m_player->start(QStringLiteral("paplay"), {wavPath});
}

void MainWindow::onPlayFinished(int exitCode)
{
    if (m_player) {
        m_player->deleteLater();
        m_player = nullptr;
    }
    m_speaking = false;
    if (m_speakButton)
        m_speakButton->setText(QStringLiteral("朗读文章"));
    if (exitCode != 0) {
        statusBar()->showMessage(QStringLiteral("播放失败：请检查音频设备"), 3000);
    }
}

// ---------- AI 文章 ----------

void MainWindow::onGenerateClicked()
{
    if (m_radioTopic->isChecked()) {
        const QString topic = m_topicEdit->text().trimmed();
        if (topic.isEmpty()) {
            infoBox(QStringLiteral("缺少主题"),
                    QStringLiteral("请先输入文章主题。"));
            return;
        }
        m_aiStatusLabel->setText(
            QStringLiteral("正在生成《%1》…（本地 AI 约 1~3 分钟）").arg(topic));
        if (m_useCurrentListCheck->isChecked()
            && m_store->currentWordListId() > 0) {
            QStringList preferred;
            const QVector<Word> listWords =
                m_store->wordsInWordList(m_store->currentWordListId());
            for (const Word &w : listWords)
                preferred << w.word;
            m_ai->generateArticle(
                topic, m_topicLevelCombo->currentIndex() + 1,
                m_wordCountSpin->value(), preferred);
        } else {
            m_ai->generateArticle(
                topic, m_topicLevelCombo->currentIndex() + 1,
                m_wordCountSpin->value());
        }
    } else {
        const QString text = m_rewriteEdit->toPlainText().trimmed();
        if (text.isEmpty()) {
            infoBox(QStringLiteral("缺少原文"),
                    QStringLiteral("请先粘贴要改写的英文文章。"));
            return;
        }
        m_aiStatusLabel->setText(
            QStringLiteral("正在改写…（本地 AI 约 1~3 分钟）"));
        m_ai->rewriteText(text, m_rewriteLevelCombo->currentIndex() + 1);
    }
    m_generateButton->setEnabled(false);
    m_cancelButton->setEnabled(true);
}

void MainWindow::onCancelClicked()
{
    m_ai->cancel();
    m_aiStatusLabel->setText(QStringLiteral("已取消。"));
    m_generateButton->setEnabled(true);
    m_cancelButton->setEnabled(false);
}

void MainWindow::onAiFinished(const QString &articleText)
{
    const bool topicMode = m_radioTopic->isChecked();
    QString title = topicMode
                        ? m_topicEdit->text().trimmed()
                        : m_rewriteEdit->toPlainText().trimmed().simplified();
    if (title.size() > 40)
        title = title.left(37) + QStringLiteral("…");
    if (title.isEmpty())
        title = QStringLiteral("AI 生成文章");
    const int level = (topicMode ? m_topicLevelCombo : m_rewriteLevelCombo)
                          ->currentIndex() + 1;
    const qint64 id = m_store->saveArticle(
        title, articleText, QStringLiteral("ai"), level);
    refreshArticleList();
    for (int i = 0; i < m_articleList->count(); ++i) {
        if (m_articleList->item(i)->data(Qt::UserRole).toLongLong() == id) {
            m_articleList->setCurrentRow(i);
            break;
        }
    }
    m_aiStatusLabel->setText(QStringLiteral("生成完成，已存入文章库。"));
    m_generateButton->setEnabled(true);
    m_cancelButton->setEnabled(false);
    m_tabs->setCurrentIndex(TabReading);
    statusBar()->showMessage(QStringLiteral("文章已生成：%1").arg(title), 5000);
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        m_tray->showMessage(QStringLiteral("English 3000"),
                            QStringLiteral("文章已生成：%1").arg(title),
                            QSystemTrayIcon::Information, 5000);
    }
}

void MainWindow::onAiFailed(const QString &message)
{
    if (m_translating) {
        m_translating = false;
        m_translateSelectionButton->setEnabled(true);
        m_translateFullButton->setEnabled(true);
        m_hideTranslateButton->setEnabled(true);
        if (m_translatePanel) {
            m_translatePanel->setPlainText(
                QStringLiteral("翻译失败：%1").arg(message));
        }
        statusBar()->showMessage(
            QStringLiteral("翻译失败：%1").arg(message), 5000);
        return;
    }
    m_aiStatusLabel->setText(QStringLiteral("生成失败：%1").arg(message));
    m_generateButton->setEnabled(true);
    m_cancelButton->setEnabled(false);
    if (message != QStringLiteral("已取消。")) {
        QMessageBox::warning(this, QStringLiteral("AI 生成失败"), message);
    }
}

void MainWindow::onTranslationFinished(const QString &translation)
{
    m_translating = false;
    m_translateSelectionButton->setEnabled(true);
    m_translateFullButton->setEnabled(true);
    m_hideTranslateButton->setEnabled(true);
    m_translatePanel->setVisible(true);
    m_translatePanel->setPlainText(translation);
    statusBar()->showMessage(QStringLiteral("翻译完成"), 3000);
}

void MainWindow::onTranslateSelection()
{
    if (m_currentArticleId < 0 || m_translating)
        return;
    QTextCursor cursor = m_reader->textCursor();
    QString selected = cursor.selectedText();
    selected.replace(QChar(0x2029), QLatin1Char('\n'));
    selected = selected.trimmed();
    if (selected.isEmpty()) {
        statusBar()->showMessage(
            QStringLiteral("请先在文章里用鼠标选中要翻译的文字"), 4000);
        return;
    }
    if (selected.size() > 2000)
        selected = selected.left(2000);
    startTranslate(selected);
}

void MainWindow::onTranslateFull()
{
    if (m_currentArticleId < 0 || m_translating)
        return;
    QString content = m_currentArticleContent.trimmed();
    if (content.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("请先选择一篇文章"), 3000);
        return;
    }
    if (content.size() > 4000) {
        content = content.left(4000);
        statusBar()->showMessage(
            QStringLiteral("文章较长，只翻译前 4000 字符"), 4000);
    }
    startTranslate(content);
}

void MainWindow::startTranslate(const QString &text)
{
    m_translating = true;
    m_translateSelectionButton->setEnabled(false);
    m_translateFullButton->setEnabled(false);
    m_hideTranslateButton->setEnabled(false);
    m_translatePanel->setVisible(true);
    m_translatePanel->setPlainText(QStringLiteral("翻译中…（本地 AI 需要十几秒）"));
    statusBar()->showMessage(QStringLiteral("正在翻译…"));
    m_ai->translateText(text);
}

void MainWindow::onTabChanged(int index)
{
    if (index == TabStudy) {
        refreshDashboard();
    } else if (index == TabReading) {
        refreshArticleList();
        if (m_useCurrentListCheck) {
            const QString name = m_store->currentWordListName();
            m_useCurrentListCheck->setText(
                name.isEmpty()
                    ? QStringLiteral("按当前词表生成（当前：无）")
                    : QStringLiteral("按当前词表生成（当前：%1）").arg(name));
        }
    } else if (index == TabWordLists) {
        m_wordListPage->refresh();
    } else if (index == TabStats) {
        refreshStats();
    }
}

void MainWindow::refreshStats()
{
    const QVector<CoveragePoint> history = m_store->coverageHistory(30);
    QVector<QPair<QDate, double>> points;
    double total = 0.0;
    for (const CoveragePoint &p : history) {
        points.append(
            {QDate::fromString(p.date, Qt::ISODate), p.coverage});
        total += p.coverage;
    }
    if (m_chart)
        m_chart->setData(points);

    const Counts counts = m_store->counts();
    const double avg =
        history.isEmpty() ? 0.0 : total / history.size();
    m_statsReadLabel->setText(
        QString::number(m_store->coverageArticleCount()));
    m_statsCoverageLabel->setText(
        QStringLiteral("%1%").arg(QString::number(avg, 'f', 1)));
    m_statsKnownLabel->setText(QString::number(counts.newTotal));
    m_statsStreakLabel->setText(QString::number(m_store->streak()));
    m_statsMasteredLabel->setText(QString::number(counts.mastered));
}

void MainWindow::applyHotkeys()
{
    if (!m_hotkeys)
        return;
    m_hotkeys->unregisterAll();
    if (m_store->getSetting(QStringLiteral("translate_enabled"),
                            QStringLiteral("1"))
        != QLatin1String("1")) {
        return;
    }
    m_hotkeys->registerKey(
        QKeySequence(m_store->getSetting(QStringLiteral("translate_hotkey"),
                                         QStringLiteral("Ctrl+Alt+T"))),
        QStringLiteral("translate"));
    m_hotkeys->registerKey(
        QKeySequence(m_store->getSetting(
            QStringLiteral("translate_screenshot_hotkey"),
            QStringLiteral("Ctrl+Alt+O"))),
        QStringLiteral("screenshot"));
}

bool MainWindow::autoPronounceEnabled() const
{
    return m_store->getSetting(QStringLiteral("auto_pronounce"),
                               QStringLiteral("1"))
        == QLatin1String("1");
}

void MainWindow::requestExample(qint64 wordId, const QString &word)
{
    if (m_exampleRequested.contains(wordId))
        return;
    m_exampleRequested.insert(wordId);
    m_pendingAiKind = QStringLiteral("example");
    m_pendingAiId = wordId;
    m_pendingAiWord = word;
    const QString prompt =
        QStringLiteral(
            "Write one short, simple English sentence using the word "
            "\"%1\". Use the exact word. Output only the sentence.")
            .arg(word);
    m_ai->chat(prompt, 120, QStringLiteral("qwen2.5:3b"));
}

void MainWindow::onAiChatFinished(const QString &text)
{
    const QString kind = m_pendingAiKind;
    const qint64 id = m_pendingAiId;
    const QString word = m_pendingAiWord;
    m_pendingAiKind.clear();
    m_pendingAiId = -1;
    m_pendingAiWord.clear();
    const QString result = text.trimmed().simplified();
    if (kind == QLatin1String("example")) {
        if (id <= 0 || result.isEmpty())
            return;
        m_store->setExampleSentence(id, result);
        if (m_sessionIndex < m_session.size()
            && m_session[m_sessionIndex].id == id) {
            m_session[m_sessionIndex].exampleSentence = result;
            m_exampleLabel->setText(result);
        }
    }
}

// ---------- 键盘与工具 ----------

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;
    if (m_tabs->currentIndex() == TabStudy
        && m_homeStack->currentIndex() == 1
        && m_studyStack->currentIndex() == 0) {
        switch (event->key()) {
        case Qt::Key_Escape:
            stopSpeaking();
            backToToday();
            event->accept();
            return;
        case Qt::Key_Space:
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (!m_revealed)
                reveal();
            event->accept();
            return;
        case Qt::Key_1:
            answer(false);
            event->accept();
            return;
        case Qt::Key_2:
            answer(true);
            event->accept();
            return;
        default:
            break;
        }
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::infoBox(const QString &title, const QString &text)
{
    QMessageBox::information(this, title, text);
}
