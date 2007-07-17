namespace eval hv3 { set {version($Id: hv3_home.tcl,v 1.15 2007/07/17 07:49:40 danielk1977 Exp $)} 1 }

# Register the about: scheme handler with ::hv3::protocol $protocol.
#
proc ::hv3::about_scheme_init {protocol} {
  set dir $::hv3::maindir
  $protocol schemehandler about [list ::hv3::about_request]
}

# This proc is called when any URI using the protocol "about:" is visited.
# Usually, this is because the user selected the Debug->About menu option.
#
proc ::hv3::about_request {downloadHandle} {
  set tkhtml_version [::tkhtml::version]
  set hv3_version ""
  foreach version [lsort [array names ::hv3::version]] {
    set t [string trim [string range $version 4 end-1]]
    append hv3_version "$t\n"
  }

  set html [subst {
    <html> <head> </head> <body>
    <h1>Tkhtml Source Code Versions</h1>
    <pre>$tkhtml_version</pre>
    <h1>Hv3 Source Code Versions</h1>
    <pre>$hv3_version</pre>
    </body> </html>
  }]

  $downloadHandle append $html
  $downloadHandle finish
}

# Register the home: scheme handler with ::hv3::protocol $protocol.
#
proc ::hv3::home_scheme_init {hv3 protocol} {
  set dir $::hv3::maindir
  $protocol schemehandler home [list ::hv3::home_request $protocol $hv3 $dir]
}

