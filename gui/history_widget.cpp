#include "history_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QSqlError>
#include <QInputDialog>
#include <QSqlRecord>
#include <QDialog>
#include <QFormLayout>
#include <QLabel>

HistoryWidget::HistoryWidget(QWidget *parent)
    : QWidget(parent)
{
    initDatabase();
    setupUi();
    connectSignals();
    loadHistory();
}

HistoryWidget::~HistoryWidget()
{
    if (m_database.isOpen()) {
        m_database.close();
    }
}

void HistoryWidget::initDatabase()
{
    m_database = QSqlDatabase::addDatabase("QSQLITE", "history_connection");
    m_database.setDatabaseName("api_checker_history.db");

    if (!m_database.open()) {
        QMessageBox::critical(this, "数据库错误",
            "无法打开历史数据库: " + m_database.lastError().text());
        return;
    }

    QSqlQuery query(m_database);
    QString createTableSql = R"(
        CREATE TABLE IF NOT EXISTS history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            start_time TEXT NOT NULL,
            end_time TEXT NOT NULL,
            input_file TEXT,
            total_keys INTEGER NOT NULL,
            valid_keys INTEGER NOT NULL,
            invalid_keys INTEGER NOT NULL,
            error_keys INTEGER NOT NULL,
            duration REAL NOT NULL,
            avg_speed REAL NOT NULL,
            api_endpoint TEXT NOT NULL,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP
        )
    )";

    if (!query.exec(createTableSql)) {
        QMessageBox::critical(this, "数据库错误",
            "无法创建历史表: " + query.lastError().text());
    }
}

void HistoryWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    QGroupBox *statsGroup = new QGroupBox("统计信息", this);
    QHBoxLayout *statsLayout = new QHBoxLayout(statsGroup);

    m_totalRecordsLabel = new QLabel("总记录: 0", this);
    m_totalKeysLabel = new QLabel("总检测: 0", this);
    m_totalValidLabel = new QLabel("总有效: 0", this);

    statsLayout->addWidget(m_totalRecordsLabel);
    statsLayout->addWidget(m_totalKeysLabel);
    statsLayout->addWidget(m_totalValidLabel);
    statsLayout->addStretch();

    QGroupBox *searchGroup = new QGroupBox("搜索", this);
    QHBoxLayout *searchLayout = new QHBoxLayout(searchGroup);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("搜索历史记录...");

    searchLayout->addWidget(m_searchEdit);

    m_historyTable = new QTableWidget(this);
    m_historyTable->setColumnCount(9);
    m_historyTable->setHorizontalHeaderLabels({
        "ID", "开始时间", "结束时间", "输入文件",
        "总数", "有效", "无效", "错误", "API端点"
    });
    m_historyTable->horizontalHeader()->setStretchLastSection(true);
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_historyTable->setAlternatingRowColors(true);
    m_historyTable->setSortingEnabled(true);

    m_historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_historyTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Stretch);

    QGroupBox *actionGroup = new QGroupBox("操作", this);
    QHBoxLayout *actionLayout = new QHBoxLayout(actionGroup);

    m_refreshButton = new QPushButton("🔄 刷新", this);
    m_deleteButton = new QPushButton("🗑️ 删除选中", this);
    m_exportButton = new QPushButton("📥 导出选中", this);
    m_exportAllButton = new QPushButton("📥 导出全部", this);
    m_resumeButton = new QPushButton("▶️ 恢复检测", this);

    actionLayout->addWidget(m_refreshButton);
    actionLayout->addWidget(m_deleteButton);
    actionLayout->addWidget(m_exportButton);
    actionLayout->addWidget(m_exportAllButton);
    actionLayout->addWidget(m_resumeButton);
    actionLayout->addStretch();

    mainLayout->addWidget(statsGroup);
    mainLayout->addWidget(searchGroup);
    mainLayout->addWidget(m_historyTable);
    mainLayout->addWidget(actionGroup);
}

