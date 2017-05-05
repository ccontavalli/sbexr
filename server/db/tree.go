package db

import (
	"net/http"
	"strings"
	"time"
)

type TreeData struct {
	Data []TreeObject `json:"data"`
}
type TreeObject struct {
	Dir    string `json:"dir,omitempty"`
	File   string `json:"file,omitempty"`
	Href   string `json:"href"`
	Parent string `json:"parent,omitempty"`
}

func LoadJsonTree(fullname string) (ApiHandler, error) {
	var symbol TreeData
	err := LoadJson(fullname, &symbol)
	return &symbol, err
}

func (data *TreeData) Delete() {
}

func (data *TreeData) HandleSearch(resp http.ResponseWriter, query *JsonRequest) (interface{}, *Stats) {
	stats := Stats{}

	result := TreeData{}
	q := strings.ToLower(query.Q)

	for count := range data.Data {
		stats.scanned += 1

		object := &data.Data[count]
		dir := strings.ToLower(object.Dir)
		file := strings.ToLower(object.File)

		if !strings.Contains(dir, q) && !strings.Contains(file, q) {
			continue
		}

		stats.matched += 1
		if stats.matched > query.S {
			startbuild := time.Now()
			stats.returned += 1
			result.Data = append(result.Data, *object)
			stats.buildresulttime += time.Since(startbuild)

			if stats.matched >= kMaxResults+query.S {
				break
			}
		}
	}

	return result, &stats
}
