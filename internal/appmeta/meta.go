package appmeta

const Name = "apidetect"

var (
	Version   = "v3.0.0"
	Commit    = "unknown"
	BuildDate = "unknown"
)

func UserAgent() string {
	return Name + "/" + Version
}
