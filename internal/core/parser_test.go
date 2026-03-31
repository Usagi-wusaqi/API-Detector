package core

import (
	"strings"
	"testing"
)

func TestParseKeys(t *testing.T) {
	input := strings.NewReader(`
# comment
sk-first

 sk-second
sk-first
# another
sk-third
`)

	keys, err := ParseKeys(input)
	if err != nil {
		t.Fatalf("ParseKeys returned error: %v", err)
	}

	want := []string{"sk-first", "sk-second", "sk-third"}
	if len(keys) != len(want) {
		t.Fatalf("unexpected key count: got %d want %d", len(keys), len(want))
	}

	for i := range want {
		if keys[i] != want[i] {
			t.Fatalf("unexpected key at %d: got %q want %q", i, keys[i], want[i])
		}
	}
}
