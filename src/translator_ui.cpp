#include "translator_ui.h"

#include "ai_client.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QDir>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRubberBand>
#include <QScreen>
#include <QSet>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <QtGui/qguiapplication_platform.h>
#include <X11/Xlib.h>
#include <xcb/xcb.h>

#undef KeyPress
#undef KeyRelease
#undef None
#endif

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace {

bool isMostlyChinese(const QString &text)
{
    int cjk = 0;
    int total = 0;
    for (const QChar c : text) {
        if (c.unicode() == 0x20 || c.unicode() == '\n')
            continue;
        ++total;
        const quint32 u = c.unicode();
        if ((u >= 0x4E00 && u <= 0x9FFF)
            || (u >= 0x3400 && u <= 0x4DBF)
            || (u >= 0xF900 && u <= 0xFAFF)) {
            ++cjk;
        }
    }
    return total > 0 && double(cjk) / total >= 0.3;
}

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
QString qtKeyToKeysymName(int qtKey)
{
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
        return QString(QChar(static_cast<char>(qtKey)));
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
        return QString(QChar(static_cast<char>(qtKey)));
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F35)
        return QStringLiteral("F%1").arg(qtKey - Qt::Key_F1 + 1);
    switch (qtKey) {
    case Qt::Key_Space: return QStringLiteral("space");
    case Qt::Key_Return:
    case Qt::Key_Enter: return QStringLiteral("Return");
    case Qt::Key_Tab: return QStringLiteral("Tab");
    case Qt::Key_Escape: return QStringLiteral("Escape");
    case Qt::Key_Backspace: return QStringLiteral("BackSpace");
    case Qt::Key_Delete: return QStringLiteral("Delete");
    case Qt::Key_Home: return QStringLiteral("Home");
    case Qt::Key_End: return QStringLiteral("End");
    case Qt::Key_PageUp: return QStringLiteral("Prior");
    case Qt::Key_PageDown: return QStringLiteral("Next");
    case Qt::Key_Left: return QStringLiteral("Left");
    case Qt::Key_Right: return QStringLiteral("Right");
    case Qt::Key_Up: return QStringLiteral("Up");
    case Qt::Key_Down: return QStringLiteral("Down");
    default: return {};
    }
}

quint32 qtModsToXMask(Qt::KeyboardModifiers mods)
{
    quint32 mask = 0;
    if (mods & Qt::ControlModifier)
        mask |= ControlMask;
    if (mods & Qt::AltModifier)
        mask |= Mod1Mask;
    if (mods & Qt::ShiftModifier)
        mask |= ShiftMask;
    if (mods & Qt::MetaModifier)
        mask |= Mod4Mask;
    return mask;
}
#endif

} // namespace

// ---------- GlobalHotkey ----------

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
{
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    m_display = XOpenDisplay(nullptr);
#endif
    qApp->installNativeEventFilter(this);
}

GlobalHotkey::~GlobalHotkey()
{
    qApp->removeNativeEventFilter(this);
    unregisterAll();
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if (m_display)
        XCloseDisplay(static_cast<Display *>(m_display));
#endif
}

