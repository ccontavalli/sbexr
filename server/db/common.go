package db

import (
	"encoding/json"
	//	"io/ioutil"
	"log"
	"os"
)

const kMaxResults = 30

type JsonRequest struct {
	Q string `json:"q"`
	S uint64 `json:"s"`
	P bool   `json:"p"`
}

func LoadJson(fullpath string, destination interface{}) error {
	fd, err := os.Open(fullpath)
	if err != nil {
		log.Printf("ERROR: OPEN FAILED FOR %s\n", fullpath)
		return err
	}

	decoder := json.NewDecoder(fd)
	err = decoder.Decode(destination)
	if err != nil {
		log.Printf("ERROR: could not parse: %s - %s\n", fullpath, err)
		return err
	}
	log.Printf("LOADED: %s\n", fullpath)
	return nil
}
