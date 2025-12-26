#include "result_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QClipboard>
#include <QApplication>

ResultWidget::ResultWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    connectSignals();
}

void ResultWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(5);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    QGroupBox *statsGroup = new QGroupBox("统计信息", this);
    QHBoxLayout *statsLayout = new QHBoxLayout(statsGroup);

    m_totalLabel = new QLabel("总计: 0", this);
    m_validLabel = new QLabel("有效: 0", this);
    m_validLabel->setStyleSheet("color: green; font-weight: bold;");
    m_invalidLabel = new QLabel("无效: 0", this);
    m_invalidLabel->setStyleSheet("color: red; font-weight: bold;");
    m_errorLabel = new QLabel("错误: 0", this);
    m_errorLabel->setStyleSheet("color: orange; font-weight: bold;");
    m_avgTimeLabel = new QLabel("平均响应: 0ms", this);
    m_speedLabel = new QLabel("速度: 0/s", this);

    statsLayout->addWidget(m_totalLabel);
    statsLayout->addWidget(m_validLabel);
    statsLayout->addWidget(m_invalidLabel);
    statsLayout->addWidget(m_errorLabel);
    statsLayout->addStretch();
    statsLayout->addWidget(m_avgTimeLabel);
    statsLayout->addWidget(m_speedLabel);

    QGroupBox *filterGroup = new QGroupBox("筛选和搜索", this);
    QHBoxLayout *filterLayout = new QHBoxLayout(filterGroup);

    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItems({"全部", "仅有效", "仅无效", "仅错误"});

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("搜索API...");

    filterLayout->addWidget(new QLabel("筛选:", this));
    filterLayout->addWidget(m_filterCombo);
    filterLayout->addSpacing(20);
    filterLayout->addWidget(new QLabel("搜索:", this));
    filterLayout->addWidget(m_searchEdit);

    m_resultTable = new QTableWidget(this);
    m_resultTable->setColumnCount(5);
    m_resultTable->setHorizontalHeaderLabels({"API Key", "状态", "消息", "响应时间", "检测时间"});
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resultTable->setAlternatingRowColors(true);
    m_resultTable->setSortingEnabled(true);

    m_resultTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_resultTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_resultTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    m_detailView = new QTextEdit(this);
    m_detailView->setReadOnly(true);
    m_detailView->setMaximumHeight(150);
    m_detailView->setPlaceholderText("选择一行查看详细信息");

    m_splitter = new QSplitter(Qt::Vertical, this);
    m_splitter->addWidget(m_resultTable);
    m_splitter->addWidget(m_detailView);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);

    QGroupBox *exportGroup = new QGroupBox("导出", this);
    QHBoxLayout *exportLayout = new QHBoxLayout(exportGroup);

    m_exportValidButton = new QPushButton("📥 导出有效", this);
    m_exportInvalidButton = new QPushButton("📥 导出无效", this);
    m_exportAllButton = new QPushButton("📥 导出全部", this);
    m_exportJsonButton = new QPushButton("📋 导出JSON", this);

    exportLayout->addWidget(m_exportValidButton);
    exportLayout->addWidget(m_exportInvalidButton);
    exportLayout->addWidget(m_exportAllButton);
    exportLayout->addWidget(m_exportJsonButton);
    exportLayout->addStretch();

    mainLayout->addWidget(statsGroup);
    mainLayout->addWidget(filterGroup);
    mainLayout->addWidget(m_splitter);
    mainLayout->addWidget(exportGroup);
}

void ResultWidget::connectSignals()
{
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ResultWidget::onFilterChanged);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &ResultWidget::onSearchTextChanged);
    connect(m_exportValidButton, &QPushButton::clicked,
            this, &ResultWidget::onExportValid);
    connect(m_exportInvalidButton, &QPushButton::clicked,
            this, &ResultWidget::onExportInvalid);
    connect(m_exportAllButton, &QPushButton::clicked,
            this, &ResultWidget::onExportAll);
    connect(m_exportJsonButton, &QPushButton::clicked,
            this, &ResultWidget::onExportJson);
    connect(m_resultTable, &QTableWidget::itemSelectionChanged,
            this, &ResultWidget::onSelectionChanged);
}

