

#----------------------------------------------------------------------
# DEFAULT BROWSER CONFIGURATION:
#
#     This should be edited for site-specific browsers. On SUSE linux
#

lappend BROWSERS Firefox {/usr/lib/firefox/firefox-bin}
lappend BROWSERS Hv3     {./hwish ../htmlwidget/hv/hv3_main.tcl}
lappend BROWSERS Opera   {opera -nosession}

set DEFAULT_BROWSERS [list Hv3 Firefox]

#----------------------------------------------------------------------
# TEST ARCHITECTURE OVERVIEW:
#
# This program is a driver for a browser compatibility test framework. 
# In this framework, each test is specified as follows:
#
#     A) A single HTML document. The document must contain the 
#        following element in the head section:
#
#           <SCRIPT src="/get_framework"></SCRIPT>
#
#        The document should have no other external dependancies. The
#        <BODY> element should not have an onLoad event handler defined.
#
#     B) The definition of a javascript function called "browser_test".
#        For example:
#
#           function browser_test () { return document.images.length }
#
# 
# A test case is executed as follows:
#
#     1. A web-browser process is started. The browser connects to an
#        HTTP server embedded in the test driver (this script) and
#        retrieves the test document (A).
#
#     2. The <SCRIPT> tag in the test document causes the browser
#        to retrieve a javascript program from the same embedded 
#        server. The browser_test() function is part of the 
#        javascript program.
#
#     3. An "onLoad" event on the <BODY> of the test document in the
#        browser causes it to execute the browser_test() function.
#        The return value of browser_test() is converted to a string
#        and an HTTP GET request made to a URI of the form:
#
#            /test_result?result=<Result of browser_test()>
#
#     4. Once the above request is seen by the embedded web-server,
#        the web-browser process is halted.
#
# Tcl proc [::browsertest::run] implements this procedure.
#
# The above 4 steps should be repeated with 2 or more browsers. The
# strings returned by the browser_test() function are compared to
# determine browser compatibility. Tcl proc [::browser::do_test] 
# implements this in terms of [::browsertest::run].
#


#----------------------------------------------------------------------
# INTERFACE:
#
# ::browsertest::run BROWSER TIMEOUT DOCUMENT FUNCTION
#
#     The low level interface. Execute a single test-case in a single
#     browser instance.
#
#
#
# ::browsertest::do_test NAME OPTIONS
#
#         -browsers     BROWSER-LIST           (default is all configured)
#         -html         HTML-DOCUMENT-BODY     (default is "")
#         -timeout      TIMEOUT                (default is 10000)
#         -expected     STRING                 (if not specified do not use)
#         -javascript   SCRIPT-FUNCTION-BODY   (mandatory)
#
#     High level interface.
#

namespace eval browsertest {

  variable listen_socket ""   ;# Socket returned by [socket -server]
  variable listen_port   ""   ;# Port number $listen_socket is listening on. 
  variable test_document ""   ;# Document for a "GET /get_test" request.
  variable test_script   ""   ;# Document for a "GET /get_script" request.
  variable test_result   ""   ;# Value returned by browser_test()

  # If the following variable is not set to an empty string, then
  # it is a [string match] style pattern applied to the name of each
  # test before it is executed. If the test-name does not match
  # the pattern, the test will not be executed.
  #
  # Note that this applies to invocations of [::browsertest::do_test] 
  # only, [::browsertest::run] will still run anything passed to it.
  #
  variable pattern       ""

  proc Init {} {
    variable listen_port 
    variable listen_socket

    if {$listen_socket eq ""} {
      set cmd [namespace code Accept]
      set listen_socket [socket -server $cmd -myaddr 127.0.0.1 0]
      set listen_port [lindex [fconfigure $listen_socket -sockname] 2]
    }
  }

  proc Accept {sock host path} {
    fconfigure $sock -blocking 0
    fileevent $sock readable [namespace code [list Request $sock]]
  }

  proc HttpResponse {sock content_type content} {
    set r ""
  
    append r "HTTP/1.0 200 OK\n"
    append r "Content-type: $content_type\n"
    append r "\n"
    append r "$content"
    append r "\n"

    puts -nonewline $sock $r
    close $sock
  }

