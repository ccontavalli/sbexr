package main

import (
	"flag"
	"github.com/ccontavalli/sbexr/server/db"
        "os"
        "fmt"
        "bufio"
        "encoding/xml"
        "strings"
        "time"
        "compress/gzip"
        "log"
        "path/filepath"
        "path"
)

// #include "../src/cindex.h"
// import "C"

var jsonfile = flag.String("json-file", "", "(mandatory) Path to the .json file to use")
var treedir = flag.String("tree-dir", "", "(mandatory) Path to the directory containing all generated .html files")
var url = flag.String("source-url", "", "(mandatory) Base url to prepend to the computed paths")
var sitemapurl = flag.String("sitemap-url", "", "(defaults to -source-url) Base url to prepend to the sitemap file")
var output = flag.String("output", ".", "Directory where to output the sitemap file")

func ErrorExit(status int, msg string, a... interface{}) {
  fmt.Fprintf(os.Stderr, msg, a...)
  os.Exit(status)
}

type SitemapIndex struct {
  XMLName xml.Name `xml:"sitemap"`
  Loc string `xml:"loc"`
  Lastmod string `xml:"lastmod,omitempty"`
  Priority string `xml:"priority,omitempty"`
}

type SitemapUrl struct {
  XMLName xml.Name `xml:"url"`
  Loc string `xml:"loc"`
  Lastmod string `xml:"lastmod,omitempty"`
  Changefreq string `xml:"changefreq,omitempty"`
  Priority string `xml:"priority,omitempty"`
}

const kMaxUrls = 8000
const kMaxSize = 8 * 1048576
// const kMaxSize = 2 * 1024 // * 1024

type SitemapWriter struct {
  MaxSize uint64
  Opening xml.Name
  Outdir string
  Count uint64

  file *os.File
  writer *gzip.Writer
  buffer *bufio.Writer
  encoder *xml.Encoder

  size uint64
}

func NewSitemapWriter() *SitemapWriter {
  opening := xml.Name{Space: "http://www.sitemaps.org/schemas/sitemap/0.9", Local: "urlset"}
  return &SitemapWriter{MaxSize: kMaxSize, Opening: opening, Outdir: *output}
}

func (sw *SitemapWriter) GetNumberedName(count uint64) string {
          return fmt.Sprintf("%s/sitemap-%03d.xml.gz", sw.Outdir, count)
}

func (sw *SitemapWriter) GetName(count uint64) string {
        var name string
        if count > 0 {
          name = sw.GetNumberedName(count)
        } else {
          name = fmt.Sprintf("%s/sitemap.xml.gz", sw.Outdir)
        }
        return name
}

func (sw *SitemapWriter) Reopen() error {
        name := sw.GetName(sw.Count)
        f, err := os.Create(name)
        if err != nil {
          return err
        }

        sw.Count += 1
        sw.size = 0
        sw.file = f
        sw.buffer = bufio.NewWriter(f)
        sw.writer = gzip.NewWriter(sw.buffer)
        sw.encoder = xml.NewEncoder(sw)

        sw.RealWrite([]byte(xml.Header))
        sw.encoder.EncodeToken(xml.StartElement{Name: sw.Opening})
        sw.encoder.Flush()
        return nil
}

func (sw *SitemapWriter) RealWrite(buffer []byte) (n int, err error) {
          written, err := sw.writer.Write(buffer)
          if err == nil && written > 0 {
            sw.size += uint64(written)
          }
          return written, err
}

func (sw *SitemapWriter) Write(buffer []byte) (n int, err error) {
        if sw.writer == nil || sw.size >= sw.MaxSize {
          sw.Flush()
          err := sw.Reopen()
          if err != nil {
            return 0, err
          }
        }

        return sw.RealWrite(buffer)
}

