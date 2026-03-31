package core

import (
	"bufio"
	"fmt"
	"io"
	"strings"
)

func ParseKeys(r io.Reader) ([]string, error) {
	scanner := bufio.NewScanner(r)
	scanner.Buffer(make([]byte, 1024), 1024*1024)

	keys := make([]string, 0)
	seen := make(map[string]struct{})

	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		if _, ok := seen[line]; ok {
			continue
		}
		seen[line] = struct{}{}
		keys = append(keys, line)
	}

	if err := scanner.Err(); err != nil {
		return nil, fmt.Errorf("read keys: %w", err)
	}
	return keys, nil
}
