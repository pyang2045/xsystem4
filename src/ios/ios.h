/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://gnu.org/licenses/>.
 */

#ifndef SYSTEM4_IOS_H
#define SYSTEM4_IOS_H

// Sets up the working directory and save folder; returns the game folder
// (malloc'd) and in *locale the .po to load, or NULL. See src/ios/ios.m.
char *ios_setup(const char **locale);

// The save folder: Documents/folder (malloc'd).
char *ios_save_dir(const char *folder);

#endif /* SYSTEM4_IOS_H */
