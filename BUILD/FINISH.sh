cflags="$c_warnings $extra_flags"
cxxflags="$cxx_warnings $base_cxxflags $extra_flags"
configure="./configure $base_configs $extra_configs"
for arg
do
  # Escape special characters so they don't confuse eval
  configure="$configure "`echo "$arg" | \
  		sed -e 's,\([^a-zA-Z0-9_.=-]\),\\\\\1,g'`
done

commands="\
$make -k clean || true 
/bin/rm -f */.deps/*.P config.cache

aclocal; autoheader; aclocal; automake; autoconf

CFLAGS=\"$cflags\" CXX=gcc CXXFLAGS=\"$cxxflags\" $configure

$make $AM_MAKEFLAGS"

if [ "x$strip" = "xyes" ]
then
  commands="$commands

mkdir tmp
nm --numeric-sort sql/mysqld  > tmp/mysqld.sym
objdump -d sql/mysqld > tmp/mysqld.S
strip sql/mysqld"
fi  

if test -z "$nonono"
then
  eval "set -x; $commands"
else
  echo "$commands"
fi
