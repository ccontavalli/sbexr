package db

import (
	"bytes"
	"fmt"
	"github.com/ccontavalli/sbexr/server/structs"
	"github.com/ccontavalli/sbexr/server/templates"
	"log"
	"net/http"
	"os"
	"regexp"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"time"
	"unsafe"
)

// #include "../../src/cindex.h"
import "C"

type SymbolNameToDetails C.SymbolNameToDetails

type CompactBinarySymbolData struct {
	minoffsets []uint32

	details  []byte
	symbols  []byte
	snippets []byte
	strings  []byte
	files    []byte
	hashes   []byte
}

type JsonBinarySymbolProvider struct {
	Href     string `json:"href"`
	Location string `json:"location"`
	Snippet  uint32 `json:"snippet"`
}
type JsonBinarySymbolKindLinkage struct {
	Linkage uint8                      `json:"linkage"`
	Access  uint8                      `json:"access"`
	Kind    uint32                     `json:"kind"`
	Defs    []JsonBinarySymbolProvider `json:"defs"`
	Decls   []JsonBinarySymbolProvider `json:"decls"`
}
type JsonBinarySymbolObject struct {
	Name  uint32                        `json:"name"`
	Kinds []JsonBinarySymbolKindLinkage `json:"kinds"`
}

func mmap(filename string) ([]byte, error) {
	file, err := os.Open(filename)
	if err != nil {
		return []byte{}, err
	}
	defer file.Close()

	return mmapFile(file)
}

func munmap(data []byte) error {
	return syscall.Munmap(data)
}

func mmapFile(f *os.File) ([]byte, error) {
	st, err := f.Stat()
	if err != nil {
		return []byte{}, err
	}
	size := st.Size()
	if int64(int(size)) != size {
		return []byte{}, fmt.Errorf("size of %d overflows int", size)
	}
	data, err := syscall.Mmap(int(f.Fd()), 0, int(size), syscall.PROT_READ, syscall.MAP_PRIVATE)
	if err != nil {
		return []byte{}, err
	}
	err = syscall.Mlock(data)
	if err != nil {
		log.Printf("MLOCK for %d bytes FAILED: %s\n", len(data), err)
	} else {
		log.Printf("MLOCK for %d bytes SUCCEEDED\n", len(data))
	}
	return data[:size], nil
}

func LoadCompactBinary(jsonfile string) (*CompactBinarySymbolData, error) {
	var symbol CompactBinarySymbolData

	detailsfile := strings.TrimSuffix(jsonfile, ".json") + ".details"
	details, err := mmap(detailsfile)
	if err != nil {
		return nil, err
	}

	symbolsfile := strings.TrimSuffix(jsonfile, ".json") + ".symbol-details"
	symbols, err := mmap(symbolsfile)
	if err != nil {
		munmap(details)
		return nil, err
	}

	snippetsfile := strings.TrimSuffix(jsonfile, ".json") + ".snippets"
	snippets, err := mmap(snippetsfile)
	if err != nil {
		munmap(details)
		munmap(symbols)
		return nil, err
	}

	stringsfile := strings.TrimSuffix(jsonfile, ".json") + ".strings"
	stringmap, err := mmap(stringsfile)
	if err != nil {
		munmap(details)
		munmap(symbols)
		munmap(snippets)
		return nil, err
	}

	filesfile := strings.TrimSuffix(jsonfile, ".json") + ".files"
	files, err := mmap(filesfile)
	if err != nil {
		munmap(details)
		munmap(symbols)
		munmap(snippets)
		munmap(stringmap)
		return nil, err
	}

	hashesfile := strings.TrimSuffix(jsonfile, ".json") + ".hash-details"
	hashes, err := mmap(hashesfile)
	if err != nil {
		munmap(details)
		munmap(symbols)
		munmap(snippets)
		munmap(stringmap)
		munmap(files)
		return nil, err
	}

	symbol.details = details
	symbol.symbols = symbols
	symbol.strings = stringmap
	symbol.snippets = snippets
	symbol.files = files
	symbol.hashes = hashes

	symbol.minoffsets = make([]uint32, 0, 1024)

	minindex := 0
        err = symbol.ForEachSymbol(0, func (_ *SymbolNameToDetails, offset uint32, name []byte) {
		for ; minindex <= len(name); minindex++ {
			// log.Printf("FOR LEN %d MIN OFFSET %d LEN %d\n", minindex, offset, len(symbol.minoffsets))
			symbol.minoffsets = append(symbol.minoffsets, offset)
		}
        })

        if err != nil {
          return nil, err
        }
	return &symbol, nil
}

