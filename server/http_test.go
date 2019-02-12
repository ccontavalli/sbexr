package main

import (
	//"bytes"
	"github.com/stretchr/testify/assert"
	"io/ioutil"
	"net/http"
	"net/http/httptest"
	"path"
	"strings"
	"testing"
)

func TestSplitJhtml(t *testing.T) {
	assert := assert.New(t)

	h, b := SplitJhtml([]byte(""))
	assert.EqualValues("", h)
	assert.EqualValues("", b)

	h, b = SplitJhtml([]byte("foo"))
	assert.EqualValues("foo", h)
	assert.EqualValues("", b)

	h, b = SplitJhtml([]byte("foo bar baz---\n---foo bar---"))
	assert.EqualValues("foo bar baz---\n---foo bar---", h)
	assert.EqualValues("", b)

	h, b = SplitJhtml([]byte("\n---\nfoo"))
	assert.EqualValues("", h)
	assert.EqualValues("foo", b)

	h, b = SplitJhtml([]byte("baz\n---\nfoo"))
	assert.EqualValues("baz", h)
	assert.EqualValues("foo", b)
}

func IsValidPage(assert *assert.Assertions, body string) {
	assert.Regexp("^<!DOCTYPE html>", body)
	assert.Regexp("</body></html>$", body)
	// Verify various component of the web page - to check that template
	// expansion worked as expected.
	assert.Regexp("optional\\.js", body)
	assert.Regexp("style\\.css", body)
	assert.Regexp("analytics\\.js", body)
	assert.Regexp("searchbox-", body)

	// TODO: check absence of {{ or {% or things like BasePage, JNavData
	// (template parameters that have not expanded!)
	assert.NotRegexp("{{", body)
	assert.NotRegexp("{%", body)
	assert.NotRegexp("%}", body)
	assert.NotRegexp("}}", body)
	assert.NotRegexp(">[ ]*[a-zA-Z]{4,}\\(", body)
}

func TestHandlers(t *testing.T) {
	assert := assert.New(t)
	index := NewIndex("../test/indexes", "../test")
	assert.NotNil(index)

	t.Logf("tagmap %+v", index.Sources.tags)
	assert.Equal(1, len(index.Sources.tags))

	resp := httptest.NewRecorder()
	req, err := http.NewRequest("GET", "/foo/bar.html", nil)
	assert.Nil(err)
	index.Sources.ServeHTTP(resp, req)
	assert.Equal(http.StatusNotFound, resp.Code)

	resp = httptest.NewRecorder()
	req, err = http.NewRequest("GET", "/output/sources/meta/globals.js", nil)
	assert.Nil(err)
	index.Sources.ServeHTTP(resp, req)
	assert.Equal(http.StatusOK, resp.Code)
	body, _ := ioutil.ReadAll(resp.Result().Body)
	assert.Regexp("\"output\"", string(body))
	assert.Regexp("\"test\"", string(body))
	assert.Regexp("\"[^\"]*../.*\\.html\"", string(body))

	resp = httptest.NewRecorder()
	req, err = http.NewRequest("GET", "/output/sources/meta/globals.json", nil)
	assert.Nil(err)
	index.Sources.ServeHTTP(resp, req)
	assert.Equal(http.StatusOK, resp.Code)
	body, _ = ioutil.ReadAll(resp.Result().Body)
	assert.Regexp("\"output\"", string(body))
	assert.Regexp("\"test\"", string(body))
	assert.Regexp("\"[^\"]*../.*\\.html\"", string(body))

	root := strings.TrimPrefix(index.Sources.tags["output"].Root, "../")
	root = strings.TrimSuffix(root, path.Ext(root))
	for _, url := range []string{"meta/about", "meta/about.html", "meta/help", "meta/help.html", "meta/help.jhtml", root, root + ".html", root + ".jhtml"} {
		url = "/output/sources/" + url
		t.Logf("TESTING URL: %s", url)

		resp = httptest.NewRecorder()
		req, err = http.NewRequest("GET", url, nil)
		assert.Nil(err)
		index.Sources.ServeHTTP(resp, req)
		assert.Equal(http.StatusOK, resp.Code)
		body, _ = ioutil.ReadAll(resp.Result().Body)

		IsValidPage(assert, string(body))
		assert.Regexp("<title>[^<]*output[^<]*</title>", string(body))
		assert.Regexp("<title>[^<]*test[^<]*</title>", string(body))
	}
}
