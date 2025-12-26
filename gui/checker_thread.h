#pragma once

#include <QThread>
#include <QStringList>
#include <QVector>
#include <QMutex>
#include <QAtomicInt>
#include <QDateTime>
#include "api_checker.h"

struct ApiCheckResult {
    QString key;
    QString status;
    QString message;
    qint64 responseTime;
    QDateTime checkedAt;

    bool isValid() const { return status == "valid"; }
    bool isInvalid() const { return status == "invalid"; }
    bool isError() const { return status == "error"; }
};

class CheckerThread : public QThread
{
    Q_OBJECT

public:
    explicit CheckerThread(QObject *parent = nullptr);
    ~CheckerThread();

    void setApiKeys(const QStringList &keys);
    void setEndpoint(const QString &endpoint);
    void setMethod(const QString &method);
    void setHeaders(const QString &headers);
    void setRequestBody(const QString &body);
    void setConcurrent(int concurrent);
    void setTimeout(int timeout);

    void stop();

    QVector<ApiCheckResult> getResults() const;

signals:
    void progress(int current, int valid, int invalid, int error);
    void finished();
    void error(const QString &errorMessage);

protected:
    void run() override;

private:
    QStringList m_apiKeys;
    QString m_endpoint;
    QString m_method;
    QString m_headers;
    QString m_requestBody;
    int m_concurrent;
    int m_timeout;

    QAtomicInt m_shouldStop;
    QVector<ApiCheckResult> m_results;
    QMutex m_resultsMutex;

    void checkApiKeys();
    ApiCheckResult checkSingleKey(const QString &key);
    QStringList parseHeaders(const QString &headersStr) const;
};
