package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
	"os/signal"
	"sort"
	"strings"
	"syscall"
	"time"

	"github.com/Usagi-wusaqi/API-Detector/internal/appmeta"
	"github.com/Usagi-wusaqi/API-Detector/internal/core"
	"github.com/Usagi-wusaqi/API-Detector/internal/output"
	"github.com/Usagi-wusaqi/API-Detector/internal/providers"
)

var errCanceled = errors.New("operation canceled")

func main() {
	if err := run(os.Args[1:]); err != nil {
		if errors.Is(err, flag.ErrHelp) {
			return
		}
		if errors.Is(err, errCanceled) {
			os.Exit(130)
		}

		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		os.Exit(1)
	}
}

func run(args []string) error {
	if len(args) == 0 {
		printUsage(os.Stdout)
		return nil
	}

	switch args[0] {
	case "check":
		return runCheck(args[1:])
	case "providers":
		return runProviders(args[1:])
	case "version", "-v", "--version":
		fmt.Fprintln(os.Stdout, appmeta.Version)
		return nil
	case "-h", "--help", "help":
		printUsage(os.Stdout)
		return nil
	default:
		printUsage(os.Stderr)
		return fmt.Errorf("unknown command %q", args[0])
	}
}

func runCheck(args []string) error {
	fs := flag.NewFlagSet("check", flag.ContinueOnError)
	fs.SetOutput(io.Discard)

	var (
		providerName string
		inputPath    string
		concurrency  int
		timeoutRaw   string
		format       string
		exportValid  string
		customURL    string
		customMethod string
		headers      headerFlags
	)

	fs.StringVar(&providerName, "provider", "openai", "provider name")
	fs.StringVar(&inputPath, "input", "", "path to input file, defaults to stdin")
	fs.IntVar(&concurrency, "concurrency", 100, "maximum concurrent checks")
	fs.StringVar(&timeoutRaw, "timeout", "10s", "per-request timeout")
	fs.StringVar(&format, "format", "text", "output format: text or json")
	fs.StringVar(&exportValid, "export-valid", "", "export valid raw keys to file")
	fs.StringVar(&customURL, "url", "", "custom endpoint URL")
	fs.StringVar(&customMethod, "method", "GET", "custom HTTP method")
	fs.Var(&headers, "header", "custom header in 'Name: Value' form; may be repeated")

	if err := fs.Parse(args); err != nil {
		printCheckUsage(os.Stderr)
		return err
	}

	timeout, err := time.ParseDuration(timeoutRaw)
	if err != nil {
		return fmt.Errorf("invalid timeout %q: %w", timeoutRaw, err)
	}
	if concurrency < 1 {
		return fmt.Errorf("concurrency must be >= 1")
	}

	reader, closeFn, err := openInput(inputPath)
	if err != nil {
		return err
	}
	defer closeFn()

	keys, err := core.ParseKeys(reader)
	if err != nil {
		return err
	}
	if len(keys) == 0 {
		return fmt.Errorf("no keys found in input")
	}

	provider, err := providers.Resolve(providerName, providers.BuildOptions{
		URL:     customURL,
		Method:  customMethod,
		Headers: headers.AsMap(),
	})
	if err != nil {
		return err
	}

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()

	checker := core.NewChecker(concurrency, timeout)
	request := core.CheckRequest{
		Keys:        keys,
		Concurrency: concurrency,
		Timeout:     timeout,
		Provider:    provider,
	}

	var results []core.CheckResult
	var summary core.CheckSummary
	onEvent := func(event core.CheckEvent) {
		results = append(results, event.Result)
		if strings.EqualFold(format, "text") {
			output.WriteTextEvent(os.Stdout, event.Index+1, len(keys), event.Result)
		}
	}

	summary, orderedResults, runErr := checker.Run(ctx, request, onEvent)
	results = orderedResults

	if exportValid != "" {
		if err := output.WriteValidKeys(exportValid, results); err != nil {
			return err
		}
	}

	switch strings.ToLower(format) {
	case "text":
		output.WriteTextSummary(os.Stdout, summary)
	case "json":
		if err := output.WriteJSONReport(os.Stdout, summary, results); err != nil {
			return err
		}
	default:
		return fmt.Errorf("unsupported format %q", format)
	}

	if errors.Is(runErr, context.Canceled) {
		return errCanceled
	}
	return runErr
}

func runProviders(args []string) error {
	if len(args) > 0 {
		return fmt.Errorf("providers does not accept arguments")
	}

	entries := providers.Builtins()
	sort.Slice(entries, func(i, j int) bool {
		return entries[i].Name < entries[j].Name
	})

	for _, entry := range entries {
		fmt.Fprintf(os.Stdout, "%-10s %-6s %s\n", entry.Name, entry.Method, entry.URL)
		if entry.Notes != "" {
			fmt.Fprintf(os.Stdout, "           %s\n", entry.Notes)
		}
	}
	return nil
}

func openInput(path string) (io.Reader, func(), error) {
	if path == "" {
		return os.Stdin, func() {}, nil
	}

	file, err := os.Open(path)
	if err != nil {
		return nil, nil, fmt.Errorf("open input file: %w", err)
	}

	return file, func() {
		_ = file.Close()
	}, nil
}

func printUsage(w io.Writer) {
	fmt.Fprintf(w, "%s %s\n", appmeta.Name, appmeta.Version)
	fmt.Fprintln(w)
	fmt.Fprintln(w, "Usage:")
	fmt.Fprintln(w, "  apidetect check [flags]")
	fmt.Fprintln(w, "  apidetect providers")
	fmt.Fprintln(w, "  apidetect version")
	fmt.Fprintln(w)
	printCheckUsage(w)
}

func printCheckUsage(w io.Writer) {
	fmt.Fprintln(w, "check flags:")
	fmt.Fprintln(w, "  --provider       Provider name (default: openai)")
	fmt.Fprintln(w, "  --input          Read keys from file; stdin when omitted")
	fmt.Fprintln(w, "  --concurrency    Maximum concurrent checks (default: 100)")
	fmt.Fprintln(w, "  --timeout        Per-request timeout (default: 10s)")
	fmt.Fprintln(w, "  --format         text or json (default: text)")
	fmt.Fprintln(w, "  --export-valid   Export valid raw keys to file")
	fmt.Fprintln(w, "  --url            Custom endpoint URL")
	fmt.Fprintln(w, "  --method         Custom HTTP method (default: GET)")
	fmt.Fprintln(w, "  --header         Custom header in 'Name: Value' form; repeatable")
}

type headerFlags []string

func (h *headerFlags) String() string {
	return strings.Join(*h, ", ")
}

func (h *headerFlags) Set(value string) error {
	if strings.TrimSpace(value) == "" {
		return fmt.Errorf("header cannot be empty")
	}
	*h = append(*h, value)
	return nil
}

func (h headerFlags) AsMap() map[string]string {
	out := make(map[string]string, len(h))
	for _, item := range h {
		parts := strings.SplitN(item, ":", 2)
		if len(parts) != 2 {
			continue
		}
		name := strings.TrimSpace(parts[0])
		value := strings.TrimSpace(parts[1])
		if name == "" {
			continue
		}
		out[name] = value
	}
	return out
}
