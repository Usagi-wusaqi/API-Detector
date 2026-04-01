package gui

import (
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"

	"github.com/Usagi-wusaqi/API-Detector/internal/core"
)

const (
	appDataDirName   = "API-Detector"
	instanceFileName = "instance.json"
	jobsFileName     = "jobs.json"
	maxStoredJobs    = 30
)

type instanceInfo struct {
	URL       string `json:"url"`
	PID       int    `json:"pid"`
	UpdatedAt string `json:"updated_at"`
}

type persistentJob struct {
	ID        string            `json:"id"`
	Status    string            `json:"status"`
	StartedAt string            `json:"started_at"`
	Summary   core.CheckSummary `json:"summary"`
	Results   []guiCheckResult  `json:"results"`
}

func appDataDir() (string, error) {
	cacheDir, err := os.UserCacheDir()
	if err != nil {
		return "", err
	}
	dir := filepath.Join(cacheDir, appDataDirName)
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return "", err
	}
	return dir, nil
}

func instanceFilePath() (string, error) {
	dir, err := appDataDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(dir, instanceFileName), nil
}

func jobsFilePath() (string, error) {
	dir, err := appDataDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(dir, jobsFileName), nil
}

func TryReuseExistingInstance() (string, bool) {
	path, err := instanceFilePath()
	if err != nil {
		return "", false
	}

	content, err := os.ReadFile(path)
	if err != nil {
		return "", false
	}

	var info instanceInfo
	if err := json.Unmarshal(content, &info); err != nil || info.URL == "" {
		_ = os.Remove(path)
		return "", false
	}

	client := &http.Client{Timeout: 1200 * time.Millisecond}
	response, err := client.Get(info.URL + "/api/health")
	if err != nil {
		_ = os.Remove(path)
		return "", false
	}
	defer response.Body.Close()

	if response.StatusCode != http.StatusOK {
		_ = os.Remove(path)
		return "", false
	}
	return info.URL, true
}

func writeInstanceFile(url string, pid int) error {
	path, err := instanceFilePath()
	if err != nil {
		return err
	}

	payload := instanceInfo{
		URL:       url,
		PID:       pid,
		UpdatedAt: time.Now().UTC().Format(time.RFC3339),
	}

	return writeJSONFile(path, payload)
}

func clearInstanceFile(url string, pid int) {
	path, err := instanceFilePath()
	if err != nil {
		return
	}

	content, err := os.ReadFile(path)
	if err != nil {
		return
	}

	var info instanceInfo
	if err := json.Unmarshal(content, &info); err != nil {
		return
	}

	if info.URL == url && info.PID == pid {
		_ = os.Remove(path)
	}
}

func loadPersistentJobs() map[string]*job {
	path, err := jobsFilePath()
	if err != nil {
		return map[string]*job{}
	}

	content, err := os.ReadFile(path)
	if err != nil {
		return map[string]*job{}
	}

	var payload []persistentJob
	if err := json.Unmarshal(content, &payload); err != nil {
		return map[string]*job{}
	}

	jobs := make(map[string]*job, len(payload))
	for _, item := range payload {
		startedAt, err := time.Parse(time.RFC3339, item.StartedAt)
		if err != nil {
			startedAt = time.Now()
		}

		status := item.Status
		if status == "running" {
			status = "canceled"
		}

		jobs[item.ID] = &job{
			id:        item.ID,
			status:    status,
			startedAt: startedAt,
			summary:   item.Summary,
			results:   toCoreResults(item.Results),
			subs:      make(map[chan jobEvent]struct{}),
		}
	}

	return jobs
}

func savePersistentJobs(jobs map[string]*job) error {
	path, err := jobsFilePath()
	if err != nil {
		return err
	}

	payload := make([]persistentJob, 0, len(jobs))
	for _, item := range jobs {
		payload = append(payload, persistentJob{
			ID:        item.id,
			Status:    item.status,
			StartedAt: item.startedAt.Format(time.RFC3339),
			Summary:   item.summary,
			Results:   toGUIResults(item.results),
		})
	}

	sort.Slice(payload, func(i, j int) bool {
		return payload[i].StartedAt > payload[j].StartedAt
	})
	if len(payload) > maxStoredJobs {
		payload = payload[:maxStoredJobs]
	}

	return writeJSONFile(path, payload)
}

func writeJSONFile(path string, payload any) error {
	file, err := os.Create(path)
	if err != nil {
		return err
	}
	defer file.Close()

	encoder := json.NewEncoder(file)
	encoder.SetIndent("", "  ")
	return encoder.Encode(payload)
}

func toCoreResults(results []guiCheckResult) []core.CheckResult {
	out := make([]core.CheckResult, 0, len(results))
	for _, result := range results {
		checkedAt, err := time.Parse(time.RFC3339, result.CheckedAt)
		if err != nil {
			checkedAt = time.Now().UTC()
		}
		out = append(out, core.CheckResult{
			Index:      result.Index,
			Key:        result.Key,
			MaskedKey:  result.MaskedKey,
			Status:     result.Status,
			Reason:     result.Reason,
			Message:    result.Message,
			HTTPStatus: result.HTTPStatus,
			LatencyMs:  result.LatencyMs,
			CheckedAt:  checkedAt,
		})
	}
	return out
}

func normalizeURL(value string) string {
	return strings.TrimRight(value, "/")
}

func DescribeInstance(url string) string {
	return fmt.Sprintf("reusing running GUI at %s", normalizeURL(url))
}
