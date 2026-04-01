package main

import (
	"context"
	"encoding/json"
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
	"github.com/Usagi-wusaqi/API-Detector/internal/clierror"
	"github.com/Usagi-wusaqi/API-Detector/internal/configutil"
	"github.com/Usagi-wusaqi/API-Detector/internal/core"
	"github.com/Usagi-wusaqi/API-Detector/internal/gui"
	"github.com/Usagi-wusaqi/API-Detector/internal/output"
	"github.com/Usagi-wusaqi/API-Detector/internal/providers"
)

func main() {
	if err := run(os.Args[1:]); err != nil {
		if errors.Is(err, flag.ErrHelp) {
			return
		}

		var exitErr clierror.ExitError
		if errors.As(err, &exitErr) {
			if exitErr.Err != nil {
				fmt.Fprintf(os.Stderr, "error: %v\n", exitErr.Err)
			}
			os.Exit(exitErr.Code)
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
	case "gui":
		return runGUI(args[1:])
	case "providers":
		return runProviders(args[1:])
	case "version", "-v", "--version":
		fmt.Fprintf(os.Stdout, "%s\ncommit=%s\nbuild_date=%s\n", appmeta.Version, appmeta.Commit, appmeta.BuildDate)
		return nil
	case "-h", "--help", "help":
		printUsage(os.Stdout)
		return nil
	default:
		printUsage(os.Stderr)
		return clierror.ExitError{
			Code: 2,
			Err:  fmt.Errorf("unknown command %q", args[0]),
		}
	}
}

func runCheck(args []string) error {
	fs := flag.NewFlagSet("check", flag.ContinueOnError)
	fs.SetOutput(io.Discard)

	var (
		providerName   string
		inputPath      string
		outputPath     string
		concurrency    int
		timeoutRaw     string
		format         string
		exportValid    string
		exportInvalid  string
		exportError    string
		customURL      string
		customMethod   string
		customBody     string
		customBodyFile string
		customAuthMode string
		failInvalid    bool
		failError      bool
		quiet          bool
		headers        headerFlags
	)

	fs.StringVar(&providerName, "provider", "openai", "provider name")
	fs.StringVar(&inputPath, "input", "", "path to input file, defaults to stdin")
	fs.StringVar(&outputPath, "output", "", "write the main report to file instead of stdout")
	fs.IntVar(&concurrency, "concurrency", 100, "maximum concurrent checks")
	fs.StringVar(&timeoutRaw, "timeout", "10s", "per-request timeout")
	fs.StringVar(&format, "format", "text", "output format: text or json")
	fs.StringVar(&exportValid, "export-valid", "", "export valid raw keys to file")
	fs.StringVar(&exportInvalid, "export-invalid", "", "export invalid raw keys to file")
	fs.StringVar(&exportError, "export-error", "", "export error raw keys to file")
	fs.StringVar(&customURL, "url", "", "custom endpoint URL")
	fs.StringVar(&customMethod, "method", "GET", "custom HTTP method")
	fs.StringVar(&customBody, "body", "", "custom HTTP request body")
	fs.StringVar(&customBodyFile, "body-file", "", "read custom HTTP request body from file")
	fs.StringVar(&customAuthMode, "auth-mode", "bearer", "custom auth mode: bearer or none")
	fs.BoolVar(&failInvalid, "fail-on-invalid", false, "return a non-zero exit code when invalid keys are found")
	fs.BoolVar(&failError, "fail-on-error", false, "return a non-zero exit code when errors are found")
	fs.BoolVar(&quiet, "quiet", false, "suppress per-key text output and keep only the final summary")
	fs.Var(&headers, "header", "custom header in 'Name: Value' form; may be repeated")

	if err := fs.Parse(args); err != nil {
		printCheckUsage(os.Stderr)
		return clierror.ExitError{Code: 2, Err: err}
	}

	timeout, err := time.ParseDuration(timeoutRaw)
	if err != nil {
		return fmt.Errorf("invalid timeout %q: %w", timeoutRaw, err)
	}
	if concurrency < 1 {
		return fmt.Errorf("concurrency must be >= 1")
	}
	if customBody != "" && customBodyFile != "" {
		return fmt.Errorf("--body and --body-file cannot be used together")
	}
	if customBodyFile != "" {
		bodyBytes, err := os.ReadFile(customBodyFile)
		if err != nil {
			return fmt.Errorf("read body file: %w", err)
		}
		customBody = string(bodyBytes)
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
		URL:      customURL,
		Method:   customMethod,
		Headers:  headers.AsMap(),
		Body:     customBody,
		AuthMode: customAuthMode,
	})
	if err != nil {
		return err
	}

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()

	reportWriter, closeReportWriter, err := openOutput(outputPath)
	if err != nil {
		return err
	}
	defer closeReportWriter()

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
		if strings.EqualFold(format, "text") && !quiet {
			output.WriteTextEvent(reportWriter, event.Index+1, len(keys), event.Result)
		}
	}

	summary, orderedResults, runErr := checker.Run(ctx, request, onEvent)
	results = orderedResults

	if exportValid != "" {
		if err := output.WriteKeysByStatus(exportValid, results, core.StatusValid); err != nil {
			return err
		}
	}
	if exportInvalid != "" {
		if err := output.WriteKeysByStatus(exportInvalid, results, core.StatusInvalid); err != nil {
			return err
		}
	}
	if exportError != "" {
		if err := output.WriteKeysByStatus(exportError, results, core.StatusError); err != nil {
			return err
		}
	}

	switch strings.ToLower(format) {
	case "text":
		output.WriteTextSummary(reportWriter, summary)
	case "json":
		if err := output.WriteJSONReport(reportWriter, summary, results); err != nil {
			return err
		}
	default:
		return fmt.Errorf("unsupported format %q", format)
	}

	if errors.Is(runErr, context.Canceled) {
		return clierror.ExitError{
			Code: 130,
			Err:  errors.New("operation canceled"),
		}
	}
	if failInvalid && summary.Invalid > 0 {
		return clierror.ExitError{
			Code: 3,
			Err:  fmt.Errorf("found %d invalid key(s)", summary.Invalid),
		}
	}
	if failError && summary.Error > 0 {
		return clierror.ExitError{
			Code: 4,
			Err:  fmt.Errorf("found %d error result(s)", summary.Error),
		}
	}
	return runErr
}