void HistoryWidget::connectSignals()
{
    connect(m_refreshButton, &QPushButton::clicked, this, &HistoryWidget::onRefresh);
    connect(m_deleteButton, &QPushButton::clicked, this, &HistoryWidget::onDeleteSelected);
    connect(m_exportButton, &QPushButton::clicked, this, &HistoryWidget::onExportSelected);
    connect(m_exportAllButton, &QPushButton::clicked, this, &HistoryWidget::onExportAll);
    connect(m_resumeButton, &QPushButton::clicked, this, &HistoryWidget::showResumeDialog);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &HistoryWidget::onSearchTextChanged);
    connect(m_historyTable, &QTableWidget::itemSelectionChanged,
            this, &HistoryWidget::onSelectionChanged);
}

void HistoryWidget::loadHistory()
{
    m_historyRecords.clear();

    QSqlQuery query(m_database);
    query.exec("SELECT * FROM history ORDER BY start_time DESC");

    while (query.next()) {
        HistoryRecord record;
        record.id = query.value("id").toInt();
        record.startTime = QDateTime::fromString(query.value("start_time").toString(), Qt::ISODate);
        record.endTime = QDateTime::fromString(query.value("end_time").toString(), Qt::ISODate);
        record.inputFile = query.value("input_file").toString();
        record.totalKeys = query.value("total_keys").toInt();
        record.validKeys = query.value("valid_keys").toInt();
        record.invalidKeys = query.value("invalid_keys").toInt();
        record.errorKeys = query.value("error_keys").toInt();
        record.duration = query.value("duration").toDouble();
        record.avgSpeed = query.value("avg_speed").toDouble();
        record.apiEndpoint = query.value("api_endpoint").toString();

        m_historyRecords.append(record);
    }

    populateTable();
    updateStatistics();
}

