#!/bin/sh
# The default path should be /usr/local

# Get some info from configure
# chmod +x ./scripts/setsomevars

machine=@MACHINE_TYPE@
system=@SYSTEM_TYPE@
version=@VERSION@
export machine system version
SOURCE=`pwd` 
CP="cp -p"

STRIP=1
DEBUG=0
SILENT=0
TMP=/tmp
SUFFIX=""

parse_arguments() {
  for arg do
    case "$arg" in
      --debug)    DEBUG=1;;
      --tmp=*)    TMP=`echo "$arg" | sed -e "s;--tmp=;;"` ;;
      --suffix=*) SUFFIX=`echo "$arg" | sed -e "s;--suffix=;;"` ;;
      --no-strip) STRIP=0 ;;
      --silent)   SILENT=1 ;;
      *)
	echo "Unknown argument '$arg'"
	exit 1
        ;;
    esac
  done
}

parse_arguments "$@"

#make

# This should really be integrated with automake and not duplicate the
# installation list.

BASE=$TMP/my_dist$SUFFIX

if [ -d $BASE ] ; then
 rm -r -f $BASE
fi

mkdir $BASE $BASE/bin $BASE/data $BASE/data/mysql $BASE/data/test \
 $BASE/include $BASE/lib $BASE/support-files $BASE/share $BASE/share/mysql \
 $BASE/tests $BASE/scripts $BASE/sql-bench $BASE/mysql-test \
 $BASE/mysql-test/t  $BASE/mysql-test/r \
 $BASE/mysql-test/include $BASE/mysql-test/std_data $BASE/man $BASE/man/man1
 
chmod o-rwx $BASE/data $BASE/data/*

for i in ChangeLog COPYING COPYING.LIB README Docs/INSTALL-BINARY \
         MySQLEULA.txt Docs/manual.html Docs/manual.txt Docs/manual_toc.html
do
  if [ -f $i ]
  then
    $CP $i $BASE
  fi
done

for i in extra/comp_err extra/replace extra/perror extra/resolveip \
  extra/my_print_defaults extra/resolve_stack_dump \
  isam/isamchk isam/pack_isam myisam/myisamchk \
  myisam/myisampack sql/mysqld client/mysqlbinlog \
  client/mysql sql/mysqld client/mysqlshow client/mysqlcheck \
  client/mysqladmin client/mysqldump client/mysqlimport client/mysqltest \
  client/mysqlmanagerc client/mysqlmanager-pwgen tools/mysqlmanager \
  client/.libs/mysql client/.libs/mysqlshow client/.libs/mysqladmin \
  client/.libs/mysqldump client/.libs/mysqlimport client/.libs/mysqltest \
  client/.libs/mysqlcheck client/.libs/mysqlbinlog \
  client/.libs/mysqlmanagerc client/.libs/mysqlmanager-pwgen \
  tools/.libs/mysqlmanager
do
  if [ -f $i ]
  then
    $CP $i $BASE/bin
  fi
done

if [ x$STRIP = x1 ] ; then
  strip $BASE/bin/*
fi

for i in sql/mysqld.sym.gz
do
  if [ -f $i ]
  then
    $CP $i $BASE/bin
  fi
done

for i in libmysql/.libs/libmysqlclient.a libmysql/.libs/libmysqlclient.so* libmysql/libmysqlclient.* libmysql_r/.libs/libmysqlclient_r.a libmysql_r/.libs/libmysqlclient_r.so* libmysql_r/libmysqlclient_r.* mysys/libmysys.a strings/libmystrings.a dbug/libdbug.a libmysqld/.libs/libmysqld.a libmysqld/.libs/libmysqld.so* libmysqld/libmysqld.a
do
  if [ -f $i ]
  then
    $CP $i $BASE/lib
   fi
done

$CP config.h include/* $BASE/include
rm $BASE/include/Makefile*; rm $BASE/include/*.in

$CP tests/*.res tests/*.tst tests/*.pl $BASE/tests
$CP support-files/* $BASE/support-files
$CP man/*.1 $BASE/man/man1

$CP -r sql/share/* $BASE/share/mysql
rm -f $BASE/share/mysql/Makefile* $BASE/share/mysql/*/*.OLD

