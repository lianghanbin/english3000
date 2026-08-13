#include "wordlist_ui.h"

#include "ai_client.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {

QStringList splitWordList(const QString &raw)
{
    QString normalized = raw;
    normalized.replace(QLatin1Char(','), QLatin1Char(' '));
    const QStringList parts =
        normalized.split(QRegularExpression(QStringLiteral("\\s+")),
                         Qt::SkipEmptyParts);
    QStringList result;
    for (const QString &part : parts) {
        QString word = part.toLower();
        bool lettersOnly = true;
        for (const QChar c : word) {
            if (!c.isLetter() && c != QLatin1Char('-')
                && c != QLatin1Char('\'')) {
                lettersOnly = false;
                break;
            }
        }
        if (lettersOnly && word.size() >= 2 && !result.contains(word))
            result << word;
    }
    return result;
}

} // namespace

WordListPage::WordListPage(WordStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
{
    m_ai = new AiClient(this);
    m_ai->setEndpoint(
        store->getSetting(QStringLiteral("ai_base_url"),
                          QStringLiteral("http://127.0.0.1:11434")),
        QStringLiteral("qwen2.5:3b")); // 词表生成用快模型
    connect(m_ai, &AiClient::wordListFinished, this,
            &WordListPage::onWordListFinished);
    connect(m_ai, &AiClient::failed, this, &WordListPage::onAiFailed);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto *topRow = new QHBoxLayout;
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索单词或释义…"));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this,
            [this] { fillCurrentScope(); });
    topRow->addWidget(m_searchEdit, 1);
    m_aiButton = new QPushButton(QStringLiteral("AI 生成词表"), this);
    m_articleButton =
        new QPushButton(QStringLiteral("从文章提取"), this);
    connect(m_aiButton, &QPushButton::clicked, this,
            &WordListPage::createAiDialog);
    connect(m_articleButton, &QPushButton::clicked, this,
            &WordListPage::createFromArticlesDialog);
    topRow->addWidget(m_aiButton);
    topRow->addWidget(m_articleButton);
    layout->addLayout(topRow);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    auto *left = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    auto *title = new QLabel(QStringLiteral("词表库"), left);
    title->setObjectName(QStringLiteral("rankLabel"));
    leftLayout->addWidget(title);
    m_listWidget = new QListWidget(left);
    m_listWidget->setAlternatingRowColors(true);
    connect(m_listWidget, &QListWidget::currentRowChanged, this,
            [this](int) { onListSelected(); });
    leftLayout->addWidget(m_listWidget, 1);
    left->setMinimumWidth(240);

    auto *right = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    m_table = new QTableWidget(0, 4, right);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("单词"), QStringLiteral("词性"),
         QStringLiteral("释义"), QStringLiteral("状态")});
    m_table->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    rightLayout->addWidget(m_table, 1);

    m_globalButtons = new QWidget(right);
    auto *globalRow = new QHBoxLayout(m_globalButtons);
    globalRow->setContentsMargins(0, 0, 0, 0);
    auto *markButton = new QPushButton(QStringLiteral("标记已会"), right);
    auto *resetButton = new QPushButton(QStringLiteral("重新学习"), right);
    auto *addButton = new QPushButton(QStringLiteral("添加生词"), right);
    auto *importButton = new QPushButton(QStringLiteral("导入 CSV"), right);
    connect(markButton, &QPushButton::clicked, this,
            &WordListPage::markSelected);
    connect(resetButton, &QPushButton::clicked, this,
            &WordListPage::resetSelected);
    connect(addButton, &QPushButton::clicked, this,
            &WordListPage::addWordDialog);
    connect(importButton, &QPushButton::clicked, this,
            &WordListPage::importDialog);
    globalRow->addWidget(markButton);
    globalRow->addWidget(resetButton);
    globalRow->addWidget(addButton);
    globalRow->addWidget(importButton);
    globalRow->addStretch();

    m_listButtons = new QWidget(right);
    auto *listRow = new QHBoxLayout(m_listButtons);
    listRow->setContentsMargins(0, 0, 0, 0);
    m_currentButton = new QPushButton(QStringLiteral("设为当前词表"), right);
    m_queueButton = new QPushButton(QStringLiteral("全部加入今日新词"), right);
    m_moreButton = new QPushButton(QStringLiteral("AI 补充词表"), right);
    m_deleteButton = new QPushButton(QStringLiteral("删除词表"), right);
    m_currentButton->setObjectName(QStringLiteral("primaryButton"));
    connect(m_currentButton, &QPushButton::clicked, this,
            &WordListPage::setCurrent);
    connect(m_queueButton, &QPushButton::clicked, this,
            &WordListPage::queueAllToToday);
    connect(m_moreButton, &QPushButton::clicked, this,
            &WordListPage::supplementAiList);
    connect(m_deleteButton, &QPushButton::clicked, this,
            &WordListPage::deleteCurrent);
    listRow->addWidget(m_currentButton);
    listRow->addWidget(m_queueButton);
    listRow->addWidget(m_moreButton);
    listRow->addWidget(m_deleteButton);
    listRow->addStretch();
    rightLayout->addWidget(m_globalButtons);
    rightLayout->addWidget(m_listButtons);

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("hintLabel"));
    layout->addWidget(m_statusLabel);

    m_currentButton->setEnabled(false);
    m_queueButton->setEnabled(false);
    m_moreButton->setEnabled(false);
    m_deleteButton->setEnabled(false);
    m_listButtons->hide();
    refresh();
}

