
if {[llength $argv]==0} {
puts stderr [subst -nocommands {
  This script is used to generate an html/javascript application to make
  running through the CSS 2.1 test suite less painful. It can also be
  used with other (similarly designed) test suites and other browsers.

  To use, cd into the directory containing the test files and run this
  script with each test file as an argument. For example, if using the
  CSS2.1 test suite:

    $argv0 t*.htm

  (since all test files in that particular test suite match the pattern 
  t*.htm). This generates 3 files: hv3_csstester.html, 
  hv3_csstester_mainframe.html and hv3_csstester_testlist.html. Then
  load hv3_csstester.html into the browser and you're away.

  This has been tested with Hv3, Opera 9 and Firefox 3.
}]
exit 0
}

set TemplateOne {
<HTML>
  <FRAMESET cols="75%,25%">
    <FRAME name="main" src="hv3_csstester_mainframe.html" width></FRAME>
    <FRAME name="list" src="hv3_csstester_testlist.html"></FRAME>
  </FRAMESET>
</HTML>
}

set TemplateTwo {
<HTML>
  <HEAD>
    <STYLE>
      #container { margin: 1cm; }
      #testframe_border { border: solid black 5px }
      #testframe_border IFRAME { width: 100%; height:400px; }
      #passed,#failed { margin: 5mm; }

    </STYLE>
    <SCRIPT>

      function loadtest(n, uri) {
        var tf = frames.testframe
        if( n==0 ){
          document.getElementById("testname").firstChild.data = " "
          tf.location = "hv3_csstester_report.html"
        } else {
          document.getElementById("testname").firstChild.data = uri+" ("+n+")"
          tf.location = uri
        }
      }

      function failed() { top.frames.list.failed() }
      function passed() { top.frames.list.passed() }

    </SCRIPT>
  <BODY>
    <DIV style="float:right">
      <INPUT type="button" value="Generate Report" onclick="loadtest(0,0)">
    </DIV>
    <H1>Test Case: <SPAN id="testname"> </H1>
    <DIV id="container">
      <DIV id="testframe_border">
        <IFRAME name="testframe">
        </IFRAME>
      </DIV>
      <INPUT type="button" id="failed" value="Failed" onclick="failed()">
      <INPUT type="button" id="passed" value="Passed" onclick="passed()">
    </DIV>

  </BODY>
</HTML>
}

set TemplateThree {
<HTML>
  <HEAD>
    <SCRIPT>

      current_testnumber = 0
      results = new Object();

      function loadtest(n) {
        if( current_testnumber>0 ){
          var b = document.getElementById("t" + current_testnumber).firstChild
          b.style.backgroundColor = "transparent"
        }
        var uri = null
        var a =  document.getElementById("t" + n)
        if( !a ){
          n = 0
        }else{
          uri = a.firstChild.firstChild.data
          a.firstChild.style.backgroundColor = "wheat"
        }
        top.frames.main.loadtest(n, uri)
        current_testnumber = n
      }

      function failed() {
        if( current_testnumber!=0 ){
          var b = document.getElementById("t" + current_testnumber)
          b.parentNode.firstChild.style.backgroundColor = "red"
          results[current_testnumber]= 0
        }
        loadtest(current_testnumber + 1)
      }

      function passed() {
        if( current_testnumber!=0 ){
          var b = document.getElementById("t" + current_testnumber)
          b.parentNode.firstChild.style.backgroundColor = "green"
          results[current_testnumber]= 1
        }
        loadtest(current_testnumber + 1)
      }

      function report() {
        var nSuccess = 0
        var nTotal = 0
        var details = ""
        for (var p in results) {
          nTotal = nTotal + 1
          nSuccess = nSuccess + results[p]
          if( results[p]==0 ){
            var a = document.getElementById("t" + p)
            details += "* " + a.firstChild.firstChild.data + "\\n"
          }
        }
        var ret = nSuccess + "/" + nTotal + " tests passed\\n\\n"
        ret    += "The following tests failed:\\n" + details
        return ret
      }

    </SCRIPT>
    <STYLE>
      a { color: darkblue; text-decoration: underline ; cursor: pointer }
    </STYLE>
  </HEAD>
  <BODY>
    <TABLE width=100%>$table_list_content</TABLE>
  </BODY>
</HTML>
}

set TemplateFour {
  <HTML>
    <BODY>
      <PRE><SCRIPT>document.write(top.frames.list.report())</SCRIPT></PRE>
    </BODY>
  </HTML>
}

proc write_file {fname content} {
  set fd [open $fname w]
  puts -nonewline $fd $content
  close $fd
}

# Build up table_list_content:
set table_list_content {}
set d 1
foreach a $argv {
  append table_list_content "<TR><TD>$d. <TD id=\"t$d\">"
  append table_list_content "<A onclick=\"loadtest($d)\">$a"
  incr d
}

write_file hv3_csstester.html $TemplateOne
write_file hv3_csstester_mainframe.html $TemplateTwo
write_file hv3_csstester_testlist.html [subst -nocommands $TemplateThree]
write_file hv3_csstester_report.html $TemplateFour  
