package providers

import (
	"net/http"

	"github.com/Usagi-wusaqi/API-Detector/internal/core"
)

const anthropicProbeBody = `{"model":"claude-sonnet-4-20250514","max_tokens":1,"messages":[{"role":"user","content":"ping"}]}`

func NewAnthropicProvider() core.Provider {
	return baseProvider{
		name:   "anthropic",
		method: http.MethodPost,
		url:    "https://api.anthropic.com/v1/messages",
		header: func(key string, request *http.Request) {
			request.Header.Set("x-api-key", key)
			request.Header.Set("anthropic-version", "2023-06-01")
			request.Header.Set("Content-Type", "application/json")
		},
		body: func() string {
			return anthropicProbeBody
		},
	}
}