void WordListPage::refresh()
{
    const qint64 current = m_store->currentWordListId();
    const qint64 previous = m_listWidget->currentItem()
                                ? m_listWidget->currentItem()
                                      ->data(Qt::UserRole)
                                      .toLongLong()
                                : kAllWordsId;
    m_listWidget->clear();

    auto *allItem = new QListWidgetItem(QStringLiteral("全部单词"));
    allItem->setData(Qt::UserRole, kAllWordsId);
    m_listWidget->addItem(allItem);
    QListWidgetItem *selectItem =
        (previous == kAllWordsId) ? allItem : nullptr;

    const QVector<WordListInfo> lists = m_store->listWordLists();
    for (const WordListInfo &info : lists) {
        const QString source =
            info.source == QLatin1String("ai")
                ? QStringLiteral("AI")
                : (info.source == QLatin1String("article")
                       ? QStringLiteral("文章")
                       : QStringLiteral("手动"));
        QString label = QStringLiteral("%1（%2 词 · %3）")
                            .arg(info.name)
                            .arg(info.wordCount)
                            .arg(source);
        if (info.id == current)
            label += QStringLiteral(" [当前]");
        auto *item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, info.id);
        m_listWidget->addItem(item);
        if (info.id == previous || info.id == current)
            selectItem = item;
    }
    if (selectItem) {
        m_listWidget->setCurrentItem(selectItem);
    } else {
        m_listWidget->setCurrentItem(allItem);
    }
    onListSelected();
}

void WordListPage::onListSelected()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    const qint64 id =
        item ? item->data(Qt::UserRole).toLongLong() : -1;
    m_scopeId = id;
    fillCurrentScope();
}

