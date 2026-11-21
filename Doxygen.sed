s/^NODISCARD/[[nodiscard]]/
s/^PJL_DISCARD//

##
# Hide same-named typedef enums.
#
# TYPEDEF_HIDES_STRUCT doesn't work for stand-alone typedefs.
##
s/^[ \t]*typedef[ \t][ \t]*enum[ \t][ \t]*\([A-Za-z_][A-Za-z0-9_]*\)[ \t][ \t]*\1[ \t]*;//