bool GlobalHotkey::registerKey(const QKeySequence &sequence,
                               const QString &token)
{
#if defined(Q_OS_WIN)
    if (sequence.isEmpty() || token.isEmpty())
        return false;
    const QKeyCombination combo = sequence[0];
    const Qt::KeyboardModifiers mods = combo.keyboardModifiers();
    const int base = int(combo.key());
    UINT mod = 0;
    if (mods & Qt::ControlModifier)
        mod |= MOD_CONTROL;
    if (mods & Qt::AltModifier)
        mod |= MOD_ALT;
    if (mods & Qt::ShiftModifier)
        mod |= MOD_SHIFT;
    if (mods & Qt::MetaModifier)
        mod |= MOD_WIN;
    UINT vk = 0;
    if (base >= Qt::Key_A && base <= Qt::Key_Z) {
        vk = 'A' + (base - Qt::Key_A);
    } else if (base >= Qt::Key_0 && base <= Qt::Key_9) {
        vk = '0' + (base - Qt::Key_0);
    } else if (base >= Qt::Key_F1 && base <= Qt::Key_F24) {
        vk = VK_F1 + (base - Qt::Key_F1);
    } else if (base == Qt::Key_Space) {
        vk = VK_SPACE;
    } else if (base == Qt::Key_Return || base == Qt::Key_Enter) {
        vk = VK_RETURN;
    } else if (base == Qt::Key_Tab) {
        vk = VK_TAB;
    } else if (base == Qt::Key_Escape) {
        vk = VK_ESCAPE;
    } else {
        return false;
    }
    const int id = m_nextId++;
    if (!RegisterHotKey(nullptr, id, mod, vk))
        return false;
    m_idToToken.insert(id, token);
    return true;
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if (!m_display || sequence.isEmpty() || token.isEmpty())
        return false;
    const QKeyCombination combo = sequence[0];
    const Qt::KeyboardModifiers mods = combo.keyboardModifiers();
    const int base = int(combo.key());
    const QString name = qtKeyToKeysymName(base);
    if (name.isEmpty())
        return false;
    Display *dpy = static_cast<Display *>(m_display);
    const KeySym sym = XStringToKeysym(name.toLatin1().constData());
    if (sym == NoSymbol)
        return false;
    const KeyCode kc = XKeysymToKeycode(dpy, sym);
    const quint32 mask = qtModsToXMask(mods);
    if (!m_conn) {
        if (auto *x11 = qGuiApp
                ->nativeInterface<QNativeInterface::QX11Application>()) {
            m_conn = x11->connection();
        }
    }
    auto *conn = static_cast<xcb_connection_t *>(m_conn);
    if (!conn)
        return false;
    const xcb_screen_t *screen =
        xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    // 在 Qt 自己的 xcb 连接上抓键，事件才能进 nativeEventFilter
    xcb_grab_key(conn, 1, screen->root, mask, kc,
                 XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    xcb_grab_key(conn, 1, screen->root, mask | XCB_MOD_MASK_2, kc,
                 XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    xcb_flush(conn);
    m_keyToToken.insert(kc, token);
    m_keyToMods.insert(kc, mask);
    return true;
#endif
}

void GlobalHotkey::unregisterAll()
{
#if defined(Q_OS_WIN)
    for (int id : m_idToToken.keys())
        UnregisterHotKey(nullptr, id);
    m_idToToken.clear();
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    auto *conn = static_cast<xcb_connection_t *>(m_conn);
    if (!conn)
        return;
    const xcb_screen_t *screen =
        xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    for (quint32 kc : m_keyToToken.keys()) {
        xcb_ungrab_key(conn, kc, screen->root, XCB_MOD_MASK_ANY);
    }
    xcb_flush(conn);
    m_keyToToken.clear();
    m_keyToMods.clear();
#else
    m_keyToToken.clear();
    m_keyToMods.clear();
#endif
}

bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType,
                                     void *message, qintptr *result)
{
#if defined(Q_OS_WIN)
    Q_UNUSED(result);
    if (eventType == QByteArrayLiteral("windows_generic_MSG") && message) {
        auto *msg = static_cast<MSG *>(message);
        if (msg->message == WM_HOTKEY) {
            const int id = static_cast<int>(msg->wParam);
            if (m_idToToken.contains(id)) {
                emit activated(m_idToToken.value(id));
                return true;
            }
        }
    }
    return false;
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    Q_UNUSED(result);
    if (eventType != QByteArrayLiteral("xcb_generic_event_t") || !message)
        return false;
    auto *event = static_cast<xcb_generic_event_t *>(message);
    if ((event->response_type & 0x7f) != XCB_KEY_PRESS)
        return false;
    auto *press = reinterpret_cast<xcb_key_press_event_t *>(event);
    const quint32 kc = press->detail;
    if (!m_keyToToken.contains(kc))
        return false;
    const quint32 state =
        press->state
        & (ControlMask | Mod1Mask | ShiftMask | Mod4Mask);
    if (state != m_keyToMods.value(kc))
        return false;
    emit activated(m_keyToToken.value(kc));
    return true;
#endif
}

// ---------- ScreenshotOverlay ----------

ScreenshotOverlay::ScreenshotOverlay(QScreen *screen)
    : QWidget(nullptr)
    , m_screen(screen)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint);
    setGeometry(screen->geometry());
    setCursor(Qt::CrossCursor);
    setWindowOpacity(0.35);
    setStyleSheet(QStringLiteral("background: black;"));
    m_band = new QRubberBand(QRubberBand::Rectangle, this);
}

