#!/usr/bin/env perl

# Copyright (c) 2015, Jason McCarver (slam@parasite.cc) All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

#
# A very simple example script that processes input
# on stdin from xclippipe.
#
# For example, I use this script with xclippipe similar to:
#   xclippipe -bg 'dark green' -above -borderless -sticky +stdout -geoemtry 50x25+1920+1175 -action.exit ctrl+d -action.primary button2 -action.clipboard 'ctrl+v|button3' -run /usr/local/bin/xclippipe_recv
#
# except I have all the options set in my .Xresources, like this:
#   xclippipe.geometry: 50x25+1920+1175
#   xclippipe.background: dark green
#   xclippipe.above: on
#   xclippipe.action.exit: ctrl+d
#   xclippipe.action.primary: button2
#   xclippipe.action.clipboard: ctrl+v|button3
#   xclippipe.stdout: off
#   xclippipe.borderless: on
#   xclippipe.sticky: on
#   xclippipe.run: /usr/local/bin/xclippipe_recv

#
# When pasted an http or https URL it will launch chrome with that URL
#

use strict;
use warnings;

my @input = <>;
chomp $input[$#input] if @input;

print STDERR "xclippipe_recv: selection=$ENV{XCP_SELECTION}\n";

if ($input[0] =~ /https?:/i) {
    print STDERR "xclippipe_recv: running google-chrome-stable '@input'\n";
    exec "google-chrome-stable", @input;
}

exit 0
