package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/Usagi-wusaqi/API-Detector/internal/appmeta"
	"github.com/Usagi-wusaqi/API-Detector/internal/clierror"
)

func TestRunVersion(t *testing.T) {
	output := captureStdout(t, func() {
		if err := run([]string{"version"}); err != nil {
			t.Fatalf("run returned error: %v", err)
		}
	})

	if !strings.Contains(output, appmeta.Version) {
		t.Fatalf("expected version output to contain %q, got %q", appmeta.Version, output)
	}
	if !strings.Contains(output, "commit=") {
		t.Fatalf("expected version output to contain commit info: %q", output)
	}
	if !strings.Contains(output, "build_date=") {
		t.Fatalf("expected version output to contain build date info: %q", output)
	}
}

func TestRunProvidersJSON(t *testing.T) {
	output := captureStdout(t, func() {
		if err := run([]string{"providers", "--format", "json"}); err != nil {
			t.Fatalf("run returned error: %v", err)
		}
	})

	var payload []map[string]any
	if err := json.Unmarshal([]byte(output), &payload); err != nil {
		t.Fatalf("json.Unmarshal returned error: %v\noutput=%s", err, output)
	}
	if len(payload) == 0 {
		t.Fatal("expected providers in json output")
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
	var exitErr clierror.ExitError
	if !errors.As(err, &exitErr) {
		t.Fatalf("expected ExitError, got %T", err)
	}
	if exitErr.Code != 3 {
		t.Fatalf("unexpected exit code: got %d want 3", exitErr.Code)
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
	var exitErr clierror.ExitError
	if !errors.As(err, &exitErr) {
		t.Fatalf("expected ExitError, got %T", err)
	}
	if exitErr.Code != 4 {
		t.Fatalf("unexpected exit code: got %d want 4", exitErr.Code)
	}
}

func TestRunCheckWritesJSONReportToFile(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
	}))
	defer server.Close()

	tempDir := t.TempDir()
	keysFile := filepath.Join(tempDir, "keys.txt")
	outputFile := filepath.Join(tempDir, "report.json")

	if err := os.WriteFile(keysFile, []byte("sk-test\n"), 0o644); err != nil {
		t.Fatalf("os.WriteFile returned error: %v", err)
	}

	err := runWithCapturedStdout(t, []string{
		"check",
		"--provider", "custom",
		"--url", server.URL,
		"--input", keysFile,
		"--format", "json",
		"--output", outputFile,
	})
	if err != nil {
		t.Fatalf("run returned error: %v", err)
	}

	content, err := os.ReadFile(outputFile)
	if err != nil {
		t.Fatalf("os.ReadFile returned error: %v", err)
	}

	var payload map[string]any
	if err := json.Unmarshal(content, &payload); err != nil {
		t.Fatalf("json.Unmarshal returned error: %v", err)
	}
	if _, ok := payload["summary"]; !ok {
		t.Fatalf("expected summary in JSON output: %s", string(content))
	}
}

func TestRunCheckQuietSuppressesPerKeyOutput(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
	}))
	defer server.Close()

	keysFile := filepath.Join(t.TempDir(), "keys.txt")
	if err := os.WriteFile(keysFile, []byte("sk-test\n"), 0o644); err != nil {
		t.Fatalf("os.WriteFile returned error: %v", err)
	}

	output := captureStdout(t, func() {
		if err := run([]string{
			"check",
			"--provider", "custom",
			"--url", server.URL,
			"--input", keysFile,
			"--quiet",
		}); err != nil {
			t.Fatalf("run returned error: %v", err)
		}
	})

	if strings.Contains(output, "[1/1]") {
		t.Fatalf("expected quiet mode to suppress per-key output: %q", output)
	}
	if !strings.Contains(output, "Checked:") {
		t.Fatalf("expected summary output in quiet mode: %q", output)
	}
}