::snit::type ::hv3::bookmarkdb {
  variable myDb ""

  typevariable Schema {
      CREATE TABLE bm_bookmarks1(
        bookmark_id     INTEGER PRIMARY KEY,
        bookmark_name   TEXT,
        bookmark_uri    TEXT,
        bookmark_tags   TEXT,

        bookmark_folder TEXT, bookmark_folder_idx INTEGER,
        UNIQUE(bookmark_folder, bookmark_folder_idx)
      );

      /* This table defines the display order for folders. Also, whether
       * or not the folder is in "hidden" state. 
       */
      CREATE TABLE bm_folders1(
        folder_id       INTEGER PRIMARY KEY,
        folder_name     TEXT UNIQUE,
        folder_hidden   BOOLEAN
      );

      CREATE TABLE bm_version1(
        version         INTEGER PRIMARY KEY
      );

      /* The "undo" log */
      CREATE TABLE bm_undo1(
        caption         TEXT,
        sql             TEXT
      );
      CREATE TRIGGER bm_undo_limit AFTER INSERT ON bm_undo1 BEGIN
        DELETE FROM bm_undo1 WHERE rowid < (10 + new.rowid);
      END;

      INSERT INTO bm_version1 VALUES(1);
  }

  typevariable BookmarkTemplate [join {
      {<DIV 
         class="bookmark" 
         active="true"
         id="${bookmark_id}"
         onmousedown="return bookmark_mousedown(this, event)"
         bookmark_id="$bookmark_id"
         bookmark_name="$bookmark_name"
         bookmark_uri="$bookmark_uri"
         bookmark_tags="$bookmark_tags"
      >}
      {<SPAN class="edit" 
         onclick="return bookmark_edit(this.parentNode)">(edit)</SPAN>}
      {<A href="$bookmark_uri">$bookmark_name</A>}
      {<FORM 
         style="display:none" 
         onsubmit="return bookmark_submit(this.parentNode)"
       >
          <TABLE width=100%>
            <TR><TD>Name: <TD width=100%><INPUT width=90% name=n></INPUT>
            <TR><TD>URI:  <TD><INPUT width=90% name=u></INPUT>
            <TR><TD>Tags: <TD><INPUT width=90% name=t></INPUT>
          </TABLE>
        </FORM>
      </DIV>}
  } ""]

  typevariable FolderTemplate [join {
    {<DIV
      class="folder"
      id="$folder_id"
      folder_id="$folder_id"
      folder_name="$folder_name"
      folder_hidden="$folder_hidden"
    >}
      {<H2 
         style="display:$folder_display"
         onmousedown="return folder_mousedown(this, event)"
         onclick="return folder_toggle(this.parentNode, event, 1)"
       >}
      {<SPAN class="edit" 
         onclick="return folder_edit(this.parentNode.parentNode)">(edit)</SPAN>}
      {<SPAN>- </SPAN>$folder_name}
      {<FORM
         style="display:none"
         onsubmit="return folder_submit(this.parentNode.parentNode)"
       >
        <TABLE width=100% style="color:black;margin-left:15px">
        <TR><TD>Name: <TD width=100%><INPUT name=n></INPUT>
        </TABLE>
      </FORM>}
      {</H2><UL style="clear:both;width:100%">}
  } ""]

  constructor {db} {
    set myDb $db

    set rc [catch { $myDb eval $Schema } msg]

    # When this is loaded, each bookmarks record is transformed to
    # the following HTML:
    #
    if {$rc == 0} {
      set folder ""

      $myDb transaction {
        set ii 0
        foreach B {
      { "Hv3 User Manual (todo)"         {home://man} }
      { "Hv3 Programmers Manual (todo)"  {home://dom} }

      { "Tkhtml and Hv3 Related" }
      { "tkhtml.tcl.tk"         {http://tkhtml.tcl.tk} }
      { "Tkhtml3 Google Group" {http://groups.google.com/group/tkhtml3} }
      { "Hv3 site at freshmeat.net" {http://freshmeat.net/hv3} }
      { "Sqlite" {http://www.sqlite.org} }
      { "Tk Combobox" {http://www.purl.org/net/oakley/tcl/combobox/index.html} }
      { "Polipo (web proxy)" {http://www.pps.jussieu.fr/~jch/software/polipo/} }
      { "SEE (javascript engine)" {http://www.adaptive-enterprises.com.au/~d/software/see/} }
      { "Icons used in Hv3" {http://e-lusion.com/design/greyscale} }

      { "Tcl" }
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
            set folder [lindex $B 0]
            $myDb eval { 
              INSERT INTO bm_folders1(folder_name, folder_hidden)
                VALUES($folder, 0)
            }
            continue
          }

          foreach {name uri} $B {
            $myDb eval { 
              INSERT INTO bm_bookmarks1(
                bookmark_name, bookmark_uri, bookmark_tags, 
                bookmark_folder, bookmark_folder_idx) 
                VALUES($name, $uri, '', $folder, $ii)
            }
            incr ii
          }
        }
      }
    }
  }

  # This method is called to add a bookmark to the system.
  #
  method add {name uri} {
    $myDb transaction {
      $myDb eval {
        INSERT INTO bm_bookmarks1 (
          bookmark_name, bookmark_uri, bookmark_tags, 
          bookmark_folder, bookmark_folder_idx
        ) VALUES(
          $name, $uri, '', '', (
            SELECT min(bookmark_folder_idx)-1 FROM bm_bookmarks1
          )
        )
      }
      $myDb eval {UPDATE bm_version1 SET version = version + 1}
    }

    $myDb last_insert_rowid
  }

  method GetFolderTemplate {} {return $FolderTemplate}
  method GetBookmarkTemplate {} {return $BookmarkTemplate}

  method db {} {return $myDb}
}

proc ::hv3::bookmarks_style {} {
  return {
    h1 {
      font-size: 1.4em;
      font-weight: normal;
    }
    h2 {
      float: left;
      width: 45%;
      border: solid 1px purple;
      border-right: none;
      border-bottom: none;
      color: purple;
      margin: 2px;
      background: #CCCCCC;
      cursor: pointer;
      font-size: 1.4em;
    }
    li {
      float: left;
      width: 50%;
      min-width: 180px;
    }
    form {
      margin: 0;
    }

    .bookmark[active="true"]:hover {
       background: white;
     }
    .bookmark {
      cursor:pointer;
      margin: 1px;
      padding: 2px 0 2px 15px;
      background: #EEEEEE;
      border: solid 2px purple;
      display: block;
      position: relative;

      float: left;
      width: 45%;
      min-width: 180px;
    }
    .bookmark a {
      text-decoration: none;
      color: black;
      display: block;
    }

    ul {
      padding: 0;
      margin: auto auto auto 15px;
    }
    .folder {
      padding: 0px 5px;
      margin: 0 0 10px 0;
      width: 100%;
      display: table;
      position: relative;
    }

    #controls {
      border-bottom: solid black 2px;
      background: white;
      position: fixed;
      top: 0px;
      left: 0px;
      right: 0px;
      z-index: 5;
    }
    .edit {
      display: block;
      float: right;
      font-size: small;
      color: darkblue;
      text-decoration: underline;
      font-weight: normal;
      padding-right: 5px;
    }

    body {
      margin-top: 3em;
    }
  }
}

proc ::hv3::bookmarks_script {} {
  return {

    var drag = new Object()
    drag.element = undefined
    drag.interval = undefined
    drag.x = undefined
    drag.y = undefined
    drag.original_x = undefined
    drag.original_y = undefined
    drag.isDelete = false

    var app = new Object()

    function mouseup_handler (event) {
      drag.element.style.top = '0px'
      drag.element.style.left = '0px'
      drag.element.style.zIndex = 'auto'
      drag.element.style.backgroundColor = ""
      clearInterval(drag.interval)
      document.onmouseup = undefined
      document.onmousemove = undefined

      if (drag.isDelete) {
        drag.element.parentNode.removeChild(drag.element)
        app.version = hv3_bookmarks.remove(drag.element)
      } else if (drag.element.onclick == ignore_click) {
        if (drag.element.className == 'bookmark') {
          app.version = hv3_bookmarks.bookmark_move(drag.element)
        }
        if (drag.element.className == 'folder') {
          app.version = hv3_bookmarks.folder_move(drag.element)
        }
      }
      drag.isDelete = false
      drag.element = undefined
      return 0
    }
    function mousemove_handler (event) {
      drag.x = event.clientX
      drag.y = event.clientY
      return 0
    }

    function ignore_click () {
      this.onclick = undefined
      return 0
    }

    function drag_cache_position(d) {
      d.drag_x1 = 0
      d.drag_y1 = 0
      for (var p = d; p != null; p = p.offsetParent) {
        d.drag_x1 += p.offsetLeft
        d.drag_y1 += p.offsetTop
      }
      d.drag_x2 = d.drag_x1 + d.offsetWidth
      d.drag_y2 = d.drag_y1 + d.offsetHeight
    }

    function drag_makedropmap(elem) {
      var dlist = document.getElementsByTagName('div');
      drag.drag_targets = new Array()

      for ( var i = 0; i < dlist.length; i++) {
        var d = dlist[i]
        if (d != elem && d.className == elem.className) {
          if (d.className == "folder" && d.id == "") {
            continue
          }
          drag_cache_position(d)
          drag.drag_targets.push(d)
        }
      }

      if (elem.className == "bookmark") {
        var hlist = document.getElementsByTagName('h2')
        for ( var i = 0; i < hlist.length; i++) {
          var h = hlist[i]
          if (h.nextSibling.style.display != "none") {
            drag_cache_position(h)
            drag.drag_targets.push(h)
          }
        }

        hlist = document.getElementsByTagName('h1')
        for ( var i = 0; i < hlist.length; i++) {
          var h = hlist[i]
          drag_cache_position(h)
          h.drag_y2 += 15
          drag.drag_targets.push(h)
        }
      }

      drag_cache_position(drag.controls)
    }

    function drag_update() {
      if (
         Math.abs(drag.x - drag.original_x) > 5 ||
         Math.abs(drag.y - drag.original_y) > 5
      ) {
        drag.element.onclick = ignore_click
      }
      drag.element.style.left = (drag.x - drag.original_x) + 'px'
      drag.element.style.top  = (drag.y - drag.original_y) + 'px'

      if (!drag.drag_targets) {
        drag_makedropmap(drag.element)
      }

      drag_cache_position(drag.element)
      var cx = (drag.element.drag_x1 + drag.element.drag_x2) / 2
      var cy = (drag.element.drag_y1 + drag.element.drag_y2) / 2

      var isDelete = ((drag.element.drag_y1+5) < drag.controls.drag_y2)
      if (isDelete && !drag.isDelete) {
        drag.element.style.backgroundColor = "black"
        drag.isDelete = isDelete
      } else if (!isDelete && drag.isDelete) {
        drag.element.style.backgroundColor = ""
        drag.isDelete = isDelete
      }

      for (var i = 0; i < drag.drag_targets.length; i++) {
        var a = drag.drag_targets[i]
        if (a.drag_x1 < cx && a.drag_x2 > cx &&
            a.drag_y1 < cy && a.drag_y2 > cy
        ) {

          var x = drag.element.drag_x1
          var y = drag.element.drag_y1

          var p = a.parentNode
          if (a.nodeName == "H2") {
            p = a.nextSibling
            a = p.firstChild
          } else if (a.nodeName == "H1") {
            p = app.nofolder.childNodes[1]
            a = p.firstChild
          }

          if (drag.element.parentNode == p) {
            for (var j = 0; j < p.childNodes.length; j++) {
              var child = p.childNodes[j]
              if (child == a) {
                break
              } else if (child == drag.element) {
                a = a.nextSibling
                break
              }
            }
          }

          p.insertBefore(drag.element, a)

          drag_cache_position(drag.element)
          var sx = drag.element.drag_x1 - x
          var sy = drag.element.drag_y1 - y

          drag.original_x += sx
          drag.original_y += sy

          drag.element.style.left = (drag.x - drag.original_x) + 'px'
          drag.element.style.top  = (drag.y - drag.original_y) + 'px'

          drag_makedropmap(drag.element)
          break
        }
      }
    }

    function mousedown_handler (elem, event) {
      clearInterval(drag.interval)
      drag.isDelete = false
      drag.element = elem

      drag.original_x = event.clientX
      drag.original_y = event.clientY
      drag.x = event.clientX
      drag.y = event.clientY
      drag.element.style.zIndex = 10
      drag.interval = setInterval(drag_update, 20)
      document.onmouseup = mouseup_handler
      document.onmousemove = mousemove_handler

      drag_makedropmap(drag.element)
      return 0
    }

    // Toggle visibility of folder contents.
    //
    function folder_toggle (folder, event, toggle) {
      var h2 = folder.childNodes[0]
      var ul = folder.childNodes[1]

      if (folder.onclick == ignore_click) return

      var isHidden = (1 * folder.getAttribute('folder_hidden'))
      if (toggle) {
        isHidden = (isHidden ? 0 : 1)
        folder.setAttribute('folder_hidden', isHidden)
        app.version = hv3_bookmarks.folder_hidden(folder)
      }

      if (isHidden) {
        /* Hide the folder contents */
        ul.style.display = 'none'
        ul.style.clear = 'none'

        h2.childNodes[1].innerHTML = '+ '
        h2.style.width = 'auto'
        h2.style.cssFloat = 'none'

        folder.style.cssFloat = 'left'
        folder.style.width = '45%'
        folder.style.clear = 'none'
        folder.style.marginBottom = '0'
      } else {
        /* Expand the folder contents */
        ul.style.display = 'table'
        ul.style.clear = 'both'

        h2.childNodes[1].innerHTML = '- '
        h2.style.width = '45%'
        h2.style.cssFloat = 'left'

        folder.style.clear = 'both'
        folder.style.cssFloat = 'none'
        folder.style.width = '100%'
        folder.style.marginBottom = '10px'
      }

      return 0
    }

    function bookmark_mousedown(elem, event) {
      mousedown_handler(elem, event)
      return 0
    }
    function folder_mousedown(elem, event) {
      mousedown_handler(elem.parentNode, event)
      return 0
    }

    function bookmark_submit(elem) {
      var f = elem.childNodes[2]
      
      var new_name = f.n.value
      var new_uri = f.u.value
      var new_tags = f.t.value

      elem.setAttribute('bookmark_name', new_name)
      elem.setAttribute('bookmark_uri',  new_uri)
      elem.setAttribute('bookmark_tags', new_tags)

      var a = elem.childNodes[1]
      a.firstChild.data = new_name
      a.href = new_uri

      app.version = hv3_bookmarks.bookmark_edit(elem)
 
      bookmark_edit(elem)
      return 0
    }

    function bookmark_edit(elem) {
      var f = elem.childNodes[2]
      var d = f.style.display
      if (d == 'none') {
        d = 'block'
        f.n.value = elem.getAttribute('bookmark_name')
        f.u.value = elem.getAttribute('bookmark_uri')
        f.t.value = elem.getAttribute('bookmark_tags')
        f.n.select()
        f.n.focus()
        elem.firstChild.firstChild.data = "(cancel)"
        elem.setAttribute("active", "false")
      } else {
        d = 'none'
        elem.firstChild.firstChild.data = "(edit)"
        elem.setAttribute("active", "true")
      }
      f.style.display = d
      return 0
    }

    function folder_submit(elem) {
      var f = elem.firstChild.childNodes[3]
      var t = elem.firstChild.childNodes[2]
      var new_name = f.n.value
      elem.setAttribute('folder_name', new_name)
      t.data = new_name

      app.version = hv3_bookmarks.folder_edit(elem)
      folder_edit(elem)
      return 0
    }

    function folder_edit(elem) {
      var ed = elem.firstChild.firstChild
      var f = elem.firstChild.childNodes[3]

      var d = f.style.display
      if (d == 'none') {
        d = 'block'
        f.n.value = elem.getAttribute('folder_name')
        f.n.select()
        f.n.focus()
        ed.firstChild.data = "(cancel)"
      } else {
        d = 'none'
        ed.firstChild.data = "(edit)"
      }
      f.style.display = d
      return 0
    }

    // The following are "onclick" handlers for the "New Bookmark"
    // and "New Folder" buttons respectively.
    //
    function bookmark_new() {
      var id = hv3_bookmarks.bookmark_new()
      refresh_content()
      bookmark_edit(document.getElementById(id))
    }
    function folder_new() {
      var id = hv3_bookmarks.folder_new()
      refresh_content()
      folder_edit(document.getElementById(id))
    }

    function refresh_content() {
      drag.content.innerHTML = hv3_bookmarks.get_html_content()
      app.version = hv3_bookmarks.get_version()
      app.nofolder = document.getElementById("")

      var dlist = document.getElementsByTagName('div');
      for ( var i = 0; i < dlist.length; i++) {
        var d = dlist[i]
        if (d.className == "folder") {
          folder_toggle(d, 0, 0)
        }
      }
    }
    function check_refresh_content() {
      if (app.version != hv3_bookmarks.get_version()) {
        refresh_content()
      }
    }

    window.onload = function () {
      document.getElementById("searchbox").focus()
      drag.controls = document.getElementById("controls")
      drag.content = document.getElementById("content")
      refresh_content()
      setInterval(check_refresh_content, 2000)
    }
  }
}

proc ::hv3::bookmarks_controls {} {
  return {
    <TABLE id="controls"><TR>
      <TD align="center">
        <INPUT type="button" value="New Folder" onclick="folder_new()">
      <TD align="center">
        <INPUT type="button" value="New Bookmark" onclick="bookmark_new()">
        </INPUT>
      <TD align="center">
        <INPUT type="button" disabled=1 value="Undo Last Action"></INPUT>
      <TD align="left" style="padding-left:15px">
        Filter:
      <TD align="left" width=100% style="padding-right:2px">
         <INPUT width=100% type="text" id="searchbox"></INPUT>
      <TD align="center">
        <INPUT type="button" disabled=1 value="Clear"></INPUT>
    </TABLE>
  }
}

# When a URI with the scheme "home:" is requested, this proc is invoked.
#
proc ::hv3::home_request {http hv3 dir downloadHandle} {

  $downloadHandle append [subst {
    <HTML>
    <STYLE>
      [::hv3::bookmarks_style]
    </STYLE>
    <SCRIPT>
      [::hv3::bookmarks_script]
    </SCRIPT>
    <BODY>
    [::hv3::bookmarks_controls]
    <H1>BOOKMARKS:</H1>
    <DIV id=content></DIV>
  }]
  $downloadHandle finish
}

proc ::hv3::compile_bookmarks_object {} {

# This is a custom object used by the javascript part of the bookmarks
# appliation to access the database.
#
::hv3::dom2::stateless Bookmarks {} {
  dom_parameter myManager

  dom_call remove {THIS node} {
    set db [$myManager db]
    bookmark_transaction $db {
      set N [GetNodeFromObj [lindex $node 1]]
      if {[$N attr class] eq "bookmark"} {
        set bookmark_id [$N attr bookmark_id]
        $db eval { DELETE FROM bm_bookmarks1 WHERE bookmark_id = $bookmark_id }
      }
      if {[$N attr class] eq "folder"} {
        set folder_name [$N attr folder_name]
        $db eval { 
          DELETE FROM bm_bookmarks1 WHERE bookmark_folder = $folder_name;
          DELETE FROM bm_folders1 WHERE folder_name = $folder_name;
        }
      }
    }
  }

  dom_call bookmark_edit {THIS node} {
    set db [$myManager db]
    bookmark_transaction $db {
      set N [GetNodeFromObj [lindex $node 1]]
      foreach v {bookmark_id bookmark_name bookmark_uri bookmark_tags} {
        set $v [$N attribute $v]
      }

      $db eval {
        UPDATE bm_bookmarks1 SET bookmark_name = $bookmark_name,
                              bookmark_uri = $bookmark_uri,
                              bookmark_tags = $bookmark_tags
        WHERE bookmark_id = $bookmark_id
      }
    }
  }

  dom_call bookmark_move {THIS node} {
    set db [$myManager db]
    bookmark_transaction $db {
      set N [GetNodeFromObj [lindex $node 1]]
      set P [$N parent]
      set F [[$N parent] parent]

      set bookmark_folder [$F attr folder_name]

      set iMax [$db onecolumn {
        SELECT max(bookmark_folder_idx) 
        FROM bm_bookmarks1 
        WHERE bookmark_folder = $bookmark_folder
      }]
      if {$iMax eq ""} {set iMax 1}
 
      foreach child [$P children] {
        set bookmark_id [$child attr bookmark_id]
        incr iMax
        $db eval {
          UPDATE bm_bookmarks1 
          SET bookmark_folder = $bookmark_folder, bookmark_folder_idx = $iMax
          WHERE bookmark_id = $bookmark_id
        }
      }
    }
  }

  dom_call folder_move {THIS node} {
    set db [$myManager db]
    bookmark_transaction $db {
      set N [GetNodeFromObj [lindex $node 1]]
      set P [$N parent]

      set iMax [$db onecolumn {
        SELECT max(folder_id) FROM bm_folders1 
      }]
      if {$iMax eq ""} {set iMax 1}

      foreach child [$P children] {
        if {[catch {set folder_id [$child attr folder_id]}]} continue
        incr iMax
        $db eval {
          UPDATE bm_folders1 SET folder_id = $iMax WHERE folder_id = $folder_id
        }
      }
    }
  }

  dom_call folder_edit {THIS node} {
    set db [$myManager db]
    bookmark_transaction $db {
      set N [GetNodeFromObj [lindex $node 1]]
      foreach v {folder_id folder_name} {
        set $v [$N attribute $v]
      }
      $db eval {
        UPDATE bm_bookmarks1 SET bookmark_folder = $folder_name
        WHERE bookmark_folder = 
            (SELECT folder_name FROM bm_folders1 WHERE folder_id = $folder_id);

        UPDATE bm_folders1 SET folder_name = $folder_name
        WHERE folder_id = $folder_id;
      }
    }
  }

  dom_call folder_hidden {THIS node} {
    set db [$myManager db]
    bookmark_transaction $db {
      set N [GetNodeFromObj [lindex $node 1]]
      foreach v {folder_id folder_hidden} {
        set $v [$N attribute $v]
      }
      $db eval {
        UPDATE bm_folders1 SET folder_hidden = $folder_hidden
        WHERE folder_id = $folder_id
      }
      $db eval {UPDATE bm_version1 SET version = version + 1}
    }
  }

  dom_call bookmark_new {THIS} {
    set db [$myManager db]
    list string [$myManager add {New Bookmark} {}]
  }

  dom_call folder_new {THIS} {
    set db [$myManager db]

    set rc 1
    set msg "column folder_name is not unique"

    $db transaction {
      set idx 1
      while {$rc && $msg eq "column folder_name is not unique"} {
        set rc [catch {
          $db eval {
            INSERT INTO bm_folders1 (
              folder_id, folder_name, folder_hidden 
            ) VALUES(
              (SELECT min(folder_id)-1 FROM bm_folders1), 'New Folder ' || $idx, 0
            );
          }
        } msg]
        incr idx
        $db eval {UPDATE bm_version1 SET version = version + 1}
      }
    }

    list string [$db last_insert_rowid]
  }

  dom_call get_html_content {THIS} {
    set ret ""

    set BookmarkTemplate [$myManager GetBookmarkTemplate]
    set FolderTemplate [$myManager GetFolderTemplate]

    set sql { 
      SELECT 
      bookmark_id, bookmark_name, bookmark_uri, bookmark_tags, 
      bookmark_folder, bookmark_folder_idx, 
      null AS folder_id, 0 AS folder_hidden
      FROM bm_bookmarks1 WHERE bookmark_folder = ''

      UNION ALL

      SELECT 
      bookmark_id, bookmark_name, bookmark_uri, bookmark_tags, 
      folder_name AS bookmark_folder, bookmark_folder_idx, folder_id, 
      folder_hidden
      FROM bm_folders1 LEFT JOIN bm_bookmarks1 ON (bookmark_folder = folder_name)

      ORDER BY folder_id, bookmark_folder_idx
    }

    set current_folder ""
    set folder_name ""
    set folder_id ""
    set folder_hidden 0
    set folder_display none
    set content_display block
    set folder_marker -
    append ret [subst -nocommands $FolderTemplate]
    
    [$myManager db] eval $sql {

      set bookmark_folder [htmlize $bookmark_folder]

      if {$bookmark_folder ne $current_folder} {
        append ret "</UL></DIV>"
        set folder_name $bookmark_folder
        set folder_display block
        set content_display block
        set folder_marker -
        if {$folder_hidden} {
          set content_display none
          set folder_marker +
        }

        append ret [subst -nocommands $FolderTemplate]
        set current_folder $bookmark_folder
      }

      if {$bookmark_id ne ""} {
        set bookmark_name [htmlize $bookmark_name]
        set bookmark_uri  [htmlize $bookmark_uri]
        set bookmark_id   [htmlize $bookmark_id]
        set bookmark_tags [htmlize $bookmark_tags]
        append ret [subst -nocommands $BookmarkTemplate]
      }
    }

    list string $ret
  }

  dom_call get_version {THIS} {
    list string [[$myManager db] onecolumn {SELECT version FROM bm_version1}]
  }
}

eval [::hv3::dom2::compile Bookmarks]

}

namespace eval ::hv3::DOM {
  proc bookmark_transaction {db script} {
    set ret -1
    $db transaction {
      uplevel $script
      $db eval {UPDATE bm_version1 SET version = version + 1}
      set ret [$db one {SELECT version FROM bm_version1}]
    }
    list number $ret
  }
}


