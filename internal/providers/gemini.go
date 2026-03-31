package providers

import (
	"net/http"

	"github.com/Usagi-wusaqi/API-Detector/internal/core"
)

func NewGeminiProvider() core.Provider {
	return baseProvider{
		name:   "gemini",
		method: http.MethodGet,
		url:    "https://generativelanguage.googleapis.com/v1beta/models",
		header: func(key string, request *http.Request) {
			request.Header.Set("x-goog-api-key", key)
		},
	}
}
