namespace eval hv3 { set {version($Id: hv3_dom_xmlhttp.tcl,v 1.2 2007/01/17 10:15:13 danielk1977 Exp $)} 1 }

#-------------------------------------------------------------------------
# ::hv3::dom::XMLHttpRequest
#
#     Hv3 will eventually feature a fully-featured XMLHttpRequest object,
#     similar to that described here:
#     
#         http://www.w3.org/TR/XMLHttpRequest/
#
#     For now, this is a partial implementation to make the
#     tests/browsertest.tcl framework work.
#
namespace eval ::hv3::dom {
  ::snit::type XMLHttpRequest {

    variable myHv3

    variable myUri ""
    variable myRequestHandle ""

    js_init {dom hv3} {
      set myHv3 $hv3
    }

    #-------------------------------------------------------------------
    # XMLHttpRequest.readyState
    #
    variable myReadyState Uninitialized
    js_get readyState {
      switch -- $myReadyState {
        Uninitialized {list number 0}
        Open          {list number 1}
        Sent          {list number 2}
        Receiving     {list number 3}
        Loaded        {list number 4}
        default       {error "Bad myReadyState value: $myReadyState"}
      }
    }

    js_scall open {THIS http_method uri args} {
      if {$myReadyState ne "Uninitialized"} {
        error "Cannot call XMLHttpRequest.open() in state $myReadyState"
      }
      set myUri [$myHv3 resolve_uri $uri]
      set myReadyState Open
      return ""
    }

    js_scall send {THIS args} {
      if {$myReadyState ne "Open"} {
        error "Cannot call XMLHttpRequest.open() in state $myReadyState"
      }

      set myRequestHandle [::hv3::download %AUTO%] 
      $myRequestHandle configure -uri $myUri
      $myRequestHandle configure -finscript [mymethod RequestFinished]
      $myHv3 makerequest $myRequestHandle

      set myReadyState Sent
      return ""
    }

    variable isFinalized 0
    method Finalize {} {
      set isFinalized 1
      if {$myReadyState ne "Sent" && $myReadyState ne "Receiving"} {
        $self destroy
      }
    }
    method RequestFinished {data} {
      if {$isFinalized} {$self destroy}
      $myRequestHandle destroy
    }

    js_finish {
    }
  }
}
#-------------------------------------------------------------------------

