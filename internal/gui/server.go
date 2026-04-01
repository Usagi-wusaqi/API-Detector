package gui

import (
	"context"
	"crypto/rand"
	"embed"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io/fs"
	"net"
	"net/http"
	"os/exec"
	"runtime"
	"strings"
	"sync"
	"syscall"
	"time"

	"github.com/Usagi-wusaqi/API-Detector/internal/configutil"
	"github.com/Usagi-wusaqi/API-Detector/internal/core"
	"github.com/Usagi-wusaqi/API-Detector/internal/providers"
)

//go:embed assets/*
var assets embed.FS

type Server struct {
	addr    string
	manager *jobManager
}

func NewServer(addr string) *Server {
	return &Server{
		addr:    addr,
		manager: newJobManager(),
	}
}

func (s *Server) Run(noOpen bool) error {
	mux := http.NewServeMux()
	mux.HandleFunc("/api/health", s.handleHealth)
	mux.HandleFunc("/api/providers", s.handleProviders)
	mux.HandleFunc("/api/jobs", s.handleJobs)
	mux.HandleFunc("/api/jobs/", s.handleJobRoutes)

	staticFS, err := fs.Sub(assets, "assets")
	if err != nil {
		return err
	}
	mux.Handle("/assets/", http.StripPrefix("/assets/", http.FileServer(http.FS(staticFS))))
	mux.HandleFunc("/", s.handleIndex)

	server := &http.Server{Handler: mux}
	listener, url, fallback, err := listenWithFallback(s.addr)
	if err != nil {
		return err
	}
	if fallback {
		fmt.Println("Requested port is busy, switched to", listener.Addr().String())
	}

	fmt.Println("GUI listening on", url)
	if !noOpen {
		go openBrowser(url)
	}

	return server.Serve(listener)
}

func (s *Server) handleHealth(w http.ResponseWriter, _ *http.Request) {
	writeJSON(w, http.StatusOK, map[string]string{"status": "ok"})
}