void ScreenshotOverlay::mousePressEvent(QMouseEvent *event)
{
    m_origin = event->pos();
    m_band->setGeometry(QRect(m_origin, QSize()));
    m_band->show();
}

void ScreenshotOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if (m_band->isVisible()) {
        m_band->setGeometry(
            QRect(m_origin, event->pos()).normalized());
    }
}

void ScreenshotOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    const QRect rect = QRect(m_origin, event->pos()).normalized();
    if (rect.width() < 5 || rect.height() < 5) {
        close();
        return;
    }
    const QPoint global = mapToGlobal(rect.topLeft());
    close();
    emit regionSelected(QRect(global, rect.size()));
}

void ScreenshotOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        close();
    else
        QWidget::keyPressEvent(event);
}

// ---------- TranslatorWindow ----------

TranslatorWindow::TranslatorWindow(WordStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint);
    setWindowTitle(QStringLiteral("翻译"));
    resize(440, 420);

    m_ai = new AiClient(this);
    m_ai->setEndpoint(
        store->getSetting(QStringLiteral("ai_base_url"),
                          QStringLiteral("http://127.0.0.1:11434")),
        store->getSetting(QStringLiteral("ai_model"),
                          QStringLiteral("qwen2.5:1.5b")));
    const bool openAi =
        store->getSetting(QStringLiteral("ai_provider"),
                          QStringLiteral("ollama"))
        == QLatin1String("openai");
    m_ai->setProvider(openAi ? AiClient::Provider::OpenAI
                             : AiClient::Provider::Ollama);
    m_ai->setApiKey(
        store->getSetting(QStringLiteral("ai_api_key")));
    connect(m_ai, &AiClient::translationFinished, this,
            &TranslatorWindow::onTranslationFinished);
    connect(m_ai, &AiClient::failed, this,
            &TranslatorWindow::onTranslationFailed);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(6);
    layout->setContentsMargins(8, 8, 8, 8);

    auto *titleRow = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("翻译"), this);
    title->setObjectName(QStringLiteral("wordLabel"));
    titleRow->addWidget(title);
    m_modelCombo = new QComboBox(this);
    m_modelCombo->addItem(QStringLiteral("快译 qwen2.5:1.5b"),
                          QStringLiteral("qwen2.5:1.5b"));
    m_modelCombo->addItem(QStringLiteral("精译 qwen3:14b"),
                          QStringLiteral("qwen3:14b"));
    const QString defModel = defaultModel();
    const int defIdx = m_modelCombo->findData(defModel);
    m_modelCombo->setCurrentIndex(defIdx >= 0 ? defIdx : 0);
    titleRow->addWidget(m_modelCombo);
    titleRow->addStretch();
    auto *shotButton = new QPushButton(QStringLiteral("截图翻译"), this);
    connect(shotButton, &QPushButton::clicked, this,
            &TranslatorWindow::startScreenshot);
    titleRow->addWidget(shotButton);
    auto *closeButton = new QPushButton(QStringLiteral("✕"), this);
    connect(closeButton, &QPushButton::clicked, this, &QWidget::hide);
    titleRow->addWidget(closeButton);
    layout->addLayout(titleRow);

    m_sourceEdit = new QPlainTextEdit(this);
    m_sourceEdit->installEventFilter(this);
    m_sourceEdit->setPlaceholderText(
        QStringLiteral("要翻译的文字（自动带入选区/剪贴板）…"));
    m_sourceEdit->setFixedHeight(52);
    layout->addWidget(m_sourceEdit);

    auto *buttonRow = new QHBoxLayout;
    m_translateButton = new QPushButton(QStringLiteral("翻译"), this);
    m_translateButton->setObjectName(QStringLiteral("primaryButton"));
    m_copyButton = new QPushButton(QStringLiteral("复制译文"), this);
    m_copyButton->setEnabled(false);
    m_clearButton = new QPushButton(QStringLiteral("清空"), this);
    connect(m_translateButton, &QPushButton::clicked, this,
            &TranslatorWindow::translate);
    connect(m_copyButton, &QPushButton::clicked, this,
            &TranslatorWindow::copyResult);
    connect(m_clearButton, &QPushButton::clicked, this,
            &TranslatorWindow::clearAll);
    buttonRow->addWidget(m_translateButton);
    buttonRow->addWidget(m_copyButton);
    buttonRow->addWidget(m_clearButton);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);

    m_resultView = new QTextBrowser(this);
    m_resultView->setMaximumHeight(200);
    layout->addWidget(m_resultView);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("hintLabel"));
    layout->addWidget(m_statusLabel);

}

