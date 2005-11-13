#
# tkhtml.tcl --
#
#     This file contains:
#
#         - The default bindings for the Html widget, and
#         - Some Tcl functions used by the stylesheet html.css.
#
# ------------------------------------------------------------------------
#
# Copyright (c) 2005 Eolas Technologies Inc.
# All rights reserved.
# 
# This Open Source project was made possible through the financial support
# of Eolas Technologies Inc.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the <ORGANIZATION> nor the names of its
#       contributors may be used to endorse or promote products derived from
#       this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

# Default bindings
#
bind Html <ButtonPress>     { focus %W }
bind Html <KeyPress-Up>     { %W yview scroll -1 units }
bind Html <KeyPress-Down>   { %W yview scroll  1 units }
bind Html <KeyPress-Return> { %W yview scroll  1 units }
bind Html <KeyPress-Right>  { %W xview scroll  1 units }
bind Html <KeyPress-Left>   { %W xview scroll -1 units }
bind Html <KeyPress-Next>   { %W yview scroll  1 pages }
bind Html <KeyPress-space>  { %W yview scroll  1 pages }
bind Html <KeyPress-Prior>  { %W yview scroll -1 pages }
bind Html <ButtonPress-4>   { %W yview scroll -2 units }
bind Html <ButtonPress-5>   { %W yview scroll  2 units }


# Some Tcl procs used by html.css
#
namespace eval tkhtml {
    proc len {val} {
        if {[regexp {^[0-9]+$} $val]} {
            append val px
        }
        return $val
    }

    swproc attr {attr {len 0 1}} {
        upvar N node
        if {[catch {$node attr $attr} val]} {error ""}
        if {$len} {
            set val [len $val]
        }
        return $val
    }

    swproc aa {tag attr {len 0 1} {if NULL}} {
        upvar N node
        for {} {$node != ""} {set node [$node parent]} {
            if {[$node tag] == $tag} {
                if {[catch {$node attr $attr} val]} {error ""}
                if {$len} {
                    set val [len $val]
                }
                if {$if != "NULL"} {
                    set val $if
                }
                return $val
            }
        }
        error "No such ancestor attribute: $tag $attr"
    }
}

