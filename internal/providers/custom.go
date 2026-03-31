package providers

import (
	"fmt"
	"net/http"
	"strings"

	"github.com/Usagi-wusaqi/API-Detector/internal/core"
)

func NewCustomBearerProvider(options BuildOptions) (core.Provider, error) {
	url := strings.TrimSpace(options.URL)
	if url == "" {
		return nil, fmt.Errorf("custom provider requires --url")
	}

	method := strings.ToUpper(strings.TrimSpace(options.Method))
	if method == "" {
		method = http.MethodGet
	}
	authMode := strings.ToLower(strings.TrimSpace(options.AuthMode))
	if authMode == "" {
		authMode = "bearer"
	}
	if authMode != "bearer" && authMode != "none" {
		return nil, fmt.Errorf("custom provider auth mode must be 'bearer' or 'none'")
	}

	headers := make(map[string]string, len(options.Headers))
	for key, value := range options.Headers {
		trimmedKey := strings.TrimSpace(key)
		if trimmedKey == "" {
			continue
		}
		headers[trimmedKey] = strings.TrimSpace(value)
	}

	return baseProvider{
		name:   "custom",
		method: method,
		url:    url,
		body: func(key string) string {
			return withKeyPlaceholders(options.Body, key)
		},
		header: func(key string, request *http.Request) {
			request.URL.RawQuery = withKeyPlaceholders(request.URL.RawQuery, key)
			request.URL.Path = withKeyPlaceholders(request.URL.Path, key)
			if authMode == "bearer" {
				request.Header.Set("Authorization", "Bearer "+key)
			}
			for header, value := range headers {
				request.Header.Set(header, withKeyPlaceholders(value, key))
			}
		},
	}, nil
}

func withKeyPlaceholders(value string, key string) string {
	return strings.ReplaceAll(value, "{key}", key)
}