void ResultWidget::setResults(const QVector<ApiCheckResult> &results)
{
    m_allResults = results;
    applyFilter();
    updateStatistics();
}

void ResultWidget::refreshResults()
{
    applyFilter();
    updateStatistics();
}

void ResultWidget::clearResults()
{
    m_allResults.clear();
    m_filteredResults.clear();
    m_resultTable->setRowCount(0);
    m_detailView->clear();
    updateStatistics();
}

void ResultWidget::exportResults()
{
    onExportAll();
}

void ResultWidget::onFilterChanged(int index)
{
    applyFilter();
}

void ResultWidget::onSearchTextChanged(const QString &text)
{
    applyFilter();
}

void ResultWidget::applyFilter()
{
    m_filteredResults.clear();

    QString filter = m_filterCombo->currentText();
    QString searchText = m_searchEdit->text().toLower();

    for (const auto &result : m_allResults) {
        bool matchesFilter = true;

        if (filter == "仅有效" && !result.isValid()) {
            matchesFilter = false;
        } else if (filter == "仅无效" && !result.isInvalid()) {
            matchesFilter = false;
        } else if (filter == "仅错误" && !result.isError()) {
            matchesFilter = false;
        }

        if (matchesFilter && !searchText.isEmpty()) {
            if (!result.key.toLower().contains(searchText) &&
                !result.message.toLower().contains(searchText)) {
                matchesFilter = false;
            }
        }

        if (matchesFilter) {
            m_filteredResults.append(result);
        }
    }

    populateTable(m_filteredResults);
}

void ResultWidget::populateTable(const QVector<ApiCheckResult> &results)
{
    m_resultTable->setRowCount(results.size());

    for (int row = 0; row < results.size(); ++row) {
        const auto &result = results[row];

        QTableWidgetItem *keyItem = new QTableWidgetItem(result.key);
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);

        QTableWidgetItem *statusItem = new QTableWidgetItem(result.status);
        statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEditable);
        if (result.isValid()) {
            statusItem->setForeground(QBrush(Qt::green));
        } else if (result.isInvalid()) {
            statusItem->setForeground(QBrush(Qt::red));
        } else {
            statusItem->setForeground(QBrush(Qt::darkYellow));
        }

        QTableWidgetItem *messageItem = new QTableWidgetItem(result.message);
        messageItem->setFlags(messageItem->flags() & ~Qt::ItemIsEditable);

        QTableWidgetItem *timeItem = new QTableWidgetItem(QString("%1ms").arg(result.responseTime));
        timeItem->setFlags(timeItem->flags() & ~Qt::ItemIsEditable);

        QTableWidgetItem *checkedAtItem = new QTableWidgetItem(
            result.checkedAt.toString("yyyy-MM-dd hh:mm:ss"));
        checkedAtItem->setFlags(checkedAtItem->flags() & ~Qt::ItemIsEditable);

        m_resultTable->setItem(row, 0, keyItem);
        m_resultTable->setItem(row, 1, statusItem);
        m_resultTable->setItem(row, 2, messageItem);
        m_resultTable->setItem(row, 3, timeItem);
        m_resultTable->setItem(row, 4, checkedAtItem);
    }
}