$CP mysql-test/mysql-test-run mysql-test/install_test_db $BASE/mysql-test/
$CP mysql-test/README $BASE/mysql-test/README
$CP mysql-test/include/*.inc $BASE/mysql-test/include
$CP mysql-test/std_data/*.dat mysql-test/std_data/*.001 $BASE/mysql-test/std_data
$CP mysql-test/t/*.test mysql-test/t/*.opt mysql-test/t/*.sh $BASE/mysql-test/t
$CP mysql-test/r/*.result mysql-test/r/*.require $BASE/mysql-test/r

$CP scripts/* $BASE/bin
rm -f $BASE/bin/Makefile* $BASE/bin/*.in $BASE/bin/*.sh $BASE/bin/mysql_install_db $BASE/bin/make_binary_distribution $BASE/bin/setsomevars $BASE/support-files/Makefile* $BASE/support-files/*.sh

$BASE/bin/replace \@localstatedir\@ ./data \@bindir\@ ./bin \@scriptdir\@ ./bin \@libexecdir\@ ./bin \@sbindir\@ ./bin \@prefix\@ . \@HOSTNAME\@ @HOSTNAME@ < $SOURCE/scripts/mysql_install_db.sh > $BASE/scripts/mysql_install_db
$BASE/bin/replace \@prefix\@ /usr/local/mysql \@bindir\@ ./bin \@MYSQLD_USER\@ root \@localstatedir\@ /usr/local/mysql/data \@HOSTNAME\@ @HOSTNAME@ < $SOURCE/support-files/mysql.server.sh > $BASE/support-files/mysql.server
$BASE/bin/replace /my/gnu/bin/hostname /bin/hostname -- $BASE/bin/mysqld_safe

# Make safe_mysqld a symlink to mysqld_safe for backwards portability
# To be removed in MySQL 4.1
(cd $BASE/bin ; ln -s mysqld_safe safe_mysqld )

mv $BASE/support-files/binary-configure $BASE/configure
chmod a+x $BASE/bin/* $BASE/scripts/* $BASE/support-files/mysql-* $BASE/support-files/mysql.server $BASE/configure
$CP -r sql-bench/* $BASE/sql-bench
rm -f $BASE/sql-bench/*.sh $BASE/sql-bench/Makefile* $BASE/lib/*.la

# Clean up if we did this from a bk tree
if [ -d $BASE/sql-bench/SCCS ] ; then 
  find $BASE/share -name SCCS -print | xargs rm -r -f
  find $BASE/sql-bench -name SCCS -print | xargs rm -r -f
fi

# Change the distribution to a long descriptive name
NEW_NAME=mysql@MYSQL_SERVER_SUFFIX@-$version-$system-$machine$SUFFIX
BASE2=$TMP/$NEW_NAME
rm -r -f $BASE2
mv $BASE $BASE2
BASE=$BASE2
#
# If we are compiling with gcc, copy libgcc.a to the distribution as libmygcc.a
#

if test "@GXX@" = "yes"
then
  cd $BASE/lib
  gcclib=`@CC@ --print-libgcc-file`
  if test $? -ne 0
  then
    print "Warning: Couldn't find libgcc.a!"
  else
    $CP $gcclib libmygcc.a
  fi
  cd $SOURCE
fi

#if we are debugging, do not do tar/gz
if [ x$DEBUG = x1 ] ; then
 exit
fi

# This is needed to prefere gnu tar instead of tar because tar can't
# always handle long filenames

PATH_DIRS=`echo $PATH | sed -e 's/^:/. /' -e 's/:$/ ./' -e 's/::/ . /g' -e 's/:/ /g' `
which_1 ()
{
  for cmd
  do
    for d in $PATH_DIRS
    do
      for file in $d/$cmd
      do
	if test -x $file -a ! -d $file
	then
	  echo $file
	  exit 0
	fi
      done
    done
  done
  exit 1
}

#
# Create the result tar file
#

tar=`which_1 gtar`
if test "$?" = "1" -o "$tar" = ""
then
  tar=tar
fi

echo "Using $tar to create archive"
cd $TMP

OPT=cvf
if [ x$SILENT = x1 ] ; then
  OPT=cf
fi

$tar $OPT $SOURCE/$NEW_NAME.tar $NEW_NAME
cd $SOURCE
echo "Compressing archive"
gzip -9 $NEW_NAME.tar
echo "Removing temporary directory"
rm -r -f $BASE

echo "$NEW_NAME.tar.gz created"
