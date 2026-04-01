package core

import (
	"context"
	"net/http"
	"time"
)

type Status string

const (
	StatusValid    Status = "valid"
	StatusInvalid  Status = "invalid"
	StatusError    Status = "error"
	StatusCanceled Status = "canceled"
)

type Reason string

const (
	ReasonOK           Reason = "ok"
	ReasonRateLimited  Reason = "rate_limited"
	ReasonUnauthorized Reason = "unauthorized"
	ReasonForbidden    Reason = "forbidden"
	ReasonTimeout      Reason = "timeout"
	ReasonNetworkError Reason = "network_error"
	ReasonEndpoint     Reason = "endpoint_error"
	ReasonServer       Reason = "server_error"
	ReasonCanceled     Reason = "canceled"
	ReasonUnknown      Reason = "unknown"
)

type ProxyMode string

const (
	ProxyModeEnv    ProxyMode = "env"
	ProxyModeDirect ProxyMode = "direct"
	ProxyModeCustom ProxyMode = "custom"
)

type ProxyConfig struct {
	Mode ProxyMode `json:"mode"`
	URL  string    `json:"url,omitempty"`
}

type Classification struct {
	Status  Status
	Reason  Reason
	Message string
}

type Provider interface {
	Name() string
	BuildRequest(ctx context.Context, key string) (*http.Request, error)
	Classify(response *http.Response, body []byte) Classification
}

type CheckRequest struct {
	Keys        []string
	Concurrency int
	Timeout     time.Duration
	Provider    Provider
	Proxy       ProxyConfig
}

type CheckEvent struct {
	Index  int
	Result CheckResult
}

type CheckResult struct {
	Index      int       `json:"index"`
	Key        string    `json:"-"`
	MaskedKey  string    `json:"masked_key"`
	Status     Status    `json:"status"`
	Reason     Reason    `json:"reason"`
	Message    string    `json:"message"`
	HTTPStatus int       `json:"http_status"`
	LatencyMs  int64     `json:"latency_ms"`
	CheckedAt  time.Time `json:"checked_at"`
}

type CheckSummary struct {
	Total      int     `json:"total"`
	Checked    int     `json:"checked"`
	Valid      int     `json:"valid"`
	Invalid    int     `json:"invalid"`
	Error      int     `json:"error"`
	Canceled   int     `json:"canceled"`
	DurationMs int64   `json:"duration_ms"`
	KeysPerSec float64 `json:"keys_per_second"`
}

func MaskKey(key string) string {
	if len(key) <= 8 {
		return "****"
	}
	return key[:4] + "..." + key[len(key)-4:]
}

func NewResult(index int, key string, classification Classification, httpStatus int, latency time.Duration) CheckResult {
	return CheckResult{
		Index:      index,
		Key:        key,
		MaskedKey:  MaskKey(key),
		Status:     classification.Status,
		Reason:     classification.Reason,
		Message:    classification.Message,
		HTTPStatus: httpStatus,
		LatencyMs:  latency.Milliseconds(),
		CheckedAt:  time.Now().UTC(),
	}
}

func ClassifyHTTPStatus(code int) Classification {
	switch {
	case code >= 200 && code < 300:
		return Classification{Status: StatusValid, Reason: ReasonOK, Message: "OK"}
	case code == http.StatusTooManyRequests:
		return Classification{Status: StatusValid, Reason: ReasonRateLimited, Message: "Rate limited"}
	case code == http.StatusUnauthorized:
		return Classification{Status: StatusInvalid, Reason: ReasonUnauthorized, Message: "Unauthorized"}
	case code == http.StatusForbidden:
		return Classification{Status: StatusInvalid, Reason: ReasonForbidden, Message: "Forbidden"}
	case code == http.StatusNotFound:
		return Classification{Status: StatusError, Reason: ReasonEndpoint, Message: "Endpoint not found"}
	case code >= 500 && code <= 599:
		return Classification{Status: StatusError, Reason: ReasonServer, Message: "Server error"}
	default:
		return Classification{Status: StatusError, Reason: ReasonUnknown, Message: http.StatusText(code)}
	}
}
