#!/usr/bin/perl -w

use IO::Socket;
use Net::Ping;
use Thread;

my @tt;

sub syn_func {
    my $port = shift;
    $syn=Net::Ping->new("syn"); 
    $syn->{ port_num }=$port; 
    $syn->ping($host); 
    if($syn->ack){ 
        print "$port open!\n";
        $syn->close;
    } 
};

$host = $ARGV[0];
chomp($host);
print("host:$host\n");
$file = "ports.txt";
open(FILE,"$file") || die "Can't find $file\n" ;


while($port = <FILE>){ 
    chomp($port);
    my $tt_1 = Thread->create(\&syn_func, $port) || die "$!";
    if($tt_1){
        push(@tt, \$tt_1);
}
}
foreach my $t1 (@tt) {
        $$t1->join;
}

print("program close!\n");
