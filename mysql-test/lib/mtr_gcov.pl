# -*- cperl -*-

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;

# These are not to be prefixed with "mtr_"

sub gcov_prepare ();
sub gcov_collect ();

##############################################################################
#
#  
#
##############################################################################

sub gcov_prepare () {

  `find $::glob_basedir -name \*.gcov \
    -or -name \*.da | xargs rm`;
}

sub gcov_collect () {

  print "Collecting source coverage info...\n";
  -f $::opt_gcov_msg and unlink($::opt_gcov_msg);
  -f $::opt_gcov_err and unlink($::opt_gcov_err);
  foreach my $d ( @::mysqld_src_dirs )
  {
    chdir("$::glob_basedir/$d");
    foreach my $f ( (glob("*.h"), glob("*.cc"), glob("*.c")) )
    {
      `$::opt_gcov $f 2>>$::opt_gcov_err  >>$::opt_gcov_msg`;
    }
    chdir($::glob_mysql_test_dir);
  }
  print "gcov info in $::opt_gcov_msg, errors in $::opt_gcov_err\n";
}


1;
