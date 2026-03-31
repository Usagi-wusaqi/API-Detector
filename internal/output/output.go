package output

import (
	"encoding/json"
	"fmt"
	"io"
	"os"

	"github.com/Usagi-wusaqi/API-Detector/internal/core"
)

type JSONReport struct {
	Summary CheckSummaryView `json:"summary"`
	Results []ResultView     `json:"results"`
}

type CheckSummaryView struct {
	Total      int     `json:"total"`
	Checked    int     `json:"checked"`
	Valid      int     `json:"valid"`
	Invalid    int     `json:"invalid"`
	Error      int     `json:"error"`
	Canceled   int     `json:"canceled"`
	DurationMs int64   `json:"duration_ms"`
	KeysPerSec float64 `json:"keys_per_second"`
}

type ResultView struct {
	Index      int         `json:"index"`
	MaskedKey  string      `json:"masked_key"`
	Status     core.Status `json:"status"`
	Reason     core.Reason `json:"reason"`
	Message    string      `json:"message"`
	HTTPStatus int         `json:"http_status"`
	LatencyMs  int64       `json:"latency_ms"`
	CheckedAt  string      `json:"checked_at"`
}

func WriteTextEvent(w io.Writer, current int, total int, result core.CheckResult) {
	fmt.Fprintf(
		w,
		"[%d/%d] %-8s %-15s code=%d latency=%dms reason=%s\n",
		current,
		total,
		result.Status,
		result.MaskedKey,
		result.HTTPStatus,
		result.LatencyMs,
		result.Reason,
	)
}

func WriteTextSummary(w io.Writer, summary core.CheckSummary) {
	fmt.Fprintln(w)
	fmt.Fprintf(w, "Checked:   %d/%d\n", summary.Checked, summary.Total)
	fmt.Fprintf(w, "Valid:     %d\n", summary.Valid)
	fmt.Fprintf(w, "Invalid:   %d\n", summary.Invalid)
	fmt.Fprintf(w, "Error:     %d\n", summary.Error)
	fmt.Fprintf(w, "Canceled:  %d\n", summary.Canceled)
	fmt.Fprintf(w, "Duration:  %dms\n", summary.DurationMs)
	fmt.Fprintf(w, "Rate:      %.2f keys/s\n", summary.KeysPerSec)
}

func WriteJSONReport(w io.Writer, summary core.CheckSummary, results []core.CheckResult) error {
	report := JSONReport{
		Summary: CheckSummaryView{
			Total:      summary.Total,
			Checked:    summary.Checked,
			Valid:      summary.Valid,
			Invalid:    summary.Invalid,
			Error:      summary.Error,
			Canceled:   summary.Canceled,
			DurationMs: summary.DurationMs,
			KeysPerSec: summary.KeysPerSec,
		},
		Results: make([]ResultView, 0, len(results)),
	}

	for _, result := range results {
		report.Results = append(report.Results, ResultView{
			Index:      result.Index,
			MaskedKey:  result.MaskedKey,
			Status:     result.Status,
			Reason:     result.Reason,
			Message:    result.Message,
			HTTPStatus: result.HTTPStatus,
			LatencyMs:  result.LatencyMs,
			CheckedAt:  result.CheckedAt.Format(timeLayout),
		})
	}

	encoder := json.NewEncoder(w)
	encoder.SetIndent("", "  ")
	return encoder.Encode(report)
}

func WriteValidKeys(path string, results []core.CheckResult) error {
	file, err := os.Create(path)
	if err != nil {
		return fmt.Errorf("create export file: %w", err)
	}
	defer file.Close()

	for _, result := range results {
		if result.Status != core.StatusValid {
			continue
		}
		if _, err := fmt.Fprintln(file, result.Key); err != nil {
			return fmt.Errorf("write export file: %w", err)
		}
	}
	return nil
}

const timeLayout = "2006-01-02T15:04:05.000Z07:00"