void HistoryWidget::populateTable()
{
    m_historyTable->setRowCount(m_historyRecords.size());

    for (int row = 0; row < m_historyRecords.size(); ++row) {
        const auto &record = m_historyRecords[row];

        m_historyTable->setItem(row, 0, new QTableWidgetItem(QString::number(record.id)));
        m_historyTable->setItem(row, 1, new QTableWidgetItem(
            record.startTime.toString("yyyy-MM-dd hh:mm:ss")));
        m_historyTable->setItem(row, 2, new QTableWidgetItem(
            record.endTime.toString("yyyy-MM-dd hh:mm:ss")));
        m_historyTable->setItem(row, 3, new QTableWidgetItem(record.inputFile));
        m_historyTable->setItem(row, 4, new QTableWidgetItem(QString::number(record.totalKeys)));
        m_historyTable->setItem(row, 5, new QTableWidgetItem(QString::number(record.validKeys)));
        m_historyTable->setItem(row, 6, new QTableWidgetItem(QString::number(record.invalidKeys)));
        m_historyTable->setItem(row, 7, new QTableWidgetItem(QString::number(record.errorKeys)));
        m_historyTable->setItem(row, 8, new QTableWidgetItem(record.apiEndpoint));

        for (int col = 0; col < 9; ++col) {
            if (auto item = m_historyTable->item(row, col)) {
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
        }
    }
}

void HistoryWidget::updateStatistics()
{
    m_totalRecordsLabel->setText(QString("总记录: %1").arg(m_historyRecords.size()));

    int totalKeys = 0, totalValid = 0;
    for (const auto &record : m_historyRecords) {
        totalKeys += record.totalKeys;
        totalValid += record.validKeys;
    }

    m_totalKeysLabel->setText(QString("总检测: %1").arg(totalKeys));
    m_totalValidLabel->setText(QString("总有效: %1").arg(totalValid));
}

void HistoryWidget::addRecord(const HistoryRecord &record)
{
    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT INTO history (start_time, end_time, input_file, total_keys,
                          valid_keys, invalid_keys, error_keys, duration,
                          avg_speed, api_endpoint)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    query.addBindValue(record.startTime.toString(Qt::ISODate));
    query.addBindValue(record.endTime.toString(Qt::ISODate));
    query.addBindValue(record.inputFile);
    query.addBindValue(record.totalKeys);
    query.addBindValue(record.validKeys);
    query.addBindValue(record.invalidKeys);
    query.addBindValue(record.errorKeys);
    query.addBindValue(record.duration);
    query.addBindValue(record.avgSpeed);
    query.addBindValue(record.apiEndpoint);

    if (!query.exec()) {
        QMessageBox::critical(this, "数据库错误",
            "无法保存历史记录: " + query.lastError().text());
        return;
    }

    loadHistory();
}

void HistoryWidget::clearAllHistory()
{
    auto reply = QMessageBox::question(this, "确认清除",
        "确定要清除所有历史记录吗？此操作不可恢复。",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QSqlQuery query(m_database);
        if (!query.exec("DELETE FROM history")) {
            QMessageBox::critical(this, "数据库错误",
                "无法清除历史记录: " + query.lastError().text());
            return;
        }

        loadHistory();
        QMessageBox::information(this, "清除完成", "所有历史记录已清除");
    }
}

void HistoryWidget::showResumeDialog()
{
    if (m_historyRecords.isEmpty()) {
        QMessageBox::information(this, "提示", "没有可恢复的历史记录");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("选择要恢复的检测");
    dialog.resize(600, 400);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QTableWidget *table = new QTableWidget(&dialog);
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels({"ID", "时间", "输入文件", "总数", "有效"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);

    table->setRowCount(m_historyRecords.size());
    for (int row = 0; row < m_historyRecords.size(); ++row) {
        const auto &record = m_historyRecords[row];
        table->setItem(row, 0, new QTableWidgetItem(QString::number(record.id)));
        table->setItem(row, 1, new QTableWidgetItem(
            record.startTime.toString("yyyy-MM-dd hh:mm:ss")));
        table->setItem(row, 2, new QTableWidgetItem(record.inputFile));
        table->setItem(row, 3, new QTableWidgetItem(QString::number(record.totalKeys)));
        table->setItem(row, 4, new QTableWidgetItem(QString::number(record.validKeys)));
    }

    layout->addWidget(table);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okButton = new QPushButton("确定", &dialog);
    QPushButton *cancelButton = new QPushButton("取消", &dialog);

    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    layout->addLayout(buttonLayout);

    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        int currentRow = table->currentRow();
        if (currentRow >= 0 && currentRow < m_historyRecords.size()) {
            const auto &record = m_historyRecords[currentRow];
            QMessageBox::information(this, "恢复功能",
                "恢复功能将在后续版本中实现\n\n"
                "记录ID: " + QString::number(record.id) + "\n"
                "输入文件: " + record.inputFile + "\n"
                "检测时间: " + record.startTime.toString());
        }
    }
}

void HistoryWidget::onRefresh()
{
    loadHistory();
}

void HistoryWidget::onDeleteSelected()
{
    int currentRow = m_historyTable->currentRow();
    if (currentRow < 0) {
        QMessageBox::warning(this, "提示", "请先选择要删除的记录");
        return;
    }

    auto reply = QMessageBox::question(this, "确认删除",
        "确定要删除选中的记录吗？",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        const auto &record = m_historyRecords[currentRow];

        QSqlQuery query(m_database);
        query.prepare("DELETE FROM history WHERE id = ?");
        query.addBindValue(record.id);

        if (!query.exec()) {
            QMessageBox::critical(this, "数据库错误",
                "无法删除记录: " + query.lastError().text());
            return;
        }

        loadHistory();
        QMessageBox::information(this, "删除完成", "记录已删除");
    }
}

void HistoryWidget::onExportSelected()
{
    int currentRow = m_historyTable->currentRow();
    if (currentRow < 0) {
        QMessageBox::warning(this, "提示", "请先选择要导出的记录");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, "导出历史记录",
        "history_record.txt", "文本文件 (*.txt);;所有文件 (*.*)");

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "导出失败", "无法打开文件进行写入");
        return;
    }

    const auto &record = m_historyRecords[currentRow];

    QTextStream out(&file);
    out << "=== API检测历史记录 ===\n\n";
    out << "记录ID: " << record.id << "\n";
    out << "开始时间: " << record.startTime.toString("yyyy-MM-dd hh:mm:ss") << "\n";
    out << "结束时间: " << record.endTime.toString("yyyy-MM-dd hh:mm:ss") << "\n";
    out << "输入文件: " << record.inputFile << "\n";
    out << "API端点: " << record.apiEndpoint << "\n";
    out << "总检测数: " << record.totalKeys << "\n";
    out << "有效数: " << record.validKeys << "\n";
    out << "无效数: " << record.invalidKeys << "\n";
    out << "错误数: " << record.errorKeys << "\n";
    out << "耗时: " << record.duration << " 秒\n";
    out << "平均速度: " << record.avgSpeed << " keys/秒\n";

    QMessageBox::information(this, "导出完成", "记录已导出");
}

