package main

import (
	"bytes"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/Usagi-wusaqi/API-Detector/internal/appmeta"
)

func TestRunVersion(t *testing.T) {
	output := captureStdout(t, func() {
		if err := run([]string{"version"}); err != nil {
			t.Fatalf("run returned error: %v", err)
		}
	})

	if strings.TrimSpace(output) != appmeta.Version {
		t.Fatalf("unexpected version output: got %q want %q", strings.TrimSpace(output), appmeta.Version)
	}
}

func TestHeaderFlagsSetRejectsInvalidHeader(t *testing.T) {
	var headers headerFlags
	if err := headers.Set("invalid-header"); err == nil {
		t.Fatal("expected invalid header to return error")
	}
}

func TestHeaderFlagsSetAcceptsValidHeader(t *testing.T) {
	var headers headerFlags
	if err := headers.Set("X-Test: 1"); err != nil {
		t.Fatalf("Set returned error: %v", err)
	}

	values := headers.AsMap()
	if got := values["X-Test"]; got != "1" {
		t.Fatalf("unexpected header value: got %q want %q", got, "1")
	}
}

func TestRunCheckFailOnInvalid(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusUnauthorized)
	}))
	defer server.Close()

	keysFile := filepath.Join(t.TempDir(), "keys.txt")
	if err := os.WriteFile(keysFile, []byte("sk-test\n"), 0o644); err != nil {
		t.Fatalf("os.WriteFile returned error: %v", err)
	}

	err := runWithCapturedStdout(t, []string{
		"check",
		"--provider", "custom",
		"--url", server.URL,
		"--input", keysFile,
		"--fail-on-invalid",
	})
	if err == nil {
		t.Fatal("expected fail-on-invalid to return an error")
	}
	if !strings.Contains(err.Error(), "invalid key") {
		t.Fatalf("unexpected error: %v", err)
	}
}

func TestRunCheckFailOnError(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusInternalServerError)
	}))
	defer server.Close()

	keysFile := filepath.Join(t.TempDir(), "keys.txt")
	if err := os.WriteFile(keysFile, []byte("sk-test\n"), 0o644); err != nil {
		t.Fatalf("os.WriteFile returned error: %v", err)
	}

	err := runWithCapturedStdout(t, []string{
		"check",
		"--provider", "custom",
		"--url", server.URL,
		"--input", keysFile,
		"--fail-on-error",
	})
	if err == nil {
		t.Fatal("expected fail-on-error to return an error")
	}
	if !strings.Contains(err.Error(), "error result") {
		t.Fatalf("unexpected error: %v", err)
	}
}

func runWithCapturedStdout(t *testing.T, args []string) error {
	t.Helper()

	var runErr error
	_ = captureStdout(t, func() {
		runErr = run(args)
	})
	return runErr
}

func captureStdout(t *testing.T, fn func()) string {
	t.Helper()

	oldStdout := os.Stdout
	reader, writer, err := os.Pipe()
	if err != nil {
		t.Fatalf("os.Pipe returned error: %v", err)
	}

	os.Stdout = writer
	defer func() {
		os.Stdout = oldStdout
	}()

	fn()

	if err := writer.Close(); err != nil {
		t.Fatalf("writer.Close returned error: %v", err)
	}

	var buffer bytes.Buffer
	if _, err := io.Copy(&buffer, reader); err != nil {
		t.Fatalf("io.Copy returned error: %v", err)
	}
	if err := reader.Close(); err != nil {
		t.Fatalf("reader.Close returned error: %v", err)
	}
	return buffer.String()
}