void WordListPage::fillCurrentScope()
{
    const bool allScope = (m_scopeId == kAllWordsId);
    m_globalButtons->setVisible(allScope);
    m_listButtons->setVisible(!allScope);
    const QString query = m_searchEdit->text().trimmed();
    m_table->setRowCount(0);
    if (allScope) {
        const QVector<Word> words = m_store->search(query);
        m_table->setRowCount(words.size());
        for (int i = 0; i < words.size(); ++i) {
            const Word &w = words[i];
            m_table->setItem(i, 0, new QTableWidgetItem(w.word));
            m_table->setItem(i, 1, new QTableWidgetItem(w.pos));
            m_table->setItem(i, 2, new QTableWidgetItem(w.meaning));
            QString state;
            if (w.box == 0)
                state = QStringLiteral("新词");
            else if (w.box == 6)
                state = QStringLiteral("已掌握");
            else
                state = QStringLiteral("学习中");
            m_table->setItem(i, 3, new QTableWidgetItem(state));
        }
        m_statusLabel->setText(
            QStringLiteral("全部单词（当前词表：%1）")
                .arg(m_store->currentWordListName().isEmpty()
                         ? QStringLiteral("无")
                         : m_store->currentWordListName()));
        return;
    }

    m_currentButton->setEnabled(true);
    m_queueButton->setEnabled(true);
    m_moreButton->setEnabled(true);
    m_deleteButton->setEnabled(true);
    QVector<Word> words = m_store->wordsInWordList(m_scopeId);
    if (!query.isEmpty()) {
        QVector<Word> filtered;
        for (const Word &w : words) {
            if (w.word.contains(query, Qt::CaseInsensitive)
                || w.meaning.contains(query, Qt::CaseInsensitive)) {
                filtered.append(w);
            }
        }
        words = filtered;
    }
    m_table->setRowCount(words.size());
    for (int i = 0; i < words.size(); ++i) {
        const Word &w = words[i];
        m_table->setItem(i, 0, new QTableWidgetItem(w.word));
        m_table->setItem(i, 1, new QTableWidgetItem(w.pos));
        m_table->setItem(i, 2, new QTableWidgetItem(w.meaning));
        const std::optional<Word> found = m_store->findWordByText(w.word);
        QString state;
        if (found) {
            if (found->box == 0)
                state = QStringLiteral("新词");
            else if (found->box == 6)
                state = QStringLiteral("已掌握");
            else
                state = QStringLiteral("学习中");
        }
        m_table->setItem(i, 3, new QTableWidgetItem(state));
    }
    QString name;
    const QVector<WordListInfo> lists = m_store->listWordLists();
    for (const WordListInfo &info : lists) {
        if (info.id == m_scopeId)
            name = info.name;
    }
    m_statusLabel->setText(
        QStringLiteral("词表「%1」· %2 个词").arg(name).arg(words.size()));
}

void WordListPage::selectList(qint64 listId)
{
    for (int i = 0; i < m_listWidget->count(); ++i) {
        if (m_listWidget->item(i)->data(Qt::UserRole).toLongLong()
            == listId) {
            m_listWidget->setCurrentRow(i);
            return;
        }
    }
    refresh();
}

void WordListPage::createAiDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("AI 生成领域词表"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText(
        QStringLiteral("领域，例如：Linux 运维、医学、法律、日常口语"));
    layout->addWidget(nameEdit);
    auto *row = new QHBoxLayout;
    row->addWidget(new QLabel(QStringLiteral("词数"), &dialog));
    auto *countSpin = new QSpinBox(&dialog);
    countSpin->setRange(50, 500);
    countSpin->setSingleStep(50);
    countSpin->setValue(200);
    row->addWidget(countSpin);
    row->addStretch();
    layout->addLayout(row);
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    auto *hint = new QLabel(
        QStringLiteral("提示：词数越多越慢，200 词约 3~5 分钟，"
                       "500 词约 10~15 分钟。生成期间请勿关闭应用。"),
        &dialog);
    hint->setObjectName(QStringLiteral("hintLabel"));
    hint->setWordWrap(true);
    layout->addWidget(hint);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString domain = nameEdit->text().trimmed();
    if (domain.isEmpty())
        return;
    m_pendingListId = -1;
    m_pendingName = domain;
    m_pendingCount = countSpin->value();
    m_aiButton->setEnabled(false);
    m_statusLabel->setText(
        QStringLiteral("正在生成「%1」词表…（本地模型需要一段时间）")
            .arg(domain));
    m_ai->generateWordList(domain, m_pendingCount);
}

