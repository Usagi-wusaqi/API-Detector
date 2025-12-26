#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QProgressBar>
#include <QSplitter>
#include <QTextEdit>
#include <QGroupBox>
#include "checker_thread.h"

class ResultWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ResultWidget(QWidget *parent = nullptr);

    void setResults(const QVector<ApiCheckResult> &results);
    void refreshResults();
    void exportResults();
    void clearResults();

private slots:
    void onFilterChanged(int index);
    void onExportValid();
    void onExportInvalid();
    void onExportAll();
    void onExportJson();
    void onSelectionChanged();
    void onSearchTextChanged(const QString &text);

private:
    void setupUi();
    void connectSignals();
    void updateStatistics();
    void populateTable(const QVector<ApiCheckResult> &results);
    void applyFilter();
    void showResultDetails(const ApiCheckResult &result);

    QTableWidget *m_resultTable;
    QLineEdit *m_searchEdit;
    QComboBox *m_filterCombo;
    QLabel *m_totalLabel;
    QLabel *m_validLabel;
    QLabel *m_invalidLabel;
    QLabel *m_errorLabel;
    QLabel *m_avgTimeLabel;
    QLabel *m_speedLabel;

    QPushButton *m_exportValidButton;
    QPushButton *m_exportInvalidButton;
    QPushButton *m_exportAllButton;
    QPushButton *m_exportJsonButton;

    QTextEdit *m_detailView;
    QSplitter *m_splitter;

    QVector<ApiCheckResult> m_allResults;
    QVector<ApiCheckResult> m_filteredResults;
};
