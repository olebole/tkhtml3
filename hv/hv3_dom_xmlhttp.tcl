namespace eval hv3 { set {version($Id: hv3_dom_xmlhttp.tcl,v 1.4 2007/06/01 18:07:48 danielk1977 Exp $)} 1 }

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
if 0 {
# TODO: Change to ::stateless
#
::hv3::dom::type XMLHttpRequest {} {
  dom_snit {
    option -hv3 -default ""

    variable myUri ""
    variable myRequestHandle ""

    variable myReadyState Uninitialized


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

  }

  dom_get readyState {
    switch -- $myReadyState {
      Uninitialized {list number 0}
      Open          {list number 1}
      Sent          {list number 2}
      Receiving     {list number 3}
      Loaded        {list number 4}
      default       {error "Bad myReadyState value: $myReadyState"}
    }
  }

  dom_call -string open {THIS http_method uri args} {
    if {$myReadyState ne "Uninitialized"} {
      error "Cannot call XMLHttpRequest.open() in state $myReadyState"
    }
    set myUri [$options(-hv3) resolve_uri $uri]
    set myReadyState Open
    return ""
  }

  dom_call -string send {THIS args} {
    if {$myReadyState ne "Open"} {
      error "Cannot call XMLHttpRequest.open() in state $myReadyState"
    }

    set myRequestHandle [::hv3::download %AUTO%] 
    $myRequestHandle configure -uri $myUri
    $myRequestHandle configure -finscript [mymethod RequestFinished]
    $options(-hv3) makerequest $myRequestHandle

    set myReadyState Sent
    return ""
  }
}
}
#-------------------------------------------------------------------------