func (sw *SitemapWriter) Close() error {
  sw.Flush()
  if sw.Count > 1 {
    // Rename sitemap.xml.gz to sitemap-000.xml.gz
    oldname := sw.GetName(0)
    newname := sw.GetNumberedName(0)
    err := os.Rename(oldname, newname)
    if err != nil {
      return err
    }

    nw := NewSitemapWriter()
    nw.MaxSize = 0xffffffffffffffff
    nw.Opening = xml.Name{Space: "http://www.sitemaps.org/schemas/sitemap/0.9", Local: "sitemapindex"}

    encoder := xml.NewEncoder(nw)
    entry := &SitemapIndex{Lastmod: time.Now().Format(time.RFC3339)}

    for i := uint64(0); i < sw.Count; i++ {
      entry.Loc = fmt.Sprintf("%s/%s", *sitemapurl, path.Base(sw.GetNumberedName(i)))
      encoder.Encode(entry)
    }

    encoder.Flush()
    nw.Close()
  }
  return nil
}

func (sw *SitemapWriter) Flush() {
  sw.size = 0

  if sw.encoder != nil {
        sw.encoder.EncodeToken(xml.EndElement{Name: sw.Opening})
        sw.encoder.Flush()
        sw.encoder = nil
  }
  if sw.writer != nil {
        sw.writer.Close()
        sw.writer = nil
  }
  if sw.buffer != nil {
        sw.buffer.Flush()
        sw.buffer = nil
  }
  if sw.file != nil {
        sw.file.Close()
        sw.file = nil
  }
}

func main() {
	flag.Parse()
        if *url == "" {
          ErrorExit(1, "Must provide --url\n")
        }
        if *sitemapurl == "" {
          sitemapurl = url
        }

        if *jsonfile == "" && *treedir == "" {
          ErrorExit(1, "Must provide --json-file or --tree-dir flag\n")
        }

        writer := NewSitemapWriter()
        reopened := uint64(1)
        entries := 0
        encoder := xml.NewEncoder(writer)
        entry := &SitemapUrl{Changefreq: "monthly", Lastmod: time.Now().Format(time.RFC3339)}

        if *jsonfile != "" {
          symbols, err := db.LoadCompactBinary(*jsonfile)
          if err != nil {
            ErrorExit(2, "Could not load json file: %v\n", err)
          }

          log.Printf("Scanning symbols %s", *jsonfile)
          counted := 0
          err = symbols.ForEachSymbol(0, func (symbol *db.SymbolNameToDetails, offset uint32, name []byte) {
            details, err := symbols.GetSymbolDetails(name, symbol.Detailoffset)
            if err != nil {
              fmt.Printf("! INVALID FILE ENTRY IN DECLS INDEX %s: %s", name, err)
              return
            }

            entry.Priority = "0.3"
            for _, kind := range details.Kinds {
              if kind.Linkage >= 3 || kind.Access <= 0 {
                entry.Priority = "0.8"
                break
              }
            }

            entry.Loc = fmt.Sprintf("%s/symbol/%s", *url, details.Hash)
            encoder.Encode(entry)
            counted++

            if writer.Count > reopened {
              entries = 0
              reopened = writer.Count
            } else {
              entries++
            }

            if entries >= kMaxUrls {
              writer.Flush()
              entries = 0
            }
          })
          log.Printf("  + added %d symbols", counted)

          if err != nil {
            ErrorExit(3, "Invalid symbol file: %v", err)
          }
        }

        if *treedir != "" {
          log.Printf("Scanning source tree %s", *treedir)
          files := 0

          entry.Priority = "0.6"

          filepath.Walk(*treedir, func (path string, info os.FileInfo, err error) error {
            relpath := strings.TrimPrefix(path, *treedir)
            entry.Loc = fmt.Sprintf("%s/%s", *url, relpath)
            encoder.Encode(entry)
            files++

            if writer.Count > reopened {
              entries = 0
              reopened = writer.Count
            } else {
              entries++
            }

            if entries >= kMaxUrls {
              writer.Flush()
              entries = 0
            }

            return nil
          })
          log.Printf("  + added %d files", files)
        }

        encoder.Flush()
        writer.Close()
}
