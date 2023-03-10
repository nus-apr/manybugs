#!/usr/bin/env perl
BEGIN {
	# add current source dir to the include-path
	# we need this for make distcheck
	(my $srcdir = $0) =~ s,/[^/]+$,/,;
	unshift @INC, $srcdir;
}

use strict;
use IO::Socket;
use Test::More tests => 3;
use LightyTest;
use warnings;
use strict;

my $tf = LightyTest->new();
my $t;

ok($tf->start_proc == 0, "Starting lighttpd") or die();
while (<>){
$t->{REQUEST}  = ( <<EOF
$_;
EOF
 );
$t->{RESPONSE} = [ { 'HTTP-Protocol' => 'HTTP/1.0', 'HTTP-Status' => 505 } ];
ok($tf->handle_http($t) == 0, 'unknown protocol');

}

ok($tf->stop_proc == 0, "Stopping lighttpd");