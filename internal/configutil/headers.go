package configutil

import (
	"fmt"
	"strings"
)

func ParseHeaderEntries(entries []string) (map[string]string, error) {
	headers := make(map[string]string, len(entries))
	for _, entry := range entries {
		trimmed := strings.TrimSpace(entry)
		if trimmed == "" {
			continue
		}

		parts := strings.SplitN(trimmed, ":", 2)
		if len(parts) != 2 {
			return nil, fmt.Errorf("header must be in 'Name: Value' form")
		}

		name := strings.TrimSpace(parts[0])
		if name == "" {
			return nil, fmt.Errorf("header name cannot be empty")
		}

		headers[name] = strings.TrimSpace(parts[1])
	}

	return headers, nil
}

func ParseHeaderBlock(block string) (map[string]string, error) {
	return ParseHeaderEntries(strings.FieldsFunc(block, func(r rune) bool {
		return r == '\r' || r == '\n'
	}))
}