TranslatorWindow::~TranslatorWindow()
{
    if (m_ocr) {
        m_ocr->kill();
        m_ocr->deleteLater();
    }
}

QString TranslatorWindow::defaultModel() const
{
    const QString model = m_store->getSetting(
        QStringLiteral("translate_default_model"),
        QStringLiteral("qwen2.5:1.5b"));
    return model.isEmpty() ? QStringLiteral("qwen2.5:1.5b") : model;
}

QString TranslatorWindow::currentModel() const
{
    return m_modelCombo->currentData().toString();
}

void TranslatorWindow::openWithText(const QString &text)
{
    if (!text.isEmpty()) {
        m_sourceEdit->setPlainText(text);
        m_sourceEdit->moveCursor(QTextCursor::End);
        show();
        raise();
        activateWindow();
        m_sourceEdit->setFocus();
        return;
    }
    // X11 主选择：用户刚用鼠标选中的文字，优先于剪贴板
    const QString sel =
        QApplication::clipboard()->text(QClipboard::Selection).trimmed();
    if (!sel.isEmpty()) {
        m_sourceEdit->setPlainText(sel);
    } else if (m_sourceEdit->toPlainText().trimmed().isEmpty()) {
        const QString clip =
            QApplication::clipboard()->text(QClipboard::Clipboard).trimmed();
        m_sourceEdit->setPlainText(clip);
    }
    show();
    raise();
    activateWindow();
    m_sourceEdit->setFocus();
}

void TranslatorWindow::demoTranslate(const QString &text)
{
    m_sourceEdit->setPlainText(text);
    show();
    raise();
    activateWindow();
    translate();
}

void TranslatorWindow::translate()
{
    const QString text = m_sourceEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("请输入要翻译的文字"));
        return;
    }
    if (text.size() > 5000) {
        m_statusLabel->setText(QStringLiteral("文字过长，最多 5000 字符"));
        return;
    }
    const bool toChinese = !isMostlyChinese(text);
    m_lastSource = text;
    m_translateButton->setEnabled(false);
    m_resultView->setPlainText(QStringLiteral("翻译中…"));
    m_statusLabel->setText(QStringLiteral("翻译中（本地模型需要几秒）"));
    m_ai->translateText(text, currentModel(), toChinese);
}

void TranslatorWindow::copyResult()
{
    const QString text = m_resultView->toPlainText();
    if (text.isEmpty() || text == QStringLiteral("翻译中…"))
        return;
    QApplication::clipboard()->setText(text);
    m_statusLabel->setText(QStringLiteral("译文已复制"));
}

void TranslatorWindow::clearAll()
{
    if (m_ai->requestType() != AiClient::RequestType::None)
        m_ai->cancel();
    m_sourceEdit->clear();
    m_resultView->clear();
    m_copyButton->setEnabled(false);
    m_statusLabel->clear();
}