func runProviders(args []string) error {
	fs := flag.NewFlagSet("providers", flag.ContinueOnError)
	fs.SetOutput(io.Discard)

	var format string
	fs.StringVar(&format, "format", "text", "output format: text or json")

	if err := fs.Parse(args); err != nil {
		printProvidersUsage(os.Stderr)
		return clierror.ExitError{Code: 2, Err: err}
	}
	if fs.NArg() > 0 {
		return clierror.ExitError{
			Code: 2,
			Err:  fmt.Errorf("providers does not accept positional arguments"),
		}
	}

	entries := providers.Builtins()
	sort.Slice(entries, func(i, j int) bool {
		return entries[i].Name < entries[j].Name
	})

	switch strings.ToLower(format) {
	case "text":
		for _, entry := range entries {
			fmt.Fprintf(os.Stdout, "%-10s %-6s %s\n", entry.Name, entry.Method, entry.URL)
			if len(entry.Aliases) > 0 {
				fmt.Fprintf(os.Stdout, "           aliases: %s\n", strings.Join(entry.Aliases, ", "))
			}
			if entry.Notes != "" {
				fmt.Fprintf(os.Stdout, "           %s\n", entry.Notes)
			}
		}
	case "json":
		encoder := json.NewEncoder(os.Stdout)
		encoder.SetIndent("", "  ")
		if err := encoder.Encode(entries); err != nil {
			return fmt.Errorf("encode providers json: %w", err)
		}
	default:
		return fmt.Errorf("unsupported providers format %q", format)
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

func openOutput(path string) (io.Writer, func(), error) {
	if path == "" {
		return os.Stdout, func() {}, nil
	}

	file, err := os.Create(path)
	if err != nil {
		return nil, nil, fmt.Errorf("create output file: %w", err)
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
	fmt.Fprintln(w, "  apidetect gui [flags]")
	fmt.Fprintln(w, "  apidetect providers [flags]")
	fmt.Fprintln(w, "  apidetect version")
	fmt.Fprintln(w)
	printCheckUsage(w)
	fmt.Fprintln(w)
	printGUIUsage(w)
	fmt.Fprintln(w)
	printProvidersUsage(w)
}

func printCheckUsage(w io.Writer) {
	fmt.Fprintln(w, "check flags:")
	fmt.Fprintln(w, "  --provider       Provider name (default: openai)")
	fmt.Fprintln(w, "  --input          Read keys from file; stdin when omitted")
	fmt.Fprintln(w, "  --output         Write the main report to file instead of stdout")
	fmt.Fprintln(w, "  --concurrency    Maximum concurrent checks (default: 100)")
	fmt.Fprintln(w, "  --timeout        Per-request timeout (default: 10s)")
	fmt.Fprintln(w, "  --format         text or json (default: text)")
	fmt.Fprintln(w, "  --export-valid   Export valid raw keys to file")
	fmt.Fprintln(w, "  --export-invalid Export invalid raw keys to file")
	fmt.Fprintln(w, "  --export-error   Export error raw keys to file")
	fmt.Fprintln(w, "  --fail-on-invalid Return non-zero when invalid keys are found")
	fmt.Fprintln(w, "  --fail-on-error  Return non-zero when error results are found")
	fmt.Fprintln(w, "  --quiet          Suppress per-key text output and keep only the final summary")
	fmt.Fprintln(w, "  --url            Custom endpoint URL")
	fmt.Fprintln(w, "  --method         Custom HTTP method (default: GET)")
	fmt.Fprintln(w, "  --auth-mode      Custom auth mode: bearer or none")
	fmt.Fprintln(w, "  --body           Custom HTTP request body")
	fmt.Fprintln(w, "  --body-file      Read custom HTTP request body from file")
	fmt.Fprintln(w, "  --header         Custom header in 'Name: Value' form; repeatable")
}

func printProvidersUsage(w io.Writer) {
	fmt.Fprintln(w, "providers flags:")
	fmt.Fprintln(w, "  --format         text or json (default: text)")
}

func runGUI(args []string) error {
	fs := flag.NewFlagSet("gui", flag.ContinueOnError)
	fs.SetOutput(io.Discard)

	var (
		listenAddr string
		noOpen     bool
	)

	fs.StringVar(&listenAddr, "listen", "127.0.0.1:8787", "listen address for the local GUI server")
	fs.BoolVar(&noOpen, "no-open", false, "do not open the browser automatically")

	if err := fs.Parse(args); err != nil {
		printGUIUsage(os.Stderr)
		return clierror.ExitError{Code: 2, Err: err}
	}
	if fs.NArg() > 0 {
		return clierror.ExitError{
			Code: 2,
			Err:  fmt.Errorf("gui does not accept positional arguments"),
		}
	}

	server := gui.NewServer(listenAddr)
	if err := server.Run(noOpen); err != nil {
		return err
	}
	return nil
}

func printGUIUsage(w io.Writer) {
	fmt.Fprintln(w, "gui flags:")
	fmt.Fprintln(w, "  --listen         Listen address for the local GUI server (default: 127.0.0.1:8787)")
	fmt.Fprintln(w, "  --no-open        Do not open the browser automatically")
}

type headerFlags []string

func (h *headerFlags) String() string {
	return strings.Join(*h, ", ")
}

func (h *headerFlags) Set(value string) error {
	trimmed := strings.TrimSpace(value)
	if trimmed == "" {
		return fmt.Errorf("header cannot be empty")
	}

	parts := strings.SplitN(trimmed, ":", 2)
	if len(parts) != 2 {
		return fmt.Errorf("header must be in 'Name: Value' form")
	}

	name := strings.TrimSpace(parts[0])
	if name == "" {
		return fmt.Errorf("header name cannot be empty")
	}

	*h = append(*h, trimmed)
	return nil
}

func (h headerFlags) AsMap() map[string]string {
	out, err := configutil.ParseHeaderEntries(h)
	if err != nil {
		return map[string]string{}
	}
	return out
}
