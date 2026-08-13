#pragma once

#include <QWidget>

#include "core.h"

class AiClient;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTableWidget;

class WordListPage : public QWidget {
    Q_OBJECT

public:
    explicit WordListPage(WordStore *store, QWidget *parent = nullptr);

public slots:
    void refresh();

private slots:
    void onListSelected();
    void createAiDialog();
    void createFromArticlesDialog();
    void supplementAiList();
    void setCurrent();
    void queueAllToToday();
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
    static constexpr qint64 kAllWordsId = -2;

    WordStore *m_store = nullptr;
    AiClient *m_ai = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QListWidget *m_listWidget = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_aiButton = nullptr;
    QPushButton *m_articleButton = nullptr;
    QPushButton *m_currentButton = nullptr;
    QPushButton *m_queueButton = nullptr;
    QPushButton *m_moreButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QWidget *m_globalButtons = nullptr;
    QWidget *m_listButtons = nullptr;
    QString m_pendingName;
    int m_pendingCount = 0;
    qint64 m_pendingListId = -1;
    qint64 m_scopeId = kAllWordsId;
};
