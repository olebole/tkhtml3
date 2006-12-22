#
# Contains the following:
#
#     ::hv3::dom::pretty_print_script
#         Proc to reformat javascript code for readability.
#
#     ::hv3::dom::get_inner_html
#         Retrieve the "innerHTML" value of a node.
#

namespace eval ::hv3::dom {

  #-------------------------------------------------------------------------
  # ::hv3::dom::Char
  #     Helper function for ::hv3::dom::pretty_print_script.
  #
  #     Return the $idx'th character of string $str
  #
  proc Char {str idx} {
    string range $str $idx $idx
  }
  
  #-------------------------------------------------------------------------
  # pretty_print_script
  #
  #         pretty_print_script JAVASCRIPT-CODE
  #
  #     Insert and delete white-space from the supplied block of javascript
  #     code to try to make it more legible.
  #
  #     This needs a lot of work...
  #
  proc pretty_print_script {javascript} {
  
    set zIn $javascript
    set zOut ""
  
    set iBracket 0     ;# Number of open ( ... ) brackets
  
    set INDENT 2
    set iIndent 0
  
    # Takes three values:
    #     2 -> Ignore both space and newline
    #     1 -> Ignore space (but not newline)
    #     0 -> Do not ignore anything
    set eNoSpace 0
  
    for {set ii 0} {$ii < [string length $zIn]} {incr ii} {
      set c [Char $zIn $ii]
  
      switch -- $c {
        "(" { incr iBracket }
        ")" { incr iBracket -1 }
        "{" { 
            if {$iBracket == 0} {
                incr iIndent $INDENT
                append c "\n"
                append c [string repeat " " $iIndent]
                set eNoSpace 2
            }
        }
        "}" { 
            if {$iBracket == 0} {
                incr iIndent [expr {$INDENT * -1}]
                set d ""
                if {$eNoSpace == 0} {
                  append d "\n[string repeat " " $iIndent]"
                }
                append d "${c}\n[string repeat " " $iIndent]"
                set c $d
                set eNoSpace 2
            }
        }
        ";" { 
            if {$iBracket == 0} {
                append c "\n"
                append c [string repeat " " $iIndent]
                set eNoSpace 2
            }
        }
  
  
        "/" {
            # This may be the start of a comment block. Copy verbatim.
            set d [Char $zIn [expr $ii + 1]]
            set iEnd -1
  
            # C++ style comment
            if {$d eq "/"} {
              set iEnd [string first */ $zIn $ii]
            }
  
            # C style comment
            if {$d eq "*"} {
              set iEnd [string first "\n" $zIn $ii]
            }
  
            if {$iEnd < 0} {set iEnd $ii}
            set c [string range $zIn $ii $iEnd]
            set ii $iEnd
  
          set eNoSpace 0
        }
  
        "\"" { 
          # String literal. Copy verbatim.
          for {set jj [expr {$ii+1}]} {$jj < [string length $zIn]} {incr jj} {
            set d [Char $zIn $jj]
            if {$d eq $c} break
            if {$d eq "\\"} {incr jj}
          }
          set c [string range $zIn $ii $jj]
          set ii $jj
  
          set eNoSpace 0
        }
        "'" { 
          # String literal. Copy verbatim.
          for {set jj [expr {$ii+1}]} {$jj < [string length $zIn]} {incr jj} {
            set d [Char $zIn $jj]
            if {$d eq $c} break
            if {$d eq "\\"} {incr jj}
          }
          set c [string range $zIn $ii $jj]
          set ii $jj
  
          set eNoSpace 0
        }
  
        " " {
          if {$eNoSpace} {set c ""}
        }
        "\n" {
          if {$eNoSpace == 2} {
            set eNoSpace 1
            set c ""
          }
        }
  
        default {
          set eNoSpace 0
        }
      }
  
      append zOut $c
    }
  
    return $zOut
  }

  proc NodeToHtml {node} {
    set tag [$node tag]
    if {$tag eq ""} {
      return [$node text -pre]
    } else {
      set inner [get_inner_html $node]
      return "<$tag>$node</$tag>"
    }
  }

  proc get_inner_html {node} {
    if {[$node tag] eq ""} {error "$node is not an HTMLElement"}

    set ret ""
    foreach child [$node children] {
      append ret [NodeToHtml $node]
    }
    return $ret
  }

  proc set_inner_html {hv3 node newHtml} {
    if {[$node tag] eq ""} {error "$node is not an HTMLElement"}

    set children [$node children]
    $node remove $children
    foreach child $children {
      $child destroy
    }

    set children [[$hv3 html] fragment $newHtml]
    $node insert $children
  }
}

