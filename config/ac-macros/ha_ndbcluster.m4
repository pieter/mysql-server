dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_NDBCLUSTER
dnl ---------------------------------------------------------------------------

NDB_VERSION_MAJOR=`echo $VERSION | cut -d. -f1`
NDB_VERSION_MINOR=`echo $VERSION | cut -d. -f2`
NDB_VERSION_BUILD=`echo $VERSION | cut -d. -f3 | cut -d- -f1`
NDB_VERSION_STATUS=`echo $VERSION | cut -d- -f2`
# if there was now -suffix, $NDB_VERSION_STATUS will be the same as $VERSION
if test "$NDB_VERSION_STATUS" = "$VERSION"
then
  NDB_VERSION_STATUS=""
fi
TEST_NDBCLUSTER=""

dnl for build ndb docs

AC_PATH_PROG(DOXYGEN, doxygen, no)
AC_PATH_PROG(PDFLATEX, pdflatex, no)
AC_PATH_PROG(MAKEINDEX, makeindex, no)

AC_SUBST(DOXYGEN)
AC_SUBST(PDFLATEX)
AC_SUBST(MAKEINDEX)


AC_DEFUN([MYSQL_CHECK_NDB_OPTIONS], [
  AC_ARG_WITH([ndb-sci],
              AC_HELP_STRING([--with-ndb-sci=DIR],
                             [Provide MySQL with a custom location of
                             sci library. Given DIR, sci library is 
                             assumed to be in $DIR/lib and header files
                             in $DIR/include.]),
              [mysql_sci_dir=${withval}],
              [mysql_sci_dir=""])

  case "$mysql_sci_dir" in
    "no" )
      have_ndb_sci=no
      AC_MSG_RESULT([-- not including sci transporter])
      ;;
    * )
      if test -f "$mysql_sci_dir/lib/libsisci.a" -a \ 
              -f "$mysql_sci_dir/include/sisci_api.h"; then
        NDB_SCI_INCLUDES="-I$mysql_sci_dir/include"
        NDB_SCI_LIBS="-L$mysql_sci_dir/lib -lsisci"
        AC_MSG_RESULT([-- including sci transporter])
        AC_DEFINE([NDB_SCI_TRANSPORTER], [1],
                  [Including Ndb Cluster DB sci transporter])
        AC_SUBST(NDB_SCI_INCLUDES)
        AC_SUBST(NDB_SCI_LIBS)
        have_ndb_sci="yes"
        AC_MSG_RESULT([found sci transporter in $mysql_sci_dir/{include, lib}])
      else
        AC_MSG_RESULT([could not find sci transporter in $mysql_sci_dir/{include, lib}])
      fi
      ;;
  esac

  AC_ARG_WITH([ndb-test],
              [
  --with-ndb-test       Include the NDB Cluster ndbapi test programs],
              [ndb_test="$withval"],
              [ndb_test=no])
  AC_ARG_WITH([ndb-docs],
              [
  --with-ndb-docs       Include the NDB Cluster ndbapi and mgmapi documentation],
              [ndb_docs="$withval"],
              [ndb_docs=no])
  AC_ARG_WITH([ndb-port],
              [
  --with-ndb-port       Port for NDB Cluster management server],
              [ndb_port="$withval"],
              [ndb_port="default"])
  AC_ARG_WITH([ndb-port-base],
              [
  --with-ndb-port-base  Base port for NDB Cluster transporters],
              [ndb_port_base="$withval"],
              [ndb_port_base="default"])
  AC_ARG_WITH([ndb-debug],
              [
  --without-ndb-debug   Disable special ndb debug features],
              [ndb_debug="$withval"],
              [ndb_debug="default"])
  AC_ARG_WITH([ndb-ccflags],
              AC_HELP_STRING([--with-ndb-ccflags=CFLAGS],
                           [Extra CFLAGS for ndb compile]),
              [ndb_ccflags=${withval}],
              [ndb_ccflags=""])
  AC_ARG_WITH([ndb-binlog],
              [
  --without-ndb-binlog       Disable ndb binlog],
              [ndb_binlog="$withval"],
              [ndb_binlog="default"])

  case "$ndb_ccflags" in
    "yes")
        AC_MSG_RESULT([The --ndb-ccflags option requires a parameter (passed to CC for ndb compilation)])
        ;;
    *)
        ndb_cxxflags_fix="$ndb_cxxflags_fix $ndb_ccflags"
    ;;
  esac

  AC_MSG_CHECKING([for NDB Cluster options])
  AC_MSG_RESULT([])
                                                                                
  have_ndb_test=no
  case "$ndb_test" in
    yes )
      AC_MSG_RESULT([-- including ndbapi test programs])
      have_ndb_test="yes"
      ;;
    * )
      AC_MSG_RESULT([-- not including ndbapi test programs])
      ;;
  esac

  have_ndb_docs=no
  case "$ndb_docs" in
    yes )
      AC_MSG_RESULT([-- including ndbapi and mgmapi documentation])
      have_ndb_docs="yes"
      ;;
    * )
      AC_MSG_RESULT([-- not including ndbapi and mgmapi documentation])
      ;;
  esac

  case "$ndb_debug" in
    yes )
      AC_MSG_RESULT([-- including ndb extra debug options])
      have_ndb_debug="yes"
      ;;
    full )
      AC_MSG_RESULT([-- including ndb extra extra debug options])
      have_ndb_debug="full"
      ;;
    no )
      AC_MSG_RESULT([-- not including ndb extra debug options])
      have_ndb_debug="no"
      ;;
    * )
      have_ndb_debug="default"
      ;;
  esac

  AC_MSG_RESULT([done.])
])

