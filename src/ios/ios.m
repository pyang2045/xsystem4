/* iOS start-up: what the command line and the working directory give on macOS.
 *
 * - The working directory becomes the app bundle, where shaders/, fonts/ and locale/ are.
 * - Saves go to Library/Application Support/xsystem4 ($XSYSTEM4_HOME, unless set).
 * - The game is the app's Documents folder (shared in the Files app), or a folder in it,
 *   that holds a .ain file.
 * - Chinese dialogue (locale/zh_CN.po) when the device's first language is Chinese.
 * - Save files put in Documents/SaveData are moved into the save folder at start-up;
 *   files they replace are kept as NAME.bak-YYYYmmdd-HHMMSS.
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
		if ([lang hasPrefix:@"zh"] && [fm fileExistsAtPath:@"locale/zh_CN.po"])
			*locale = "locale/zh_CN.po";

		NSString *docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
				NSUserDomainMask, YES).firstObject;
		char *game = find_game(docs);
		if (!game) {
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game files not found",
				"Copy the game folder (the one holding Haharanman.ain) into this app's "
				"folder in the Files app (On My iPad), then start the app again.", NULL);
			exit(1);
		}
		NOTICE("Game: %s", game);
		return game;
	}
}

void ios_import_saves(const char *save_dir)
{
	@autoreleasepool {
		NSFileManager *fm = NSFileManager.defaultManager;
		NSString *src = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
				NSUserDomainMask, YES).firstObject stringByAppendingPathComponent:@"SaveData"];
		NSArray<NSString *> *names = [fm contentsOfDirectoryAtPath:src error:nil];
		if (!names.count)
			return;
		NSString *dest = [fm stringWithFileSystemRepresentation:save_dir length:strlen(save_dir)];
		[fm createDirectoryAtPath:dest withIntermediateDirectories:YES attributes:nil error:nil];
		NSDateFormatter *fmt = [NSDateFormatter new];
		fmt.dateFormat = @"yyyyMMdd-HHmmss";
		NSString *stamp = [fmt stringFromDate:NSDate.date];
		for (NSString *name in names) {
			NSString *from = [src stringByAppendingPathComponent:name];
			NSString *to = [dest stringByAppendingPathComponent:name];
			BOOL is_dir;
			if ([name hasPrefix:@"."] || ![fm fileExistsAtPath:from isDirectory:&is_dir] || is_dir)
				continue;
			if ([fm fileExistsAtPath:to])
				[fm moveItemAtPath:to toPath:[to stringByAppendingFormat:@".bak-%@", stamp] error:nil];
			NSError *err = nil;
			if ([fm moveItemAtPath:from toPath:to error:&err])
				NOTICE("Imported save %s", name.UTF8String);
			else
				WARNING("Importing save %s: %s", name.UTF8String, err.localizedDescription.UTF8String);
		}
		if (![fm contentsOfDirectoryAtPath:src error:nil].count)
			[fm removeItemAtPath:src error:nil];
	}
}
