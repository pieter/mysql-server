%define mysql_version		@VERSION@
%define shared_lib_version	@SHARED_LIB_VERSION@
%define release			1
%define mysqld_user		mysql

%define see_base For a description of MySQL see the base MySQL RPM or http://www.mysql.com

Name: MySQL
Summary:	MySQL: a very fast and reliable SQL database engine
Group:		Applications/Databases
Summary(pt_BR): MySQL: Um servidor SQL r�pido e confi�vel.
Group(pt_BR):	Aplica��es/Banco_de_Dados
Version:	@MYSQL_NO_DASH_VERSION@
Release:	%{release}
Copyright:	GPL / LGPL
Source:		http://www.mysql.com/Downloads/MySQL-@MYSQL_BASE_VERSION@/mysql-%{mysql_version}.tar.gz
Icon:		mysql.gif
URL:		http://www.mysql.com/
Packager:	Lenz Grimmer <lenz@mysql.com>
Vendor:		MySQL AB
Requires: fileutils sh-utils
Provides:	msqlormysql MySQL-server mysql
Obsoletes:	mysql

# Think about what you use here since the first step is to
# run a rm -rf
BuildRoot:    %{_tmppath}/%{name}-%{version}-build

# From the manual
%description
The MySQL(TM) software delivers a very fast, multi-threaded, multi-user,
and robust SQL (Structured Query Language) database server. MySQL Server
is intended for mission-critical, heavy-load production systems as well
as for embedding into mass-deployed software. MySQL is a trademark of
MySQL AB.