AC_DEFUN([NDBCLUSTER_WORKAROUNDS], [

  #workaround for Sun Forte/x86 see BUG#4681
  case $SYSTEM_TYPE-$MACHINE_TYPE-$ac_cv_prog_gcc in
    *solaris*-i?86-no)
      CFLAGS="$CFLAGS -DBIG_TABLES"
      CXXFLAGS="$CXXFLAGS -DBIG_TABLES"
      ;;
    *)
      ;;
  esac

  # workaround for Sun Forte compile problem for ndb
  case $SYSTEM_TYPE-$ac_cv_prog_gcc in
    *solaris*-no)
      ndb_cxxflags_fix="$ndb_cxxflags_fix -instances=static"
      ;;
    *)
      ;;
  esac

  # ndb fail for whatever strange reason to link Sun Forte/x86
  # unless using incremental linker
  case $SYSTEM_TYPE-$MACHINE_TYPE-$ac_cv_prog_gcc-$have_ndbcluster in
    *solaris*-i?86-no-yes)
      CXXFLAGS="$CXXFLAGS -xildon"
      ;;
    *)
      ;;
  esac
])

AC_DEFUN([MYSQL_SETUP_NDBCLUSTER], [

  AC_MSG_RESULT([Using NDB Cluster])
  with_partition="yes"
  ndb_cxxflags_fix=""
  TEST_NDBCLUSTER="--ndbcluster"

  ndbcluster_includes="-I\$(top_builddir)/storage/ndb/include -I\$(top_srcdir)/storage/ndb/include -I\$(top_srcdir)/storage/ndb/include/ndbapi -I\$(top_srcdir)/storage/ndb/include/mgmapi"
  ndbcluster_libs="\$(top_builddir)/storage/ndb/src/.libs/libndbclient.a"
  ndbcluster_system_libs=""
  ndb_mgmclient_libs="\$(top_builddir)/storage/ndb/src/mgmclient/libndbmgmclient.la"

  MYSQL_CHECK_NDB_OPTIONS
  NDBCLUSTER_WORKAROUNDS

  MAKE_BINARY_DISTRIBUTION_OPTIONS="$MAKE_BINARY_DISTRIBUTION_OPTIONS --with-ndbcluster"

  CXXFLAGS="$CXXFLAGS \$(NDB_CXXFLAGS)"
  if test "$have_ndb_debug" = "default"
  then
    have_ndb_debug=$with_debug
  fi

  if test "$have_ndb_debug" = "yes"
  then
    # Medium debug.
    NDB_DEFS="-DNDB_DEBUG -DVM_TRACE -DERROR_INSERT -DARRAY_GUARD"
  elif test "$have_ndb_debug" = "full"
  then
    NDB_DEFS="-DNDB_DEBUG_FULL -DVM_TRACE -DERROR_INSERT -DARRAY_GUARD"
  else
    # no extra ndb debug but still do asserts if debug version
    if test "$with_debug" = "yes" -o "$with_debug" = "full"
    then
      NDB_DEFS=""
    else
      NDB_DEFS="-DNDEBUG"
    fi
  fi

  if test X"$ndb_port" = Xdefault
  then
    ndb_port="1186"
  fi
  
  have_ndb_binlog="no"
  if test X"$ndb_binlog" = Xdefault ||
     test X"$ndb_binlog" = Xyes
  then
    if test X"$have_row_based" = Xyes
    then
      have_ndb_binlog="yes"
    fi
  fi

  if test X"$have_ndb_binlog" = Xyes
  then
    AC_DEFINE([WITH_NDB_BINLOG], [1],
              [Including Ndb Cluster Binlog])
    AC_MSG_RESULT([Including Ndb Cluster Binlog])
  else
    AC_MSG_RESULT([Not including Ndb Cluster Binlog])
  fi

  ndb_transporter_opt_objs=""
  if test "$ac_cv_func_shmget" = "yes" &&
     test "$ac_cv_func_shmat" = "yes" &&
     test "$ac_cv_func_shmdt" = "yes" &&
     test "$ac_cv_func_shmctl" = "yes" &&
     test "$ac_cv_func_sigaction" = "yes" &&
     test "$ac_cv_func_sigemptyset" = "yes" &&
     test "$ac_cv_func_sigaddset" = "yes" &&
     test "$ac_cv_func_pthread_sigmask" = "yes"
  then
     AC_DEFINE([NDB_SHM_TRANSPORTER], [1],
               [Including Ndb Cluster DB shared memory transporter])
     AC_MSG_RESULT([Including ndb shared memory transporter])
     ndb_transporter_opt_objs="$ndb_transporter_opt_objs SHM_Transporter.lo SHM_Transporter.unix.lo"
  else
     AC_MSG_RESULT([Not including ndb shared memory transporter])
  fi
  
  if test X"$have_ndb_sci" = Xyes
  then
    ndb_transporter_opt_objs="$ndb_transporter_opt_objs SCI_Transporter.lo"
  fi
  
  ndb_opt_subdirs=
  ndb_bin_am_ldflags="-static"
  if test X"$have_ndb_test" = Xyes
  then
    ndb_opt_subdirs="test"
    ndb_bin_am_ldflags=""
  fi

  if test X"$have_ndb_docs" = Xyes
  then
    ndb_opt_subdirs="$ndb_opt_subdirs docs"
    ndb_bin_am_ldflags=""
  fi

  AC_SUBST(NDB_VERSION_MAJOR)
  AC_SUBST(NDB_VERSION_MINOR)
  AC_SUBST(NDB_VERSION_BUILD)
  AC_SUBST(NDB_VERSION_STATUS)
  AC_DEFINE_UNQUOTED([NDB_VERSION_MAJOR], [$NDB_VERSION_MAJOR],
                     [NDB major version])
  AC_DEFINE_UNQUOTED([NDB_VERSION_MINOR], [$NDB_VERSION_MINOR],
                     [NDB minor version])
  AC_DEFINE_UNQUOTED([NDB_VERSION_BUILD], [$NDB_VERSION_BUILD],
                     [NDB build version])
  AC_DEFINE_UNQUOTED([NDB_VERSION_STATUS], ["$NDB_VERSION_STATUS"],
                     [NDB status version])

  AC_SUBST(ndbcluster_includes)
  AC_SUBST(ndbcluster_libs)
  AC_SUBST(ndbcluster_system_libs)
  AC_SUBST(ndb_mgmclient_libs)
  AC_SUBST(NDB_SCI_LIBS)

  AC_SUBST(ndb_transporter_opt_objs)
  AC_SUBST(ndb_port)
  AC_SUBST(ndb_bin_am_ldflags)
  AC_SUBST(ndb_opt_subdirs)

  AC_SUBST(NDB_DEFS)
  AC_SUBST(ndb_cxxflags_fix)

  NDB_SIZEOF_CHARP="$ac_cv_sizeof_charp"
  NDB_SIZEOF_CHAR="$ac_cv_sizeof_char"
  NDB_SIZEOF_SHORT="$ac_cv_sizeof_short"
  NDB_SIZEOF_INT="$ac_cv_sizeof_int"
  NDB_SIZEOF_LONG="$ac_cv_sizeof_long"
  NDB_SIZEOF_LONG_LONG="$ac_cv_sizeof_long_long"
  AC_SUBST([NDB_SIZEOF_CHARP])
  AC_SUBST([NDB_SIZEOF_CHAR])
  AC_SUBST([NDB_SIZEOF_SHORT])
  AC_SUBST([NDB_SIZEOF_INT])
  AC_SUBST([NDB_SIZEOF_LONG])
  AC_SUBST([NDB_SIZEOF_LONG_LONG])

  AC_CONFIG_FILES(storage/ndb/include/Makefile dnl
   storage/ndb/src/Makefile storage/ndb/src/common/Makefile dnl
   storage/ndb/docs/Makefile dnl
   storage/ndb/tools/Makefile dnl
   storage/ndb/src/common/debugger/Makefile dnl
   storage/ndb/src/common/debugger/signaldata/Makefile dnl
   storage/ndb/src/common/portlib/Makefile dnl
   storage/ndb/src/common/util/Makefile dnl
   storage/ndb/src/common/logger/Makefile dnl
   storage/ndb/src/common/transporter/Makefile dnl
   storage/ndb/src/common/mgmcommon/Makefile dnl
   storage/ndb/src/kernel/Makefile dnl
   storage/ndb/src/kernel/error/Makefile dnl
   storage/ndb/src/kernel/blocks/Makefile dnl
   storage/ndb/src/kernel/blocks/dbdict/Makefile dnl
   storage/ndb/src/kernel/blocks/dbdih/Makefile dnl
   storage/ndb/src/kernel/blocks/dblqh/Makefile dnl
   storage/ndb/src/kernel/blocks/dbtup/Makefile dnl
   storage/ndb/src/kernel/blocks/backup/Makefile dnl
   storage/ndb/src/kernel/vm/Makefile dnl
   storage/ndb/src/mgmapi/Makefile dnl
   storage/ndb/src/ndbapi/Makefile dnl
   storage/ndb/src/mgmsrv/Makefile dnl
   storage/ndb/src/mgmclient/Makefile dnl
   storage/ndb/src/cw/Makefile dnl
   storage/ndb/src/cw/cpcd/Makefile dnl
   storage/ndb/test/Makefile dnl
   storage/ndb/test/src/Makefile dnl
   storage/ndb/test/ndbapi/Makefile dnl
   storage/ndb/test/ndbapi/bank/Makefile dnl
   storage/ndb/test/tools/Makefile dnl
   storage/ndb/test/run-test/Makefile dnl
   storage/ndb/include/ndb_version.h storage/ndb/include/ndb_global.h dnl
   storage/ndb/include/ndb_types.h dnl
  )
])

AC_SUBST(TEST_NDBCLUSTER)                                                                                
dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_NDBCLUSTER SECTION
dnl ---------------------------------------------------------------------------
