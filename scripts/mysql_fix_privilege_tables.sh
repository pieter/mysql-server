#!/bin/sh
# This script is a wrapper to pipe the mysql_fix_privilege_tables.sql
# through the mysql client program to the mysqld server

# Default values (Can be changed in my.cnf)
password=""
host="localhost"
user="root"
sql_only=0
basedir=""
verbose=0
args=""
port=""
socket=""
database="mysql"
bindir=""

file=mysql_fix_privilege_tables.sql

# The following code is almost identical to the code in mysql_install_db.sh

parse_arguments() {
  # We only need to pass arguments through to the server if we don't
  # handle them here.  So, we collect unrecognized options (passed on
  # the command line) into the args variable.
  pick_args=
  if test "$1" = PICK-ARGS-FROM-ARGV
  then
    pick_args=1
    shift
  fi

  for arg do
    case "$arg" in
      --basedir=*) basedir=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
      --user=*) user=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
      --password=*) password=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
      --host=*) host=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
      --sql|--sql-only) sql_only=1;;
      --verbose) verbose=1 ;;
      --port=*) port=`echo "$arg" | sed -e "s;--port=;;"` ;;
      --socket=*) socket=`echo "$arg" | sed -e "s;--socket=;;"` ;;
      --database=*) database=`echo "$arg" | sed -e "s;--database=;;"` ;;
      --bindir=*) bindir=`echo "$arg" | sed -e "s;--bindir=;;"` ;;
      *)
        if test -n "$pick_args"
        then
          # This sed command makes sure that any special chars are quoted,
          # so the arg gets passed exactly to the server.
          args="$args "`echo "$arg" | sed -e 's,\([^a-zA-Z0-9_.-]\),\\\\\1,g'`
        fi
        ;;
    esac
  done
}

# Get first arguments from the my.cfg file, groups [mysqld] and
# [mysql_install_db], and then merge with the command line arguments

for dir in ./bin @bindir@ @bindir@ extra $bindir/../bin $bindir/../extra
do
  if test -x $dir/my_print_defaults
  then
    print_defaults="$dir/my_print_defaults"
    break
  fi
done

parse_arguments `$print_defaults $defaults mysql_install_db mysql_fix_privilege_tables`
parse_arguments PICK-ARGS-FROM-ARGV "$@"

if test -z "$basedir"
then
  basedir=@prefix@
  if test -z "$bindir"
  then
     bindir=@bindir@
  fi
  execdir=@libexecdir@ 
  pkgdatadir=@pkgdatadir@
else
  if test -z "$bindir"
  then
    bindir="$basedir/bin"
  fi
  if test -x "$basedir/libexec/mysqld"
  then
    execdir="$basedir/libexec"
  elif test -x "@libexecdir@/mysqld"
  then
    execdir="@libexecdir@"
  else
    execdir="$basedir/bin"
  fi
fi

# The following test is to make this script compatible with the 4.0 where
# the first argument was the password
if test -z "$password"
then
  password=`echo $args | sed -e 's/ *//g'`
fi

cmd="$bindir/mysql -f --user=$user --host=$host"
if test -z "$password" ; then
  cmd="$cmd --password=$password"
fi
if test ! -z "$port"; then
  cmd="$cmd --port=$port"
fi
if test ! -z "$socket"; then
  cmd="$cmd --socket=$socket"
fi
cmd="$cmd --database=$database"

if test $sql_only = 1
then
  cmd="cat"
fi

# Find where first mysql_fix_privilege_tables.sql is located
for i in $basedir/support-files $basedir/share $basedir/share/mysql \
        $basedir/scripts @pkgdatadir@ . ./scripts
do
  if test -f $i/$file
  then
    pkgdatadir=$i
    break
  fi
done

sql_file="$pkgdatadir/$file"
if test ! -f $sql_file
then
  echo "Could not find file '$file'."
  echo "Please use --basedir to specify the directory where MySQL is installed"
  exit 1
fi

s_echo()
{
   if test $sql_only = 0
   then
     echo $1
   fi
}

s_echo "This scripts updates all the mysql privilege tables to be usable by"
s_echo "MySQL 4.0 and above."
s_echo ""
s_echo "This is needed if you want to use the new GRANT functions,"
s_echo "CREATE AGGREGATE FUNCTION or want to use the more secure passwords in 4.1"
s_echo ""

if test $verbose = 1
then
  s_echo "You can safely ignore all 'Duplicate column' and 'Unknown column' errors"
  s_echo "as this just means that your tables where already up to date."
  s_echo "This script is safe to run even if your tables are already up to date!"
  s_echo ""
fi

if test $verbose = 0
then
  cat $sql_file | $cmd > /dev/null 2>&1
else
  cat $sql_file | $cmd > /dev/null
fi
if test $? = 0
then
  s_echo "done"
else
  s_echo "Got a failure from command:"
  s_echo "$cmd"
  s_echo "Please check the above output and try again."
  if test $verbose = 0
  then
    s_echo ""
    s_echo "Running the script with the --verbose option may give you some information"
    s_echo "of what went wrong."
  fi
  s_echo ""
  s_echo "If you get an 'Access denied' error, you should run this script again and"
  s_echo "give the MySQL root user password as an argument with the --password= option"
fi
