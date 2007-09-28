
namespace eval ::hv3::bookmarks {

  proc noop {args} {}

  proc initialise_database {} {
    set rc [catch {
      ::hv3::sqlitedb eval {

        CREATE TABLE bm_bookmark2(
          bookmarkid INTEGER PRIMARY KEY,
          caption TEXT,
          uri TEXT,
          description TEXT,
          image BLOB
        );

        CREATE TABLE bm_folder2(
          folderid INTEGER PRIMARY KEY,
          name TEXT
        );

        CREATE TABLE bm_tree2(
          linkid     INTEGER PRIMARY KEY,
          folderid   INTEGER,               -- Index into bm_folder2
          objecttype TEXT,                  -- Either "f" or "b"
          objectid   INTEGER,               -- Index into object table

          UNIQUE(objecttype, objectid)
        );

        CREATE INDEX bm_tree2_i1 ON bm_tree2(folderid, objecttype);
      }
    } msg]

    if {$rc == 0} {
      set folderid 0

      ::hv3::sqlitedb transaction {
        foreach B {

      { "tkhtml.tcl.tk"             {http://tkhtml.tcl.tk} }
      { "Tkhtml3 Mailing List"      {http://groups.google.com/group/tkhtml3} }
      { "Hv3 site at freshmeat.net" {http://freshmeat.net/hv3} }

      { "Components Used By Hv3" }
      { "Sqlite" {http://www.sqlite.org} }
      { "Tk Combobox" {http://www.purl.org/net/oakley/tcl/combobox/index.html} }
      { "Polipo (web proxy)" {http://www.pps.jussieu.fr/~jch/software/polipo/} }
      { "SEE (javascript engine)" {http://www.adaptive-enterprises.com.au/~d/software/see/} }
      { "Icons used in Hv3" {http://e-lusion.com/design/greyscale} }

      { "Tcl Sites" }
      { "Tcl site"         {http://www.tcl.tk} }
      { "Tcl wiki"         {http://mini.net/tcl/} }
      { "ActiveState"      {http://www.activestate.com/} }
      { "Evolane (eTcl)"   {http://www.evolane.com/} }
      { "comp.lang.tcl"    {http://groups.google.com/group/comp.lang.tcl} }
      { "tclscripting.com" {http://www.tclscripting.com/} }

      { "WWW" }
      { "W3 Consortium"   {http://www.w3.org} }
      { "CSS 1.0"         {http://www.w3.org/TR/CSS1} }
      { "CSS 2.1"         {http://www.w3.org/TR/CSS21/} }
      { "HTML 4.01"       {http://www.w3.org/TR/html4/} }
      { "W3 DOM Pages"    {http://www.w3.org/DOM/} }
      { "Web Apps 1.0"    {http://www.whatwg.org/specs/web-apps/current-work/} }
      { "Acid 2 Test"     {http://www.webstandards.org/files/acid2/test.html} }

        } {
          if {[llength $B] == 1} {
            set f [lindex $B 0]
            ::hv3::sqlitedb eval { 
              INSERT INTO bm_folder2(name) VALUES($f);
            }
            set folderid [::hv3::sqlitedb last_insert_rowid]
            ::hv3::sqlitedb eval { 
              INSERT INTO bm_tree2(folderid, objecttype, objectid) 
              VALUES(0, 'f', $folderid);
            }
          } else {
            foreach {name uri} $B {
              ::hv3::sqlitedb eval { 
                INSERT INTO bm_bookmark2(caption, uri, description)
                  VALUES($name, $uri, '');
                INSERT INTO bm_tree2(folderid, objecttype, objectid) 
                  VALUES($folderid, 'b', last_insert_rowid());
              }
            }
          }
        }
      }
    }
  }

  proc init {hv3} {
    initialise_database

    set frames [[winfo parent [winfo parent $hv3]] child_frames]
    set tree_hv3 [[lindex $frames 0] hv3]
    set html_hv3 [[lindex $frames 1] hv3]
    set browser  [[winfo parent $hv3] browser]

    set controller [$html_hv3 html].controller
    set treewidget [$tree_hv3 html].treewidget

    controller $controller $browser $html_hv3 $treewidget
    treewidget $treewidget $browser $controller

    $controller populate folder 0

    place $controller -x 0.0 -y 0.0 -relwidth 1.0 -height 50
    place $treewidget -x 0.0 -y 0.0 -relwidth 1.0 -relheight 1.0

    focus ${controller}.filter
  }

  proc dropobject {target drag insertAfter inFolder} {
    foreach {drag_type drag_id}     $drag {}
    foreach {target_type target_id} $target {}

    set drag_type [string range $drag_type 0 0]
    set target_type [string range $target_type 0 0]

    set targetfolder ""
    set new_items [list]

    set N [::hv3::sqlitedb one {SELECT max(linkid) FROM bm_tree2}]
    incr N
    
    if {($target_type eq "f") && (
         $inFolder || $drag_type eq "b" || $target_id <= 0
        )
    } {
      set targetfolder $target_id
      lappend new_items $N $target_id $drag_type $drag_id
    } else {
      set targetfolder [::hv3::sqlitedb one {
        SELECT folderid 
        FROM bm_tree2 
        WHERE objecttype = $target_type AND objectid = $target_id
      }]
    }

    ::hv3::sqlitedb eval {
      SELECT objecttype, objectid 
      FROM bm_tree2
      WHERE folderid = $targetfolder
      ORDER BY linkid ASC
    } {
      set isTarget [expr {
          $objecttype eq $target_type && 
          $objectid == $target_id
      }]
      if {!$insertAfter && $isTarget} {
        incr N
        lappend new_items $N $targetfolder $drag_type $drag_id
      }

      if {$objecttype ne $drag_type || $objectid != $drag_id} {
        incr N
        lappend new_items $N $targetfolder $objecttype $objectid
      }

      if {$insertAfter && $isTarget} {
        incr N
        lappend new_items $N $targetfolder $drag_type $drag_id
      }
    }

    foreach {a b c d} $new_items {
      ::hv3::sqlitedb eval {
        REPLACE INTO bm_tree2(linkid, folderid, objecttype, objectid)
        VALUES($a, $b, $c, $d)
      }
    }
  }


  ::snit::widget treewidget {

    # The browser (::hv3::browser_toplevel) containing this frameset
    #
    variable myBrowser

    # The controller that controls the left-hand frame of the frameset.
    #
    variable myController

    # Id of the current canvas text item that the mouse is hovering over.
    # A negative value means the mouse is not currently over any item.
    #
    variable myCurrentHover -1

    variable myTextId -array ""

    variable myIconId -array ""

    variable myTreeStart 0

    variable myOpenFolder -array ""

    variable myDragX ""
    variable myDragY ""
    variable myDragObject ""
    variable myPressedX ""
    variable myPressedY ""
    variable myPressedItem ""
    variable myPressedItemColor ""

    method click_new_folder {} {
      ${win}.newfolder configure -state normal
      ::hv3::bookmarks::new_folder $self
    }
    method click_new_bookmark {} {
      ${win}.newbookmark configure -state normal
      ::hv3::bookmarks::new_bookmark $self
    }

    constructor {browser controller} {
      set myBrowser $browser
      set myController $controller

      ::hv3::button ${win}.importexport      \
          -text "Import/Export Bookmarks..." \
          -state disabled
      ::hv3::button ${win}.newfolder    \
          -text "New Folder" \
          -command [list $self click_new_folder] 
      ::hv3::button ${win}.newbookmark \
          -text "New Bookmark"         \
          -command [list $self click_new_bookmark] 
      ::hv3::scrolled canvas ${win}.canvas -background white

      frame ${win}.tc
      ::hv3::button ${win}.tc.expand              \
           -text "Expand All"                     \
           -command [list $self expand_all]
      ::hv3::button ${win}.tc.collapse            \
            -text "Collapse All"                  \
            -command [list $self collapse_all]

      pack ${win}.tc.expand -side left -fill x
      pack ${win}.tc.collapse -side left -fill x

      pack ${win}.importexport -side top -fill x
      pack ${win}.newfolder -side top -fill x
      pack ${win}.newbookmark -side top -fill x
      pack ${win}.canvas -side top -fill both -expand 1
      pack ${win}.tc -side bottom

      bind ${win}.canvas <Motion>   [list $self motion_event %x %y]
      bind ${win}.canvas <Leave>    [list $self leave_event]
      bind ${win}.canvas <ButtonPress-1> [list $self press_event %x %y]
      bind ${win}.canvas <ButtonRelease-1> [list $self release_event %x %y]
    }

    method populate_tree {} {
      set C ${win}.canvas.widget

      set y 20
      set x 10
      set yincr [expr [font metrics Hv3DefaultFont -linespace] + 5]

      set myPressedItem ""
      $C delete all
      array unset myTextId
      array unset myIconId

      set myTreeStart $y
      incr y $yincr
      ::hv3::sqlitedb transaction { set y [$self drawSubTree $x $y 0] }
      incr y $yincr

      # Color for special links.
      #
      set c darkblue

      # Create the special "trash" folder.
      #
      set f Hv3DefaultFont
      set x2 [expr {$x + [image width itrash] + 5}]
      if {[$myController current] eq "folder -1"} {
        set f [concat [font actual Hv3DefaultFont] -weight bold]
      }
      set tid [$C create text $x2 $y -text "Trash" -anchor sw -font $f -fill $c]
      set myTextId($tid)  "folder -1"
      set myTextId($tid) "folder -1 {Click to view trashcan contents}"
      if {[info exists myOpenFolder(-1)]} {
        set tid [$C create image $x $y -image iopentrash -anchor sw]
      } else {
        set tid [$C create image $x $y -image itrash -anchor sw]
      }
      set myIconId($tid) "folder -1"
      incr y $yincr
      if {[info exists myOpenFolder(-1)]} {
        set y [$self drawSubTree [expr $x + $yincr] $y -1]
      }

      incr y $yincr
      foreach {label textid} [list                                   \
          "25 Most Recently Viewed URIs"                             \
          {recent 25 "Click to view 25 most recently viewed URIs"}   \
          "All Recently Viewed URIs"                                 \
          {recent -1 "Click to view all recently viewed URIs"}       \
          "Bookmarks"                                                \
          {folder 0 "Click to view root folder of bookmarks tree"}   \
      ] {
        set f Hv3DefaultFont
        if {[lrange $textid 0 1] eq [$myController current]} {
          set f [concat [font actual Hv3DefaultFont] -weight bold]
        }
        set tid [$C create text $x $y -text $label -anchor sw -font $f -fill $c]
        set myTextId($tid) $textid
        incr y $yincr
      }


      $C configure -scrollregion [concat 0 0 [lrange [$C bbox all] 2 3]]
    }

    method set_drag_tag {item} {
      set C ${win}.canvas
      set tag1 [lindex [$C itemcget $item -tags] 0]
      if {[string range $tag1 0 7] eq "bookmark" ||
          [string range $tag1 0 5] eq "folder"
      } {
        $C itemconfigure $tag1 -tags draggable
        $C raise draggable

        set info ""
        if {[info exists myTextId($item)]} {
          set info [lrange $myTextId($item) 0 1]
        } elseif {[info exists myIconId($item)]} {
          set info $myIconId($item)
        }

        set myDragObject $info
      }
      return
    }

    method motion_event {x y} {
      set C ${win}.canvas
      set x [$C canvasx $x]
      set y [$C canvasy $y]
      set hover [$C find overlapping $x $y $x $y]
      if {$myCurrentHover>=0 && $hover != $myCurrentHover} {
        $C delete underline
      }

      if {
          $myDragObject eq "" &&
          $myPressedItem ne "" && 
          (abs($myPressedX-$x) > 6 || abs($myPressedY-$y)>6)
      } {
        $self set_drag_tag $myPressedItem
        if {$myPressedItemColor ne ""} {
          $C itemconfigure $myPressedItem -fill $myPressedItemColor
          set myPressedItemColor ""
          $C delete underline
        }
      }

      if {$myDragObject ne ""} {
        $C move draggable [expr {$x-$myDragX}] [expr {$y-$myDragY}]
        set myDragX $x
        set myDragY $y
        return
      }

      if {[$C type $hover] eq "text"} {
        set bbox [$C bbox $hover]
        set y [expr [lindex $bbox 3] - 2]
        $C create line [lindex $bbox 0] $y [lindex $bbox 2] $y -tags underline
        $C configure -cursor hand2
        $myBrowser set_frame_status [lindex $myTextId($hover) 2]
      } else {
        set hover -1
        $C configure -cursor ""
        $myBrowser set_frame_status ""
      }

      set myCurrentHover $hover
    }

    method leave_event {} {
      set C ${win}.canvas
      $C delete underline
      set myCurrentHover -1
      $myBrowser set_frame_status ""
      $C configure -cursor ""
    }

    method press_event {x y} {
      set C ${win}.canvas
      set x [$C canvasx $x]
      set y [$C canvasy $y]
      set myPressedX $x
      set myPressedY $y
      set myDragX $x
      set myDragY $y

      set myPressedItem [$C find overlapping $x $y $x $y]
      if {[$C type $myPressedItem] eq "text"} {
        set myPressedItemColor [$C itemcget $myPressedItem -fill]
        $C itemconfigure $myPressedItem -fill red
      } else {
        set myPressedItemColor ""
      }
    }

    method drop {target drag insertAfter} {
      ::hv3::bookmarks::dropobject $target $drag $insertAfter \
          [info exists myOpenFolder([lindex $target 1])]
      eval $myController populate [$myController current]
    }

    method release_event {x y} { 
      if {$myPressedItem eq ""} return

      set C ${win}.canvas
      if {[$C type $myPressedItem] eq "text"} {
        $C itemconfigure $myPressedItem -fill $myPressedItemColor
      }
      
      set x [$C canvasx $x]
      set y [$C canvasy $y]

      if {$myDragObject ne ""} {
        set item [lindex [
          $C find overlapping [expr $x-3] [expr $y-3] [expr $x+3] [expr $y+3]
        ] 0]

        # We were dragging either a bookmark or a folder. Let's see what
        # we dropped it over:
        set dropid ""
        if {[info exists myTextId($item)]} {
          if {[lindex $myTextId($item) 0] ne "recent"} {
            set dropid [lrange $myTextId($item) 0 1]
          }
        } elseif {[info exists myIconId($item)]} {
          set dropid [lrange $myIconId($item) 0 1]
        }

        if {$dropid eq $myDragObject} {
          set dropid ""
        }
        if {$dropid eq "" && $y <= $myTreeStart} {
          set dropid [list folder 0]
        }

        if {$dropid ne "" && $myDragObject ne ""} {
          ::hv3::sqlitedb transaction {
            $self drop $dropid $myDragObject [expr $myDragY > $myPressedY]
          }
        }
        
        $self populate_tree
      } else {
        set item [lindex [$C find overlapping $x $y $x $y] 0]
        if {$item eq $myPressedItem} {
          $self click_event $item
        }
      }
   
      set myDragObject ""
      set myPressedItem ""
    }

    method click_event {click} {
      set C ${win}.canvas

      if {$click eq ""} return
      set clicktype [$C type $click]
 
      if {$clicktype eq "text"} {
        foreach {type id msg} $myTextId($click) {}
        switch -exact -- $type {
          folder {
            if {![info exists myOpenFolder($id)]} {
              set myOpenFolder($id) 1
            }
            $myController time populate_folder $id
          }
          bookmark {
            # Find the URI for this bookmark and send the browser there :)
            set uri [::hv3::sqlitedb eval {
              SELECT uri FROM bm_bookmark2 WHERE bookmarkid = $id
            }]
            if {$uri ne ""} {
              $myBrowser goto $uri
            }
          }
          recent {
            $myController time populate_recent $id
          }
        }
      } elseif {$clicktype eq "image"} {
        foreach {type id} $myIconId($click) {}
        switch -exact -- $type {
          folder {
            if {[info exists myOpenFolder($id)]} {
              unset myOpenFolder($id)
            } else {
              set myOpenFolder($id) 1
            }
            $self populate_tree
          }
        }
      }
    }

    method expand_all {} {
      ::hv3::sqlitedb eval { SELECT folderid FROM bm_folder2 } {
        set myOpenFolder($folderid) 1
      }
      $self populate_tree
    }
    method collapse_all {} {
      array unset myOpenFolder
      $self populate_tree
    }

    method drawSubTree {x y folderid {tags ""}} {
      set C ${win}.canvas

      set yincr [expr [font metrics Hv3DefaultFont -linespace] + 5]
      set x2 [expr {$x + [image width idir] + 5}]
      foreach {page pageid} [$myController current] {}

      ::hv3::sqlitedb eval {
        SELECT bookmarkid, caption, uri
        FROM bm_tree2, bm_bookmark2
        WHERE 
          bm_tree2.folderid = $folderid AND
          bm_tree2.objecttype = 'b' AND
          bm_tree2.objectid = bm_bookmark2.bookmarkid
        ORDER BY(bm_tree2.linkid)
      } {
        set font Hv3DefaultFont
        if {$page eq "bookmark" && $pageid eq $bookmarkid} {
          set font [concat [font actual Hv3DefaultFont] -weight bold]
        }
        set t [concat "bookmark$bookmarkid" $tags]

        set tid [$C create image $x $y -image ifile -anchor sw -tags $t]
        set myIconId($tid) [list bookmark $bookmarkid]
        set tid [
          $C create text $x2 $y -text $caption -anchor sw -font $font -tags $t
        ]
        set myTextId($tid) [list bookmark $bookmarkid "hyper-link: $uri"]
        incr y $yincr
      }

      ::hv3::sqlitedb eval {
        SELECT 
          bm_folder2.folderid AS thisfolderid, 
          name
        FROM bm_tree2, bm_folder2
        WHERE 
          bm_tree2.folderid = $folderid AND
          bm_tree2.objecttype = 'f' AND
          bm_tree2.objectid = bm_folder2.folderid
        ORDER BY(bm_tree2.linkid)
      } {
        set font Hv3DefaultFont
        if {$page eq "folder" && $pageid eq $thisfolderid} {
          set font [concat [font actual Hv3DefaultFont] -weight bold]
        }
        set t [concat "folder$thisfolderid" $tags]

        set image idir
        if {[info exists myOpenFolder($thisfolderid)]} {
          set image iopendir
        }

        set tid [$C create image $x $y -image $image -anchor sw -tags $t]
        set myIconId($tid) [list folder $thisfolderid]
        set tid [
          $C create text $x2 $y -text $name -anchor sw -font $font -tags $t
        ]
        set myTextId($tid) \
            [list folder $thisfolderid "Click to view folder \"$name\""]
        incr y $yincr
        if {[info exists myOpenFolder($thisfolderid)]} {
          set y [$self drawSubTree [expr $x + $yincr] $y $thisfolderid $t]
        }
      }

      return $y
    }
  }

  ::snit::widget controller {
    variable myHv3
    variable myBrowser
    variable myTree

    variable myPage ""
    variable myPageId ""

    constructor {browser hv3 tree} {
      set myHv3 $hv3
      set myTree $tree
      set myBrowser $browser

      set searchcmd [subst -nocommands {
          $self populate search [${win}.filter get]
      }]

      ::hv3::label ${win}.filter_label -text "Search bookmarks: " \
          -background white
      ::hv3::entry  ${win}.filter
      ::hv3::button ${win}.go -text Go -command $searchcmd
  
      pack ${win}.filter_label -side left
      pack ${win}.filter -side left
      pack ${win}.go -side left

      bind ${win}.filter <KeyPress-Return> $searchcmd

      $hv3 configure -targetcmd [list $self TargetCmd]

      $hull configure -background white
    }

    method TargetCmd {node} {
      set bookmarks_page [$node attr -default "" bookmarks_page]
      if {$bookmarks_page ne ""} {
        eval $self populate $bookmarks_page
        return ::hv3::bookmarks::noop
      }
      return [$myBrowser hv3]
    }

    method time {args} {
      set t [time {eval $self $args}]
      $myHv3 parse "
        <p class=time>
          Page generated in [lrange $t 0 1]
        </p>
      "
    }

    method start_page {title} {
      $myHv3 reset 0
      $myHv3 parse {
        <STYLE>
          :visited { color: darkblue; }
          .lastvisited,.uri,.description { display: block ; padding-left: 10ex }
          .uri { font-size: small; color: green }
          .time,.info {font-size: small; text-align: center; font-style: italic}
          .info { margin: 0 10px }

          .viewbookmark { float:right }
          .viewfolder   { width:90% ; margin: 5px auto }
          .delete,.rename { float:right }

          .edit { width: 90% }

          a[href]:hover  { text-decoration: none; }
          a[href]:active { color: red; }

          h1 { margin-top: 50px ; font-size: 1.4em; }
        </STYLE>
      }
      $myHv3 parse "<H1>$title</H1>"
    }

    proc configurecmd {win args} {
      set descent [font metrics [$win cget -font] -descent]
      set ascent  [font metrics [$win cget -font] -ascent]
      expr {([winfo reqheight $win] + $descent - $ascent) / 2}
    }

    method populate {page pageid} {
      switch -exact -- $page {
        recent   { $self time populate_recent   $pageid }
        folder   { $self time populate_folder   $pageid }
        search   { $self time populate_search   $pageid }
        default {
          error [join "
            {Bad page: \"$page\"}
            {- should be recent, folder, bookmark or search}
          "]
        }
      } 
    }

    method populate_recent {nLimit} {
      $self set_page_id recent $nLimit

      if {$nLimit > 0} {
        $self start_page "$nLimit Most Recently Visited URIs"
      } else {
        $self start_page "All Recently Visited URIs"
      }

      if {$nLimit > 0} {
        $myHv3 parse [subst {
          <P class="info"> 
              To view all visited URIs in the database, 
              <SPAN id=viewall></SPAN>
          </P>
        }]

        set node [$myHv3 search #viewall]
        set widget [$myHv3 html].document.viewall
        ::hv3::button $widget                            \
            -text "Click here"                           \
            -command [list $self time populate_recent -1]    
        $node replace $widget                            \
            -deletecmd [list destroy $widget]            \
            -configurecmd [myproc configurecmd $widget]
      }

      set sql { 
        SELECT uri, title, lastvisited 
        FROM visiteddb 
        WHERE title IS NOT NULL
        ORDER BY oid DESC 
        LIMIT $nLimit
      }
      set N 0
      ::hv3::sqlitedb eval $sql {
        incr N
        set lastvisited [clock format $lastvisited]
        if {$title eq ""} {set title $uri}
        $myHv3 parse [subst -nocommands {
          <P style="clear:both">
            <SPAN style="white-space:nowrap">
              ${N}. <A href="$uri" target="_top">$title</A><BR>
              <SPAN class="uri">$uri</SPAN>
            </SPAN>
            <SPAN class="lastvisited">Last visited: $lastvisited</SPAN>
          </P>
        }]
      }
    }

    method delete_folder_contents {folderid} {
      ::hv3::sqlitedb eval {
        SELECT objectid AS thisfolderid 
        FROM bm_tree2 
        WHERE folderid = $folderid AND objecttype = 'f'
      } {
        $self delete_folder_contents $thisfolderid
      }
      ::hv3::sqlitedb eval {
        DELETE FROM bm_folder2 WHERE folderid IN (
          SELECT objectid FROM bm_tree2 
          WHERE folderid = $folderid AND objecttype = 'f'
        );
        DELETE FROM bm_bookmark2 WHERE bookmarkid IN (
          SELECT objectid FROM bm_tree2 
          WHERE folderid = $folderid AND objecttype = 'b'
        );
        DELETE FROM bm_tree2 WHERE folderid = $folderid
      }
      if {$folderid == -1} {
        $self populate $myPage $myPageId
      }
    }

    method populate_folder {folderid} {
      $self set_page_id folder $folderid
      set isEmpty 1

      if {$folderid > 0} {
        set name [::hv3::sqlitedb one {
          SELECT name FROM bm_folder2 WHERE folderid = $folderid
        }]
        $self start_page "Folder \"$name\"<DIV class=rename></DIV>"
          # Set up the "really delete" button.
        set node [$myHv3 search .rename]
        set rename [::hv3::button [$myHv3 html].document.rename \
          -text "Rename Folder..."                          \
          -command [list ::hv3::bookmarks::rename_folder $self $folderid]
        ]
  
        $node replace $rename -deletecmd [list destroy $rename]
      } else {
        if {$folderid == 0} {
          $self start_page "Bookmarks"
        }
        if {$folderid == -1} {
          $self start_page "Trash<DIV class=delete></DIV>"

          # Set up the "really delete" button.
          set node [$myHv3 search .delete]
          set trashbutton [::hv3::button [$myHv3 html].document.trash \
              -text "Permanently delete trashcan contents"            \
              -command [list $self delete_folder_contents -1]
          ]
  
          $node replace $trashbutton -deletecmd [list destroy $trashbutton]
        }
      }

      set N 0
      ::hv3::sqlitedb eval {
        SELECT bookmarkid, caption, uri, description, image
        FROM bm_tree2, bm_bookmark2
        WHERE 
          bm_tree2.folderid = $folderid AND
          bm_tree2.objecttype = 'b' AND
          bm_tree2.objectid = bm_bookmark2.bookmarkid
        ORDER BY(bm_tree2.linkid)
      } {
        incr N
        $myHv3 parse [subst -nocommands {
          <P>
            <SPAN class="viewbookmark" bookmarkid=$bookmarkid></SPAN>
            <SPAN style="white-space:nowrap">
              ${N}. <A href="$uri" target="_top">$caption</A><BR>
            </SPAN>
            <SPAN class="description">$description</SPAN>
            <SPAN class="uri">$uri</SPAN>
          </P>
        }]
        set isEmpty 0
      }
      $myHv3 parse <HR>
      ::hv3::sqlitedb eval {
        SELECT linkid, folderid AS thisfolderid, 'Parent Folder' AS name
        FROM bm_tree2 
        WHERE objecttype = 'f' AND objectid = $folderid

        UNION ALL

        SELECT linkid, bm_folder2.folderid AS thisfolderid, name
        FROM bm_tree2, bm_folder2
        WHERE 
          bm_tree2.folderid = $folderid AND
          bm_tree2.objecttype = 'f' AND
          bm_tree2.objectid = bm_folder2.folderid
        ORDER BY 1
      } {
        $myHv3 parse [subst -nocommands {
            <P><A href="." bookmarks_page="folder $thisfolderid">$name</A></P>
        }]
        set foldername($thisfolderid) $name
        set isEmpty 0
      }

      set parent [$myHv3 html].document
      foreach node [$myHv3 search .viewbookmark] {
        set bookmarkid [$node attribute bookmarkid]
        set widget ${parent}.bookmark${bookmarkid}
        ::hv3::button $widget                             \
            -text "Edit..."                               \
            -command [list ::hv3::bookmarks::edit_bookmark $self $bookmarkid]
        $node replace $widget -deletecmd [list destroy $widget]
      }
      foreach node [$myHv3 search .viewfolder] {
        set thisfolderid [$node attribute folderid]
        set widget ${parent}.folder${thisfolderid}
        ::hv3::button $widget                             \
            -text "View folder \"$foldername($thisfolderid)\"..."              \
            -command [list $self time populate_folder $thisfolderid]
        $node replace $widget -deletecmd [list destroy $widget]
      }

      if {$isEmpty && [info exists trashbutton]} {
        $trashbutton configure -state disabled
      }
    }

    method populate_search {search} {
      $self set_page_id search $search

      $self start_page "Search bookmarks for \"$search\""

      set N 0
      set like %${search}%
      ::hv3::sqlitedb eval {
        SELECT bookmarkid, caption, uri, description, image
        FROM bm_bookmark2
        WHERE uri LIKE $like OR caption LIKE $like OR description LIKE $like
      } {
        incr N
        $myHv3 parse [subst -nocommands {
          <P>
            <SPAN style="white-space:nowrap">
              ${N}. <A href="$uri" target="_top">$caption</A><BR>
            </SPAN>
            <SPAN class="description">$description</SPAN>
            <SPAN class="uri">$uri</SPAN>
          </P>
        }]
      }
    }

    method set_page_id {page pageid} { 
      set myPage $page
      set myPageId $pageid
      $myTree populate_tree
    }
    method sub_page_id {pageid} {
      set myPageId $pageid
    }
    method current {} { list $myPage $myPageId }
  }

  proc store_new_folder {treewidget} {
    set name [.new.entry get]

    destroy .new

    ::hv3::sqlitedb transaction {
      ::hv3::sqlitedb eval { INSERT INTO bm_folder2(name) VALUES($name) }
      set object [list folder [::hv3::sqlitedb last_insert_rowid]]
      ::hv3::bookmarks::dropobject [list folder 0] $object 0 1
    }

    if {$treewidget ne ""} {
      $treewidget populate_tree
    }
  }

  proc store_rename_folder {htmlwidget folderid} {
    set name [.new.entry get]
    ::hv3::sqlitedb eval {
      UPDATE bm_folder2 SET name = $name WHERE folderid = $folderid
    }
    destroy .new
    if {$htmlwidget ne ""} {
      eval $htmlwidget populate [$htmlwidget current]
    }
  }

  proc store_new_bookmark {treewidget} {
    set caption [.new.caption get]
    set uri [.new.uri get]
    set description [.new.desc get 0.0 end]
    destroy .new

    ::hv3::sqlitedb transaction {
      ::hv3::sqlitedb eval {
        INSERT INTO bm_bookmark2(caption, uri, description) 
        VALUES($caption, $uri, $description)
      }
      set object [list bookmark [::hv3::sqlitedb last_insert_rowid]]
      ::hv3::bookmarks::dropobject [list folder 0] $object 0 1
    }

    if {$treewidget ne ""} {
      $treewidget populate_tree
    }
  }

  proc store_edit_bookmark {htmlwidget bookmarkid} {
    set caption     [.new.caption get]
    set uri         [.new.uri get]
    set description [.new.desc get 0.0 end]

    ::hv3::sqlitedb eval {
      UPDATE bm_bookmark2 
      SET caption = $caption, uri = $uri, description = $description
      WHERE bookmarkid = $bookmarkid
    }
    destroy .new
    if {$htmlwidget ne ""} {
      eval $htmlwidget populate [$htmlwidget current]
    }
  }

  proc new_folder {treewidget} {
    toplevel .new

    ::hv3::label .new.label -text "Create New Bookmarks Folder: " -anchor s
    ::hv3::entry .new.entry -width 60
    ::hv3::button .new.save                            \
        -text "Save"                                   \
        -command [list ::hv3::bookmarks::store_new_folder $treewidget]
    ::hv3::button .new.cancel -text "Cancel" -command {destroy .new}

    grid .new.label -columnspan 2 -sticky ew
    grid .new.entry -columnspan 2 -sticky ew -padx 5
    grid .new.cancel .new.save -pady 5 -padx 5

    grid columnconfigure .new 1 -weight 1
    grid columnconfigure .new 0 -weight 1

    bind .new.entry <KeyPress-Return> {.new.save invoke}
    .new.entry insert 0 "New Folder"
    .new.entry selection range 0 end
    focus .new.entry

    grab .new
    wm transient .new .
    # wm protocol .new WM_DELETE_WINDOW { grab release .new ; destroy .new }
    raise .new
    tkwait window .new
  }

  proc rename_folder {treewidget folderid} {
    toplevel .new

    set name [::hv3::sqlitedb one {
      SELECT name FROM bm_folder2 WHERE folderid = $folderid
    }]

    ::hv3::label .new.label -text "Rename Folder \"$name\" To: " -anchor s
    ::hv3::entry .new.entry -width 60
    set cmd [list ::hv3::bookmarks::store_rename_folder $treewidget $folderid]
    ::hv3::button .new.save -text "Save" -command $cmd
    ::hv3::button .new.cancel -text "Cancel" -command {destroy .new}

    grid .new.label -columnspan 2 -sticky ew
    grid .new.entry -columnspan 2 -sticky ew -padx 5
    grid .new.cancel .new.save -pady 5 -padx 5

    grid columnconfigure .new 1 -weight 1
    grid columnconfigure .new 0 -weight 1

    bind .new.entry <KeyPress-Return> {.new.save invoke}
    .new.entry insert 0 $name
    .new.entry selection range 0 end
    focus .new.entry

    grab .new
    wm transient .new .
    # wm protocol .new WM_DELETE_WINDOW { grab release .new ; destroy .new }
    raise .new
    tkwait window .new
  }

  proc create_bookmark_dialog {} {
    toplevel .new

    ::hv3::label .new.label -text "Create New Bookmark: " -anchor s
    ::hv3::entry .new.caption -width 40
    ::hv3::entry .new.uri -width 40
    ::hv3::text .new.desc -height 10 -background white -borderwidth 1 -width 40

    ::hv3::label .new.l_caption -text "Caption: "
    ::hv3::label .new.l_uri -text "Uri: "
    ::hv3::label .new.l_desc -text "Description: "

    ::hv3::button .new.save -text "Save"
    ::hv3::button .new.cancel -text "Cancel" -command {destroy .new}

    grid .new.label -columnspan 2

    grid .new.l_caption     .new.caption -pady 5 -padx 5
    grid .new.l_uri         .new.uri -pady 5 -padx 5
    grid .new.l_desc .new.desc -pady 5 -padx 5

    grid configure .new.caption -columnspan 2 -sticky ew
    grid configure .new.uri -columnspan 2 -sticky ew 
    grid configure .new.desc -columnspan 2 -sticky ewns

    grid configure .new.l_caption -sticky e
    grid configure .new.l_uri -sticky e
    grid configure .new.l_desc -sticky e

    grid .new.save .new.cancel -pady 5 -padx 5 -sticky e
    grid configure .new.save -column 2

    grid columnconfigure .new 1 -weight 1
    grid rowconfigure .new 3 -weight 1

    bind .new.caption <KeyPress-Return> {.new.save invoke}
    bind .new.uri     <KeyPress-Return> {.new.save invoke}
  }

  proc new_bookmark {treewidget} {
    set hv3 ""
    if {[$treewidget info type] eq "::hv3::hv3"} {
      set hv3 $treewidget
      set treewidget ""
    }

    create_bookmark_dialog 
    .new.save configure \
        -command [list ::hv3::bookmarks::store_new_bookmark $treewidget]

    focus .new.caption
    if {$hv3 ne ""} {
      .new.caption insert 0 [$hv3 title]
      .new.uri insert 0     [$hv3 uri get]
      .new.caption selection range 0 end
    }

    grab .new
    wm transient .new .
    # wm protocol .new WM_DELETE_WINDOW { grab release .new ; destroy .new }
    raise .new
    tkwait window .new
  }

  proc edit_bookmark {htmlwidget bookmarkid} {
    create_bookmark_dialog 
    set cmd [list ::hv3::bookmarks::store_edit_bookmark $htmlwidget $bookmarkid]
    .new.save configure -command $cmd

    focus .new.caption
    ::hv3::sqlitedb eval {
      SELECT caption, uri, description 
      FROM bm_bookmark2 
      WHERE bookmarkid = $bookmarkid
    } {}
    .new.caption insert 0 $caption
    .new.uri insert 0     $uri
    .new.desc insert 0.0     $description
    .new.caption selection range 0 end

    grab .new
    wm transient .new .
    # wm protocol .new WM_DELETE_WINDOW { grab release .new ; destroy .new }
    raise .new
    tkwait window .new
  }
}

