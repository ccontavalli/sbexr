package db

import (
	"bytes"
	"testing"
	"github.com/stretchr/testify/assert"
)

/*
func TestOutputAscii(t *testing.T) {
	for i := 0; i <= 127; i++ {
		fmt.Printf("0x%02x, ", int(bytes.ToLower([]byte{byte(i)})[0]))
	}
	for i := 128; i <= 255; i++ {
		fmt.Printf("0x%02x, ", i)
	}
}*/

func TestCaseContains(t *testing.T) {
	assert := assert.New(t)

	assert.False(CaseContains([]byte(""), []byte("foo")))
	assert.False(CaseContains([]byte("f"), []byte("foo")))
	assert.False(CaseContains([]byte("fo"), []byte("foo")))
	assert.True(CaseContains([]byte("foo"), []byte("foo")))
	assert.True(CaseContains([]byte(" foo"), []byte("foo")))
	assert.True(CaseContains([]byte("foo "), []byte("foo")))
	assert.True(CaseContains([]byte(" foo "), []byte("foo")))
	assert.False(CaseContains([]byte(" fofo "), []byte("foo")))
	assert.True(CaseContains([]byte(" fofoo "), []byte("foo")))

	assert.True(CaseContains([]byte("foobar"), []byte("foo")))
	assert.False(CaseContains([]byte("foobar"), []byte("blah")))
	assert.True(CaseContains([]byte("foobar"), []byte("bar")))
	assert.True(CaseContains([]byte("bababarba"), []byte("bar")))
}

var sentences [][]byte = [][]byte{
	[]byte("Daric mohock unautomatic palpitant diptych. Order divisiveness shanghaiing monochromatism mongo. Alberti bolyai flintiness ronco eternalized."),
	[]byte("Personating specialising inigo emasculator nitrogenise. Traceries periodicalist frustrate tye palaearctic. Culch penetrometer ballpark biformity unloosed."),
	[]byte("Sigmate hemagogue hardwall flatulently interparliamentary. Impetuosity defiling gownsman malpractice saunciest. Overexuberance maundy tunic ploce topsail."),
	[]byte("Francisco instigatingly unveridical convenances crabeater. Unextruded petrolatum repellingly unspelled denature. Recarrying unbinned asphaltic microfarad subadministrative."),
	[]byte("Alkalinization rsa lea infamousness venturer. Liberalised parchisi dividable covertly unhedged. Robot unsectionalized riparian rooty asset."),
}
var words [][]byte = [][]byte{
	[]byte(""),
	[]byte("z"),
	[]byte("be"),
	[]byte("is"),
	[]byte("iod"),
	[]byte("pet"),
	[]byte("daric"),
	[]byte("asphalt"),
	[]byte("topsail."),
	[]byte("frustrate"),
	[]byte("unextruded"),
}

func BenchmarkContains(b *testing.B) {
	for n := 0; n < b.N; n++ {
		for _, word := range words {
			for _, sentence := range sentences {
				bytes.Contains(sentence, word)
			}
		}
	}
}

func BenchmarkCaseContains(b *testing.B) {
	for n := 0; n < b.N; n++ {
		for _, word := range words {
			for _, sentence := range sentences {
				// bytes.Contains(sentence, word)
				CaseContains(sentence, word)
			}
		}
	}
}
func BenchmarkToLowerContains(b *testing.B) {
	for n := 0; n < b.N; n++ {
		for _, word := range words {
			for _, sentence := range sentences {
				sentence = bytes.ToLower(sentence)
				bytes.Contains(sentence, word)
			}
		}
	}
}
