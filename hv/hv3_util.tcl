namespace eval hv3 { set {version($Id: hv3_util.tcl,v 1.2 2008/01/06 08:45:28 danielk1977 Exp $)} 1 }


namespace eval hv3 {
  proc scrollbar {args} {
    set w [eval [linsert $args 0 ::scrollbar]]
    $w configure -highlightthickness 0
    $w configure -borderwidth 1
    return $w
  }

  # scrolledwidget
  #
  #     Widget to add automatic scrollbars to a widget supporting the
  #     [xview], [yview], -xscrollcommand and -yscrollcommand interface (e.g.
  #     html, canvas or text).
  #
  ::snit::widget scrolledwidget {
    component myWidget
    variable  myVsb
    variable  myHsb
  
    option -propagate -default 0 -configuremethod set_propagate
    option -scrollbarpolicy -default auto -configuremethod set_policy
    option -takefocus -default 0
  
    method set_propagate {option value} {
      grid propagate $win $value
      set options(-propagate) $value
    }
  
    variable myTakeControlCb ""
    method take_control {callback} {
      if {$myTakeControlCb ne ""} {
        uplevel #0 $myTakeControlCb
      }
      set myTakeControlCb $callback
    }
  
    proc scrollme {var args} {
      if {[set $var] ne ""} {
        uplevel #0 [set $var]
        set $var ""
      }
      eval $args
    }
  
    constructor {widget args} {
      # Create the three widgets - one user widget and two scrollbars.
      set myWidget [eval [linsert $widget 1 ${win}.widget]]
  
      set v [myvar myTakeControlCb]
      set w $myWidget
      set scrollme [myproc scrollme]
      bind $w <KeyPress-Up>     [list $scrollme $v $w yview scroll -1 units]
      bind $w <KeyPress-Down>   [list $scrollme $v $w yview scroll  1 units]
      bind $w <KeyPress-Return> [list $scrollme $v $w yview scroll  1 units]
      bind $w <KeyPress-Right>  [list $scrollme $v $w xview scroll  1 units]
      bind $w <KeyPress-Left>   [list $scrollme $v $w xview scroll -1 units]
      bind $w <KeyPress-Next>   [list $scrollme $v $w yview scroll  1 pages]
      bind $w <KeyPress-space>  [list $scrollme $v $w yview scroll  1 pages]
      bind $w <KeyPress-Prior>  [list $scrollme $v $w yview scroll -1 pages]
  
      set myVsb [::hv3::scrollbar ${win}.vsb -orient vertical -takefocus 0] 
      set myHsb [::hv3::scrollbar ${win}.hsb -orient horizontal -takefocus 0]
  
      $myVsb configure -cursor "top_left_arrow"
      $myHsb configure -cursor "top_left_arrow"
  
      grid configure $myWidget -column 0 -row 0 -sticky nsew
      grid columnconfigure $win 0 -weight 1
      grid rowconfigure    $win 0 -weight 1
      grid propagate       $win $options(-propagate)
  
      # First, set the values of -width and -height to the defaults for 
      # the scrolled widget class. Then configure this widget with the
      # arguments provided.
      $self configure -width  [$myWidget cget -width] 
      $self configure -height [$myWidget cget -height]
      $self configurelist $args
  
      # Wire up the scrollbars using the standard Tk idiom.
      $myWidget configure -yscrollcommand [list $self scrollcallback $myVsb]
      $myWidget configure -xscrollcommand [list $self scrollcallback $myHsb]
      set v [myvar myTakeControlCb]
      $myVsb configure -command [list [myproc scrollme] $v $myWidget yview]
      $myHsb configure -command [list [myproc scrollme] $v $myWidget xview]
  
      # Propagate events from the scrolled widget to this one.
      bindtags $myWidget [concat [bindtags $myWidget] $win]
    }
  
    method scrollcallback {scrollbar first last} {

      $scrollbar set $first $last
      set ismapped   [expr [winfo ismapped $scrollbar] ? 1 : 0]
  
      if {$options(-scrollbarpolicy) eq "auto"} {
        set isrequired [expr ($first == 0.0 && $last == 1.0) ? 0 : 1]
      } else {
        set isrequired $options(-scrollbarpolicy)
      }
  
      if {$isrequired && !$ismapped} {
        switch [$scrollbar cget -orient] {
          vertical   {grid configure $scrollbar  -column 1 -row 0 -sticky ns}
          horizontal {grid configure $scrollbar  -column 0 -row 1 -sticky ew}
        }
      } elseif {$ismapped && !$isrequired} {
        grid forget $scrollbar
      }
    }

    method set_policy {option value} {
      if {$value ne $options($option)} {
        set options($option) $value
        eval $self scrollcallback $myHsb [$myWidget xview]
        eval $self scrollcallback $myVsb [$myWidget yview]
      }
    }
  
    method widget {} {return $myWidget}
  
    delegate option -width     to hull
    delegate option -height    to hull
    delegate option *          to myWidget
    delegate method *          to myWidget
  }
  
