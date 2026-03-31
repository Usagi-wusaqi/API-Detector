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
		body: func() string {
			return options.Body
		},
		header: func(key string, request *http.Request) {
			request.URL.RawQuery = strings.ReplaceAll(request.URL.RawQuery, "{key}", key)
			request.URL.Path = strings.ReplaceAll(request.URL.Path, "{key}", key)
			request.Header.Set("Authorization", "Bearer "+key)
			for header, value := range headers {
				request.Header.Set(header, value)
			}
		},
	}, nil
}
