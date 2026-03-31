package providers

import (
	"context"
	"fmt"
	"net/http"
	"sort"
	"strings"

	"github.com/Usagi-wusaqi/API-Detector/internal/core"
)

type Metadata struct {
	Name   string
	Method string
	URL    string
	Notes  string
}

type BuildOptions struct {
	URL     string
	Method  string
	Headers map[string]string
}

type providerFactory func(BuildOptions) (core.Provider, error)

type catalogEntry struct {
	meta    Metadata
	factory providerFactory
}

var catalog = map[string]catalogEntry{
	"anthropic": {
		meta: Metadata{
			Name:   "anthropic",
			Method: http.MethodPost,
			URL:    "https://api.anthropic.com/v1/messages",
			Notes:  "Uses x-api-key and anthropic-version headers.",
		},
		factory: func(BuildOptions) (core.Provider, error) {
			return NewAnthropicProvider(), nil
		},
	},
	"gemini": {
		meta: Metadata{
			Name:   "gemini",
			Method: http.MethodGet,
			URL:    "https://generativelanguage.googleapis.com/v1beta/models",
			Notes:  "Uses x-goog-api-key header.",
		},
		factory: func(BuildOptions) (core.Provider, error) {
			return NewGeminiProvider(), nil
		},
	},
	"openai": {
		meta: Metadata{
			Name:   "openai",
			Method: http.MethodGet,
			URL:    "https://api.openai.com/v1/models",
			Notes:  "OpenAI Models API.",
		},
		factory: func(BuildOptions) (core.Provider, error) {
			return NewOpenAICompatibleProvider("openai", "https://api.openai.com/v1/models"), nil
		},
	},
	"groq": {
		meta: Metadata{
			Name:   "groq",
			Method: http.MethodGet,
			URL:    "https://api.groq.com/openai/v1/models",
			Notes:  "Groq OpenAI-compatible models endpoint.",
		},
		factory: func(BuildOptions) (core.Provider, error) {
			return NewOpenAICompatibleProvider("groq", "https://api.groq.com/openai/v1/models"), nil
		},
	},
	"mistral": {
		meta: Metadata{
			Name:   "mistral",
			Method: http.MethodGet,
			URL:    "https://api.mistral.ai/v1/models",
			Notes:  "Mistral list models endpoint.",
		},
		factory: func(BuildOptions) (core.Provider, error) {
			return NewOpenAICompatibleProvider("mistral", "https://api.mistral.ai/v1/models"), nil
		},
	},
	"deepseek": {
		meta: Metadata{
			Name:   "deepseek",
			Method: http.MethodGet,
			URL:    "https://api.deepseek.com/models",
			Notes:  "DeepSeek list models endpoint.",
		},
		factory: func(BuildOptions) (core.Provider, error) {
			return NewOpenAICompatibleProvider("deepseek", "https://api.deepseek.com/models"), nil
		},
	},
	"openrouter": {
		meta: Metadata{
			Name:   "openrouter",
			Method: http.MethodGet,
			URL:    "https://openrouter.ai/api/v1/models",
			Notes:  "OpenRouter models endpoint.",
		},
		factory: func(BuildOptions) (core.Provider, error) {
			return NewOpenAICompatibleProvider("openrouter", "https://openrouter.ai/api/v1/models"), nil
		},
	},
	"custom": {
		meta: Metadata{
			Name:   "custom",
			Method: http.MethodGet,
			URL:    "",
			Notes:  "Custom Bearer-authenticated endpoint.",
		},
		factory: func(options BuildOptions) (core.Provider, error) {
			return NewCustomBearerProvider(options)
		},
	},
}

func Builtins() []Metadata {
	out := make([]Metadata, 0, len(catalog))
	for _, entry := range catalog {
		out = append(out, entry.meta)
	}
	sort.Slice(out, func(i, j int) bool {
		return out[i].Name < out[j].Name
	})
	return out
}

func Resolve(name string, options BuildOptions) (core.Provider, error) {
	entry, ok := catalog[strings.ToLower(strings.TrimSpace(name))]
	if !ok {
		return nil, fmt.Errorf("unknown provider %q", name)
	}
	return entry.factory(options)
}

type baseProvider struct {
	name   string
	method string
	url    string
	header func(key string, request *http.Request)
	body   func() string
}

func (p baseProvider) Name() string {
	return p.name
}

func (p baseProvider) BuildRequest(ctx context.Context, key string) (*http.Request, error) {
	var bodyReader *strings.Reader
	if p.body != nil {
		bodyReader = strings.NewReader(p.body())
	} else {
		bodyReader = strings.NewReader("")
	}

	request, err := http.NewRequestWithContext(ctx, p.method, p.url, bodyReader)
	if err != nil {
		return nil, err
	}
	request.Header.Set("User-Agent", "apidetect/0.1")
	if p.header != nil {
		p.header(key, request)
	}
	return request, nil
}

func (p baseProvider) Classify(response *http.Response, _ []byte) core.Classification {
	return core.ClassifyHTTPStatus(response.StatusCode)
}