The MySQL software has Dual Licensing, which means you can use the MySQL
software free of charge under the GNU General Public License
(http://www.gnu.org/licenses/). You can also purchase commercial MySQL
licenses from MySQL AB if you do not wish to be bound by the terms of
the GPL. See the chapter "Licensing and Support" in the manual for
further info.

The MySQL web site (http://www.mysql.com/) provides the latest
news and information about the MySQL software. Also please see the
documentation and the manual for more information.

%package client
Release: %{release}
Summary: MySQL - Client
Group: Applications/Databases
Summary(pt_BR): MySQL - Cliente
Group(pt_BR): Aplica��es/Banco_de_Dados
Obsoletes: mysql-client
Provides: mysql-client

%description client
This package contains the standard MySQL clients. 

%{see_base}

%description client -l pt_BR
Este pacote cont�m os clientes padr�o para o MySQL.

%package bench
Release: %{release}
Requires: %{name}-client MySQL-DBI-perl-bin perl
Summary: MySQL - Benchmarks and test system
Group: Applications/Databases
Summary(pt_BR): MySQL - Medi��es de desempenho
Group(pt_BR): Aplica��es/Banco_de_Dados
Provides: mysql-bench
Obsoletes: mysql-bench

%description bench
This package contains MySQL benchmark scripts and data.

%{see_base}

%description bench -l pt_BR
Este pacote cont�m medi��es de desempenho de scripts e dados do MySQL.

%package devel
Release: %{release}
Requires: %{name}-client
Summary: MySQL - Development header files and libraries
Group: Applications/Databases
Summary(pt_BR): MySQL - Medi��es de desempenho
Group(pt_BR): Aplica��es/Banco_de_Dados
Provides: mysql-devel
Obsoletes: mysql-devel

%description devel
This package contains the development header files and libraries
necessary to develop MySQL client applications.

%{see_base}

%description devel -l pt_BR
Este pacote cont�m os arquivos de cabe�alho (header files) e bibliotecas 
necess�rias para desenvolver aplica��es clientes do MySQL. 

%package shared
Release: %{release}
Summary: MySQL - Shared libraries
Group: Applications/Databases

%description shared
This package contains the shared libraries (*.so*) which certain
languages and applications need to dynamically load and use MySQL.

%package Max
Release: %{release}
Summary: MySQL - server with Berkeley DB and Innodb support
Group: Applications/Databases
Provides: mysql-Max
Obsoletes: mysql-Max
Requires: MySQL = %{version}

%description Max 
Optional MySQL server binary that supports features
like transactional tables. To active this binary, just install this
package after the MySQL package.

%prep
%setup -n mysql-%{mysql_version}

%build
# The all-static flag is to make the RPM work on different
# distributions. This version tries to put shared mysqlclient libraries
# in a separate package.

BuildMySQL() {
# The --enable-assembler simply does nothing on systems that does not
# support assembler speedups.
sh -c  "PATH=\"${MYSQL_BUILD_PATH:-/bin:/usr/bin}\" \
	CC=\"${MYSQL_BUILD_CC:-gcc}\" \
	CFLAGS=\"${MYSQL_BUILD_CFLAGS:- -O3}\" \
	CXX=\"${MYSQL_BUILD_CXX:-gcc}\" \
	CXXFLAGS=\"${MYSQL_BUILD_CXXFLAGS:- -O3 \
	          -felide-constructors -fno-exceptions -fno-rtti \
		  }\" \
	./configure \
 	    $* \
	    --enable-assembler \
	    --enable-local-infile \
            --with-mysqld-user=%{mysqld_user} \
            --with-unix-socket-path=/var/lib/mysql/mysql.sock \
            --prefix=/ \
	    --with-extra-charsets=complex \
            --exec-prefix=/usr \
            --libexecdir=/usr/sbin \
            --sysconfdir=/etc \
            --datadir=/usr/share \
            --localstatedir=/var/lib/mysql \
            --infodir=%{_infodir} \
            --includedir=/usr/include \
            --mandir=%{_mandir} \
	    --with-comment=\"Official MySQL RPM\";
	    # Add this for more debugging support
	    # --with-debug
	    # Add this for MyISAM RAID support:
	    # --with-raid
	    "

 # benchdir does not fit in above model. Maybe a separate bench distribution
 make benchdir_root=$RPM_BUILD_ROOT/usr/share/
}

# Use our own copy of glibc

OTHER_LIBC_DIR=/usr/local/mysql-glibc
USE_OTHER_LIBC_DIR=""
if test -d "$OTHER_LIBC_DIR"
then
  USE_OTHER_LIBC_DIR="--with-other-libc=$OTHER_LIBC_DIR"
fi

# Use the build root for temporary storage of the shared libraries.

RBR=$RPM_BUILD_ROOT
MBD=$RPM_BUILD_DIR/mysql-%{mysql_version}

# Clean up the BuildRoot first
[ "$RBR" != "/" ] && [ -d $RBR ] && rm -rf $RBR;
mkdir -p $RBR

#
# Use MYSQL_BUILD_PATH so that we can use a dedicated version of gcc
#
PATH=${MYSQL_BUILD_PATH:-/bin:/usr/bin}
export PATH

# We need to build shared libraries separate from mysqld-max because we
# are using --with-other-libc

BuildMySQL "--disable-shared $USE_OTHER_LIBC_DIR --with-berkeley-db --with-innodb --with-mysqld-ldflags='-all-static' --with-server-suffix='-Max'"

# Save everything for debug
# tar cf $RBR/all.tar .

# Save mysqld-max
mv sql/mysqld sql/mysqld-max
nm --numeric-sort sql/mysqld-max > sql/mysqld-max.sym

# Save manual to avoid rebuilding
mv Docs/manual.ps Docs/manual.ps.save
make distclean
mv Docs/manual.ps.save Docs/manual.ps

#now build and save shared libraries
BuildMySQL "--enable-shared --enable-thread-safe-client --without-server "
(cd libmysql/.libs; tar cf $RBR/shared-libs.tar *.so*)
(cd libmysql_r/.libs; tar rf $RBR/shared-libs.tar *.so*)

# Save manual to avoid rebuilding
mv Docs/manual.ps Docs/manual.ps.save
make distclean
mv Docs/manual.ps.save Docs/manual.ps

# RPM:s destroys Makefile.in files, so we generate them here
automake

BuildMySQL "--disable-shared" \
	   "--with-mysqld-ldflags='-all-static'" \
	   "--with-client-ldflags='-all-static'" \
  	   "$USE_OTHER_LIBC_DIR" \
	   "--without-berkeley-db --without-innodb"
nm --numeric-sort sql/mysqld > sql/mysqld.sym

%install -n mysql-%{mysql_version}
RBR=$RPM_BUILD_ROOT
MBD=$RPM_BUILD_DIR/mysql-%{mysql_version}
# Ensure that needed directories exists
install -d $RBR/etc/{logrotate.d,rc.d/init.d}
install -d $RBR/var/lib/mysql/mysql
install -d $RBR/usr/share/{sql-bench,mysql-test}
install -d $RBR%{_mandir}
install -d $RBR/usr/{sbin,lib,include}

# Install all binaries stripped
make install-strip DESTDIR=$RBR benchdir_root=/usr/share/

# Install shared libraries (Disable for architectures that don't support it)
(cd $RBR/usr/lib; tar xf $RBR/shared-libs.tar)

# install and strip saved mysqld-max
install -s -m755 $MBD/sql/mysqld-max $RBR/usr/sbin/mysqld-max

# install symbol files ( for stack trace resolution)
install -m644 $MBD/sql/mysqld-max.sym $RBR/usr/lib/mysql/mysqld-max.sym
install -m644 $MBD/sql/mysqld.sym $RBR/usr/lib/mysql/mysqld.sym

# Install logrotate and autostart
install -m644 $MBD/support-files/mysql-log-rotate $RBR/etc/logrotate.d/mysql
install -m755 $MBD/support-files/mysql.server $RBR/etc/rc.d/init.d/mysql

%pre
if test -x /etc/rc.d/init.d/mysql
then
  /etc/rc.d/init.d/mysql stop > /dev/null 2>&1
  echo "Giving mysqld a couple of seconds to exit nicely"
  sleep 5
fi

%post
mysql_datadir=/var/lib/mysql

# Create data directory if needed
if test ! -d $mysql_datadir;		then mkdir $mysql_datadir; fi
if test ! -d $mysql_datadir/mysql;	then mkdir $mysql_datadir/mysql; fi
if test ! -d $mysql_datadir/test;	then mkdir $mysql_datadir/test; fi

# Make MySQL start/shutdown automatically when the machine does it.
/sbin/chkconfig --add mysql

# Create a MySQL user. Do not report any problems if it already
# exists. This is redhat specific and should be handled more portable
useradd -M -r -d $mysql_datadir -s /bin/bash -c "MySQL server" mysql 2> /dev/null || true 

# Change permissions so that the user that will run the MySQL daemon
# owns all database files.
chown -R mysql $mysql_datadir

# Initiate databases
mysql_install_db -IN-RPM

# Change permissions again to fix any new files.
chown -R mysql $mysql_datadir

# Fix permissions for the permission database so that only the user
# can read them.
chmod -R og-rw $mysql_datadir/mysql

# Restart in the same way that mysqld will be started normally.
/etc/rc.d/init.d/mysql start

# Allow safe_mysqld to start mysqld and print a message before we exit
sleep 2

%post Max
# Restart mysqld, to use the new binary.
# There may be a better way to handle this.
/etc/rc.d/init.d/mysql stop > /dev/null 2>&1
echo "Giving mysqld a couple of seconds to restart"
sleep 5
/etc/rc.d/init.d/mysql start
sleep 2

%preun
if test $1 = 0
then
  if test -x /etc/rc.d/init.d/mysql
  then
    /etc/rc.d/init.d/mysql stop > /dev/null
  fi

  # Remove autostart of mysql
  /sbin/chkconfig --del mysql
fi
# We do not remove the mysql user since it may still own a lot of
# database files.

%clean
[ "$RBR" != "/" ] && [ -d $RBR ] && rm -rf $RBR;

%files
%defattr(-, root, root)
%doc %attr(644, root, root) COPYING COPYING.LIB README
%doc %attr(644, root, root) Docs/manual.{html,ps,texi,txt} Docs/manual_toc.html
%doc %attr(644, root, root) support-files/my-*.cnf

%doc %attr(644, root, root) %{_infodir}/mysql.info*

%doc %attr(644, root, man) %{_mandir}/man1/isamchk.1*
%doc %attr(644, root, man) %{_mandir}/man1/isamlog.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysql_zap.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqld.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqld_multi.1*
%doc %attr(644, root, man) %{_mandir}/man1/safe_mysqld.1*
%doc %attr(644, root, man) %{_mandir}/man1/perror.1*
%doc %attr(644, root, man) %{_mandir}/man1/replace.1*

%attr(755, root, root) /usr/bin/isamchk
%attr(755, root, root) /usr/bin/isamlog
%attr(755, root, root) /usr/bin/my_print_defaults
%attr(755, root, root) /usr/bin/myisamchk
%attr(755, root, root) /usr/bin/myisamlog
%attr(755, root, root) /usr/bin/myisampack
%attr(755, root, root) /usr/bin/mysql_convert_table_format
%attr(755, root, root) /usr/bin/mysql_fix_privilege_tables
%attr(755, root, root) /usr/bin/mysql_install_db
%attr(755, root, root) /usr/bin/mysql_setpermission
%attr(755, root, root) /usr/bin/mysql_zap
%attr(755, root, root) /usr/bin/mysqlbug
%attr(755, root, root) /usr/bin/mysqld_multi
%attr(755, root, root) /usr/bin/mysqldumpslow
%attr(755, root, root) /usr/bin/mysqlhotcopy
%attr(755, root, root) /usr/bin/mysqltest
%attr(755, root, root) /usr/bin/pack_isam
%attr(755, root, root) /usr/bin/perror
%attr(755, root, root) /usr/bin/replace
%attr(755, root, root) /usr/bin/resolve_stack_dump
%attr(755, root, root) /usr/bin/resolveip
%attr(755, root, root) /usr/bin/safe_mysqld

%attr(755, root, root) /usr/sbin/mysqld
%attr(644, root, root) /usr/lib/mysql/mysqld.sym

%attr(644, root, root) /etc/logrotate.d/mysql
%attr(755, root, root) /etc/rc.d/init.d/mysql

%attr(755, root, root) /usr/share/mysql/

%files client
%defattr(-, root, root)
%attr(755, root, root) /usr/bin/msql2mysql
%attr(755, root, root) /usr/bin/mysql
%attr(755, root, root) /usr/bin/mysql_find_rows
%attr(755, root, root) /usr/bin/mysqlaccess
%attr(755, root, root) /usr/bin/mysqladmin
%attr(755, root, root) /usr/bin/mysqlbinlog
%attr(755, root, root) /usr/bin/mysqlcheck
%attr(755, root, root) /usr/bin/mysqldump
%attr(755, root, root) /usr/bin/mysqlimport
%attr(755, root, root) /usr/bin/mysqlshow

%doc %attr(644, root, man) %{_mandir}/man1/mysql.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqlaccess.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqladmin.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqldump.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqlshow.1*

%post shared
/sbin/ldconfig

%postun shared
/sbin/ldconfig

%files devel
%defattr(644 root, root)
%attr(755, root, root) /usr/bin/comp_err
%attr(755, root, root) /usr/bin/mysql_config
%dir %attr(755, root, root) /usr/include/mysql
%dir %attr(755, root, root) /usr/lib/mysql
/usr/include/mysql/*
/usr/lib/mysql/*.a

%files shared
%defattr(755 root, root)
# Shared libraries (omit for architectures that don't support them)
/usr/lib/*.so*

%files bench
%defattr(-, root, root)
%attr(-, root, root) /usr/share/sql-bench
%attr(-, root, root) /usr/share/mysql-test

%files Max
%defattr(-, root, root)
%attr(755, root, root) /usr/sbin/mysqld-max
%attr(644, root, root) /usr/lib/mysql/mysqld-max.sym

%changelog 

* Tue Sep 24 2002 Lenz Grimmer <lenz@mysql.com>

- MySQL-Max now requires MySQL to be the same version (to
  avoid version mismatches e.g. mixing 3.23.xx and 4.0 packages)

* Thu Jul 30 2002 Lenz Grimmer <lenz@mysql.com>

- Use some more macros (mandir and infodir)
- Updated package description
- Install binaries stripped to save disk space
- Rearranged file list (make sure man pages are in
  the same package as the binaries)
- clean up the BuildRoot directory afterwards
- added mysqldumpslow to the server package

* Mon Jul 15 2002 Lenz Grimmer <lenz@mysql.com>

- updated Packager tag

* Fri Feb 15 2002 Sasha

- changed build to use --with-other-libc

* Fri Apr 13 2001 Monty

- Added mysqld-max to the distribution

* Tue Jan 2  2001  Monty

- Added mysql-test to the bench package

* Fri Aug 18 2000 Tim Smith <tim@mysql.com>

- Added separate libmysql_r directory; now both a threaded
  and non-threaded library is shipped.

* Wed Sep 28 1999 David Axmark <davida@mysql.com>

- Added the support-files/my-example.cnf to the docs directory.

- Removed devel dependency on base since it is about client
  development.

* Wed Sep 8 1999 David Axmark <davida@mysql.com>

- Cleaned up some for 3.23.

* Thu Jul 1 1999 David Axmark <davida@mysql.com>

- Added support for shared libraries in a separate sub
  package. Original fix by David Fox (dsfox@cogsci.ucsd.edu)

- The --enable-assembler switch is now automatically disables on
  platforms there assembler code is unavailable. This should allow
  building this RPM on non i386 systems.

* Mon Feb 22 1999 David Axmark <david@detron.se>

- Removed unportable cc switches from the spec file. The defaults can
  now be overridden with environment variables. This feature is used
  to compile the official RPM with optimal (but compiler version
  specific) switches.

- Removed the repetitive description parts for the sub rpms. Maybe add
  again if RPM gets a multiline macro capability.

- Added support for a pt_BR translation. Translation contributed by
  Jorge Godoy <jorge@bestway.com.br>.

* Wed Nov 4 1998 David Axmark <david@detron.se>

- A lot of changes in all the rpm and install scripts. This may even
  be a working RPM :-)

* Sun Aug 16 1998 David Axmark <david@detron.se>

- A developers changelog for MySQL is available in the source RPM. And
  there is a history of major user visible changed in the Reference
  Manual.  Only RPM specific changes will be documented here.
