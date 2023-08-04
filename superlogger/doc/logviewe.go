package main

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"
)

// Struct to store each log entry
type LogEntry struct {
	Timestamp time.Time
	LogID     string
	Variables []string
}

// LogViewer
type LogViewer struct {
	Project string
}

func (v *LogViewer) Replay() {
	// Load logid references and format strings
	logIDMap := make(map[string]string)

	data, err := os.ReadFile(fmt.Sprintf("/var/log/%s/logids", v.Project))
	if err != nil {
		fmt.Printf("failed to read logids file: %v\n", err)
		return
	}

	lines := strings.Split(string(data), "\n")
	for _, line := range lines {
		parts := strings.Split(line, "\t")
		if len(parts) == 2 {
			logIDMap[parts[0]] = parts[1]
		}
	}

	// Iterate over all files in the /var/log/<project> directory
	err = filepath.Walk(fmt.Sprintf("/var/log/%s", v.Project), func(path string, info os.FileInfo, err error) error {
		if err != nil {
			fmt.Printf("prevent panic by handling failure accessing a path %q: %v\n", path, err)
			return err
		}

		if !info.IsDir() {
			file, err := os.Open(path)
			if err != nil {
				fmt.Printf("failed to open file: %v", err)
				return err
			}

			scanner := bufio.NewScanner(file)
			for scanner.Scan() {
				line := scanner.Text()
				parts := strings.Split(line, "\t")
				if len(parts) > 5 {
					format := logIDMap[parts[1]]
					fmt.Printf(format+": ", parts[2], parts[3], parts[4], parts[5])
				}
			}

			if err := file.Close(); err != nil {
				fmt.Printf("failed to close file: %v", err)
				return err
			}
		}

		return nil
	})

	if err != nil {
		fmt.Printf("error walking the path /var/log/%s: %v\n", v.Project, err)
		return
	}
}

func main() {
	logViewer := LogViewer{Project: "testProject"}
	logViewer.Replay()
}