func (s *Server) handleIndex(w http.ResponseWriter, r *http.Request) {
	if r.URL.Path != "/" {
		http.NotFound(w, r)
		return
	}
	content, err := assets.ReadFile("assets/index.html")
	if err != nil {
		http.Error(w, "failed to load gui", http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	_, _ = w.Write(content)
}

func (s *Server) handleProviders(w http.ResponseWriter, _ *http.Request) {
	writeJSON(w, http.StatusOK, providers.Builtins())
}

func (s *Server) handleJobs(w http.ResponseWriter, r *http.Request) {
	if r.Method == http.MethodGet {
		writeJSON(w, http.StatusOK, s.manager.list())
		return
	}
	if r.Method != http.MethodPost {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}

	var payload startJobPayload
	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		http.Error(w, "invalid json payload", http.StatusBadRequest)
		return
	}

	id, err := s.manager.start(payload)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	writeJSON(w, http.StatusAccepted, map[string]string{"id": id})
}

func (s *Server) handleJobRoutes(w http.ResponseWriter, r *http.Request) {
	parts := strings.Split(strings.TrimPrefix(r.URL.Path, "/api/jobs/"), "/")
	if len(parts) == 0 || parts[0] == "" {
		http.NotFound(w, r)
		return
	}

	id := parts[0]
	if len(parts) == 1 && r.Method == http.MethodGet {
		job, err := s.manager.snapshot(id)
		if err != nil {
			http.Error(w, err.Error(), http.StatusNotFound)
			return
		}
		writeJSON(w, http.StatusOK, job)
		return
	}

	if len(parts) == 2 && parts[1] == "cancel" && r.Method == http.MethodPost {
		if err := s.manager.cancel(id); err != nil {
			http.Error(w, err.Error(), http.StatusNotFound)
			return
		}
		w.WriteHeader(http.StatusAccepted)
		return
	}

	if len(parts) == 2 && parts[1] == "events" && r.Method == http.MethodGet {
		if err := s.manager.stream(w, r, id); err != nil {
			http.Error(w, err.Error(), http.StatusNotFound)
		}
		return
	}

	if len(parts) == 2 && parts[1] == "results" && r.Method == http.MethodGet {
		status := r.URL.Query().Get("status")
		content, err := s.manager.export(id, status)
		if err != nil {
			http.Error(w, err.Error(), http.StatusBadRequest)
			return
		}
		filename := "results.txt"
		if status != "" {
			filename = status + "_keys.txt"
		}
		w.Header().Set("Content-Type", "text/plain; charset=utf-8")
		w.Header().Set("Content-Disposition", fmt.Sprintf(`attachment; filename="%s"`, filename))
		_, _ = w.Write(content)
		return
	}

	if len(parts) == 2 && parts[1] == "report" && r.Method == http.MethodGet {
		job, err := s.manager.snapshot(id)
		if err != nil {
			http.Error(w, err.Error(), http.StatusNotFound)
			return
		}
		w.Header().Set("Content-Type", "application/json")
		w.Header().Set("Content-Disposition", fmt.Sprintf(`attachment; filename="job_%s_report.json"`, id))
		_ = json.NewEncoder(w).Encode(job)
		return
	}

	http.NotFound(w, r)
}

type startJobPayload struct {
	Provider       string `json:"provider"`
	Keys           string `json:"keys"`
	Concurrency    int    `json:"concurrency"`
	Timeout        string `json:"timeout"`
	ProxyMode      string `json:"proxy_mode"`
	ProxyURL       string `json:"proxy_url"`
	CustomURL      string `json:"custom_url"`
	CustomMethod   string `json:"custom_method"`
	CustomAuthMode string `json:"custom_auth_mode"`
	CustomHeaders  string `json:"custom_headers"`
	CustomBody     string `json:"custom_body"`
}

type jobSnapshot struct {
	ID        string            `json:"id"`
	Status    string            `json:"status"`
	StartedAt string            `json:"started_at"`
	Summary   core.CheckSummary `json:"summary"`
	Results   []guiCheckResult  `json:"results"`
}

type jobEvent struct {
	Type    string            `json:"type"`
	Result  guiCheckResult    `json:"result,omitempty"`
	Summary core.CheckSummary `json:"summary"`
	Results []guiCheckResult  `json:"results,omitempty"`
}

type guiCheckResult struct {
	Index      int         `json:"index"`
	Key        string      `json:"key"`
	MaskedKey  string      `json:"masked_key"`
	Status     core.Status `json:"status"`
	Reason     core.Reason `json:"reason"`
	Message    string      `json:"message"`
	HTTPStatus int         `json:"http_status"`
	LatencyMs  int64       `json:"latency_ms"`
	CheckedAt  string      `json:"checked_at"`
}

type job struct {
	id        string
	status    string
	cancel    context.CancelFunc
	startedAt time.Time
	summary   core.CheckSummary
	results   []core.CheckResult
	subs      map[chan jobEvent]struct{}
}

type jobManager struct {
	mu   sync.RWMutex
	jobs map[string]*job
}

func newJobManager() *jobManager {
	return &jobManager{jobs: make(map[string]*job)}
}

func (m *jobManager) start(payload startJobPayload) (string, error) {
	headers, err := configutil.ParseHeaderBlock(payload.CustomHeaders)
	if err != nil {
		return "", err
	}

	timeout, err := time.ParseDuration(strings.TrimSpace(payload.Timeout))
	if err != nil {
		return "", err
	}

	provider, err := providers.Resolve(payload.Provider, providers.BuildOptions{
		URL:      payload.CustomURL,
		Method:   payload.CustomMethod,
		Headers:  headers,
		Body:     payload.CustomBody,
		AuthMode: payload.CustomAuthMode,
	})
	if err != nil {
		return "", err
	}

	keys, err := core.ParseKeys(strings.NewReader(payload.Keys))
	if err != nil {
		return "", err
	}
	if len(keys) == 0 {
		return "", errors.New("no keys provided")
	}

	if payload.Concurrency < 1 {
		payload.Concurrency = 100
	}

	id := newJobID()
	ctx, cancel := context.WithCancel(context.Background())
	job := &job{
		id:        id,
		status:    "running",
		cancel:    cancel,
		startedAt: time.Now(),
		subs:      make(map[chan jobEvent]struct{}),
	}

	m.mu.Lock()
	m.jobs[id] = job
	m.mu.Unlock()

	go func() {
		checker, err := core.NewChecker(payload.Concurrency, timeout, core.ProxyConfig{
			Mode: core.ProxyMode(payload.ProxyMode),
			URL:  payload.ProxyURL,
		})
		if err != nil {
			m.mu.Lock()
			job.status = "done"
			job.summary.Total = len(keys)
			job.summary.Checked = len(keys)
			for index, key := range keys {
				result := core.NewResult(index, key, core.Classification{
					Status:  core.StatusError,
					Reason:  core.ReasonUnknown,
					Message: err.Error(),
				}, 0, 0)
				job.results = append(job.results, result)
				job.summary.Error++
			}
			m.broadcast(job, jobEvent{Type: "complete", Summary: job.summary})
			m.mu.Unlock()
			return
		}
		summary, results, _ := checker.Run(ctx, core.CheckRequest{
			Keys:        keys,
			Concurrency: payload.Concurrency,
			Timeout:     timeout,
			Provider:    provider,
			Proxy: core.ProxyConfig{
				Mode: core.ProxyMode(payload.ProxyMode),
				URL:  payload.ProxyURL,
			},
		}, func(event core.CheckEvent) {
			m.mu.Lock()
			job.results = append(job.results, event.Result)
			job.summary = accumulateSummary(job.summary, event.Result, len(keys), job.startedAt)
			summaryCopy := job.summary
			m.broadcast(job, jobEvent{Type: "result", Result: toGUIResult(event.Result), Summary: summaryCopy})
			m.mu.Unlock()
		})

		m.mu.Lock()
		job.summary = summary
		job.results = results
		if job.status != "canceled" {
			job.status = "done"
		}
		m.broadcast(job, jobEvent{Type: "complete", Summary: summary})
		m.mu.Unlock()
	}()

	return id, nil
}

func (m *jobManager) snapshot(id string) (jobSnapshot, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()

	job, ok := m.jobs[id]
	if !ok {
		return jobSnapshot{}, errors.New("job not found")
	}

	return jobSnapshot{
		ID:        job.id,
		Status:    job.status,
		StartedAt: job.startedAt.Format(time.RFC3339),
		Summary:   job.summary,
		Results:   toGUIResults(job.results),
	}, nil
}

func (m *jobManager) list() []jobSnapshot {
	m.mu.RLock()
	defer m.mu.RUnlock()

	out := make([]jobSnapshot, 0, len(m.jobs))
	for _, job := range m.jobs {
		out = append(out, jobSnapshot{
			ID:        job.id,
			Status:    job.status,
			StartedAt: job.startedAt.Format(time.RFC3339),
			Summary:   job.summary,
			Results:   nil,
		})
	}
	return out
}

func (m *jobManager) cancel(id string) error {
	m.mu.Lock()
	defer m.mu.Unlock()

	job, ok := m.jobs[id]
	if !ok {
		return errors.New("job not found")
	}
	job.status = "canceled"
	job.cancel()
	return nil
}

func (m *jobManager) export(id string, status string) ([]byte, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()

	job, ok := m.jobs[id]
	if !ok {
		return nil, errors.New("job not found")
	}

	var filter core.Status
	if status != "" {
		filter = core.Status(status)
		switch filter {
		case core.StatusValid, core.StatusInvalid, core.StatusError, core.StatusCanceled:
		default:
			return nil, errors.New("unsupported status filter")
		}
	}

	var builder strings.Builder
	for _, result := range job.results {
		if status != "" && result.Status != filter {
			continue
		}
		builder.WriteString(result.Key)
		builder.WriteByte('\n')
	}

	return []byte(builder.String()), nil
}

func (m *jobManager) stream(w http.ResponseWriter, r *http.Request, id string) error {
	m.mu.Lock()
	job, ok := m.jobs[id]
	if !ok {
		m.mu.Unlock()
		return errors.New("job not found")
	}

	flusher, ok := w.(http.Flusher)
	if !ok {
		m.mu.Unlock()
		return errors.New("streaming unsupported")
	}

	w.Header().Set("Content-Type", "text/event-stream")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")

	ch := make(chan jobEvent, 64)
	job.subs[ch] = struct{}{}
	snapshot := jobSnapshot{
		ID:        job.id,
		Status:    job.status,
		StartedAt: job.startedAt.Format(time.RFC3339),
		Summary:   job.summary,
		Results:   toGUIResults(job.results),
	}
	m.mu.Unlock()

	defer func() {
		m.mu.Lock()
		delete(job.subs, ch)
		close(ch)
		m.mu.Unlock()
	}()

	writeSSE(w, "snapshot", snapshot)
	flusher.Flush()

	for {
		select {
		case <-r.Context().Done():
			return nil
		case event := <-ch:
			writeSSE(w, event.Type, event)
			flusher.Flush()
		}
	}
}

func (m *jobManager) broadcast(job *job, event jobEvent) {
	for ch := range job.subs {
		select {
		case ch <- event:
		default:
		}
	}
}

func accumulateSummary(summary core.CheckSummary, result core.CheckResult, total int, startedAt time.Time) core.CheckSummary {
	summary.Total = total
	summary.Checked++
	switch result.Status {
	case core.StatusValid:
		summary.Valid++
	case core.StatusInvalid:
		summary.Invalid++
	case core.StatusError:
		summary.Error++
	case core.StatusCanceled:
		summary.Canceled++
	}
	summary.DurationMs = time.Since(startedAt).Milliseconds()
	if summary.DurationMs > 0 {
		summary.KeysPerSec = float64(summary.Checked) / (float64(summary.DurationMs) / 1000)
	}
	return summary
}

func writeJSON(w http.ResponseWriter, status int, payload any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(payload)
}

func writeSSE(w http.ResponseWriter, event string, payload any) {
	data, _ := json.Marshal(payload)
	fmt.Fprintf(w, "event: %s\n", event)
	fmt.Fprintf(w, "data: %s\n\n", data)
}

func toGUIResults(results []core.CheckResult) []guiCheckResult {
	out := make([]guiCheckResult, 0, len(results))
	for _, result := range results {
		out = append(out, toGUIResult(result))
	}
	return out
}

func toGUIResult(result core.CheckResult) guiCheckResult {
	return guiCheckResult{
		Index:      result.Index,
		Key:        result.Key,
		MaskedKey:  result.MaskedKey,
		Status:     result.Status,
		Reason:     result.Reason,
		Message:    result.Message,
		HTTPStatus: result.HTTPStatus,
		LatencyMs:  result.LatencyMs,
		CheckedAt:  result.CheckedAt.Format(time.RFC3339),
	}
}

func newJobID() string {
	var bytes [16]byte
	_, _ = rand.Read(bytes[:])
	return hex.EncodeToString(bytes[:])
}

func openBrowser(url string) {
	var cmd *exec.Cmd
	switch runtime.GOOS {
	case "windows":
		cmd = exec.Command("rundll32", "url.dll,FileProtocolHandler", url)
	case "darwin":
		cmd = exec.Command("open", url)
	default:
		cmd = exec.Command("xdg-open", url)
	}
	_ = cmd.Start()
}

func listenWithFallback(addr string) (net.Listener, string, bool, error) {
	listener, err := net.Listen("tcp", addr)
	if err == nil {
		return listener, "http://" + listener.Addr().String(), false, nil
	}
	if !isAddrInUse(err) {
		return nil, "", false, err
	}

	host, _, splitErr := net.SplitHostPort(addr)
	if splitErr != nil {
		return nil, "", false, err
	}

	listener, fallbackErr := net.Listen("tcp", net.JoinHostPort(host, "0"))
	if fallbackErr != nil {
		return nil, "", false, fallbackErr
	}
	return listener, "http://" + listener.Addr().String(), true, nil
}

func isAddrInUse(err error) bool {
	if errors.Is(err, syscall.EADDRINUSE) {
		return true
	}

	message := strings.ToLower(err.Error())
	return strings.Contains(message, "address already in use") ||
		strings.Contains(message, "only one usage of each socket address")
}
