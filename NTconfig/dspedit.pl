#!/usr/bin/perl -w

my $ProjectDir;
my $ProjectFilename;
my $ProjectName;
my $EditFilename;
my $ProperFilename;
my $command;
my $tmpProj = "_newproj_.dsp";

if(@ARGV < 2)
{
  Usage();
  die "Incorrect Usage";
}

sub Usage
{
  print "dspedit <command> <project> <srcfile>\n";
}

$command         = $ARGV[0];
$ProjectFilename = $ARGV[1];
$EditFilename    = $ARGV[2];

if( $command =~ /add/ or $command =~ /remove/ )
{
  if( @ARGV < 3)
  {
    Usage();
    die "Incorrect Usage";
  }
}

print "Command: $command\n";
print "ProjectFile: $ProjectFilename\n";

if( $command =~ /add/ or $command =~ /remove/ )
{
  $ProperFilename = MakeProperFilename($EditFilename);
  print "EditFilename: $EditFilename\n";
  print "ProperFilename: $ProperFilename\n";

  open (OUTPUT, "> $tmpProj")
  or die "Can't open $tmpProj";
}

open (INPUT, $ProjectFilename)
  or die "Can't open project $ProjectFilename";

my $NewSourceFileList;

#$projectHeader    = "# Microsoft Developer Studio Project File";
#$targetType       = "# TARGTYPE";
#$beginProject     = "# Begin Project";
#$endProject       = "# End Project";
#$beginTarget      = "# Begin Target";
$endTarget        = "# End Target";
#$beginGroup       = "# Begin Group";
#$endGroup         = "# End Group";
$beginSource      = "# Begin Source File";
$endSource        = "# End Source File";
$sourceEq         = "SOURCE=";

#print "Searching for all source files\n";
my @AllSourceFiles;
while ( my $line = <INPUT> )
{
  MyChomp($line);
  if( $line =~ /($sourceEq)(.+)/ )
  {
    #print "$2\n";
    push( @AllSourceFiles, $2 );
  }
}

seek(INPUT,0,0); 

#print "Running Command ";
if( $command =~ /list/ )
{
  #print "List\n";
  List();
}
elsif( $command =~ /add/ )
{
  #print "Add\n";
  AddFile();
}
elsif( $command =~ /remove/ )
{
  #print "Remove\n";
  RemoveFile();
}
else
{
  Usage();
  die "Incorrect Usage";
}

close OUTPUT;
close INPUT;

sub List
{
  #print "Listing all files in project $ProjectFilename\n";
  foreach $line (@AllSourceFiles)
  {
    print "$line\n";
  }
}

sub AddFile
{
  print "Adding $ProperFilename to project $ProjectFilename\n";
  # see if file already exists
  foreach $file (@AllSourceFiles)
  {
    if($file =~ /\Q$ProperFilename\E/)
    {
      RemoveTempFiles();
      die "SrcFile $ProperFilename already exists in $ProjectFilename";
    }
  }

  InsertIntoProjectFile($ProperFilename);

  print "Done Adding file\n";

  MoveFiles();
}

sub RemoveFile
{
  print "Removing $ProperFilename from $ProjectFilename\n";
  my $srcline;
  my $fileEdited = 0;
  while( $line = <INPUT> )
  {
    if( $line =~ /\Q$beginSource\E/ )
    {
      print "Found Begin Source\n";
      $srcline = $line;
      while( not $srcline =~ /\Q$endSource\E/)
      {
        $srcline .= <INPUT>;
      }
      print "Found End Source\n";

      if( LineContainsFile($srcline) )
      {
        # don't output the block to new project
        print "Found block containing $ProperFilename\n";

        $fileEdited = 1;
      }
      else
      {
        print OUTPUT $srcline;
      }
    }
    else
    {
      #print "Found non source block\n";
      print OUTPUT $line;
    }
  }

  # Change project file only if there were changes
  if($fileEdited) {
    MoveFiles();
  }
  else {
    print "$ProperFilename not found in $ProjectFilename\n";
    RemoveTempFiles();
  }
}

sub LineContainsFile
{
  return ($_[0] =~ /\Q$ProperFilename\E/);
}

sub InsertIntoProjectFile
{
  $srcFilename = $_[0];
  #print "Inserting $srcFilename into project\n";
  # Try to leave the project file as unchanged as possible
  while( $line = <INPUT> )
  {
    if( $line =~ /$endTarget/ )
    {
      print "Found end target\n";
      $insert = CreateSourceFileBlock($srcFilename);
      print OUTPUT $insert;
      print OUTPUT $line;
    }
    else
    {
      print OUTPUT $line;
    }
  }
}

sub PathToDos
{
  $str = $_[0];
  $str =~ s/\//\\/g;
  return $str;
}

sub MakeProperFilename
{
  $filename = $_[0];
  # convert to dos pathname
  $filename = PathToDos($filename);

  # make sure path has current directory at front
  if( not $filename =~ m/^.\.\\/ )
  {
    print "Not absolute path\n";
    $abpath = "\.\.\\";
    $filename = $abpath . $filename;
  }

  return $filename;
}

sub CreateSourceFileBlock
{
  $srcName = $_[0];
  $srcblock = "# Begin Source File\r\n\r\nSOURCE=$srcName\r\n# End Source File\r\n";
  print "Created Source Block\n$srcblock\n";
  return $srcblock;
}

sub MyChomp
{
  $_[0] =~ s/^(.*?)(?:\x0D\x0A|\x0A|\x0D|\x0C|\x{2028}|\x{2029})/$1/s;
}

sub MoveFiles
{
  #print "Moving Files\n";
  $backupFile = $ProjectFilename . ".backup";
  if( not rename $ProjectFilename,  $backupFile ) {
    warn "rename $ProjectFilename to $backupFile failed: $!\n";
  }
  if( not rename $tmpProj, $ProjectFilename ) {
    warn "rename $tmpProj to $ProjectFilename failed: $!\n";
  }
}

sub RemoveTempFiles
{
  #print "Removing temp files\n";
  if( -e $tmpProj )
  {
    unlink $tmpProj;
  }
}