  # Wrapper around the ::hv3::scrolledwidget constructor. 
  #
  # Example usage to create a 400x400 canvas widget named ".c" with 
  # automatic scrollbars:
  #
  #     ::hv3::scrolled canvas .c -width 400 -height 400
  #
  proc scrolled {widget name args} {
    return [eval [concat ::hv3::scrolledwidget $name $widget $args]]
  }
}

namespace eval ::hv3::string {

  # A generic tokeniser procedure for strings. This proc splits the
  # input string $input into a list of tokens, where each token is either:
  #
  #     * A continuous set of alpha-numeric characters, or
  #     * A quoted string (quoted by " or '), or
  #     * Any single character.
  #
  # White-space characters are not returned in the list of tokens.
  #
  proc tokenise {input} {
    set tokens [list]
    set zIn [string trim $input]
  
    while {[string length $zIn] > 0} {
  
      if {[ regexp {^([[:alnum:]_.-]+)(.*)$} $zIn -> zToken zIn ]} {
        # Contiguous alpha-numeric characters
        lappend tokens $zToken
  
      } elseif {[ regexp {^(["'])} $zIn -> zQuote]} {      #;'"
        # Quoted string
  
        set nEsc 0
        for {set nToken 1} {$nToken < [string length $zIn]} {incr nToken} {
          set c [string range $zIn $nToken $nToken]
          if {$c eq $zQuote && 0 == ($nEsc%2)} break
          set nEsc [expr {($c eq "\\") ? $nEsc+1 : 0}]
        }
        set zToken [string range $zIn 0 $nToken]
        set zIn [string range $zIn [expr {$nToken+1}] end]
  
        lappend tokens $zToken
  
      } else {
        lappend tokens [string range $zIn 0 0]
        set zIn [string range $zIn 1 end]
      }
  
      set zIn [string trimleft $zIn]
    }
  
    return $tokens
  }

  # Dequote $input, if it appears to be a quoted string (starts with 
  # a single or double quote character).
  #
  proc dequote {input} {
    set zIn $input
    set zQuote [string range $zIn 0 0]
    if {$zQuote eq "\"" || $zQuote eq "\'"} {
      set zIn [string range $zIn 1 end]
      if {[string range $zIn end end] eq $zQuote} {
        set zIn [string range $zIn 0 end-1]
      }
      set zIn [regsub {\\(.)} $zIn {\1}]
    }
    return $zIn
  }


  # A procedure to parse an HTTP content-type (media type). See section
  # 3.7 of the http 1.1 specification.
  #
  # A list of exactly three elements is returned. These are the type,
  # subtype and charset as specified in the parsed content-type. Any or
  # all of the fields may be empty strings, if they are not present in
  # the input or a parse error occurs.
  #
  proc parseContentType {contenttype} {
    set tokens [::hv3::string::tokenise $contenttype]

    set type [lindex $tokens 0]
    set subtype [lindex $tokens 2]

    set enc ""
    foreach idx [lsearch -regexp -all $tokens (?i)charset] {
      if {[lindex $tokens [expr {$idx+1}]] eq "="} {
        set enc [::hv3::string::dequote [lindex $tokens [expr {$idx+2}]]]
        break
      }
    }

    return [list $type $subtype $enc]
  }

  proc htmlize {zIn} {
    string map [list "<" "&lt;" ">" "&gt;" "&" "&amp;" "\"" "&quote;"] $zIn
  }

}


proc ::hv3::char {text idx} {
  return [string range $text $idx $idx]
}

proc ::hv3::next_word {text idx idx_out} {

  while {[char $text $idx] eq " "} { incr idx }

  set idx2 $idx
  set c [char $text $idx2] 

  if {$c eq "\""} {
    # Quoted identifier
    incr idx2
    set c [char $text $idx2] 
    while {$c ne "\"" && $c ne ""} {
      incr idx2
      set c [char $text $idx2] 
    }
    incr idx2
    set word [string range $text [expr $idx+1] [expr $idx2 - 2]]
  } else {
    # Unquoted identifier
    while {$c ne ">" && $c ne " " && $c ne ""} {
      incr idx2
      set c [char $text $idx2] 
    }
    set word [string range $text $idx [expr $idx2 - 1]]
  }

  uplevel [list set $idx_out $idx2]
  return $word
}

proc ::hv3::sniff_doctype {text pIsXhtml} {
  upvar $pIsXhtml isXHTML
  # <!DOCTYPE TopElement Availability "IDENTIFIER" "URL">

  set QuirksmodeIdentifiers [list \
    "-//w3c//dtd html 4.01 transitional//en" \
    "-//w3c//dtd html 4.01 frameset//en"     \
    "-//w3c//dtd html 4.0 transitional//en" \
    "-//w3c//dtd html 4.0 frameset//en" \
    "-//softquad software//dtd hotmetal pro 6.0::19990601::extensions to html 4.0//en" \
    "-//softquad//dtd hotmetal pro 4.0::19971010::extensions to html 4.0//en" \
    "-//ietf//dtd html//en//3.0" \
    "-//w3o//dtd w3 html 3.0//en//" \
    "-//w3o//dtd w3 html 3.0//en" \
    "-//w3c//dtd html 3 1995-03-24//en" \
    "-//ietf//dtd html 3.0//en" \
    "-//ietf//dtd html 3.0//en//" \
    "-//ietf//dtd html 3//en" \
    "-//ietf//dtd html level 3//en" \
    "-//ietf//dtd html level 3//en//3.0" \
    "-//ietf//dtd html 3.2//en" \
    "-//as//dtd html 3.0 aswedit + extensions//en" \
    "-//advasoft ltd//dtd html 3.0 aswedit + extensions//en" \
    "-//ietf//dtd html strict//en//3.0" \
    "-//w3o//dtd w3 html strict 3.0//en//" \
    "-//ietf//dtd html strict level 3//en" \
    "-//ietf//dtd html strict level 3//en//3.0" \
    "html" \
    "-//ietf//dtd html//en" \
    "-//ietf//dtd html//en//2.0" \
    "-//ietf//dtd html 2.0//en" \
    "-//ietf//dtd html level 2//en" \
    "-//ietf//dtd html level 2//en//2.0" \
    "-//ietf//dtd html 2.0 level 2//en" \
    "-//ietf//dtd html level 1//en" \
    "-//ietf//dtd html level 1//en//2.0" \
    "-//ietf//dtd html 2.0 level 1//en" \
    "-//ietf//dtd html level 0//en" \
    "-//ietf//dtd html level 0//en//2.0" \
    "-//ietf//dtd html strict//en" \
    "-//ietf//dtd html strict//en//2.0" \
    "-//ietf//dtd html strict level 2//en" \
    "-//ietf//dtd html strict level 2//en//2.0" \
    "-//ietf//dtd html 2.0 strict//en" \
    "-//ietf//dtd html 2.0 strict level 2//en" \
    "-//ietf//dtd html strict level 1//en" \
    "-//ietf//dtd html strict level 1//en//2.0" \
    "-//ietf//dtd html 2.0 strict level 1//en" \
    "-//ietf//dtd html strict level 0//en" \
    "-//ietf//dtd html strict level 0//en//2.0" \
    "-//webtechs//dtd mozilla html//en" \
    "-//webtechs//dtd mozilla html 2.0//en" \
    "-//netscape comm. corp.//dtd html//en" \
    "-//netscape comm. corp.//dtd html//en" \
    "-//netscape comm. corp.//dtd strict html//en" \
    "-//microsoft//dtd internet explorer 2.0 html//en" \
    "-//microsoft//dtd internet explorer 2.0 html strict//en" \
    "-//microsoft//dtd internet explorer 2.0 tables//en" \
    "-//microsoft//dtd internet explorer 3.0 html//en" \
    "-//microsoft//dtd internet explorer 3.0 html strict//en" \
    "-//microsoft//dtd internet explorer 3.0 tables//en" \
    "-//sun microsystems corp.//dtd hotjava html//en" \
    "-//sun microsystems corp.//dtd hotjava strict html//en" \
    "-//ietf//dtd html 2.1e//en" \
    "-//o'reilly and associates//dtd html extended 1.0//en" \
    "-//o'reilly and associates//dtd html extended relaxed 1.0//en" \
    "-//o'reilly and associates//dtd html 2.0//en" \
    "-//sq//dtd html 2.0 hotmetal + extensions//en" \
    "-//spyglass//dtd html 2.0 extended//en" \
    "+//silmaril//dtd html pro v0r11 19970101//en" \
    "-//w3c//dtd html experimental 19960712//en" \
    "-//w3c//dtd html 3.2//en" \
    "-//w3c//dtd html 3.2 final//en" \
    "-//w3c//dtd html 3.2 draft//en" \
    "-//w3c//dtd html experimental 970421//en" \
    "-//w3c//dtd html 3.2s draft//en" \
    "-//w3c//dtd w3 html//en" \
    "-//metrius//dtd metrius presentational//en" \
  ]

  set isXHTML 0
  set idx [string first <!DOCTYPE $text]
  if {$idx < 0} { return "quirks" }

  # Try to parse the TopElement bit. No quotes allowed.
  incr idx [string length "<!DOCTYPE "]
  while {[string range $text $idx $idx] eq " "} { incr idx }

  set TopElement   [string tolower [next_word $text $idx idx]]
  set Availability [string tolower [next_word $text $idx idx]]
  set Identifier   [string tolower [next_word $text $idx idx]]
  set Url          [next_word $text $idx idx]

#  foreach ii [list TopElement Availability Identifier Url] {
#    puts "$ii -> [set $ii]"
#  }

  # Figure out if this should be handled as XHTML
  #
  if {[string first xhtml $Identifier] >= 0} {
    set isXHTML 1
  }
  if {$Availability eq "public"} {
    set s [expr [string length $Url] > 0]
    if {
         $Identifier eq "-//w3c//dtd xhtml 1.0 transitional//en" ||
         $Identifier eq "-//w3c//dtd xhtml 1.0 frameset//en" ||
         ($s && $Identifier eq "-//w3c//dtd html 4.01 transitional//en") ||
         ($s && $Identifier eq "-//w3c//dtd html 4.01 frameset//en")
    } {
      return "almost standards"
    }
    if {[lsearch $QuirksmodeIdentifiers $Identifier] >= 0} {
      return "quirks"
    }
  }

  return "standards"
}


proc ::hv3::configure_doctype_mode {html text pIsXhtml} {
  upvar $pIsXhtml isXHTML
  set mode [sniff_doctype $text isXHTML]

  switch -- $mode {
    "quirks"           { set defstyle [::tkhtml::htmlstyle -quirks] }
    "almost standards" { set defstyle [::tkhtml::htmlstyle] }
    "standards"        { set defstyle [::tkhtml::htmlstyle]
    }
  }

  $html configure -defaultstyle $defstyle -mode $mode

  return $mode
}
