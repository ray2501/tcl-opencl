# -*- tcl -*-
# Tcl package index file, version 1.1
#
if {[package vsatisfies [package provide Tcl] 9.0-]} {
    package ifneeded opencl 0.7 \
	    [list load [file join $dir libtcl9opencl0.7.so] [string totitle opencl]]
} else {
    package ifneeded opencl 0.7 \
	    [list load [file join $dir libopencl0.7.so] [string totitle opencl]]
}
