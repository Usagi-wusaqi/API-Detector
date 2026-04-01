package gui

import (
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"
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