func (data *CompactBinarySymbolData) ForEachSymbol(offset uint32, process func (*SymbolNameToDetails, uint32, []byte)) error {
	for offset < uint32(len(data.symbols)) {
		symbol, newoffset, name, err := data.GetSymbolName(C.NameOffsetT(offset))
                if err != nil {
                  return err
                }

                process(symbol, offset, name)
                offset = newoffset
	}
        return nil
}

func (data *CompactBinarySymbolData) Delete() {
	munmap(data.snippets)
	munmap(data.strings)
	munmap(data.details)
	munmap(data.symbols)
	munmap(data.files)
	munmap(data.hashes)
}

func (data *CompactBinarySymbolData) HandlePage(resp http.ResponseWriter, req *http.Request, tag *TagData, mutex *sync.RWMutex) {
	page := strings.TrimPrefix(req.URL.Path, tag.Url)
	hash, err := strconv.ParseUint(page, 16, 64)
	if err != nil {
		fmt.Fprintf(resp, "INVALID PAGE REQUEST - COULD NOT PARSE HASH")
		return
	}

	details, err := data.GetHashDetails(hash)
	if err != nil {
		fmt.Fprintf(resp, "UNKNOWN SYMBOL %d", hash)
		return
	}
	// log.Printf("TEST PAGE YAY %s - %s - %d - %v - %+v", req.URL.Path, page, hash, err, details)
	symbolpage := &templates.SymbolPage{
		BasePage: templates.BasePage{
			Project: tag.Project,
			Tag:     tag.Tag,
		},
		Symbol: details,
	}
	templates.WritePageTemplate(resp, symbolpage)
}

func (data *CompactBinarySymbolData) GetHashData(hash uint64) *C.SymbolHashToDetails {
	pool := data.hashes
	expected := C.uint64_t(hash)
	minindex := 0
	maxindex := len(data.hashes) / C.sizeof_SymbolHashToDetails
	for minindex < maxindex {
		tocheck := minindex + (maxindex-minindex)/2
		hashdetails := (*C.SymbolHashToDetails)(unsafe.Pointer(&pool[tocheck*C.sizeof_SymbolHashToDetails]))
		if hashdetails.hash == expected {
			return hashdetails
		}
		if expected > hashdetails.hash {
			minindex = tocheck + 1
		} else {
			maxindex = tocheck
		}
	}
	return nil
}
func (data *CompactBinarySymbolData) GetHashDetails(hash uint64) (*structs.SymbolObject, error) {
	hashdata := data.GetHashData(hash)
	if hashdata == nil {
		return nil, fmt.Errorf("Could not find symbol by hash %d", hash)
	}
	return data.GetSymbolDetails(nil, hashdata.Detailoffset)
}

