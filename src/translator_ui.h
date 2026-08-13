#pragma once

#include <QAbstractNativeEventFilter>
#include <QHash>
#include <QObject>
#include <QString>
#include <QWidget>

#include "core.h"

class QComboBox;
class QKeySequence;
class QLabel;
class QPlainTextEdit;
class QProcess;
class QRubberBand;
class QTableWidget;
class QTextBrowser;
class QPushButton;
class AiClient;

// ---------- X11 全局热键 ----------

class GlobalHotkey : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey() override;

    bool registerKey(const QKeySequence &sequence, const QString &token);
    void unregisterAll();

signals:
    void activated(const QString &token);

protected:
    bool nativeEventFilter(const QByteArray &eventType, void *message,
                           qintptr *result) override;

private:
    struct DisplayDeleter;
    void *m_display = nullptr; // Display*
    void *m_conn = nullptr;    // xcb_connection_t*
    QHash<quint32, QString> m_keyToToken;   // keycode -> token
    QHash<quint32, quint32> m_keyToMods;    // keycode -> modifier mask
};

// ---------- 截图遮罩 ----------

class ScreenshotOverlay : public QWidget {
    Q_OBJECT

public:
    explicit ScreenshotOverlay(QScreen *screen);

signals:
    void regionSelected(const QRect &screenRect);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QScreen *m_screen = nullptr;
    QPoint m_origin;
    QRubberBand *m_band = nullptr;
};

// ---------- 翻译小窗 ----------

class TranslatorWindow : public QWidget {
    Q_OBJECT

public:
    explicit TranslatorWindow(WordStore *store, QWidget *parent = nullptr);
    ~TranslatorWindow() override;

    void openWithText(const QString &text = {});
    void startScreenshot();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void translate();
    void copyResult();
    void clearAll();
    void onTranslationFinished(const QString &translation);
    void onTranslationFailed(const QString &message);
    void onOcrFinished(int exitCode);

private:
    QString currentModel() const;
    QString defaultModel() const;
    void runOcr(const QPixmap &pixmap, const QRect &screenRect);

    WordStore *m_store = nullptr;
    AiClient *m_ai = nullptr;
    QPlainTextEdit *m_sourceEdit = nullptr;
    QComboBox *m_modelCombo = nullptr;
    QTextBrowser *m_resultView = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_translateButton = nullptr;
    QPushButton *m_copyButton = nullptr;
    QPushButton *m_clearButton = nullptr;
    QProcess *m_ocr = nullptr;
    QPixmap m_captured;
    QString m_lastSource;
};

// ---------- 截图结果窗 ----------

class ScreenshotResultWindow : public QWidget {
    Q_OBJECT

public:
    explicit ScreenshotResultWindow(WordStore *store, const QPixmap &pixmap,
                                    const QString &ocrText,
                                    QWidget *parent = nullptr);
    ~ScreenshotResultWindow() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void copyTranslation();
    void onTranslationFinished(const QString &translation);
    void onTranslationFailed(const QString &message);

private:
    WordStore *m_store = nullptr;
    AiClient *m_ai = nullptr;
    QLabel *m_imageLabel = nullptr;
    QTextBrowser *m_ocrView = nullptr;
    QTextBrowser *m_resultView = nullptr;
    QPushButton *m_copyButton = nullptr;
    QString m_ocrText;
};
