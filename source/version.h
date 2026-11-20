/*
 * version.h
 *
 * The firmware's version: the one definition that $POEI,VER and $POEI,ID both
 * report.
 */

#ifndef VERSION_H_
#define VERSION_H_

/*
 * Exactly X.Y.Z: three whole numbers and nothing else. No quote marks, no
 * spaces, no 'v'. SPARC CS reads the field that strictly and refuses a board
 * whose version it cannot read, so this is a TEXT printed with %s, never a bare
 * token run through a stringizing macro (that would print the quote marks of a
 * quoted definition).
 *
 * What the three numbers mean, agreed with the SPARC CS side:
 *   X  changes only when a message SPARC CS relies on changes in a way an older
 *      SPARC CS cannot read. SPARC CS uses a board only when X is one it knows.
 *   Y  changes when a command or a field is added that an older SPARC CS still
 *      works with.
 *   Z  changes for a fix that changes no message.
 * MEAS and RTD keep their fields exactly, in number and in order, for every
 * 2.y.z: installed SPARC CS builds match those lines by their field count.
 */
#define FW_VERSION "2.0.0"

#endif /* VERSION_H_ */
