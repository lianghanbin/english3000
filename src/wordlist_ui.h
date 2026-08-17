#pragma once

#include <QWidget>

#include "core.h"

class AiClient;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QProgressBar;
class QTableWidget;

class WordListPage : public QWidget {
    Q_OBJECT

public:
    explicit WordListPage(WordStore *store, QWidget *parent = nullptr);
    void applyAiSettings();

public slots:
    void refresh();
    void jumpToList(qint64 listId);

signals:
    void wordSpeakRequested(const QString &word);

private slots:
    void onListSelected();
    void createAiDialog();
    void createFromArticlesDialog();
    void supplementAiList();
    void setCurrent();
    void resetCurrent();
    void saveOrder();
    void deleteCurrent();
    void markSelected();
    void resetSelected();
    void addWordDialog();
    void importDialog();
    void onWordListFinished(const QString &rawText);
    void onAiFailed(const QString &message);

private:
    void fillCurrentScope();
    void selectList(qint64 listId);

    WordStore *m_store = nullptr;
    AiClient *m_ai = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QListWidget *m_listWidget = nullptr;
    QTableWidget *m_table = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_aiButton = nullptr;
    QPushButton *m_articleButton = nullptr;
    QPushButton *m_currentButton = nullptr;
    QPushButton *m_resetButton = nullptr;
    QPushButton *m_moreButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QWidget *m_globalButtons = nullptr;
    QWidget *m_listButtons = nullptr;
    QString m_pendingName;
    int m_pendingCount = 0;
    qint64 m_pendingListId = -1;
    qint64 m_scopeId = -1;
};