func (data *CompactBinarySymbolData) GetSymbolDetails(name []byte, offset C.DetailOffsetT) (*structs.SymbolObject, error) {
	details, kinds, err := GetDetails(data.details, offset)
	if err != nil {
		return nil, err
	}

	toappend := structs.SymbolObject{}
	toappend.Hash = fmt.Sprintf("%x", uint64(details.hash))
	toappend.Kinds = make([]structs.SymbolKindLinkage, details.kindsize)

	if name == nil {
		_, _, foundname, err := data.GetSymbolName(details.nameoffset)
		if err != nil {
			return nil, err
		}
		toappend.Name = string(foundname)
	} else {
		toappend.Name = string(name)
	}

	for i, kindlinkage := range kinds {
		okindlinkage := &toappend.Kinds[i]
		okindlinkage.Linkage = int(kindlinkage.linkage)
		okindlinkage.Access = int(kindlinkage.access)
		tmp, err := GetString(data.strings, uint32(kindlinkage.name))
		if err != nil {
			return nil, err
		}
		okindlinkage.Kind = string(tmp)

		defs, err := kindlinkage.GetDefs(data.details)
		okindlinkage.Defs = make([]structs.SymbolProvider, len(defs))
		for j, provider := range defs {
			oprovider := &okindlinkage.Defs[j]
			oprovider.Href = MakeHtmlPathFromId(provider.fid, provider.sid)
			location, err := GetFilePathLineColumn(data.files, provider.fid.pathoffset, &provider.sid)
			if err != nil {
				return nil, err
			}
			oprovider.Location = string(location)
			tmp, err = GetString(data.snippets, uint32(provider.snippet))
			if err != nil {
				return nil, err
			}
			oprovider.Snippet = string(tmp)
		}

		decls, err := kindlinkage.GetDecls(data.details)
		okindlinkage.Decls = make([]structs.SymbolProvider, len(decls))
		for j, provider := range decls {
			oprovider := &okindlinkage.Decls[j]
			oprovider.Href = MakeHtmlPathFromId(provider.fid, provider.sid)
			location, err := GetFilePathLineColumn(data.files, provider.fid.pathoffset, &provider.sid)
			if err != nil {
				return nil, err
			}
			oprovider.Location = string(location)
			tmp, err = GetString(data.snippets, uint32(provider.snippet))
			if err != nil {
				return nil, err
			}
			oprovider.Snippet = string(tmp)
		}
	}
	return &toappend, nil
}

func min(a, b uint32) uint32 {
	if a < b {
		return a
	}
	return b
}


func (data *CompactBinarySymbolData) HandleSearch(resp http.ResponseWriter, query *JsonRequest) (interface{}, *Stats) {
	stats := Stats{}

	compilestart := time.Now()
	result := structs.SymbolData{}
	regex, err := regexp.Compile("(?i)" + query.Q)
	// regex, err := regexp.Compile(query.Q)
	if err != nil {
		result.Error = err.Error()
		return result, &stats
	}

	// Find a static prefix to the regexp, if we can.
	strprefix, full := regex.LiteralPrefix()
	if len(strprefix) <= 0 {
		supportregex, err := regexp.Compile("^" + query.Q + "$")
		if err == nil {
			strprefix, full = supportregex.LiteralPrefix()
		}
	}
	// Start scanning the index from the minimal length necessary.
	prefix := bytes.ToLower([]byte(strprefix))
	minoffset := data.minoffsets[min(uint32(len(prefix)), uint32(len(data.minoffsets)))]
	compiletime := time.Since(compilestart)

	stats.optimized = fmt.Sprintf("prefix='%s', minoffset='%d', setup='%s', full=%v", prefix, minoffset, compiletime, full)

	offset := minoffset
	for offset < uint32(len(data.symbols)) {
		stats.scanned += 1
		if stats.scanned&8192 == 0 {
			runtime.Gosched()
		}

		symbol, newoffset, name, err := data.GetSymbolName(C.NameOffsetT(offset))
		offset = newoffset

		if err != nil {
			log.Printf("ERROR: invalid symbols file - ABORTING SEARCH - %s\n", err)
			break
		}

		if !(CaseContains(name, prefix) && (full || regex.Match(name))) {
			continue
		}
		stats.matched += 1

		if stats.matched > query.S {
			startbuild := time.Now()

			toappend, err := data.GetSymbolDetails(name, symbol.Detailoffset)
			if err != nil {
				log.Printf("! INVALID FILE ENTRY IN DECLS INDEX %s: %s", string(name), err)
				continue
			}

			stats.returned += 1
			result.Data = append(result.Data, toappend)
			stats.buildresulttime += time.Since(startbuild)

			if stats.matched >= kMaxResults+query.S {
				break
			}
		}
	}

	return result, &stats
}

func LoadSymbols(jsonfile string) (ApiHandler, error) {
	if !strings.HasSuffix(jsonfile, ".json") {
		return nil, fmt.Errorf("jsonfile must end in .json")
	}

	detailsfile := strings.TrimSuffix(jsonfile, ".json") + ".details"
	_, err := os.Stat(detailsfile)
	if err != nil {
		return nil, err
	}

	return LoadCompactBinary(jsonfile)
}

