#!/usr/local/bin/wish4.2

###############################################################################
# Miscellaneous global variables
set npresets 0

###############################################################################
# Variables that control main application
set c(port)  0
set c(loss)  0
set c(min_delay) 0
set c(max_delay) 0
set c(dup_pr) 0

###############################################################################
# functions to be implemented in C

###############################################################################
proc set_params {lr min max dup_pr} {
    global c
    set c(loss)      $lr
    set c(min_delay) $min
    set c(max_delay) $max
    set c(dup_pr)    $dup_pr
    update_engine 
}

# loads presets from file <name>
proc load_presets {name} {
    global npresets loss_rate min_delay max_delay dup_pr
    set f [open $name r]
    set cnt 0
    while {![eof $f]} {
	gets $f l
	if {[string first "#" $f]==-1} { 
	    # comment string in presets file is #
	    if {[scan $l "%f %d %d %f" loss_rate($cnt) min_delay($cnt) max_delay($cnt) dup_pr($cnt)]!=-1} {
		incr cnt
	    } else {
		puts "warning preset line not recognized:\n $l"
	    }
	}
    }
    close $f
    set npresets $cnt
    return $cnt;
}

proc update_from_remote {} {
    global c
    query_engine $c(port)
}

###############################################################################
# drop down list (mickey mouse widget)
proc dropdown {w varName command argx} {
    global $varName c
    set args [split $argx]

    set firstValue [lindex $args 0]
    if [info exists $varName] {
        set $varName $firstValue
    }

    menubutton $w -textvariable var -indicatoron 0 -menu $w.menu -text $firstValue -textvariable $varName -relief raised 
    menu $w.menu -tearoff 0
    foreach i $args {
        $w.menu add radiobutton -variable $varName -label $i -value $i -command "$command [lindex $i 0]" 
    }
    return $w.menu
}

###############################################################################
# The UI

frame .fport
label .fport.title -text "Port" -justify right
set l [split [get_ports]]
dropdown .fport.ports c(port) query_engine $l
pack .fport -side top -fill x -expand 0 -side top
pack .fport.title -side left -expand 1 -anchor e
pack .fport.ports -side right -anchor w -expand 1 

frame .cmd 
button .cmd.refresh -text "Refresh" -command { query_engine $c(port) }
button .cmd.apply   -text "Apply"   -command { update_engine }
button .cmd.quit    -text "Quit"    -command { ui_exit }
pack .cmd -side bottom -fill x
pack .cmd.refresh .cmd.apply .cmd.quit -side left -anchor w -fill x -expand 1

frame .ms
pack .ms -side left -fill x -expand 0

###############################################################################
# preset buttons

catch {
    load_presets presets
    frame .ms.presets
    pack .ms.presets -side left 

    set i 0
    while {$i<$npresets} {
	button .ms.presets.b$i -text "Preset $i" -command "set_params $loss_rate($i) $min_delay($i) $max_delay($i) dup_pr($i)"
	pack .ms.presets.b$i -side top -fill x -expand 1
	incr i
    }
    set dummy ""
} load_error

if {$load_error != ""} {
    puts "Load settings: $load_error"
}

frame .ms.settings
pack .ms.settings -side right 

frame .ms.settings.l
pack .ms.settings.l -side left -fill y -expand 1
frame .ms.settings.r
pack .ms.settings.r -side right -fill y -expand 1

label .ms.settings.l.l0 -anchor w -text "Loss (%)" 
label .ms.settings.l.l1 -anchor w -text "Min delay (ms)" 
label .ms.settings.l.l2 -anchor w -text "Max delay (ms)" 
label .ms.settings.l.ld -anchor w -text "Duplication (%)"
pack .ms.settings.l.l0 .ms.settings.l.l1 .ms.settings.l.l2 .ms.settings.l.ld -side top -fill x -pady 2

entry .ms.settings.r.e0 -textvariable c(loss) -justify right 
entry .ms.settings.r.e1 -textvariable c(min_delay) -justify right 
entry .ms.settings.r.e2 -textvariable c(max_delay) -justify right
entry .ms.settings.r.e3 -textvariable c(dup_pr) -justify right 
pack  .ms.settings.r.e0 .ms.settings.r.e1 .ms.settings.r.e2 .ms.settings.r.e3 -side top -fill x -pady 1

query_engine $c(port)








