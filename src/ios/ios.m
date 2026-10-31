/* iOS start-up: what the command line and the working directory give on macOS.
 *
 * - The working directory becomes the app bundle, where shaders/, fonts/ and locale/ are.
 * - $XSYSTEM4_HOME (config files), unless set, is Library/Application Support/xsystem4.
 * - The game is the app's Documents folder (shared in the Files app), or a folder in it,
 *   that holds a .ain file.
 * - Saves go to Documents/SAVE FOLDER (the .ini's SaveFolder, SaveData by default), where
 *   the Files app and Finder show them.
 * - A translation when the device's first language starts with a key of the Info.plist
 *   dictionary XSys4Translations (language prefix -> .po file in the bundle, e.g. "zh" ->
 *   "locale/zh_CN.po"); the app's packaging sets it.
 *
 * This program is free software; you can redistribute it and/or modify
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

#import <Foundation/Foundation.h>
#include <SDL.h>
#include <stdlib.h>
#include <unistd.h>

#include "system4.h"
#include "ios.h"

static bool has_ain(NSString *dir)
{
	NSArray<NSString *> *names = [NSFileManager.defaultManager contentsOfDirectoryAtPath:dir error:nil];
	for (NSString *name in names) {
		if ([name.pathExtension caseInsensitiveCompare:@"ain"] == NSOrderedSame)
			return true;
	}
	return false;
}

static char *find_game(NSString *docs)
{
	if (has_ain(docs))
		return strdup(docs.fileSystemRepresentation);
	NSArray<NSString *> *names = [[NSFileManager.defaultManager contentsOfDirectoryAtPath:docs error:nil]
		sortedArrayUsingSelector:@selector(compare:)];
	for (NSString *name in names) {
		NSString *dir = [docs stringByAppendingPathComponent:name];
		BOOL is_dir;
		if ([NSFileManager.defaultManager fileExistsAtPath:dir isDirectory:&is_dir] && is_dir && has_ain(dir))
			return strdup(dir.fileSystemRepresentation);
	}
	return NULL;
}

char *ios_setup(const char **locale)
{
	@autoreleasepool {
		NSFileManager *fm = NSFileManager.defaultManager;
		chdir(NSBundle.mainBundle.resourcePath.fileSystemRepresentation);

		if (!getenv("XSYSTEM4_HOME")) {
			NSString *support = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory,
					NSUserDomainMask, YES).firstObject stringByAppendingPathComponent:@"xsystem4"];
			[fm createDirectoryAtPath:support withIntermediateDirectories:YES attributes:nil error:nil];
			setenv("XSYSTEM4_HOME", support.fileSystemRepresentation, 1);
		}

		*locale = NULL;
		NSString *lang = NSLocale.preferredLanguages.firstObject;
		NSDictionary *translations = [NSBundle.mainBundle objectForInfoDictionaryKey:@"XSys4Translations"];
		if ([translations isKindOfClass:NSDictionary.class]) {
			for (NSString *prefix in translations) {
				NSString *po = translations[prefix];
				if ([lang hasPrefix:prefix] && [po isKindOfClass:NSString.class] && [fm fileExistsAtPath:po]) {
					*locale = strdup(po.fileSystemRepresentation);
					break;
				}
			}
		}

		NSString *docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
				NSUserDomainMask, YES).firstObject;
		char *game = find_game(docs);
		if (!game) {
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game files not found",
				"Copy the game folder (the one holding the .ain file) into this app's "
				"folder in the Files app (On My iPad), then start the app again.", NULL);
			exit(1);
		}
		NOTICE("Game: %s", game);
		return game;
	}
}

char *ios_save_dir(const char *folder)
{
	@autoreleasepool {
		NSString *docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
				NSUserDomainMask, YES).firstObject;
		NSString *name = [NSString stringWithUTF8String:folder];
		return strdup([docs stringByAppendingPathComponent:name].fileSystemRepresentation);
	}
}
