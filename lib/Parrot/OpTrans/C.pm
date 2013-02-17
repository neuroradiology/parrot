# Copyright (C) 2002-2010, Parrot Foundation.
# $Id$

=head1 NAME

Parrot::OpTrans::C - Ops to C Code Generation

=head1 DESCRIPTION

C<Parrot::OpTrans::C> inherits from C<Parrot::OpTrans> to provide a
function-based (slow or fast core) run loop.

=head2 Instance Methods

=over 4

=cut

package Parrot::OpTrans::C;

use strict;
use warnings;

use base qw( Parrot::OpTrans );

=item C<core_type()>

Returns C<PARROT_FUNCTION_CORE>.

=cut

sub core_type {
    return 'PARROT_FUNCTION_CORE';
}

=item C<defines()>

Returns the C C<#define> macros for register access etc.

=cut

sub defines {
    my $type = __PACKAGE__;
    return <<END;

/* defines - $0 -> $type */
#define REL_PC     ((size_t)(cur_opcode - (opcode_t *)interp->code->base.data))
#define CUR_OPCODE cur_opcode
#define IREG(i) REG_INT(interp, cur_opcode[i])
#define NREG(i) REG_NUM(interp, cur_opcode[i])
#define PREG(i) REG_PMC(interp, cur_opcode[i])
#define SREG(i) REG_STR(interp, cur_opcode[i])
#define ICONST(i) cur_opcode[i]
#define NCONST(i) Parrot_pcc_get_num_constants(interp, interp->ctx)[cur_opcode[i]]
#define SCONST(i) Parrot_pcc_get_str_constants(interp, interp->ctx)[cur_opcode[i]]
#undef  PCONST
#define PCONST(i) Parrot_pcc_get_pmc_constants(interp, interp->ctx)[cur_opcode[i]]

static int get_op(PARROT_INTERP, const char * name, int full);
END
}

=item C<gen_goto($where)>

Reimplements the superclass method so that C<$where> is suitably cast.

=cut

sub gen_goto {
    my ( $self, $where_str ) = @_;

    return "    return $where_str";
}

=item C<expr_address($address)>

Returns the C code for C<ADDRESS($address)>. Called by C<goto_address()>.

=cut

sub expr_address {
    my ( $self, $addr ) = @_;

    return $addr;
}

=item C<expr_offset($offset)>

Returns the C code for C<OFFSET($offset)>. Called by C<goto_offset()>.

=cut

sub expr_offset {
    my ( $self, $offset ) = @_;

    return "cur_opcode + $offset";
}

our %arg_maps = (
    'op' => "cur_opcode[%ld]",

    'i'  => "IREG(%ld)",
    'n'  => "NREG(%ld)",
    'p'  => "PREG(%ld)",
    's'  => "SREG(%ld)",
    'k'  => "PREG(%ld)",
    'ki' => "IREG(%ld)",

    'ic'  => "cur_opcode[%ld]",
    'nc'  => "NCONST(%ld)",
    'pc'  => "CONST(%ld)->u.key",
    'sc'  => "CONST(%ld)->u.string",
    'kc'  => "CONST(%ld)->u.key",
    'kic' => "cur_opcode[%ld]"
);

=item C<access_arg($type, $value, $op)>

Returns the C code for the specified op argument type (see
C<Parrot::OpTrans>) and value. C<$op> is an instance of C<Parrot::Op>.

=cut

sub access_arg {
    my ( $self, $type, $num, $op ) = @_;

    die "Unrecognized type '$type' for num '$num' in opcode @{[$op->full_name]}"
        unless exists $arg_maps{$type};

    return sprintf( $arg_maps{$type}, $num );
}

=item C<restart_offset($offset)>

Returns the C code for C<restart OFFSET($offset)>.

=cut

sub restart_offset {
    my ( $self, $offset ) = @_;

    return "interp->resume_offset = REL_PC + $offset; interp->resume_flag = 1";
}

=item C<restart_address($address)>

Returns the C code for C<restart ADDRESS($address)>.

=cut

sub restart_address {
    my ( $self, $addr ) = @_;

    return "interp->resume_offset = $addr; interp->resume_flag = 1";
}

=back

=head1 SEE ALSO

=over 4

=item C<Parrot::OpTrans>

=back

=cut

1;

# Local Variables:
#   mode: cperl
#   cperl-indent-level: 4
#   fill-column: 100
# End:
# vim: expandtab shiftwidth=4:
