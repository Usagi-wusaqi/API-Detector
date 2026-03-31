package main

import (
	"bytes"
	"io"
	"os"
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