func (data *CompactBinarySymbolData) GetSymbolName(offset C.NameOffsetT) (*SymbolNameToDetails, uint32, []byte, error) {
        pool := data.symbols

	if uintptr(offset)+C.sizeof_SymbolNameToDetails >= uintptr(len(pool)) {
		return nil, 0, []byte{}, fmt.Errorf("GetSymbolName: invalid offset %d, overflows %d", offset, len(pool))
	}

	symbol := (*SymbolNameToDetails)(unsafe.Pointer(&pool[offset]))
	namestart := uint32(offset) + C.sizeof_SymbolNameToDetails
	nameend := namestart + uint32(symbol.namesize)

	if uintptr(nameend) > uintptr(len(pool)) {
		return nil, 0, []byte{}, fmt.Errorf("GetSymbolName: invalid namesize %d in structs.SymbolnameToDetails entry at %d, overflows %d", symbol.namesize, offset, len(pool))
	}
	return symbol, nameend, pool[namestart:nameend], nil
}

func (kind *C.SymbolDetailKind) GetDefs(pool []byte) ([]*C.SymbolDetailProvider, error) {
	if uintptr(unsafe.Pointer(kind)) < uintptr(unsafe.Pointer(&pool[0])) || uintptr(unsafe.Pointer(kind)) > uintptr(unsafe.Pointer(&pool[len(pool)-1])) {
		return []*C.SymbolDetailProvider{}, fmt.Errorf("GetDefs invoked on invalid structs.SymbolDetailKind or pool!")
	}

	providers := make([]*C.SymbolDetailProvider, kind.defsize)
	provstart := uintptr(unsafe.Pointer(kind)) - uintptr(unsafe.Pointer(&pool[0])) + C.sizeof_SymbolDetailKind
	for count := 0; count < int(kind.defsize); count++ {
		provend := provstart + C.sizeof_SymbolDetailProvider

		if provend > uintptr(len(pool)) {
			return []*C.SymbolDetailProvider{}, fmt.Errorf("GetDefs for kind %p ends up fetching a provider past the pool end provider start %d, end %d, pool end %d!", kind, provstart, provend, len(pool))
		}

		provider := (*C.SymbolDetailProvider)(unsafe.Pointer(&pool[provstart]))
		providers[count] = provider
		provstart = provend
	}
	return providers, nil
}

func (kind *C.SymbolDetailKind) GetDecls(pool []byte) ([]*C.SymbolDetailProvider, error) {
	if uintptr(unsafe.Pointer(kind)) < uintptr(unsafe.Pointer(&pool[0])) || uintptr(unsafe.Pointer(kind)) > uintptr(unsafe.Pointer(&pool[len(pool)-1])) {
		return []*C.SymbolDetailProvider{}, fmt.Errorf("GetDecls invoked on invalid structs.SymbolDetailKind or pool!")
	}

	providers := make([]*C.SymbolDetailProvider, kind.declsize)
	provstart := uintptr(unsafe.Pointer(kind)) - uintptr(unsafe.Pointer(&pool[0])) + C.sizeof_SymbolDetailKind + (C.sizeof_SymbolDetailProvider * uintptr(kind.defsize))
	for count := 0; count < int(kind.declsize); count++ {
		provend := provstart + C.sizeof_SymbolDetailProvider

		if provend > uintptr(len(pool)) {
			return []*C.SymbolDetailProvider{}, fmt.Errorf("GetDecls for kind %p ends up fetching a provider past the pool end provider start %d, end %d, pool end %d!", kind, provstart, provend, len(pool))
		}

		provider := (*C.SymbolDetailProvider)(unsafe.Pointer(&pool[provstart]))
		providers[count] = provider
		provstart = provend
	}
	return providers, nil
}

