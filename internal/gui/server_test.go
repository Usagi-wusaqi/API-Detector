package gui

import (
	"net"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/Usagi-wusaqi/API-Detector/internal/core"
)

func TestHandleProviders(t *testing.T) {
	server := NewServer("127.0.0.1:0")
	request := httptest.NewRequest(http.MethodGet, "/api/providers", nil)
	recorder := httptest.NewRecorder()

	server.handleProviders(recorder, request)

	if recorder.Code != http.StatusOK {
		t.Fatalf("unexpected status code: got %d want %d", recorder.Code, http.StatusOK)
	}
	if !strings.Contains(recorder.Body.String(), "\"openai\"") {
		t.Fatalf("expected providers response to include openai: %s", recorder.Body.String())
	}
}

func TestHandleHealth(t *testing.T) {
	server := NewServer("127.0.0.1:0")
	request := httptest.NewRequest(http.MethodGet, "/api/health", nil)
	recorder := httptest.NewRecorder()

	server.handleHealth(recorder, request)

	if recorder.Code != http.StatusOK {
		t.Fatalf("unexpected status code: got %d want %d", recorder.Code, http.StatusOK)
	}
	if !strings.Contains(recorder.Body.String(), `"status":"ok"`) {
		t.Fatalf("unexpected health body: %s", recorder.Body.String())
	}
}

func TestJobManagerStartAndSnapshot(t *testing.T) {
	target := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
	}))
	defer target.Close()

	manager := newJobManager()
	jobID, err := manager.start(startJobPayload{
		Provider:     "custom",
		Keys:         "sk-test\n",
		Concurrency:  1,
		Timeout:      "2s",
		ProxyMode:    string(core.ProxyModeDirect),
		CustomURL:    target.URL,
		CustomMethod: "GET",
	})
	if err != nil {
		t.Fatalf("start returned error: %v", err)
	}

	deadline := time.Now().Add(2 * time.Second)
	for {
		snapshot, err := manager.snapshot(jobID)
		if err != nil {
			t.Fatalf("snapshot returned error: %v", err)
		}
		if snapshot.Status == "done" {
			if snapshot.Summary.Valid != 1 {
				t.Fatalf("unexpected summary: %#v", snapshot.Summary)
			}
			if len(snapshot.Results) != 1 || snapshot.Results[0].Key != "sk-test" {
				t.Fatalf("unexpected results: %#v", snapshot.Results)
			}
			return
		}
		if time.Now().After(deadline) {
			t.Fatalf("job did not complete in time")
		}
		time.Sleep(20 * time.Millisecond)
	}
}

func TestJobManagerExportByStatus(t *testing.T) {
	manager := newJobManager()
	manager.jobs["job-1"] = &job{
		id:     "job-1",
		status: "done",
		results: []core.CheckResult{
			{Key: "sk-valid", Status: core.StatusValid},
			{Key: "sk-invalid", Status: core.StatusInvalid},
			{Key: "sk-error", Status: core.StatusError},
		},
	}

	content, err := manager.export("job-1", string(core.StatusInvalid))
	if err != nil {
		t.Fatalf("export returned error: %v", err)
	}
	if strings.TrimSpace(string(content)) != "sk-invalid" {
		t.Fatalf("unexpected export content: %q", string(content))
	}
}

func TestListenWithFallbackWhenPortOccupied(t *testing.T) {
	occupied, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("net.Listen returned error: %v", err)
	}
	defer occupied.Close()

	listener, url, fallback, err := listenWithFallback(occupied.Addr().String())
	if err != nil {
		t.Fatalf("listenWithFallback returned error: %v", err)
	}
	defer listener.Close()

	if !fallback {
		t.Fatal("expected fallback to be used")
	}
	if !strings.HasPrefix(url, "http://127.0.0.1:") {
		t.Fatalf("unexpected fallback url: %q", url)
	}
}

func TestJobManagerStartWithInvalidProxyAddsErrorResults(t *testing.T) {
	manager := newJobManager()
	jobID, err := manager.start(startJobPayload{
		Provider:  "openai",
		Keys:      "sk-test\n",
		Timeout:   "2s",
		ProxyMode: string(core.ProxyModeCustom),
		ProxyURL:  "bad-proxy",
	})
	if err != nil {
		t.Fatalf("start returned error: %v", err)
	}

	deadline := time.Now().Add(time.Second)
	for {
		snapshot, err := manager.snapshot(jobID)
		if err != nil {
			t.Fatalf("snapshot returned error: %v", err)
		}
		if snapshot.Status == "done" {
			if snapshot.Summary.Error != 1 {
				t.Fatalf("unexpected summary: %#v", snapshot.Summary)
			}
			return
		}
		if time.Now().After(deadline) {
			t.Fatal("job did not complete in time")
		}
		time.Sleep(20 * time.Millisecond)
	}
}
