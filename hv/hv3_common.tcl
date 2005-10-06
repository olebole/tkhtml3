
###########################################################################
# Some handy utilities used by the rest of the hv3 app. The public
# interface to this file consists of the commands:
#
#      swproc
#

# swproc --
# 
#         swproc NAME ARGS BODY
#
#     [swproc] is very similar to the proc command, except any procedure
#     arguments with default values must be specified with switches instead
#     of on the command line. For example, the following are equivalent:
#
#         proc   abc {a {b hello} {c goodbye}} {...}
#         abc one two
#
#         swproc swabc {a {b hello} {c goodbye}} {...}
#         swabc one -b two
#
#     This means, in the above example, that it is possible to call [swabc]
#     and supply a value for parameter "c" but not "b". This is not
#     possible with commands created by regular Tcl [proc].
#
proc swproc {procname arguments script} {
  uplevel [subst {
    proc $procname {args} {
      ::tkhtml::swproc_rt [list $arguments] \$args
      $script
    }
  }]
}
