package core

import (
	"context"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"
)

type testProvider struct {
	url string
}

func (p testProvider) Name() string {
	return "test"
}

func (p testProvider) BuildRequest(ctx context.Context, key string) (*http.Request, error) {
	request, err := http.NewRequestWithContext(ctx, http.MethodGet, p.url+"?key="+key, nil)
	if err != nil {
		return nil, err
	}
	return request, nil
}

func (p testProvider) Classify(response *http.Response, _ []byte) Classification {
	return ClassifyHTTPStatus(response.StatusCode)
}

func TestCheckerRunMaintainsInputOrder(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		switch r.URL.Query().Get("key") {
		case "sk-valid":
			w.WriteHeader(http.StatusOK)
		case "sk-invalid":
			w.WriteHeader(http.StatusUnauthorized)
		default:
			w.WriteHeader(http.StatusTooManyRequests)
		}
	}))
	defer server.Close()

	checker := NewChecker(4, 2*time.Second)
	summary, results, err := checker.Run(context.Background(), CheckRequest{
		Keys:        []string{"sk-valid", "sk-invalid", "sk-rate"},
		Concurrency: 4,
		Timeout:     2 * time.Second,
		Provider:    testProvider{url: server.URL},
	}, nil)
	if err != nil {
		t.Fatalf("Run returned error: %v", err)
	}

	if len(results) != 3 {
		t.Fatalf("unexpected result count: got %d want 3", len(results))
	}

	if results[0].Key != "sk-valid" || results[0].Status != StatusValid {
		t.Fatalf("unexpected first result: %#v", results[0])
	}
	if results[1].Key != "sk-invalid" || results[1].Status != StatusInvalid {
		t.Fatalf("unexpected second result: %#v", results[1])
	}
	if results[2].Key != "sk-rate" || results[2].Reason != ReasonRateLimited {
		t.Fatalf("unexpected third result: %#v", results[2])
	}

	if summary.Valid != 2 || summary.Invalid != 1 || summary.Error != 0 {
		t.Fatalf("unexpected summary: %#v", summary)
	}
}

func TestCheckerRunCancellationMarksRemainingKeys(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		time.Sleep(100 * time.Millisecond)
		w.WriteHeader(http.StatusOK)
	}))
	defer server.Close()

	ctx, cancel := context.WithCancel(context.Background())
	cancel()

	checker := NewChecker(2, 500*time.Millisecond)
	summary, results, err := checker.Run(ctx, CheckRequest{
		Keys:        []string{"a", "b", "c"},
		Concurrency: 2,
		Timeout:     500 * time.Millisecond,
		Provider:    testProvider{url: server.URL},
	}, nil)
	if err == nil {
		t.Fatalf("expected cancellation error")
	}

	if len(results) != 3 {
		t.Fatalf("unexpected result count: got %d want 3", len(results))
	}

	for _, result := range results {
		if result.Status != StatusCanceled {
			t.Fatalf("expected canceled result, got %#v", result)
		}
	}

	if summary.Canceled != 3 {
		t.Fatalf("unexpected canceled count: %#v", summary)
	}
}