void HistoryWidget::onExportAll()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出全部历史",
        "history_all.txt", "文本文件 (*.txt);;所有文件 (*.*)");

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "导出失败", "无法打开文件进行写入");
        return;
    }

    QTextStream out(&file);
    out << "=== API检测历史记录汇总 ===\n\n";
    out << "总记录数: " << m_historyRecords.size() << "\n\n";

    for (const auto &record : m_historyRecords) {
        out << "--- 记录 " << record.id << " ---\n";
        out << "时间: " << record.startTime.toString("yyyy-MM-dd hh:mm:ss") << "\n";
        out << "总数: " << record.totalKeys << " | 有效: " << record.validKeys
            << " | 无效: " << record.invalidKeys << " | 错误: " << record.errorKeys << "\n";
        out << "耗时: " << record.duration << "秒 | 速度: " << record.avgSpeed << " keys/秒\n\n";
    }

    QMessageBox::information(this, "导出完成",
        QString("已导出 %1 条记录").arg(m_historyRecords.size()));
}

void HistoryWidget::onSearchTextChanged(const QString &text)
{
    for (int row = 0; row < m_historyTable->rowCount(); ++row) {
        bool visible = false;

        for (int col = 0; col < m_historyTable->columnCount(); ++col) {
            if (auto item = m_historyTable->item(row, col)) {
                if (item->text().contains(text, Qt::CaseInsensitive)) {
                    visible = true;
                    break;
                }
            }
        }

        m_historyTable->setRowHidden(row, !visible);
    }
}

void HistoryWidget::onSelectionChanged()
{
    bool hasSelection = m_historyTable->currentRow() >= 0;
    m_deleteButton->setEnabled(hasSelection);
    m_exportButton->setEnabled(hasSelection);
}

void HistoryWidget::onViewDetails()
{
    int currentRow = m_historyTable->currentRow();
    if (currentRow >= 0 && currentRow < m_historyRecords.size()) {
        const auto &record = m_historyRecords[currentRow];

        QString details;
        details += QString("<b>记录ID:</b> %1<br>").arg(record.id);
        details += QString("<b>开始时间:</b> %1<br>").arg(
            record.startTime.toString("yyyy-MM-dd hh:mm:ss"));
        details += QString("<b>结束时间:</b> %1<br>").arg(
            record.endTime.toString("yyyy-MM-dd hh:mm:ss"));
        details += QString("<b>输入文件:</b> %1<br>").arg(record.inputFile);
        details += QString("<b>API端点:</b> %1<br>").arg(record.apiEndpoint);
        details += QString("<b>总检测数:</b> %1<br>").arg(record.totalKeys);
        details += QString("<b>有效数:</b> %1<br>").arg(record.validKeys);
        details += QString("<b>无效数:</b> %1<br>").arg(record.invalidKeys);
        details += QString("<b>错误数:</b> %1<br>").arg(record.errorKeys);
        details += QString("<b>耗时:</b> %1 秒<br>").arg(record.duration);
        details += QString("<b>平均速度:</b> %1 keys/秒").arg(record.avgSpeed);

        QMessageBox::information(this, "记录详情", details);
    }
}