void ResultWidget::updateStatistics()
{
    int total = m_allResults.size();
    int valid = 0, invalid = 0, error = 0;
    qint64 totalTime = 0;

    for (const auto &result : m_allResults) {
        if (result.isValid()) {
            valid++;
        } else if (result.isInvalid()) {
            invalid++;
        } else {
            error++;
        }
        totalTime += result.responseTime;
    }

    m_totalLabel->setText(QString("总计: %1").arg(total));
    m_validLabel->setText(QString("有效: %1 (%2%)")
        .arg(valid).arg(total > 0 ? (valid * 100.0 / total) : 0, 0, 'f', 1));
    m_invalidLabel->setText(QString("无效: %1 (%2%)")
        .arg(invalid).arg(total > 0 ? (invalid * 100.0 / total) : 0, 0, 'f', 1));
    m_errorLabel->setText(QString("错误: %1 (%2%)")
        .arg(error).arg(total > 0 ? (error * 100.0 / total) : 0, 0, 'f', 1));

    if (total > 0) {
        m_avgTimeLabel->setText(QString("平均响应: %1ms").arg(totalTime / total));
    } else {
        m_avgTimeLabel->setText("平均响应: 0ms");
    }

    m_speedLabel->setText(QString("显示: %1/%2").arg(m_filteredResults.size()).arg(total));
}

void ResultWidget::onSelectionChanged()
{
    int currentRow = m_resultTable->currentRow();
    if (currentRow >= 0 && currentRow < m_filteredResults.size()) {
        showResultDetails(m_filteredResults[currentRow]);
    } else {
        m_detailView->clear();
    }
}

void ResultWidget::showResultDetails(const ApiCheckResult &result)
{
    QString details;
    details += QString("<b>API Key:</b> %1<br>").arg(result.key);
    details += QString("<b>状态:</b> %1<br>").arg(result.status);
    details += QString("<b>消息:</b> %1<br>").arg(result.message);
    details += QString("<b>响应时间:</b> %1ms<br>").arg(result.responseTime);
    details += QString("<b>检测时间:</b> %1<br>").arg(
        result.checkedAt.toString("yyyy-MM-dd hh:mm:ss"));

    m_detailView->setHtml(details);
}

void ResultWidget::onExportValid()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出有效API",
        "valid_keys.txt", "文本文件 (*.txt);;所有文件 (*.*)");

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "导出失败", "无法打开文件进行写入");
        return;
    }

    QTextStream out(&file);
    int count = 0;
    for (const auto &result : m_allResults) {
        if (result.isValid()) {
            out << result.key << "\n";
            count++;
        }
    }

    QMessageBox::information(this, "导出完成",
        QString("已导出 %1 个有效API").arg(count));
}

void ResultWidget::onExportInvalid()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出无效API",
        "invalid_keys.txt", "文本文件 (*.txt);;所有文件 (*.*)");

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "导出失败", "无法打开文件进行写入");
        return;
    }

    QTextStream out(&file);
    int count = 0;
    for (const auto &result : m_allResults) {
        if (result.isInvalid()) {
            out << result.key << " # " << result.message << "\n";
            count++;
        }
    }

    QMessageBox::information(this, "导出完成",
        QString("已导出 %1 个无效API").arg(count));
}

void ResultWidget::onExportAll()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出全部结果",
        "all_results.txt", "文本文件 (*.txt);;所有文件 (*.*)");

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "导出失败", "无法打开文件进行写入");
        return;
    }

    QTextStream out(&file);
    for (const auto &result : m_allResults) {
        QString statusIcon = result.isValid() ? "✓" : (result.isInvalid() ? "✗" : "!");
        out << statusIcon << " " << result.key << " - " << result.message << "\n";
    }

    QMessageBox::information(this, "导出完成",
        QString("已导出 %1 条记录").arg(m_allResults.size()));
}

void ResultWidget::onExportJson()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出JSON",
        "results.json", "JSON文件 (*.json);;所有文件 (*.*)");

    if (fileName.isEmpty()) {
        return;
    }

    QJsonArray jsonArray;
    for (const auto &result : m_allResults) {
        QJsonObject obj;
        obj["key"] = result.key;
        obj["status"] = result.status;
        obj["message"] = result.message;
        obj["response_time_ms"] = result.responseTime;
        obj["checked_at"] = result.checkedAt.toString(Qt::ISODate);
        jsonArray.append(obj);
    }

    QJsonDocument doc(jsonArray);

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "导出失败", "无法打开文件进行写入");
        return;
    }

    file.write(doc.toJson());
    QMessageBox::information(this, "导出完成",
        QString("已导出 %1 条记录").arg(m_allResults.size()));
}
