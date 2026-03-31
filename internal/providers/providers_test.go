package providers

import (
	"context"
	"net/http"
	"testing"
)

func TestResolveOpenAICompatibleProvider(t *testing.T) {
	provider, err := Resolve("openai", BuildOptions{})
	if err != nil {
		t.Fatalf("Resolve returned error: %v", err)
	}

	request, err := provider.BuildRequest(context.Background(), "sk-test")
	if err != nil {
		t.Fatalf("BuildRequest returned error: %v", err)
	}

	if request.Method != http.MethodGet {
		t.Fatalf("unexpected method: got %s want GET", request.Method)
	}
	if got := request.Header.Get("Authorization"); got != "Bearer sk-test" {
		t.Fatalf("unexpected authorization header: %q", got)
	}
}

func TestResolveAnthropicProvider(t *testing.T) {
	provider, err := Resolve("anthropic", BuildOptions{})
	if err != nil {
		t.Fatalf("Resolve returned error: %v", err)
	}

	request, err := provider.BuildRequest(context.Background(), "test-key")
	if err != nil {
		t.Fatalf("BuildRequest returned error: %v", err)
	}

	if request.Method != http.MethodPost {
		t.Fatalf("unexpected method: got %s want POST", request.Method)
	}
	if got := request.Header.Get("x-api-key"); got != "test-key" {
		t.Fatalf("unexpected x-api-key header: %q", got)
	}
	if got := request.Header.Get("anthropic-version"); got != "2023-06-01" {
		t.Fatalf("unexpected anthropic-version header: %q", got)
	}
}

func TestResolveCustomProviderRequiresURL(t *testing.T) {
	if _, err := Resolve("custom", BuildOptions{}); err == nil {
		t.Fatal("expected Resolve to require custom URL")
	}
}
