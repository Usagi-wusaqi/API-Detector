package core

import (
	"context"
	"errors"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/url"
	"strings"
	"sync"
	"time"
)

type Checker struct {
	client *http.Client
}

func NewChecker(concurrency int, timeout time.Duration, proxyConfig ProxyConfig) (*Checker, error) {
	proxyFunc, err := buildProxyFunc(proxyConfig)
	if err != nil {
		return nil, err
	}

	transport := &http.Transport{
		Proxy:               proxyFunc,
		DialContext:         (&net.Dialer{Timeout: timeout, KeepAlive: 30 * time.Second}).DialContext,
		MaxIdleConns:        concurrency * 2,
		MaxIdleConnsPerHost: concurrency,
		MaxConnsPerHost:     concurrency,
		IdleConnTimeout:     90 * time.Second,
		ForceAttemptHTTP2:   true,
	}

	return &Checker{
		client: &http.Client{
			Timeout:   timeout,
			Transport: transport,
		},
	}, nil
}

func (c *Checker) Run(ctx context.Context, request CheckRequest, onEvent func(CheckEvent)) (CheckSummary, []CheckResult, error) {
	if request.Provider == nil {
		return CheckSummary{}, nil, fmt.Errorf("provider is required")
	}
	if request.Concurrency < 1 {
		return CheckSummary{}, nil, fmt.Errorf("concurrency must be >= 1")
	}
	if request.Timeout <= 0 {
		return CheckSummary{}, nil, fmt.Errorf("timeout must be > 0")
	}

	start := time.Now()
	results := make([]CheckResult, len(request.Keys))
	filled := make([]bool, len(request.Keys))

	type job struct {
		index int
		key   string
	}
	type indexedResult struct {
		index  int
		result CheckResult
	}

	jobs := make(chan job)
	done := make(chan indexedResult)
	unsentCanceledCh := make(chan []indexedResult, 1)
	var wg sync.WaitGroup

	for i := 0; i < request.Concurrency; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for item := range jobs {
				if ctx.Err() != nil {
					done <- indexedResult{index: item.index, result: canceledResult(item.index, item.key)}
					continue
				}
				done <- indexedResult{index: item.index, result: c.checkOne(ctx, request.Provider, item.index, item.key)}
			}
		}()
	}

	go func() {
		for i, key := range request.Keys {
			select {
			case <-ctx.Done():
				unsentCanceled := make([]indexedResult, 0, len(request.Keys)-i)
				for j := i; j < len(request.Keys); j++ {
					unsentCanceled = append(unsentCanceled, indexedResult{
						index:  j,
						result: canceledResult(j, request.Keys[j]),
					})
				}
				close(jobs)
				unsentCanceledCh <- unsentCanceled
				return
			case jobs <- job{index: i, key: key}:
			}
		}
		close(jobs)
		unsentCanceledCh <- nil
	}()

	go func() {
		wg.Wait()
		close(done)
	}()

	var summary CheckSummary
	summary.Total = len(request.Keys)

	for item := range done {
		results[item.index] = item.result
		filled[item.index] = true
		summary = accumulate(summary, item.result)
		if onEvent != nil {
			onEvent(CheckEvent{Index: item.index, Result: item.result})
		}
	}

	unsentCanceled := <-unsentCanceledCh
	for _, item := range unsentCanceled {
		if filled[item.index] {
			continue
		}
		results[item.index] = item.result
		filled[item.index] = true
		summary = accumulate(summary, item.result)
		if onEvent != nil {
			onEvent(CheckEvent{Index: item.index, Result: item.result})
		}
	}

	ordered := make([]CheckResult, 0, len(results))
	for i, result := range results {
		if !filled[i] {
			continue
		}
		ordered = append(ordered, result)
	}

	duration := time.Since(start)
	summary.DurationMs = duration.Milliseconds()
	if duration > 0 {
		summary.KeysPerSec = float64(summary.Checked) / duration.Seconds()
	}

	if err := ctx.Err(); errors.Is(err, context.Canceled) {
		return summary, ordered, context.Canceled
	}
	return summary, ordered, nil
}

func (c *Checker) checkOne(ctx context.Context, provider Provider, index int, key string) CheckResult {
	start := time.Now()

	request, err := provider.BuildRequest(ctx, key)
	if err != nil {
		return NewResult(index, key, Classification{
			Status:  StatusError,
			Reason:  ReasonUnknown,
			Message: "Build request failed",
		}, 0, time.Since(start))
	}

	response, err := c.client.Do(request)
	if err != nil {
		switch {
		case errors.Is(err, context.Canceled):
			return canceledResult(index, key)
		case isTimeout(err):
			return NewResult(index, key, Classification{
				Status:  StatusError,
				Reason:  ReasonTimeout,
				Message: "Timeout",
			}, 0, time.Since(start))
		default:
			return NewResult(index, key, Classification{
				Status:  StatusError,
				Reason:  ReasonNetworkError,
				Message: "Network error",
			}, 0, time.Since(start))
		}
	}
	defer response.Body.Close()

	body, _ := io.ReadAll(io.LimitReader(response.Body, 4096))
	classification := provider.Classify(response, body)
	return NewResult(index, key, classification, response.StatusCode, time.Since(start))
}

func canceledResult(index int, key string) CheckResult {
	return NewResult(index, key, Classification{
		Status:  StatusCanceled,
		Reason:  ReasonCanceled,
		Message: "Canceled",
	}, 0, 0)
}

func accumulate(summary CheckSummary, result CheckResult) CheckSummary {
	summary.Checked++
	switch result.Status {
	case StatusValid:
		summary.Valid++
	case StatusInvalid:
		summary.Invalid++
	case StatusError:
		summary.Error++
	case StatusCanceled:
		summary.Canceled++
	}
	return summary
}

func isTimeout(err error) bool {
	var netErr net.Error
	return errors.As(err, &netErr) && netErr.Timeout()
}

func buildProxyFunc(proxyConfig ProxyConfig) (func(*http.Request) (*url.URL, error), error) {
	mode := proxyConfig.Mode
	if mode == "" {
		mode = ProxyModeEnv
	}

	switch mode {
	case ProxyModeEnv:
		return http.ProxyFromEnvironment, nil
	case ProxyModeDirect:
		return nil, nil
	case ProxyModeCustom:
		raw := strings.TrimSpace(proxyConfig.URL)
		if raw == "" {
			return nil, fmt.Errorf("proxy url is required when proxy mode is custom")
		}
		parsed, err := url.Parse(raw)
		if err != nil {
			return nil, fmt.Errorf("parse proxy url: %w", err)
		}
		if parsed.Scheme == "" || parsed.Host == "" {
			return nil, fmt.Errorf("proxy url must include scheme and host")
		}
		return http.ProxyURL(parsed), nil
	default:
		return nil, fmt.Errorf("unsupported proxy mode %q", mode)
	}
}
