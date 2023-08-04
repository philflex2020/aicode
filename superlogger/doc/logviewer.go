package main

import (
    "fmt"
    "os"
    "bufio"
    "path/filepath"
    "sort"
    "strings"
    "time"
)

type LogEntry struct {
    Timestamp time.Time
    Message   string
}

func readLogFormats(project string) map[string]string {
    file, _ := os.Open(fmt.Sprintf("/var/log/%s/logids", project))
    defer file.Close()

    scanner := bufio.NewScanner(file)
    logFormats := make(map[string]string)
    for scanner.Scan() {
        parts := strings.SplitN(scanner.Text(), ":", 2)
        if len(parts) == 2 {
            logFormats[parts[0]] = parts[1]
        }
    }

    return logFormats
}

func readLogEntries(logFile string, logFormats map[string]string) []LogEntry {
    file, _ := os.Open(logFile)
    defer file.Close()

    scanner := bufio.NewScanner(file)
    var logEntries []LogEntry
    for scanner.Scan() {
        parts := strings.Split(scanner.Text(), "\t")
        timestamp, _ := time.Parse(time.RFC3339, parts[0])
		fmt.Printf("Number of parts: %d\n", len(parts)) // print the number of parts

        if len(parts) < 6 {
            continue  // Skip this line, as it does not have the expected format.
        }
        format := logFormats[parts[1]]
        message := fmt.Sprintf(format, parts[2], parts[3], parts[4], parts[5])
        logEntries = append(logEntries, LogEntry{timestamp, message})
    }

    return logEntries
}

func main() {
    project := "testProject"

    logFormats := readLogFormats(project)

    var logEntries []LogEntry
    filepath.Walk(fmt.Sprintf("/var/log/%s", project), func(path string, info os.FileInfo, err error) error {
        if !info.IsDir() {
            entries := readLogEntries(path, logFormats)
            logEntries = append(logEntries, entries...)
        }
        return nil
    })

    sort.Slice(logEntries, func(i, j int) bool {
        return logEntries[i].Timestamp.Before(logEntries[j].Timestamp)
    })

    for _, entry := range logEntries {
        fmt.Println(entry.Timestamp.Format(time.RFC3339), entry.Message)
    }
}
