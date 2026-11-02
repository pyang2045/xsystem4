/* iOS start-up: what the command line and the working directory give on macOS.
 *
 * - The working directory becomes the app bundle, where shaders/, fonts/ and locale/ are.
 * - $XSYSTEM4_HOME (config files), unless set, is Library/Application Support/xsystem4.
 * - The games are the app's Documents folder (shared in the Files app), or folders in it,
 *   that hold a .ain file, each with its languages in locale/<lang>.po. One game with no
 *   language starts at once; otherwise a list asks, at every launch.
 * - Saves go to Documents/SAVE FOLDER/GAME FOLDER (SAVE FOLDER is the .ini's SaveFolder,
 *   SaveData by default), where the Files app and Finder show them. A game's languages
 *   share them.
 * - A translation's characters draw with fonts/NotoSansCJKsc-Regular.otf if the app has it.
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
#include "translation.h"
#include "xsystem4.h"

static bool has_ain(NSString *dir)
{
	NSArray<NSString *> *names = [NSFileManager.defaultManager contentsOfDirectoryAtPath:dir error:nil];
	for (NSString *name in names) {
		if ([name.pathExtension caseInsensitiveCompare:@"ain"] == NSOrderedSame)
			return true;
	}
	return false;
}

@interface IOSEntry : NSObject
@property NSString *title, *dir, *po;
@end
@implementation IOSEntry
@end

static NSString *language_title(NSString *lang)
{
	return [lang hasPrefix:@"zh"] ? @"中文" : lang;
}

static void add_game(NSMutableArray<IOSEntry *> *entries, NSString *dir, NSString *title)
{
	IOSEntry *entry = [IOSEntry new];
	entry.title = title;
	entry.dir = dir;
	[entries addObject:entry];
	NSString *locale = [dir stringByAppendingPathComponent:@"locale"];
	NSArray<NSString *> *names = [[NSFileManager.defaultManager contentsOfDirectoryAtPath:locale error:nil]
		sortedArrayUsingSelector:@selector(compare:)];
	for (NSString *name in names) {
		if ([name.pathExtension caseInsensitiveCompare:@"po"] != NSOrderedSame)
			continue;
		entry = [IOSEntry new];
		entry.title = [NSString stringWithFormat:@"%@ (%@)", title,
			language_title(name.stringByDeletingPathExtension)];
		entry.dir = dir;
		entry.po = [locale stringByAppendingPathComponent:name];
		[entries addObject:entry];
	}
}

static NSArray<IOSEntry *> *find_games(NSString *docs)
{
	NSMutableArray<IOSEntry *> *entries = [NSMutableArray array];
	if (has_ain(docs))
		add_game(entries, docs, @"Game");
	NSArray<NSString *> *names = [[NSFileManager.defaultManager contentsOfDirectoryAtPath:docs error:nil]
		sortedArrayUsingSelector:@selector(compare:)];
	for (NSString *name in names) {
		NSString *dir = [docs stringByAppendingPathComponent:name];
		BOOL is_dir;
		if ([NSFileManager.defaultManager fileExistsAtPath:dir isDirectory:&is_dir] && is_dir && has_ain(dir))
			add_game(entries, dir, name);
	}
	return entries;
}

static IOSEntry *choose_entry(NSArray<IOSEntry *> *entries)
{
	if (entries.count == 1)
		return entries[0];
	const char *pick = dev_getenv("XSYS4_IOS_PICK");
	if (pick && (NSUInteger)atoi(pick) < entries.count)
		return entries[atoi(pick)];
	SDL_MessageBoxButtonData *buttons = xcalloc(entries.count, sizeof(SDL_MessageBoxButtonData));
	for (NSUInteger i = 0; i < entries.count; i++) {
		buttons[i].buttonid = (int)i;
		buttons[i].text = entries[i].title.UTF8String;
	}
	SDL_MessageBoxData box = {
		.flags = SDL_MESSAGEBOX_INFORMATION,
		.title = "Choose a game",
		.message = "",
		.numbuttons = (int)entries.count,
		.buttons = buttons,
	};
	int chosen = -1;
	if (SDL_ShowMessageBox(&box, &chosen) < 0 || chosen < 0)
		chosen = 0;
	free(buttons);
	return entries[chosen];
}

extern char *translation_font_path;
static NSString *save_subfolder;

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

		if ([fm fileExistsAtPath:@"fonts/NotoSansCJKsc-Regular.otf"])
			translation_font_path = "fonts/NotoSansCJKsc-Regular.otf";

		NSString *docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
				NSUserDomainMask, YES).firstObject;
		NSArray<IOSEntry *> *entries = find_games(docs);
		if (!entries.count) {
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game files not found",
				"Copy a game folder (the one holding the .ain file) into this app's "
				"folder in the Files app (On My iPad), then start the app again.", NULL);
			exit(1);
		}
		IOSEntry *entry = choose_entry(entries);
		*locale = entry.po ? strdup(entry.po.fileSystemRepresentation) : NULL;
		char *game = strdup(entry.dir.fileSystemRepresentation);
		save_subfolder = [entry.dir isEqualToString:docs] ? @"" : entry.dir.lastPathComponent;
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
		NSString *dir = [[docs stringByAppendingPathComponent:name] stringByAppendingPathComponent:save_subfolder];
		return strdup(dir.fileSystemRepresentation);
	}
}
