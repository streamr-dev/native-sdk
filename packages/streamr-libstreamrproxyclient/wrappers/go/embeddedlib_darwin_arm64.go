package streamrproxyclient

import (
	"embed"
	"os"
	"path/filepath"
)

//go:embed dist/arm64-osx/lib/Debug/libstreamrproxyclient.1.0.0.dylib
var libFileFs embed.FS

const libFullPath = "dist/arm64-osx/lib/Debug/libstreamrproxyclient.1.0.0.dylib"
const libName = "libstreamrproxyclient.1.0.0.dylib"

func SaveLibToTempFile() (string, error) {
	dirName, err := os.MkdirTemp("", "*")
	if err != nil {
		return "", err
	}

	libPath := filepath.Join(dirName, libName)
	libFileBytes, err := libFileFs.ReadFile(libFullPath)
	if err != nil {
		return "", err
	}

	err = os.WriteFile(libPath, libFileBytes, 0o600)
	if err != nil {
		return "", err
	}
	println(libPath)
	return libPath, nil
}