  proc Decode {component} {
    set zIn $component
    set zOut ""

    while {[regexp {^([^%]*)(%..)(.*)$} $zIn -> start esc tail]} {
      append zOut $start
      set zIn $tail
      set hex "0x[string range $esc 1 end]"
      append zOut [format %c $hex]
    }
    append zOut $zIn

    return $zOut
  }

  proc Request {sock} {
    variable test_document 
    variable test_script 
    variable test_result
  
    set line [gets $sock]
    if {[fblocked $sock]} return
    if {[eof $sock]} {
      close $sock
      return
    }
  
    if {[regexp {^GET.*get_test} $line]} {
      HttpResponse $sock text/html $test_document
    } elseif {[regexp {^GET.*get_framework} $line]} {
      HttpResponse $sock text/javascript $test_script
    } elseif {[regexp {^GET.*test_result.result=([^ ]*)} $line -> result]} {
      set test_result [Decode $result]
      close $sock
    } elseif {[regexp {^GET.*} $line]} {
      close $sock
    }
  }

  # run --
  #
  #     run BROWSER TIMEOUT DOCUMENT FUNCTION
  #
  proc run {browser timeout document function} {
    variable listen_port 
    variable test_document 
    variable test_script 
    variable test_result
  
    # Set up the listening socket (if it is not already ready)
    Init
  
    # Set the global variable $test_document. This is the content that
    # will be returned to a request on the /get_test URI.
    #
    set test_document $document
  
    # Set up the script infrastructure:
    set    test_script $function
    append test_script "\n"
    append test_script {
      function run_browser_test() {
        result = browser_test().toString()
        enc_result = encodeURIComponent(result);
        req = new XMLHttpRequest()
        req.open("GET", "/test_result?result=" + enc_result)
        req.send("")
      }
      window.onload = run_browser_test
    }
  
    # If the specified browser is not in the global $::BROWSERS array,
    # raise a Tcl exception.
    #
    array set b $::BROWSERS
    if {![info exists b($browser)]} {
      error "No such configured browser: $browser"
    }
  
    # [exec] the browser. Load the /get_test URI initially.
    #
    set doc_uri "http://127.0.0.1:$listen_port/get_test"
    set pid [eval exec $b($browser) [list $doc_uri] &]

    set timeout_msg "BROWSER TIMEOUT ($timeout ms)"
    after $timeout [list set [namespace current]::test_result $timeout_msg]
  
    set test_result ""
    vwait [namespace current]::test_result
  
    # [kill] the browser process.
    #
    exec kill $pid
  
    return $test_result
  }

  proc do_test {name args} {
    variable pattern
    if {$pattern ne "" && ![string match $pattern $name]} return

    # Argument processing:
    #
    set opts(-browsers) $::DEFAULT_BROWSERS
    set opts(-timeout)  10000
    set opts(-html)     ""
    array set opts $args
    if {![info exists opts(-javascript)]} {
      error "Missing mandatory -javascript option"
    }
    foreach option [array names opts] {
      switch -- $option {
        -browsers     {}
        -timeout      {}
        -html         {}
        -javascript   {}
        -expected     {set results(Expected) $opts(-expected)}
        default {
          error "Unknown option: $option"
        }
      }
    }

    puts -nonewline "$name ." 
    flush stdout

    # Figure out the complete HTML test document
    #
    set html {<HTML><HEAD><SCRIPT src="/get_framework"></SCRIPT></HEAD>}
    append html $opts(-html)

    # Figure out the complete javascript test function
    #
    set    javascript "function browser_test () {\n"
    append javascript $opts(-javascript)
    append javascript "\n}\n"

    foreach browser $opts(-browsers) {
      set res [run $browser $opts(-timeout) $html $javascript]
      set results($browser) $res
      puts -nonewline "."
      flush stdout
    }

    set ok 1
    foreach browser [array names results] {
      if {$results($browser) ne $res} {set ok 0}
    }

    if {$ok} {
      puts " Ok ($opts(-browsers))"
    } else {
      puts " Error:"
      foreach browser [lsort [array names results]] {
        puts [format {  %-10s {%s}} ${browser}: $results($browser)]
      }
    }
   
  }
}

proc usage {} {
  puts stderr "Usage: "
  puts stderr "  $::argv0 ?PATTERN?"
  exit
}

proc main {args} {
  if {[llength $args] > 1} usage
  set ::browsertest::pattern [lindex $args 0]
  source [file join [file dirname [info script]] tree1.bt]
}

eval main $argv
