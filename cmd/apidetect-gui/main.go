package main

import (
	"flag"
	"fmt"
	"os"

	"github.com/Usagi-wusaqi/API-Detector/internal/appmeta"
	"github.com/Usagi-wusaqi/API-Detector/internal/gui"
)

func main() {
	fs := flag.NewFlagSet("apidetect-gui", flag.ExitOnError)
	listenAddr := fs.String("listen", "127.0.0.1:8787", "listen address for the local GUI server")
	noOpen := fs.Bool("no-open", false, "do not open the browser automatically")
	version := fs.Bool("version", false, "print version information")
	fs.BoolVar(version, "v", false, "print version information")

	fs.Parse(os.Args[1:])

	if *version {
		fmt.Fprintf(os.Stdout, "%s\ncommit=%s\nbuild_date=%s\n", appmeta.Version, appmeta.Commit, appmeta.BuildDate)
		return
	}

	server := gui.NewServer(*listenAddr)
	if err := server.Run(*noOpen); err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		os.Exit(1)
	}
}
