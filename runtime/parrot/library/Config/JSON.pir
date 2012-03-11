# Copyright (C) 2007-2009, Parrot Foundation.

=head1 Config::JSON

Provides a simple wrapper to read and write JSON config files.

=cut

.namespace [ 'Config' ; 'JSON' ]

=head2 ReadConfig(filename)

Given a filename, parse the file containing valid JSON and return a
PMC containing the data.

If the data is not valid, an exception will be thrown.

=cut

.sub 'ReadConfig'
    .param string filename

    # Slurp in the file
    .local string text
    .local pmc fh

    fh = new ['FileHandle']
    fh.'open'(filename, 'r')
    if fh goto slurp_file
    $P0 = new 'Exception'
    $S0 = concat "Can't open file: ", filename
    $P0 = $S0
    throw $P0

  slurp_file:
    fh.'encoding'('utf8')
    text = fh.'readall'()

    # Convert the text to an object and return it.
    .local pmc json, code
    $P0 = load_language 'data_json'
    $I0 = $P0.'is_initialized'('load')
    if $I0 goto done_initialization

    $P1 = $P0.'subs_by_tag'('load')
    $P2 = iter $P1
  loop_top:
    unless $P2 goto loop_bottom
    $P3 = shift $P2
    $P3()
    goto loop_top
  loop_bottom:
    $P0.'mark_initialized'('load')
  done_initialization:
    json = compreg 'data_json'
    code = json.'compile'(text)
    .tailcall code()
.end

=head2 WriteConfig(config, filename, ?:compact)

Given a PMC and a filename, render the pmc as JSON and store the contents
into the named file, overwriting the existing contents.

Any exceptions generated by the conversion or writing to the file will
be passed through.

If a true value is passed for the optional named parameter 'compact', then
the rendered JSON will not be formatted. The default is false.

=cut

.sub 'WriteConfig'
    .param pmc    config
    .param string filename
    .param int    compact     :optional :named('compact')
    .param int    has_compact :opt_flag

    if has_compact goto done_options
    compact = 0

  done_options:
    .local int expanded
    expanded = not compact

    # render the object as a string.
    $P0 = load_bytecode 'JSON.pbc'
    $I0 = $P0.'is_initialized'('load')
    if $I0 goto done_initialization

    $P1 = $P0.'subs_by_tag'('load')
    $P2 = iter $P1
  loop_top:
    unless $P2 goto loop_bottom
    $P3 = shift $P2
    $P3()
    goto loop_top
  loop_bottom:
    $P0.'mark_initialized'('load')
  done_initialization:

    .local string output
    output = _json( config, expanded )

    # write out the file..
    $P1 = new ['FileHandle']
    $P1.'open'(filename, 'w')
    print $P1, output
    print $P1, "\n"
    $P1.'close'()

.end

# Local Variables:
#   mode: pir
#   fill-column: 100
# End:
# vim: expandtab shiftwidth=4 ft=pir:
