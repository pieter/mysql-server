#!@PERL@

use strict;
use Getopt::Long;
use Data::Dumper;
use File::Basename;
use File::Path;
use DBI;

=head1 NAME

mysqlhotcopy - fast on-line hot-backup utility for local MySQL databases

=head1 SYNOPSIS

  mysqlhotcopy db_name

  mysqlhotcopy --suffix=_copy db_name_1 ... db_name_n

  mysqlhotcopy db_name_1 ... db_name_n /path/to/new_directory

WARNING: THIS IS VERY MUCH A FIRST-CUT ALPHA. Comments/patches welcome.

=cut

# Documentation continued at end of file

my $VERSION = "1.7";

my $OPTIONS = <<"_OPTIONS";

Usage: $0 db_name [new_db_name | directory]

  -?, --help           display this helpscreen and exit
  -u, --user=#         user for database login if not current user
  -p, --password=#     password to use when connecting to server
  -P, --port=#         port to use when connecting to local server
  -S, --socket=#       socket to use when connecting to local server

  --allowold           don't abort if target already exists (rename it _old)
  --keepold            don't delete previous (now renamed) target when done
  --noindices          don't copy index files
  --method=#           method for copy (only "cp" currently supported)

  -q, --quiet          be silent except for errors
  --debug              enable debug
  -n, --dryrun         report actions without doing them

  --regexp=#           copy all databases with names matching regexp
  --suffix=#           suffix for names of copied databases
  --checkpoint=#       insert checkpoint entry into specified db.table
  --flushlog           flush logs once all tables are locked 
_OPTIONS

sub usage {
    die @_, $OPTIONS;
}

my %opt = (
    user	=> getpwuid($>),
    indices	=> 1,	# for safety
    allowold	=> 0,	# for safety
    keepold	=> 0,
    method	=> "cp",
    flushlog    => 0,
);
Getopt::Long::Configure(qw(no_ignore_case)); # disambuguate -p and -P
GetOptions( \%opt,
    "help",
    "user|u=s",
    "password|p=s",
    "port|P=s",
    "socket|S=s",
    "allowold!",
    "keepold!",
    "indices!",
    "method=s",
    "debug",
    "quiet|q",
    "mv!",
    "regexp=s",
    "suffix=s",
    "checkpoint=s",
    "flushlog",
    "dryrun|n",
) or usage("Invalid option");

# @db_desc
# ==========
# a list of hash-refs containing:
#
#   'src'    - name of the db to copy
#   'target' - destination directory of the copy
#   'tables' - array-ref to list of tables in the db
#   'files'  - array-ref to list of files to be copied
#

my @db_desc = ();
my $tgt_name = undef;
  
if ( $opt{regexp} || $opt{suffix} || @ARGV > 2 ) {
    $tgt_name   = pop @ARGV unless ( exists $opt{suffix} );
    @db_desc = map { { 'src' => $_ } } @ARGV;
}
else {
    usage("Database name to hotcopy not specified") unless ( @ARGV );

    @db_desc = ( { 'src' => $ARGV[0] } );
    if ( @ARGV == 2 ) {
	$tgt_name   = $ARGV[1];
    }
    else {
	$opt{suffix} = "_copy";
    }
}

my $mysqld_help;
my %mysqld_vars;
my $start_time = time;
$0 = $1 if $0 =~ m:/([^/]+)$:;
$opt{quiet} = 0 if $opt{debug};
$opt{allowold} = 1 if $opt{keepold};

# --- connect to the database ---
my $dsn = ";host=localhost";
$dsn .= ";port=$opt{port}" if $opt{port};
$dsn .= ";mysql_socket=$opt{socket}" if $opt{socket};

my $dbh = DBI->connect("dbi:mysql:$dsn;mysql_read_default_group=mysqlhotcopy",
                        $opt{user}, $opt{password},
{
    RaiseError => 1,
    PrintError => 0,
    AutoCommit => 1,
});

# --- check that checkpoint table exists if specified ---
if ( $opt{checkpoint} ) {
    eval { $dbh->do( qq{ select time_stamp, src, dest, msg 
			 from $opt{checkpoint} where 1 != 1} );
       };

    die "Error accessing Checkpoint table ($opt{checkpoint}): $@"
      if ( $@ );
}

# --- get variables from database ---
my $sth_vars = $dbh->prepare("show variables like 'datadir'");
$sth_vars->execute;
while ( my ($var,$value) = $sth_vars->fetchrow_array ) {
    $mysqld_vars{ $var } = $value;
}
my $datadir = $mysqld_vars{'datadir'}
    || die "datadir not in mysqld variables";
$datadir =~ s:/$::;


# --- get target path ---
my ($tgt_dirname, $to_other_database);
$to_other_database=0;
if ($tgt_name =~ m:^\w+$: && @db_desc <= 1)
{
    $tgt_dirname = "$datadir/$tgt_name";
    $to_other_database=1;
}
elsif ($tgt_name =~ m:/: || $tgt_name eq '.') {
    $tgt_dirname = $tgt_name;
}
elsif ( $opt{suffix} ) {
    print "copy suffix $opt{suffix}\n" unless $opt{quiet};
}
else {
    die "Target '$tgt_name' doesn't look like a database name or directory path.\n";
}

# --- resolve database names from regexp ---
if ( defined $opt{regexp} ) {
    my $sth_dbs = $dbh->prepare("show databases");
    $sth_dbs->execute;
    while ( my ($db_name) = $sth_dbs->fetchrow_array ) {
	push @db_desc, { 'src' => $db_name } if ( $db_name =~ m/$opt{regexp}/o );
    }
}

# --- get list of tables to hotcopy ---

my $hc_locks = "";
my $hc_tables = "";
my $num_tables = 0;
my $num_files = 0;

foreach my $rdb ( @db_desc ) {
    my $db = $rdb->{src};
    eval { $dbh->do( "use $db" ); };
    die "Database '$db' not accessible: $@"  if ( $@ );
    my @dbh_tables = $dbh->func( '_ListTables' );

    my $db_dir = "$datadir/$db";
    opendir(DBDIR, $db_dir ) 
      or die "Cannot open dir '$db_dir': $!";
 
    my @db_files = grep { /.+\.\w+$/ } readdir(DBDIR)
      or warn "'$db' is an empty database\n";

    closedir( DBDIR );

    unless ($opt{indices}) {
	@db_files = grep { not /\.(ISM|MYI)$/ } @db_files;
    }

    $rdb->{files}  = [ @db_files ];
    my @hc_tables = map { "$db.$_" } @dbh_tables;
    $rdb->{tables} = [ @hc_tables ];

    $hc_locks .= ", "  if ( length $hc_locks && @hc_tables );
    $hc_locks .= join ", ", map { "$_ READ" } @hc_tables;
    $hc_tables .= ", "  if ( length $hc_tables && @hc_tables );
    $hc_tables .= join ", ", @hc_tables;

    $num_tables += scalar @hc_tables;
    $num_files  += scalar @{$rdb->{files}};
}

# --- resolve targets for copies ---

my @targets = ();

if (length $tgt_name ) {
    # explicit destination directory specified

    # GNU `cp -r` error message
    die "copying multiple databases, but last argument ($tgt_dirname) is not a directory\n"
      if ( @db_desc > 1 && !(-e $tgt_dirname && -d $tgt_dirname ) );

    if ($to_other_database)
    {
      foreach my $rdb ( @db_desc ) {
	$rdb->{target} = "$tgt_dirname";
      }
    }
    else
    {
      die "Last argument ($tgt_dirname) is not a directory\n"
	if (!(-e $tgt_dirname && -d $tgt_dirname ) );
      foreach my $rdb ( @db_desc ) {
	$rdb->{target} = "$tgt_dirname/$rdb->{src}";
      }
    }
  }
else {
  die "Error: expected \$opt{suffix} to exist" unless ( exists $opt{suffix} );
  
  foreach my $rdb ( @db_desc ) {
    $rdb->{target} = "$datadir/$rdb->{src}$opt{suffix}";
  }
}

print Dumper( \@db_desc ) if ( $opt{debug} );

# --- bail out if all specified databases are empty ---

die "No tables to hot-copy" unless ( length $hc_locks );

# --- create target directories ---

my @existing = ();
foreach my $rdb ( @db_desc ) {
    push @existing, $rdb->{target} if ( -d  $rdb->{target} );
}

die "Can't hotcopy to '", join( "','", @existing ), "' because already exist and --allowold option not given.\n"
  if ( @existing && !$opt{allowold} );

retire_directory( @existing ) if ( @existing );

foreach my $rdb ( @db_desc ) {
    my $tgt_dirpath = $rdb->{target};
    if ( $opt{dryrun} ) {
	print "mkdir $tgt_dirpath, 0750\n";
    }
    else {
	mkdir($tgt_dirpath, 0750)
	  or die "Can't create '$tgt_dirpath': $!\n";
    }
}

##############################
# --- PERFORM THE HOT-COPY ---
#
# Note that we try to keep the time between the LOCK and the UNLOCK
# as short as possible, and only start when we know that we should
# be able to complete without error.

# read lock all the tables we'll be copying
# in order to get a consistent snapshot of the database

if ( $opt{checkpoint} ) {
    # convert existing READ lock on checkpoint table into WRITE lock
    unless ( $hc_locks =~ s/$opt{checkpoint}\s+READ/$opt{checkpoint} WRITE/ ) {
	$hc_locks .= ", $opt{checkpoint} WRITE";
    }
}

my $hc_started = time;	# count from time lock is granted

if ( $opt{dryrun} ) {
    print "LOCK TABLES $hc_locks\n";
    print "FLUSH TABLES /*!32323 $hc_tables */\n";
    print "FLUSH LOGS\n" if ( $opt{flushlog} );
}
else {
    my $start = time;
    $dbh->do("LOCK TABLES $hc_locks");
    printf "Locked $num_tables tables in %d seconds.\n", time-$start unless $opt{quiet};
    $hc_started = time;	# count from time lock is granted

    # flush tables to make on-disk copy uptodate
    $start = time;
    $dbh->do("FLUSH TABLES /*!32323 $hc_tables */");
    printf "Flushed tables ($hc_tables) in %d seconds.\n", time-$start unless $opt{quiet};
    $dbh->do( "FLUSH LOGS" ) if ( $opt{flushlog} );
}
    
my @failed = ();

foreach my $rdb ( @db_desc ) {
    my @files = map { "$datadir/$rdb->{src}/$_" } @{$rdb->{files}};
    next unless @files;
    eval { copy_files($opt{method}, \@files, $rdb->{target} ); };
    
    push @failed, "$rdb->{src} -> $rdb->{target} failed: $@"
      if ( $@ );
    
    if ( $opt{checkpoint} ) {
	my $msg = ( $@ ) ? "Failed: $@" : "Succeeded";
	
	eval {
	    $dbh->do( qq{ insert into $opt{checkpoint} (src, dest, msg) 
			  VALUES ( '$rdb->{src}', '$rdb->{target}', '$msg' )
			} ); 
	};
	
	if ( $@ ) {
	    warn "Failed to update checkpoint table: $@\n";
	}
    }
}

if ( $opt{dryrun} ) {
    print "UNLOCK TABLES\n";
    if ( @existing && !$opt{keepold} ) {
	my @oldies = map { $_ . '_old' } @existing;
	print "rm -rf @oldies\n" 
    }
    $dbh->disconnect();
    exit(0);
}
else {
    $dbh->do("UNLOCK TABLES");
}

my $hc_dur = time - $hc_started;
printf "Unlocked tables.\n" unless $opt{quiet};

#
# --- HOT-COPY COMPLETE ---
###########################

$dbh->disconnect;

if ( @failed ) {
    # hotcopy failed - cleanup
    # delete any @targets 
    # rename _old copy back to original

    print "Deleting @targets \n" if $opt{debug};
    rmtree([@targets]);
    if (@existing) {
	print "Restoring @existing from back-up\n" if $opt{debug};
        foreach my $dir ( @existing ) {
	    rename("${dir}_old", $dir )
	      or warn "Can't rename ${dir}_old to $dir: $!\n";
	}
    }

    die join( "\n", @failed );
}
else {
    # hotcopy worked
    # delete _old unless $opt{keepold}

    if ( @existing && !$opt{keepold} ) {
	my @oldies = map { $_ . '_old' } @existing;
	print "Deleting previous copy in @oldies\n" if $opt{debug};
	rmtree([@oldies]);
    }

    printf "$0 copied %d tables (%d files) in %d second%s (%d seconds overall).\n",
	    $num_tables, $num_files,
	    $hc_dur, ($hc_dur==1)?"":"s", time - $start_time
	unless $opt{quiet};
}

exit 0;


# ---

sub copy_files {
    my ($method, $files, $target) = @_;
    my @cmd;
    print "Copying ".@$files." files...\n" unless $opt{quiet};
    if ($method =~ /^s?cp\b/) { # cp or scp with optional flags
	@cmd = ($method);
	# add option to preserve mod time etc of copied files
	# not critical, but nice to have
	push @cmd, "-p" if $^O =~ m/^(solaris|linux)$/;
	# add files to copy and the destination directory
	push @cmd, @$files, $target;
    }
    else {
	die "Can't use unsupported method '$method'\n";
    }

    if ( $opt{dryrun} ) {
	print "@cmd\n";
	next;
    }

    print "Executing '@cmd'\n" if $opt{debug};
    my $cp_status = system @cmd;
    if ($cp_status != 0) {
	die "Error: @cmd failed ($cp_status) while copying files.\n";
    }
}

sub retire_directory {
    my ( @dir ) = @_;

    foreach my $dir ( @dir ) {
	my $tgt_oldpath = $dir . '_old';
	if ( $opt{dryrun} ) {
	    print "rmtree $tgt_oldpath\n" if ( -d $tgt_oldpath );
	    print "rename $dir, $tgt_oldpath\n";
	    next;
	}

	if ( -d $tgt_oldpath ) {
	    print "Deleting previous 'old' hotcopy directory ('$tgt_oldpath')\n" unless $opt{quiet};
	    rmtree([$tgt_oldpath])
	}
	rename($dir, $tgt_oldpath)
	  or die "Can't rename $dir=>$tgt_oldpath: $!\n";
	print "Existing hotcopy directory renamed to '$tgt_oldpath'\n" unless $opt{quiet};
    }
}

__END__

=head1 DESCRIPTION

mysqlhotcopy is designed to make stable copies of live MySQL databases.

Here "live" means that the database server is running and the database
may be in active use. And "stable" means that the copy will not have
any corruptions that could occur if the table files were simply copied
without first being locked and flushed from within the server.

=head1 OPTIONS

=over 4

=item --checkpoint checkpoint-table

As each database is copied, an entry is written to the specified
checkpoint-table.  This has the happy side-effect of updating the
MySQL update-log (if it is switched on) giving a good indication of
where roll-forward should begin for backup+rollforward schemes.

The name of the checkpoint table should be supplied in database.table format.
The checkpoint-table must contain at least the following fields:

=over 4

  time_stamp timestamp not null
  src varchar(32)
  dest varchar(60)
  msg varchar(255)

=back

=item --suffix suffix

Each database is copied back into the originating datadir under
a new name. The new name is the original name with the suffix
appended. 

If only a single db_name is supplied and the --suffix flag is not
supplied, then "--suffix=_copy" is assumed.

=item --allowold

Move any existing version of the destination to a backup directory for
the duration of the copy. If the copy successfully completes, the backup 
directory is deleted - unless the --keepold flag is set.  If the copy fails,
the backup directory is restored.

The backup directory name is the original name with "_old" appended.
Any existing versions of the backup directory are deleted.

=item --keepold

Behaves as for the --allowold, with the additional feature 
of keeping the backup directory after the copy successfully completes.

=item --flushlog

Rotate the log files by executing "FLUSH LOGS" after all tables are
locked, and before they are copied.

=item --regexp pattern

Copy all databases with names matching the pattern.  

=item -?, --help

Display helpscreen and exit

=item -u, --user=#         

user for database login if not current user

=item -p, --password=#     

password to use when connecting to server

=item -P, --port=#         

port to use when connecting to local server

=item -S, --socket=#         

UNIX domain socket to use when connecting to local server

=item  --noindices          

don't copy index files

=item  --method=#           

method for copy (only "cp" currently supported)

=item -q, --quiet              

be silent except for errors

=item  --debug

Debug messages are displayed 

=item -n, --dryrun

Display commands without actually doing them

=back

=head1 WARRANTY

This software is free and comes without warranty of any kind. You
should never trust backup software without studying the code yourself.
Study the code inside this script and only rely on it if I<you> believe
that it does the right thing for you.

Patches adding bug fixes, documentation and new features are welcome.

=head1 TO DO

Allow a list of tables (or regex) to be given on the command line to
enable a logically-related subset of the tables to be hot-copied
rather than force the whole db to be copied in one go.

Extend the above to allow multiple subsets of tables to be specified
on the command line:

  mysqlhotcopy db newdb  t1 t2 /^foo_/ : t3 /^bar_/ : +

where ":" delimits the subsets, the /^foo_/ indicates all tables
with names begining with "foo_" and the "+" indicates all tables
not copied by the previous subsets.

newdb is either another not existing database or a full path to a directory
where we can create a directory 'db'

Add option to lock each table in turn for people who don't need
cross-table integrity.

Add option to FLUSH STATUS just before UNLOCK TABLES.

Add support for other copy methods (eg tar to single file?).

Add support for forthcoming MySQL ``RAID'' table subdirectory layouts.

Add option to only copy the first 65KB of index files. That simplifies
recovery (recovery with no index file at all is complicated).

=head1 AUTHOR

Tim Bunce

Martin Waite - added checkpoint, flushlog, regexp and dryrun options

Ralph Corderoy - Added synonyms for commands
=cut
