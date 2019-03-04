// This file is automatically generated by qtc from "help.qtpl".
// See https://github.com/valyala/quicktemplate for details.

//line help.qtpl:1
package templates

//line help.qtpl:1
import (
	qtio422016 "io"

	qt422016 "github.com/valyala/quicktemplate"
)

//line help.qtpl:1
var (
	_ = qtio422016.Copy
	_ = qt422016.AcquireByteBuffer
)

//line help.qtpl:2
type HelpPage struct {
	BasePage
}

//line help.qtpl:7
func (page *HelpPage) StreamGetBody(qw422016 *qt422016.Writer) {
	//line help.qtpl:7
	qw422016.N().S(`
<div class="container-fluid" id="sbexr-help-data">

<h4>KEY Bindings</h4>
  <dl class="dl-horizontal">
     <dt>ESC</dt><dd>CLOSE this help screen</dd>
     <dt>?</dt><dd>Toggle the help screen</dd>
     <dt>a</dt><dd>Toggle the about page</dd>
  </dl>

  <dl class="dl-horizontal">
     <dt>j</dt><dd>Scroll down a few lines</dd>
     <dt>k</dt><dd>Scroll up a few lines</dd>
     <dt>g</dt><dd>Scroll to the top of the file</dd>
     <dt>G</dt><dd>Scroll to the bottom of the file</dd>
  </dl>

  <dl class="dl-horizontal">
     <dt>:</dt><dd>Go to the specified line number<br />(press :, type number, enter)</dd>
     <dt>f</dt><dd>Go to the specified file<br />(press f, autocomplete, enter)</dd>
     <dt>s</dt><dd>Find the specified symbol<br />(press s, autocomplete, enter)</dd>
     <dt>t</dt><dd>Find the specified text<br />(press t, write terms, enter)</dd>
  </dl>

<h4>With your mouse</h4>
  <dl class="dl-horizontal">
   <dt>click</dt><dd>on objects or links to go to their best definition</dd>
   <dt>drag</dt><dd>on objects or links to open a context menu</dd>
  </dl>

</div>
`)
//line help.qtpl:38
}

//line help.qtpl:38
func (page *HelpPage) WriteGetBody(qq422016 qtio422016.Writer) {
	//line help.qtpl:38
	qw422016 := qt422016.AcquireWriter(qq422016)
	//line help.qtpl:38
	page.StreamGetBody(qw422016)
	//line help.qtpl:38
	qt422016.ReleaseWriter(qw422016)
//line help.qtpl:38
}

//line help.qtpl:38
func (page *HelpPage) GetBody() string {
	//line help.qtpl:38
	qb422016 := qt422016.AcquireByteBuffer()
	//line help.qtpl:38
	page.WriteGetBody(qb422016)
	//line help.qtpl:38
	qs422016 := string(qb422016.B)
	//line help.qtpl:38
	qt422016.ReleaseByteBuffer(qb422016)
	//line help.qtpl:38
	return qs422016
//line help.qtpl:38
}
