package streamrproxyclient

import (
	"embed"
	"os"
	"path/filepath"
)

//go:embed dist/arm64-osx/lib/Release/libstreamrproxyclient.dylib
var libFileFs embed.FS

const libFullPath = "dist/arm64-osx/lib/Release/libstreamrproxyclient.dylib"
const libName = "libstreamrproxyclient.dylib"

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
	return libPath, nil
}