void WordListPage::onWordListFinished(const QString &rawText)
{
    m_aiButton->setEnabled(true);
    m_moreButton->setEnabled(true);
    const QStringList words = splitWordList(rawText);
    if (words.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("AI 没有返回有效单词"));
        return;
    }
    if (m_pendingListId > 0) {
        const qint64 listId = m_pendingListId;
        m_pendingListId = -1;
        QSet<QString> existing;
        const QVector<Word> current = m_store->wordsInWordList(listId);
        for (const Word &w : current)
            existing.insert(w.word);
        int added = 0;
        int order = current.size();
        for (const QString &word : words) {
            if (existing.contains(word))
                continue;
            QString pos;
            QString meaning;
            const std::optional<Word> found = m_store->findWordByText(word);
            if (found) {
                pos = found->pos;
                meaning = found->meaning;
            } else {
                const std::optional<Word> dict = m_store->lookupDict(word);
                if (dict) {
                    pos = dict->pos;
                    meaning = dict->meaning;
                }
            }
            if (m_store->addWordToList(listId, word, pos, meaning, order)) {
                existing.insert(word);
                ++added;
                ++order;
            }
        }
        selectList(listId);
        m_statusLabel->setText(
            QStringLiteral("已补充 %1 个新词，词表共 %2 个")
                .arg(added)
                .arg(existing.size()));
        return;
    }
    const qint64 listId = m_store->createWordList(
        m_pendingName,
        QStringLiteral("AI 生成领域词表"),
        QStringLiteral("ai"));
    if (listId < 0) {
        m_statusLabel->setText(
            QStringLiteral("创建失败：同名词表可能已存在"));
        return;
    }
    for (int i = 0; i < words.size(); ++i) {
        const QString &word = words[i];
        QString pos;
        QString meaning;
        const std::optional<Word> found = m_store->findWordByText(word);
        if (found) {
            pos = found->pos;
            meaning = found->meaning;
        } else {
            const std::optional<Word> dict = m_store->lookupDict(word);
            if (dict) {
                pos = dict->pos;
                meaning = dict->meaning;
            }
        }
        m_store->addWordToList(listId, word, pos, meaning, i);
    }
    selectList(listId);
    m_statusLabel->setText(
        QStringLiteral("词表已生成：「%1」共 %2 个词")
            .arg(m_pendingName)
            .arg(words.size()));
}

void WordListPage::onAiFailed(const QString &message)
{
    m_aiButton->setEnabled(true);
    m_moreButton->setEnabled(true);
    m_statusLabel->setText(QStringLiteral("生成失败：%1").arg(message));
}

void WordListPage::supplementAiList()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item)
        return;
    const qint64 listId = item->data(Qt::UserRole).toLongLong();
    QString name;
    const QVector<WordListInfo> lists = m_store->listWordLists();
    for (const WordListInfo &info : lists) {
        if (info.id == listId) {
            name = info.name;
            break;
        }
    }
    if (name.isEmpty())
        return;
    m_pendingListId = listId;
    m_pendingName = name;
    m_aiButton->setEnabled(false);
    m_moreButton->setEnabled(false);
    m_statusLabel->setText(
        QStringLiteral("正在为「%1」补充词表…（本地模型需要一段时间）")
            .arg(name));
    m_ai->generateWordList(name, 100);
}

void WordListPage::createFromArticlesDialog()
{
    const QVector<Article> articles = m_store->listArticles();
    if (articles.isEmpty()) {
        m_statusLabel->setText(
            QStringLiteral("文章库为空，请先导入或生成文章"));
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("从文章提取领域词表"));
    dialog.resize(480, 420);
    auto *layout = new QVBoxLayout(&dialog);
    auto *nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText(QStringLiteral("词表名称，如：Linux 运维"));
    layout->addWidget(nameEdit);
    auto *list = new QListWidget(&dialog);
    for (const Article &a : articles) {
        auto *item = new QListWidgetItem(a.title);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        item->setData(Qt::UserRole, a.id);
        list->addItem(item);
    }
    layout->addWidget(list, 1);
    auto *row = new QHBoxLayout;
    row->addWidget(new QLabel(QStringLiteral("提取词数上限"), &dialog));
    auto *limitSpin = new QSpinBox(&dialog);
    limitSpin->setRange(20, 300);
    limitSpin->setSingleStep(20);
    limitSpin->setValue(100);
    row->addWidget(limitSpin);
    row->addStretch();
    layout->addLayout(row);
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;
    QVector<qint64> ids;
    for (int i = 0; i < list->count(); ++i) {
        if (list->item(i)->checkState() == Qt::Checked)
            ids.append(list->item(i)->data(Qt::UserRole).toLongLong());
    }
    if (ids.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("请至少勾选一篇文章"));
        return;
    }
    QString name = nameEdit->text().trimmed();
    if (name.isEmpty())
        name = QStringLiteral("文章词表");
    const QVector<Word> words =
        m_store->extractDomainWordsFromArticles(ids, limitSpin->value());
    const qint64 listId = m_store->createWordList(
        name, QStringLiteral("从文章提取的高频词"),
        QStringLiteral("article"));
    if (listId < 0) {
        m_statusLabel->setText(
            QStringLiteral("创建失败：同名词表可能已存在"));
        return;
    }
    for (int i = 0; i < words.size(); ++i) {
        m_store->addWordToList(listId, words[i].word, words[i].pos,
                               words[i].meaning, i);
    }
    selectList(listId);
    m_statusLabel->setText(
        QStringLiteral("词表已生成：「%1」共 %2 个词")
            .arg(name)
            .arg(words.size()));
}