func TestRunCheckExportsInvalidAndErrorKeys(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		switch r.URL.Query().Get("key") {
		case "sk-invalid":
			w.WriteHeader(http.StatusUnauthorized)
		case "sk-error":
			w.WriteHeader(http.StatusInternalServerError)
		default:
			w.WriteHeader(http.StatusOK)
		}
	}))
	defer server.Close()

	tempDir := t.TempDir()
	keysFile := filepath.Join(tempDir, "keys.txt")
	invalidFile := filepath.Join(tempDir, "invalid.txt")
	errorFile := filepath.Join(tempDir, "error.txt")

	if err := os.WriteFile(keysFile, []byte("sk-valid\nsk-invalid\nsk-error\n"), 0o644); err != nil {
		t.Fatalf("os.WriteFile returned error: %v", err)
	}

	err := runWithCapturedStdout(t, []string{
		"check",
		"--provider", "custom",
		"--url", server.URL + "?key={key}",
		"--input", keysFile,
		"--export-invalid", invalidFile,
		"--export-error", errorFile,
	})
	if err != nil {
		t.Fatalf("run returned error: %v", err)
	}

	invalidContent, err := os.ReadFile(invalidFile)
	if err != nil {
		t.Fatalf("os.ReadFile invalid file returned error: %v", err)
	}
	if strings.TrimSpace(string(invalidContent)) != "sk-invalid" {
		t.Fatalf("unexpected invalid export: %q", string(invalidContent))
	}

	errorContent, err := os.ReadFile(errorFile)
	if err != nil {
		t.Fatalf("os.ReadFile error file returned error: %v", err)
	}
	if strings.TrimSpace(string(errorContent)) != "sk-error" {
		t.Fatalf("unexpected error export: %q", string(errorContent))
	}
}

func TestRunCheckCustomBodyFile(t *testing.T) {
	var requestBody string
	var authorization string
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body, err := io.ReadAll(r.Body)
		if err != nil {
			t.Fatalf("io.ReadAll returned error: %v", err)
		}
		requestBody = string(body)
		authorization = r.Header.Get("Authorization")
		w.WriteHeader(http.StatusOK)
	}))
	defer server.Close()

	tempDir := t.TempDir()
	keysFile := filepath.Join(tempDir, "keys.txt")
	bodyFile := filepath.Join(tempDir, "body.json")

	if err := os.WriteFile(keysFile, []byte("sk-test\n"), 0o644); err != nil {
		t.Fatalf("os.WriteFile keys returned error: %v", err)
	}
	if err := os.WriteFile(bodyFile, []byte(`{"ping":true}`), 0o644); err != nil {
		t.Fatalf("os.WriteFile body returned error: %v", err)
	}

	err := runWithCapturedStdout(t, []string{
		"check",
		"--provider", "custom",
		"--url", server.URL,
		"--method", "POST",
		"--body-file", bodyFile,
		"--input", keysFile,
	})
	if err != nil {
		t.Fatalf("run returned error: %v", err)
	}

	if requestBody != `{"ping":true}` {
		t.Fatalf("unexpected request body: %q", requestBody)
	}
	if authorization != "Bearer sk-test" {
		t.Fatalf("unexpected authorization header: %q", authorization)
	}
}

func TestRunCheckCustomNoBearerWithHeaderPlaceholder(t *testing.T) {
	var authHeader string
	var requestBody string
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body, err := io.ReadAll(r.Body)
		if err != nil {
			t.Fatalf("io.ReadAll returned error: %v", err)
		}
		authHeader = r.Header.Get("x-api-key")
		requestBody = string(body)
		if got := r.Header.Get("Authorization"); got != "" {
			t.Fatalf("expected Authorization header to be empty, got %q", got)
		}
		w.WriteHeader(http.StatusOK)
	}))
	defer server.Close()

	keysFile := filepath.Join(t.TempDir(), "keys.txt")
	if err := os.WriteFile(keysFile, []byte("sk-test\n"), 0o644); err != nil {
		t.Fatalf("os.WriteFile keys returned error: %v", err)
	}

	err := runWithCapturedStdout(t, []string{
		"check",
		"--provider", "custom",
		"--url", server.URL,
		"--method", "POST",
		"--auth-mode", "none",
		"--header", "x-api-key: {key}",
		"--body", `{"key":"{key}"}`,
		"--input", keysFile,
	})
	if err != nil {
		t.Fatalf("run returned error: %v", err)
	}

	if authHeader != "sk-test" {
		t.Fatalf("unexpected x-api-key header: %q", authHeader)
	}
	if requestBody != `{"key":"sk-test"}` {
		t.Fatalf("unexpected request body: %q", requestBody)
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