func GetDetails(pool []byte, offset C.DetailOffsetT) (*C.SymbolDetail, []*C.SymbolDetailKind, error) {
	if uintptr(offset)+C.sizeof_SymbolDetail >= uintptr(len(pool)) {
		return nil, []*C.SymbolDetailKind{}, fmt.Errorf("GetDetails: Invalid offset %d, overflows %d", offset, len(pool))
	}

	symbol := (*C.SymbolDetail)(unsafe.Pointer(&pool[offset]))
	kindstart := offset + C.sizeof_SymbolDetail

	kinds := make([]*C.SymbolDetailKind, symbol.kindsize)
	for count := 0; count < int(symbol.kindsize); count++ {
		if uintptr(kindstart)+C.sizeof_SymbolDetailKind > uintptr(len(pool)) {
			return nil, []*C.SymbolDetailKind{}, fmt.Errorf("GetDetails: Invalid kindsize %d in structs.SymbolDetailKind. Entry at %d, overflows %d", symbol.kindsize, kindstart, len(pool))
		}

		kind := (*C.SymbolDetailKind)(unsafe.Pointer(&pool[kindstart]))
		kindend := kindstart + C.sizeof_SymbolDetailKind + (C.DetailOffsetT(kind.defsize+kind.declsize) * C.sizeof_SymbolDetailProvider)

		if uintptr(kindend) > uintptr(len(pool)) {
			return nil, []*C.SymbolDetailKind{}, fmt.Errorf("GetDetails: Invalid kindsize %d in structs.SymbolDetailKind. Entry at %d-%d, overflows %d", symbol.kindsize, kindstart, kindend, len(pool))
		}

		kinds[count] = kind
		kindstart = kindend
	}
	return symbol, kinds, nil
}

func GetFilePathLineColumn(pool []byte, offset C.FileOffsetT, sid *C.SymbolId) (string, error) {
	path, err := GetFilePath(pool, offset)
	if err != nil {
		return "", err
	}

	startline := (sid.eid >> C.kBeginLineShift) & C.kLineMask
	endline := (sid.eid >> C.kEndLineShift) & C.kLineMask
	startcolumn := (sid.eid >> C.kBeginColumnShift) & C.kColumnMask
	endcolumn := (sid.eid >> C.kEndColumnShift) & C.kColumnMask

	return fmt.Sprintf("%s:%d:%d-%d:%d", path, startline, startcolumn, endline, endcolumn), nil
}

func GetFilePath(pool []byte, offset C.FileOffsetT) ([]byte, error) {
	pathstart := offset + C.sizeof_FileDetail
	if uintptr(pathstart) > uintptr(len(pool)) {
		return []byte{}, fmt.Errorf("GetFilePath: invalid file start offset %d, overflows %d", pathstart, len(pool))
	}
	filedetail := (*C.FileDetail)(unsafe.Pointer(&pool[offset]))
	pathend := pathstart + C.FileOffsetT(filedetail.pathsize)

	if uintptr(pathend) > uintptr(len(pool)) {
		return []byte{}, fmt.Errorf("GetFilePath: invalid file end offset %d, overflows %d", pathend, len(pool))
	}

	retval := pool[pathstart:pathend]
	return retval, nil
}

func MakeHtmlPathFromHash(hash uint64) string {
	return fmt.Sprintf("../%02x/%014x.html", hash&0xff, (hash>>8)&0xffffffffffffff)
}
func MakeHtmlPathFromId(fid C.FileId, sid C.SymbolId) string {
	if sid.eid != sid.sid && sid.sid != 0 {
		return fmt.Sprintf("%s#%016x%016x", MakeHtmlPathFromHash(uint64(fid.hash)), uint64(sid.sid), uint64(sid.eid))
	}
	return fmt.Sprintf("%s#%016x", MakeHtmlPathFromHash(uint64(fid.hash)), sid.eid)
}

func GetString(pool []byte, offset uint32) ([]byte, error) {
	if uintptr(offset)+unsafe.Sizeof(offset) >= uintptr(len(pool)) {
		return []byte{}, fmt.Errorf("GetString: invalid offset %d, overflows %d", offset, len(pool))
	}
	size := *(*uint32)(unsafe.Pointer(&pool[offset]))
	end := offset + size + uint32(unsafe.Sizeof(offset))
	if end > uint32(len(pool)) {
		return []byte{}, fmt.Errorf("GetString: invalid offset %d, overflows %d", end, len(pool))
	}

	retval := pool[offset+uint32(unsafe.Sizeof(offset)) : end]
	return retval, nil
}