void TranslatorWindow::onTranslationFinished(const QString &translation)
{
    m_resultView->setPlainText(translation);
    m_copyButton->setEnabled(true);
    m_translateButton->setEnabled(true);
    m_statusLabel->setText(QStringLiteral("翻译完成"));
    if (!m_lastSource.trimmed().isEmpty()) {
        const QVector<Word> unknown =
            m_store->extractUnknownWords(m_lastSource, 20);
        for (const Word &w : unknown) {
            m_store->queueWordFromTranslation(
                w.word, w.meaning,
                WordStore::sentenceContaining(m_lastSource, w.word));
        }
    }
}

void TranslatorWindow::onTranslationFailed(const QString &message)
{
    if (message == QStringLiteral("已取消。")) {
        m_translateButton->setEnabled(true);
        return;
    }
    m_resultView->setPlainText(QStringLiteral("翻译失败：%1").arg(message));
    m_translateButton->setEnabled(true);
    m_statusLabel->setText(QStringLiteral("翻译失败"));
}

void TranslatorWindow::startScreenshot()
{
#if !defined(Q_OS_LINUX) || defined(Q_OS_ANDROID)
    m_statusLabel->setText(QStringLiteral("截图翻译暂仅支持 Linux"));
    show();
    return;
#else
    hide();
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    auto *overlay = new ScreenshotOverlay(screen);
    overlay->setAttribute(Qt::WA_DeleteOnClose);
    connect(overlay, &ScreenshotOverlay::regionSelected, this,
            [this, screen](const QRect &rect) {
                const QRect local(
                    rect.x() - screen->geometry().x(),
                    rect.y() - screen->geometry().y(),
                    rect.width(), rect.height());
                const QPixmap pixmap =
                    screen->grabWindow(0, local.x(), local.y(),
                                       local.width(), local.height());
                runOcr(pixmap, local);
            });
    overlay->showFullScreen();
    overlay->raise();
    overlay->activateWindow();
#endif
}

void TranslatorWindow::runOcr(const QPixmap &pixmap, const QRect &screenRect)
{
    Q_UNUSED(screenRect);
    m_captured = pixmap;
    const QString path =
        QDir::tempPath() + QStringLiteral("/english3000_ocr.png");
    if (!m_captured.save(path)) {
        m_statusLabel->setText(QStringLiteral("截图保存失败"));
        return;
    }
    if (m_ocr) {
        m_ocr->kill();
        m_ocr->deleteLater();
    }
    m_statusLabel->setText(QStringLiteral("正在识别截图文字…"));
    m_ocr = new QProcess(this);
    connect(m_ocr, QOverload<int, QProcess::ExitStatus>::of(
                       &QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
                onOcrFinished(exitCode);
            });
    m_ocr->start(QStringLiteral("tesseract"),
                 {path, QStringLiteral("stdout"), QStringLiteral("-l"),
                  QStringLiteral("eng"), QStringLiteral("--psm"),
                  QStringLiteral("6")});
}

void TranslatorWindow::onOcrFinished(int exitCode)
{
    if (!m_ocr)
        return;
    const QString text =
        QString::fromUtf8(m_ocr->readAllStandardOutput()).trimmed();
    m_ocr->deleteLater();
    m_ocr = nullptr;
    if (exitCode != 0 || text.isEmpty()) {
        m_statusLabel->setText(
            QStringLiteral("未识别到英文文字（可能需要 tesseract）"));
        show();
        return;
    }
    auto *win = new ScreenshotResultWindow(m_store, m_captured, text);
    win->setAttribute(Qt::WA_DeleteOnClose);
    win->show();
}

void TranslatorWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        event->accept();
        return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && !(event->modifiers() & Qt::ShiftModifier)) {
        translate();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool TranslatorWindow::event(QEvent *event)
{
    if (event->type() == QEvent::WindowDeactivate) {
        // 截图模式下不禁用失焦自动隐藏，避免截图时窗口消失
        if (qEnvironmentVariableIntValue("ENGLISH3000_SCREENSHOT") == 1)
            return QWidget::event(event);
        QTimer::singleShot(200, this, [this] {
            if (!isActiveWindow() && !QApplication::activePopupWidget())
                hide();
        });
    }
    return QWidget::event(event);
}

bool TranslatorWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_sourceEdit && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if ((keyEvent->key() == Qt::Key_Return
             || keyEvent->key() == Qt::Key_Enter)
            && !(keyEvent->modifiers() & Qt::ShiftModifier)) {
            translate();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ---------- ScreenshotResultWindow ----------

ScreenshotResultWindow::ScreenshotResultWindow(WordStore *store,
                                               const QPixmap &pixmap,
                                               const QString &ocrText,
                                               QWidget *parent)
    : QWidget(parent)
    , m_store(store)
    , m_ocrText(ocrText)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint);
    setWindowTitle(QStringLiteral("截图翻译"));
    resize(620, 700);

    m_ai = new AiClient(this);
    m_ai->setEndpoint(
        store->getSetting(QStringLiteral("ai_base_url"),
                          QStringLiteral("http://127.0.0.1:11434")),
        store->getSetting(QStringLiteral("ai_model"),
                          QStringLiteral("qwen2.5:1.5b")));
    const bool openAi =
        store->getSetting(QStringLiteral("ai_provider"),
                          QStringLiteral("ollama"))
        == QLatin1String("openai");
    m_ai->setProvider(openAi ? AiClient::Provider::OpenAI
                             : AiClient::Provider::Ollama);
    m_ai->setApiKey(
        store->getSetting(QStringLiteral("ai_api_key")));
    connect(m_ai, &AiClient::translationFinished, this,
            &ScreenshotResultWindow::onTranslationFinished);
    connect(m_ai, &AiClient::failed, this,
            &ScreenshotResultWindow::onTranslationFailed);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto *titleRow = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("截图翻译"), this);
    title->setObjectName(QStringLiteral("wordLabel"));
    titleRow->addWidget(title);
    titleRow->addStretch();
    m_copyButton = new QPushButton(QStringLiteral("复制译文"), this);
    m_copyButton->setEnabled(false);
    connect(m_copyButton, &QPushButton::clicked, this,
            &ScreenshotResultWindow::copyTranslation);
    titleRow->addWidget(m_copyButton);
    auto *closeButton = new QPushButton(QStringLiteral("✕"), this);
    connect(closeButton, &QPushButton::clicked, this, &QWidget::close);
    titleRow->addWidget(closeButton);
    layout->addLayout(titleRow);

    m_imageLabel = new QLabel(this);
    const QPixmap scaled = pixmap.scaled(
        QSize(560, 240), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_imageLabel->setPixmap(scaled);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_imageLabel);

    m_ocrView = new QTextBrowser(this);
    m_ocrView->setPlainText(m_ocrText);
    m_ocrView->setMaximumHeight(110);
    layout->addWidget(m_ocrView);

    m_resultView = new QTextBrowser(this);
    m_resultView->setPlainText(QStringLiteral("翻译中…"));
    m_resultView->setMaximumHeight(130);
    layout->addWidget(m_resultView);

    layout->addStretch(1);

    const QString model = store->getSetting(
        QStringLiteral("translate_default_model"),
        QStringLiteral("qwen2.5:1.5b"));
    m_ai->translateText(m_ocrText, model, true);
}

ScreenshotResultWindow::~ScreenshotResultWindow()
{
}

void ScreenshotResultWindow::copyTranslation()
{
    const QString text = m_resultView->toPlainText();
    if (text.isEmpty() || text == QStringLiteral("翻译中…"))
        return;
    QApplication::clipboard()->setText(text);
}

void ScreenshotResultWindow::onTranslationFinished(const QString &translation)
{
    m_resultView->setPlainText(translation);
    m_copyButton->setEnabled(true);
    if (!m_ocrText.trimmed().isEmpty()) {
        const QVector<Word> unknown =
            m_store->extractUnknownWords(m_ocrText, 20);
        for (const Word &w : unknown) {
            m_store->queueWordFromTranslation(
                w.word, w.meaning,
                WordStore::sentenceContaining(m_ocrText, w.word));
        }
    }
}

void ScreenshotResultWindow::onTranslationFailed(const QString &message)
{
    m_resultView->setPlainText(QStringLiteral("翻译失败：%1").arg(message));
}

void ScreenshotResultWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}
