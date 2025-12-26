#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QGroupBox>
#include <QSqlDatabase>
#include <QSqlTableModel>
#include <QSqlQuery>
#include <QDateTime>

struct HistoryRecord {
    int id;
    QDateTime startTime;
    QDateTime endTime;
    QString inputFile;
    int totalKeys;
    int validKeys;
    int invalidKeys;
    int errorKeys;
    double duration;
    double avgSpeed;
    QString apiEndpoint;
};

class HistoryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryWidget(QWidget *parent = nullptr);
    ~HistoryWidget();

    void addRecord(const HistoryRecord &record);
    void clearAllHistory();
    void showResumeDialog();

private slots:
    void onRefresh();
    void onDeleteSelected();
    void onExportSelected();
    void onExportAll();
    void onSearchTextChanged(const QString &text);
    void onSelectionChanged();
    void onViewDetails();

private:
    void setupUi();
    void connectSignals();
    void initDatabase();
    void loadHistory();
    void populateTable();
    void updateStatistics();

    QTableWidget *m_historyTable;
    QLineEdit *m_searchEdit;
    QLabel *m_totalRecordsLabel;
    QLabel *m_totalKeysLabel;
    QLabel *m_totalValidLabel;

    QPushButton *m_refreshButton;
    QPushButton *m_deleteButton;
    QPushButton *m_exportButton;
    QPushButton *m_exportAllButton;
    QPushButton *m_resumeButton;

    QSqlDatabase m_database;
    QVector<HistoryRecord> m_historyRecords;
};
