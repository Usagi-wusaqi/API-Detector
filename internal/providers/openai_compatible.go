package providers

import (
	"net/http"

	"github.com/Usagi-wusaqi/API-Detector/internal/core"
)

func NewOpenAICompatibleProvider(name string, url string) core.Provider {
	return baseProvider{
		name:   name,
		method: http.MethodGet,
		url:    url,
		header: func(key string, request *http.Request) {
			request.Header.Set("Authorization", "Bearer "+key)
		},
	}
}
