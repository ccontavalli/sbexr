package main

import (
	"log"
	"net/http"
	"strings"
	"time"
        "github.com/ccontavalli/sbexr/server/structs"
)

type SymbolData struct {
	Data []*structs.SymbolObject `json:"data"`
        Error string `json:"error,omitempty"`
}

func LoadJsonSymbols(fullname string) (ApiHandler, error) {
	var symbol SymbolData
	err := LoadJson(fullname, &symbol)
	return &symbol, err
}

func (data *SymbolData) Delete() {
}

func (data *SymbolData) HandleSearch(resp http.ResponseWriter, query *JsonRequest) (interface {}, *Stats) {
        stats := Stats{}

	result := SymbolData{}
	q := strings.ToLower(query.Q)

	for count = range data.Data {
                stats.scanned += 1
		object := data.Data[count]
		name := strings.ToLower(object.Name)
		if !strings.Contains(name, q) {
                  continue
                }

                stats.matched += 1
		if stats.matched > query.S {
                  stats.returned += 1
                  startbuild := time.Now()
		  result.Data = append(result.Data, object)
                  stats.buildresulttime += time.Since(startbuild)
		}

		if stats.matched >= kMaxResults+query.S {
			break
		}
	}

        return result
}