void WordListPage::setCurrent()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item)
        return;
    const qint64 id = item->data(Qt::UserRole).toLongLong();
    const QString label = item->text();
    m_store->setCurrentWordList(id);
    refresh();
    m_statusLabel->setText(
        QStringLiteral("当前词表已切换为「%1」").arg(label));
}

void WordListPage::queueAllToToday()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item)
        return;
    const qint64 id = item->data(Qt::UserRole).toLongLong();
    const int count = m_store->queueWordListToToday(id);
    m_statusLabel->setText(
        QStringLiteral("已加入今日新词 %1 个（排在队列最前）").arg(count));
}

void WordListPage::deleteCurrent()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item)
        return;
    const qint64 id = item->data(Qt::UserRole).toLongLong();
    const auto answer = QMessageBox::question(
        this, QStringLiteral("删除词表"),
        QStringLiteral("确定删除词表「%1」？不影响已学单词。")
            .arg(item->text()),
        QMessageBox::Ok | QMessageBox::Cancel);
    if (answer != QMessageBox::Ok)
        return;
    m_store->deleteWordList(id);
    refresh();
    m_statusLabel->setText(QStringLiteral("词表已删除"));
}

void WordListPage::markSelected()
{
    const int row = m_table->currentRow();
    if (row < 0)
        return;
    const QString word = m_table->item(row, 0)->text();
    const std::optional<Word> found = m_store->findWordByText(word);
    if (found) {
        m_store->markKnown(found->id);
        fillCurrentScope();
        m_statusLabel->setText(QStringLiteral("已标记：%1").arg(word));
    }
}

void WordListPage::resetSelected()
{
    const int row = m_table->currentRow();
    if (row < 0)
        return;
    const QString word = m_table->item(row, 0)->text();
    const std::optional<Word> found = m_store->findWordByText(word);
    if (found) {
        m_store->resetWord(found->id);
        fillCurrentScope();
        m_statusLabel->setText(QStringLiteral("已重置：%1").arg(word));
    }
}

void WordListPage::addWordDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("添加生词"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *wordEdit = new QLineEdit(&dialog);
    wordEdit->setPlaceholderText(QStringLiteral("单词，如 kernel"));
    auto *posEdit = new QLineEdit(&dialog);
    posEdit->setPlaceholderText(QStringLiteral("词性，如 n.（可留空）"));
    auto *meaningEdit = new QLineEdit(&dialog);
    meaningEdit->setPlaceholderText(QStringLiteral("中文释义，如 内核"));
    layout->addWidget(wordEdit);
    layout->addWidget(posEdit);
    layout->addWidget(meaningEdit);
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const qint64 id = m_store->addWord(wordEdit->text(), posEdit->text(),
                                       meaningEdit->text());
    if (id < 0) {
        m_statusLabel->setText(QStringLiteral("添加失败：单词已存在或为空"));
    } else {
        m_statusLabel->setText(QStringLiteral("已添加：%1").arg(wordEdit->text()));
        fillCurrentScope();
    }
}

void WordListPage::importDialog()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择词表 CSV"), QString(),
        QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;
    const int count = m_store->importCsv(path, false);
    if (count < 0) {
        m_statusLabel->setText(QStringLiteral("导入失败"));
        return;
    }
    m_statusLabel->setText(
        QStringLiteral("已导入/更新 %1 个单词").arg(count));
    fillCurrentScope();
}
